/* SPDX-License-Identifier: GPL-2.0-or-later
 * Original clipped bitmap text rasterizer. Font data is the unmodified SDL
 * debug font (Zlib + public-domain VGA data); see its retained source notice.
 */
#include "portable_text.h"
#include <limits.h>
typedef uint8_t Uint8;
#define SDL_arraysize(a) (sizeof(a) / sizeof((a)[0]))
#define SDL_COMPILE_TIME_ASSERT(name, test) _Static_assert(test, #name)
#include "../../../third_party/sdl_debug_font/SDL_render_debug_font.h"
#undef SDL_arraysize
#undef SDL_COMPILE_TIME_ASSERT

static int cell_width(int px) { return (px * 3 + 3) / 4; }
static int64_t tab_next(int64_t at, int cell, const int *tabs, size_t n) {
    if (tabs) for (size_t i = 0; i < n; ++i) if (tabs[i] > at) return tabs[i];
    return at + cell * 4;
}
/* Consume one UTF-8 sequence; supported Latin-1 uses its font glyph, others '?'. */
static unsigned next_character(const unsigned char **cursor) {
    const unsigned char *s = *cursor;
    unsigned c = *s++, value, count;
    if (c < 128) { *cursor = s; return c; }
    if (c >= 0xc2 && c <= 0xdf) { value = c & 31; count = 1; }
    else if (c >= 0xe0 && c <= 0xef) { value = c & 15; count = 2; }
    else if (c >= 0xf0 && c <= 0xf4) { value = c & 7; count = 3; }
    else { *cursor = s; return '?'; }
    for (unsigned i = 0; i < count; ++i) {
        if ((*s & 0xc0) != 0x80) { *cursor = s; return '?'; }
        value = (value << 6) | (*s++ & 63);
    }
    *cursor = s;
    return value <= 255 ? value : '?';
}
int sr_text_width(const char *text, int px, const int *tabs, size_t n) {
    int64_t at = 0, widest = 0;
    const unsigned char *s = (const unsigned char *)text;
    if (!s || px < 1 || px > 256) return 0;
    while (*s) {
        unsigned c = next_character(&s);
        if (c == '\n') { if (at > widest) widest = at; at = 0; }
        else at = c == '\t' ? tab_next(at, cell_width(px), tabs, n) : at + cell_width(px);
        if (at > INT_MAX) return INT_MAX;
    }
    if (at > widest) widest = at;
    return (int)widest;
}
void sr_text_fill(uint32_t *dst, int width, int height, int stride,
                  int x, int y, int w, int h, uint32_t color) {
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    if (!dst || width <= 0 || height <= 0 || stride < width || w <= 0 || h <= 0) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right > width) right = width;
    if (bottom > height) bottom = height;
    for (int yy = y; yy < bottom; ++yy)
        for (int xx = x; xx < right; ++xx) dst[(size_t)yy * stride + xx] = color;
}
void sr_text_draw(uint32_t *dst, int width, int height, int stride, int x, int y,
                  const char *text, int px, uint32_t color, const int *tabs, size_t n) {
    const unsigned char *s = (const unsigned char *)text;
    int64_t at = 0, row = y;
    if (!dst || !s || width <= 0 || height <= 0 || stride < width || px < 1 || px > 256) return;
    const int cell = cell_width(px);
    while (*s) {
        unsigned c = next_character(&s), glyph;
        if (c == '\n') { at = 0; row += px; continue; }
        if (c == '\t') { at = tab_next(at, cell, tabs, n); continue; }
        if (c <= 32 || (c >= 127 && c <= 160)) { at += cell; continue; }
        glyph = c < 127 ? c - 33 : c - 67;
        if (glyph >= SDL_DEBUG_FONT_NUM_GLYPHS) glyph = '?' - 33;
        for (int yy = 0; yy < px; ++yy) {
            int64_t dy = row + yy;
            if (dy < 0 || dy >= height) continue;
            const unsigned bits = SDL_RenderDebugTextFontData[glyph * 8 + yy * 8 / px];
            for (int xx = 0; xx < cell; ++xx) {
                int64_t dx = (int64_t)x + at + xx;
                if (dx < 0 || dx >= width) continue;
                if (bits & (1u << (xx * 8 / cell))) dst[(size_t)dy * stride + (size_t)dx] = color;
            }
        }
        at += cell;
    }
}
