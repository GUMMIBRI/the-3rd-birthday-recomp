/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SR_PLATFORM_HOST_DIR_H
#define SR_PLATFORM_HOST_DIR_H
#include <stddef.h>
#include <stdint.h>

#define SR_DIR_NAME_CAPACITY 256
#define SR_DIR_PATH_CAPACITY 4096
typedef struct SrHostDir SrHostDir;
typedef enum SrDirStatus {
    SR_DIR_OK = 0,
    SR_DIR_MISSING = -1,
    SR_DIR_DENIED = -2,
    SR_DIR_NOT_DIRECTORY = -3,
    SR_DIR_INVALID = -4,
    SR_DIR_TOO_LONG = -5,
    SR_DIR_IO = -6,
    SR_DIR_NOMEM = -7,
    SR_DIR_UNSUPPORTED = -8
} SrDirStatus;
typedef enum SrDirKind {
    SR_DIR_REGULAR,
    SR_DIR_DIRECTORY,
    SR_DIR_SYMLINK,
    SR_DIR_OTHER
} SrDirKind;
typedef struct SrDirError { SrDirStatus status; uint32_t native_code; } SrDirError;
typedef struct SrDirEntry {
    char name[SR_DIR_NAME_CAPACITY];
    char path[SR_DIR_PATH_CAPACITY];
    SrDirKind kind;
} SrDirEntry;

/* Caller supplies a trusted, app-anchored directory. This API is nonrecursive and
 * does not validate/mutate mount ancestors. It rejects a symlink/reparse root and
 * classifies leaf links without following them. Concurrent filesystem replacement
 * is not a sandbox guarantee; consumers must not blindly fopen a classified path.
 * Only close a successfully opened handle. Error native_code is errno/GetLastError;
 * zero indicates a validation failure. Entry order is host filesystem order.
 */
int sr_dir_open(const char *root, SrHostDir **out, SrDirError *error);
/* 1 = entry, 0 = end (including an empty directory), negative = SrDirStatus. */
int sr_dir_next(SrHostDir *dir, SrDirEntry *entry, SrDirError *error);
int sr_dir_close(SrHostDir *dir, SrDirError *error);
/* A leaf is one nonempty component, excluding '.', '..', separators, controls,
 * Windows device-stream delimiters and wildcard characters. Never truncates.
 * out must not overlap root or leaf. */
int sr_dir_join(const char *root, const char *leaf, char *out, size_t capacity,
                SrDirError *error);
#endif
