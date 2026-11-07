/* SPDX-License-Identifier: GPL-2.0-or-later
 * Native writes remain unavailable until the actual console provider and scalar
 * ABI boundary are qualified. This is an explicit error, never a fake save.
 */
#ifndef SR_STORAGE_GATE_H
#define SR_STORAGE_GATE_H
#include <stdio.h>
static inline int sr_storage_require_write(void) {
#if defined(SR_NATIVE_GAME) && !defined(SR_PS5_WRITABLE_STORAGE_VERIFIED)
    static int reported;
    if (!reported) {
        fprintf(stderr, "native storage: writes unavailable; console-relative provider/ABI is not qualified\n");
        reported = 1;
    }
    return 0;
#else
    return 1;
#endif
}
#endif
