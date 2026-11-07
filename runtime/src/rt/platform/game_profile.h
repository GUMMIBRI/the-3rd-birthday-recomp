/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SR_NATIVE_GAME_PROFILE_H
#define SR_NATIVE_GAME_PROFILE_H
#include <stdint.h>
typedef struct SrNativeGameProfile {
    uint32_t base, entry, module_gp, image_size;
    const unsigned char *image;
    const char *input_sha256;
    const char *image_sha256;
} SrNativeGameProfile;
/* Defined only by private preparation from the same owned EBOOT as generated C. */
extern const SrNativeGameProfile sr_native_game;
#endif
