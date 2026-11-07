/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SR_PORTABLE_TEXT_H
#define SR_PORTABLE_TEXT_H
#include <stddef.h>
#include <stdint.h>
/* Pixels are opaque BGRA words, matching the existing Vulkan upload panels.
 * Tab stops are pixel offsets relative to the text origin. Size is 1..256.
 * The caller owns at least stride*height pixels; stride is measured in pixels.
 */
int sr_text_width(const char *text, int px, const int *tabs, size_t tab_count);
void sr_text_draw(uint32_t *dst, int width, int height, int stride, int x, int y,
                  const char *text, int px, uint32_t color,
                  const int *tabs, size_t tab_count);
void sr_text_fill(uint32_t *dst, int width, int height, int stride,
                  int x, int y, int w, int h, uint32_t color);
#endif
