/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "platform/png.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    const uint32_t rgba[4] = {0xff0000ff, 0xff00ff00, 0xffff0000, 0x80332211};
    const uint32_t bgra[1] = {0x00112233};
    int width = 99, height = 99;
    uint32_t *decoded;
    if (argc != 2) return 1;
    if (!sr_png_save(argv[1], rgba, 2, 2, 0)) return 2;
    decoded = sr_png_load(argv[1], &width, &height);
    if (!decoded || width != 2 || height != 2 || memcmp(rgba, decoded, sizeof(rgba))) return 3;
    free(decoded);
    if (!sr_png_save(argv[1], bgra, 1, 1, 1)) return 4;
    decoded = sr_png_load(argv[1], &width, &height);
    if (!decoded || decoded[0] != 0xff332211) return 5;
    free(decoded);
    if (sr_png_save(argv[1], rgba, 4097, 1, 0)) return 6;
    if (sr_png_load("__missing_png_check__.png", &width, &height) || width || height) return 7;
    FILE *file = fopen(argv[1], "wb");
    if (!file) return 8;
    fwrite("invalid PNG", 1, 11, file); fclose(file);
    if (sr_png_load(argv[1], &width, &height)) return 9;
    remove(argv[1]);
    puts("PNG RGBA/BGRA round-trip, dimensions, malformed input and missing-file checks passed");
    return 0;
}
