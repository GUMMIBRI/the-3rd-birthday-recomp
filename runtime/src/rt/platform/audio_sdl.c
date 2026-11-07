/* SPDX-License-Identifier: GPL-2.0-or-later
 * Native SDL3 audio output, preserving the upstream eight-channel mixing ring.
 * Select this source instead of WinMM audio.c in the PS5 application build.
 */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RING_FRAMES (1u << 16)
#define RING_MASK (RING_FRAMES - 1)
#define BLOCK_FRAMES 1024
static int32_t left[RING_FRAMES], right[RING_FRAMES];
static uint64_t play, channel_write[8];
static SDL_Mutex *lock;
static SDL_AudioStream *stream;
static int initialized, ready, failed;
unsigned long g_audio_frames[8];
static int16_t clamp16(int32_t sample) {
    return sample < -32768 ? -32768 : sample > 32767 ? 32767 : (int16_t)sample;
}
static void SDLCALL feed(void *userdata, SDL_AudioStream *device, int additional, int total) {
    int16_t output[BLOCK_FRAMES * 2];
    (void)userdata; (void)total;
    while (additional >= 4) {
        int frames = additional / 4;
        if (frames > BLOCK_FRAMES) frames = BLOCK_FRAMES;
        SDL_LockMutex(lock);
        for (int i = 0; i < frames; ++i) {
            unsigned at = (unsigned)((play + (uint64_t)i) & RING_MASK);
            output[i * 2] = clamp16(left[at]);
            output[i * 2 + 1] = clamp16(right[at]);
            left[at] = right[at] = 0;
        }
        play += (unsigned)frames;
        SDL_UnlockMutex(lock);
        if (!SDL_PutAudioStreamData(device, output, frames * 4)) {
            SDL_LockMutex(lock); failed = 1; SDL_UnlockMutex(lock);
            SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "native game audio stream failed: %s", SDL_GetError());
            return;
        }
        additional -= frames * 4;
    }
}
int sr_audio_init(void) {
    SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, 44100};
    if (initialized) return ready;
    initialized = 1;
    if (getenv("SR_NOAUDIO")) return 0;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fprintf(stderr, "audio: SDL initialization failed: %s\n", SDL_GetError()); return 0;
    }
    lock = SDL_CreateMutex();
    if (!lock) { SDL_QuitSubSystem(SDL_INIT_AUDIO); return 0; }
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed, NULL);
    if (!stream || !SDL_ResumeAudioStreamDevice(stream)) {
        fprintf(stderr, "audio: SDL device open/resume failed: %s\n", SDL_GetError());
        if (stream) SDL_DestroyAudioStream(stream);
        stream = NULL; SDL_DestroyMutex(lock); lock = NULL;
        SDL_QuitSubSystem(SDL_INIT_AUDIO); return 0;
    }
    ready = 1;
    fprintf(stderr, "audio: native SDL 44100 Hz stereo stream open\n");
    return 1;
}
void sr_audio_push(int channel, const int16_t *samples, int frames, int volume_left, int volume_right) {
    if (!samples || frames <= 0 || !sr_audio_init()) return;
    channel = channel < 0 ? 0 : channel & 7;
    if (volume_left < 0) volume_left = 0;
    if (volume_left > 0x8000) volume_left = 0x8000;
    if (volume_right < 0) volume_right = 0;
    if (volume_right > 0x8000) volume_right = 0x8000;
    g_audio_frames[channel] += (unsigned long)frames;
    SDL_LockMutex(lock);
    uint64_t at = channel_write[channel];
    if (at < play) at = play;
    if ((uint64_t)frames > play + RING_FRAMES - at) frames = (int)(play + RING_FRAMES - at);
    for (int i = 0; i < frames; ++i) {
        unsigned index = (unsigned)((at + (uint64_t)i) & RING_MASK);
        left[index] += ((int32_t)samples[i * 2] * volume_left) >> 15;
        right[index] += ((int32_t)samples[i * 2 + 1] * volume_right) >> 15;
    }
    channel_write[channel] = at + (unsigned)frames;
    SDL_UnlockMutex(lock);
}
int sr_audio_block_frames(void) { return BLOCK_FRAMES; }
int sr_audio_queued(int channel) {
    if (!ready) return -1;
    channel &= 7;
    SDL_LockMutex(lock);
    uint64_t at = channel_write[channel], consumed = play;
    int error = failed;
    SDL_UnlockMutex(lock);
    if (error) return -1;
    return at > consumed ? (int)(at - consumed) : 0;
}
void sr_audio_shutdown(void) {
    if (stream) SDL_DestroyAudioStream(stream); /* joins callback before mutex release */
    stream = NULL;
    if (lock) SDL_DestroyMutex(lock);
    lock = NULL;
    if (ready) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    ready = initialized = failed = 0;
    play = 0;
    SDL_memset(channel_write, 0, sizeof(channel_write));
    SDL_memset(left, 0, sizeof(left)); SDL_memset(right, 0, sizeof(right));
}
