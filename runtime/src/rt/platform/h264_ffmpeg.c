/* SPDX-License-Identifier: GPL-2.0-or-later
 * Native H.264 movie decoder. Link real FFmpeg parser/decoder/swscale libraries;
 * missing codecs are errors, never a successful blank-frame backend.
 */
#include "../recomp.h"
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libswscale/swscale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define DECODERS 4
#define INPUT_LIMIT (32u * 1024u * 1024u)
typedef struct {
    int used, failed, parser_flushed, draining, finished;
    AVCodecContext *codec;
    AVCodecParserContext *parser;
    AVFrame *frame;
    AVPacket *packet;
    struct SwsContext *scale;
    uint8_t *ps, *es;
    uint32_t ps_size, ps_at, es_size, es_at, parser_pending;
} MovieDecoder;
static MovieDecoder decoders[DECODERS];
static void failed(MovieDecoder *d, const char *stage) {
    if (!d->failed) fprintf(stderr, "native H264 decoder failed at %s\n", stage);
    d->failed = 1;
}
static int append(uint8_t **buffer, uint32_t *size, const uint8_t *data, uint32_t bytes) {
    if (bytes > INPUT_LIMIT || *size > INPUT_LIMIT - bytes) return 0;
    uint8_t *next = realloc(*buffer, (size_t)*size + bytes + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!next) return 0;
    memcpy(next + *size, data, bytes);
    *size += bytes;
    memset(next + *size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
    *buffer = next;
    return 1;
}
static void compact(uint8_t *buffer, uint32_t *size, uint32_t *at) {
    if (*at) { memmove(buffer, buffer + *at, *size - *at); *size -= *at; *at = 0; }
}
/* Match the upstream PSMF MPEG-PS packet semantics; retain incomplete packets. */
static int parse_ps(MovieDecoder *d) {
    const uint8_t *p = d->ps + d->ps_at;
    uint32_t n = d->ps_size - d->ps_at;
    if (n < 4) return 0;
    if (p[0] || p[1] || p[2] != 1) { ++d->ps_at; return 1; }
    unsigned code = p[3];
    if (code == 0xba) {
        if (n < 14 || n < 14u + (p[13] & 7u)) return 0;
        d->ps_at += 14u + (p[13] & 7u); return 1;
    }
    if (code == 0xb9 || code < 0xbb) { d->ps_at += 4; return 1; }
    if (n < 6) return 0;
    uint32_t length = ((uint32_t)p[4] << 8) | p[5], total = length + 6;
    if (n < total) return 0;
    if (code >= 0xe0 && code <= 0xef && length >= 3 && (p[6] & 0xc0) == 0x80) {
        uint32_t header = 9u + p[8];
        if (header > total) { failed(d, "PES header bounds"); return 0; }
        compact(d->es, &d->es_size, &d->es_at);
        if (!append(&d->es, &d->es_size, p + header, total - header)) { failed(d, "ES input budget"); return 0; }
    }
    d->ps_at += total;
    return 1;
}
void sr_h264_destroy(int id) {
    if (id < 0 || id >= DECODERS) return;
    MovieDecoder *d = &decoders[id];
    avcodec_free_context(&d->codec);
    if (d->parser) av_parser_close(d->parser);
    av_frame_free(&d->frame); av_packet_free(&d->packet);
    sws_freeContext(d->scale); free(d->ps); free(d->es);
    memset(d, 0, sizeof(*d));
}
int sr_h264_create(void) {
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) { fprintf(stderr, "native game: FFmpeg H264 decoder is unavailable\n"); return -1; }
    for (int i = 0; i < DECODERS; ++i) if (!decoders[i].used) {
        MovieDecoder *d = &decoders[i];
        d->used = 1;
        d->codec = avcodec_alloc_context3(codec); d->parser = av_parser_init(AV_CODEC_ID_H264);
        d->frame = av_frame_alloc(); d->packet = av_packet_alloc();
        if (!d->codec || !d->parser || !d->frame || !d->packet) { sr_h264_destroy(i); return -1; }
        d->codec->thread_count = 1; d->codec->thread_type = 0;
        d->codec->max_pixels = 512 * 512;
        if (avcodec_open2(d->codec, codec, NULL) < 0) { sr_h264_destroy(i); return -1; }
        return i;
    }
    return -1;
}
void sr_h264_feed(int id, const uint8_t *data, uint32_t bytes) {
    if (id < 0 || id >= DECODERS || !decoders[id].used || !data || !bytes) return;
    MovieDecoder *d = &decoders[id];
    if (d->failed || d->draining) return;
    compact(d->ps, &d->ps_size, &d->ps_at);
    if (!append(&d->ps, &d->ps_size, data, bytes)) { failed(d, "PS input budget"); return; }
    while (!d->failed && parse_ps(d)) { }
}
uint32_t sr_h264_pending_bytes(int id) {
    if (id < 0 || id >= DECODERS || !decoders[id].used) return 0;
    MovieDecoder *d = &decoders[id];
    uint32_t ps = d->ps_size - d->ps_at;
    return (ps >= 2048 ? ps : 0) + d->es_size - d->es_at + d->parser_pending + (uint32_t)d->packet->size;
}
int sr_h264_finished(int id) {
    if (id < 0 || id >= DECODERS || !decoders[id].used || decoders[id].failed) return -1;
    return decoders[id].finished;
}
static int output(MovieDecoder *d, uint8_t *destination, int stride, int mode) {
    int width = d->frame->width, height = d->frame->height;
    if (!destination || !g_mem || width <= 0 || width > 512 || height <= 0 || height > 272 ||
        stride < width || stride > 2048 || mode < 0 || mode > 3) return -1;
    size_t bytes = (size_t)stride * height * (mode == 3 ? 4u : 2u);
    uintptr_t at = (uintptr_t)destination;
    uintptr_t ram = (uintptr_t)g_mem, vram = ram - 0x04000000u;
    if (!((at >= ram && at < ram + 0x02000000u && bytes <= ram + 0x02000000u - at) ||
          (at >= vram && at < vram + 0x00200000u && bytes <= vram + 0x00200000u - at))) return -1;
    uint8_t *rgba = malloc((size_t)width * height * 4 + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!rgba) return -1;
    d->scale = sws_getCachedContext(d->scale, width, height, (enum AVPixelFormat)d->frame->format,
        width, height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);
    if (!d->scale) { free(rgba); return -1; }
    uint8_t *planes[4] = {rgba, NULL, NULL, NULL};
    int strides[4] = {width * 4, 0, 0, 0};
    if (sws_scale(d->scale, (const uint8_t *const *)d->frame->data, d->frame->linesize,
                  0, height, planes, strides) != height) { free(rgba); return -1; }
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const uint8_t *p = rgba + ((size_t)y * width + x) * 4;
        size_t offset = (size_t)y * stride + x;
        if (mode == 3) memcpy(destination + offset * 4, p, 4);
        else {
            uint16_t pixel = mode == 0 ? (uint16_t)((p[0] >> 3) | ((p[1] >> 2) << 5) | ((p[2] >> 3) << 11)) :
                mode == 1 ? (uint16_t)((p[0] >> 3) | ((p[1] >> 3) << 5) | ((p[2] >> 3) << 10) | ((p[3] >> 7) << 15)) :
                (uint16_t)((p[0] >> 4) | ((p[1] >> 4) << 4) | ((p[2] >> 4) << 8) | ((p[3] >> 4) << 12));
            memcpy(destination + offset * 2, &pixel, 2);
        }
    }
    free(rgba); return 1;
}
int sr_h264_frame(int id, int eos, uint8_t *destination, int stride, int mode) {
    if (id < 0 || id >= DECODERS || !decoders[id].used) return -1;
    MovieDecoder *d = &decoders[id];
    if (d->failed) return -1;
    if (eos && d->ps_size - d->ps_at > 3) {
        failed(d, "truncated final MPEG-PS element"); return -1;
    }
    for (int guard = 0; guard < 4096; ++guard) {
        int status = avcodec_receive_frame(d->codec, d->frame);
        if (status == 0) {
            int result = output(d, destination, stride, mode);
            av_frame_unref(d->frame);
            if (result < 0) failed(d, "frame output");
            return result;
        }
        if (status == AVERROR_EOF) { d->finished = 1; return 0; }
        if (status != AVERROR(EAGAIN)) { failed(d, "receive frame"); return -1; }
        if (d->packet->size) {
            status = avcodec_send_packet(d->codec, d->packet);
            if (status == AVERROR(EAGAIN)) continue;
            if (status < 0) { failed(d, "send packet"); return -1; }
            av_packet_unref(d->packet); continue;
        }
        uint8_t *packet = NULL; int packet_bytes = 0;
        if (d->es_at < d->es_size) {
            int consumed = av_parser_parse2(d->parser, d->codec, &packet, &packet_bytes,
                d->es + d->es_at, (int)(d->es_size - d->es_at), AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (consumed < 0) { failed(d, "parse H264"); return -1; }
            if ((unsigned)consumed > INPUT_LIMIT - d->parser_pending) { failed(d, "parser input budget"); return -1; }
            d->es_at += (unsigned)consumed; d->parser_pending += (unsigned)consumed;
            if (!consumed && !packet_bytes) return 0;
        } else if (eos && !d->parser_flushed) {
            status = av_parser_parse2(d->parser, d->codec, &packet, &packet_bytes,
                                     NULL, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (status < 0) { failed(d, "flush parser"); return -1; }
            d->parser_flushed = 1;
        } else if (eos && !d->draining) {
            status = avcodec_send_packet(d->codec, NULL);
            if (status == AVERROR(EAGAIN)) continue;
            if (status < 0 && status != AVERROR_EOF) { failed(d, "drain decoder"); return -1; }
            d->draining = 1; d->parser_pending = 0; continue;
        } else return 0;
        if (packet_bytes > 0) {
            if ((unsigned)packet_bytes > INPUT_LIMIT) { failed(d, "parsed packet budget"); return -1; }
            if (av_new_packet(d->packet, packet_bytes) < 0) { failed(d, "packet allocation"); return -1; }
            memcpy(d->packet->data, packet, (size_t)packet_bytes);
            d->parser_pending = 0;
        }
    }
    failed(d, "bounded decode progress"); return -1;
}
