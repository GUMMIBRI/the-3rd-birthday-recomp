/* SPDX-License-Identifier: GPL-2.0-or-later
 * ISO offsets must remain 64 bit on every host, including Windows LLP64.
 * On 32-bit POSIX, define _FILE_OFFSET_BITS=64 before any system header.
 */
#ifndef SR_PLATFORM_ISO_FILE_H
#define SR_PLATFORM_ISO_FILE_H
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
static inline int sr_iso_seek(FILE *file, int64_t offset, int origin) {
    return _fseeki64(file, offset, origin);
}
static inline int64_t sr_iso_tell(FILE *file) { return _ftelli64(file); }
#else
#include <sys/types.h>
_Static_assert(sizeof(off_t) >= sizeof(int64_t), "ISO file offsets require 64-bit off_t");
static inline int sr_iso_seek(FILE *file, int64_t offset, int origin) {
    return fseeko(file, (off_t)offset, origin);
}
static inline int64_t sr_iso_tell(FILE *file) { return (int64_t)ftello(file); }
#endif
#endif
