/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SR_ANCHOR_READ_PS5_H
#define SR_ANCHOR_READ_PS5_H
#include <stdint.h>
#include "native_directory_ps5.h"

/* Read-only, scalar ABI. No provider stat/DIR/errno representation crosses it.
 * Error classes are SrNativeDirectoryErrorClass wire values. native_code is
 * diagnostic only: consumers must not compare it to another SDK's errno macros.
 */
typedef struct SrReadAnchor SrReadAnchor;
typedef struct SrReadFile SrReadFile;
typedef struct SrAnchorReadError {
    uint32_t error_class;
    int32_t native_code;
} SrAnchorReadError;

#define SR_ANCHOR_READ_PATH_MAX 1023u
#define SR_ANCHOR_READ_COMPONENT_MAX 255u
#define SR_ANCHOR_READ_DEPTH_MAX 64u
#define SR_ANCHOR_READ_REQUEST_MAX (1024u * 1024u)

/* Caller has already admitted the existing app-owned directory. On success this
 * takes exclusive ownership of exactly existing_fd; never reopens it by path.
 * On failure ownership remains with caller. It does not establish which app owns
 * that directory. One host thread owns each capability and all calls/cleanup.
 */
SrReadAnchor *sr_anchor_read_adopt(int existing_fd, SrAnchorReadError *error);

/* No absolute/device paths, empty/dot/dotdot components, links or truncation.
 * Each ancestor is opened relative to the retained descriptor. Final capability
 * owns an independent regular-file fd and survives release of the app anchor.
 * size is the nonnegative scalar fstat size at open, not a stat struct.
 */
SrReadFile *sr_anchor_read_open(SrReadAnchor *anchor, const char *relative_path,
                               uint64_t *size, SrAnchorReadError *error);

/* 0 success (including a short read or EOF), -1 failure. bytes_read is reset on
 * every call. Reads are bounded to the size snapshot taken at open, <=1MiB/call.
 * pread preserves the descriptor position. Buffer storage is caller-owned.
 */
int sr_anchor_read_at(SrReadFile *file, uint64_t offset, void *buffer,
                      uint32_t requested, uint32_t *bytes_read, SrAnchorReadError *error);

/* Consume and clear the caller's handle even if close reports an error. Do not
 * retry close on EINTR: the descriptor's close state can be platform-dependent.
 * Null handle values are accepted for cleanup; dangling aliases are invalid.
 */
int sr_anchor_read_file_close(SrReadFile **file, SrAnchorReadError *error);
int sr_anchor_read_release(SrReadAnchor **anchor, SrAnchorReadError *error);
#endif
