/* SPDX-License-Identifier: GPL-2.0-or-later
 * PNG only; stb retains its own MIT license in the unmodified headers.
 */
#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#include "../../../third_party/stb/stb_image.h"
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../third_party/stb/stb_image_write.h"

#define PNG_INPUT_LIMIT (64u * 1024u * 1024u)
uint32_t *sr_png_load(const char *path, int *width, int *height) {
    FILE *file;
    unsigned char *input, *pixels;
    size_t length;
    int w, h, channels;
    long end;
    if (!path || !width || !height) return NULL;
    *width = *height = 0;
    file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) || (end = ftell(file)) <= 0 ||
        (unsigned long)end > PNG_INPUT_LIMIT || fseek(file, 0, SEEK_SET)) { fclose(file); return NULL; }
    length = (size_t)end;
    input = malloc(length);
    if (!input) { fclose(file); return NULL; }
    if (fread(input, 1, length, file) != length || ferror(file)) {
        free(input); fclose(file); return NULL;
    }
    if (fclose(file) != 0) { free(input); return NULL; }
    if (!stbi_info_from_memory(input, (int)length, &w, &h, &channels) ||
        w <= 0 || h <= 0 || w > 4096 || h > 4096) { free(input); return NULL; }
    pixels = stbi_load_from_memory(input, (int)length, &w, &h, &channels, 4);
    free(input);
    if (!pixels) return NULL;
    *width = w; *height = h;
    return (uint32_t *)pixels;
}
typedef struct { FILE *file; int failed; } PngOutput;
static void write_bytes(void *context, void *data, int size) {
    PngOutput *out = context;
    if (size < 0 || (!out->failed && fwrite(data, 1, (size_t)size, out->file) != (size_t)size)) out->failed = 1;
}
int sr_png_save(const char *path, const uint32_t *pixels, int width, int height, int bgra) {
    PngOutput out;
    uint32_t *converted = NULL;
    int encoded;
    if (!path || !pixels || width <= 0 || height <= 0 || width > 4096 || height > 4096) return 0;
    if (bgra) {
        size_t count = (size_t)width * height;
        converted = malloc(count * sizeof(*converted));
        if (!converted) return 0;
        for (size_t i = 0; i < count; ++i) {
            uint32_t p = pixels[i];
            converted[i] = 0xff000000u | (p & 0xff00u) | ((p & 0xffu) << 16) | ((p >> 16) & 0xffu);
        }
        pixels = converted;
    }
    out.file = fopen(path, "wb"); out.failed = 0;
    if (!out.file) { free(converted); return 0; }
    encoded = stbi_write_png_to_func(write_bytes, &out, width, height, 4, pixels, width * 4);
    if (fflush(out.file) != 0) out.failed = 1;
    if (fclose(out.file) != 0) out.failed = 1;
    free(converted);
    return encoded && !out.failed;
}
