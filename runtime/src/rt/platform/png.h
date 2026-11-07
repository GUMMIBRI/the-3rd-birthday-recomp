/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SR_PLATFORM_PNG_H
#define SR_PLATFORM_PNG_H
#include <stdint.h>
/* RGBA8 words with red in the low byte; returned memory uses free(). */
uint32_t *sr_png_load(const char *path, int *width, int *height);
int sr_png_save(const char *path, const uint32_t *pixels, int width, int height, int bgra);
#endif
