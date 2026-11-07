/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SR_NATIVE_DIRECTORY_PS5_H
#define SR_NATIVE_DIRECTORY_PS5_H
#include <stdint.h>

/* Scalar wire ABI: never expose the provider's DIR, dirent, stat or errno macro.
 * Values are independent of the SDK's DT_* or S_IF* values.
 */
enum SrNativeDirectoryKind {
    SR_NATIVE_REGULAR = 0,
    SR_NATIVE_DIRECTORY = 1,
    SR_NATIVE_SYMLINK = 2,
    SR_NATIVE_OTHER = 3
};
typedef struct SrNativeDirectoryInfo {
    uint64_t size;
    uint32_t kind;
    uint32_t reserved;
} SrNativeDirectoryInfo;
enum SrNativeDirectoryErrorClass {
    SR_NATIVE_ERROR_OK = 0,
    SR_NATIVE_ERROR_MISSING = 1,
    SR_NATIVE_ERROR_DENIED = 2,
    SR_NATIVE_ERROR_NOT_DIRECTORY = 3,
    SR_NATIVE_ERROR_TOO_LONG = 4,
    SR_NATIVE_ERROR_NOMEM = 5,
    SR_NATIVE_ERROR_UNSUPPORTED = 6,
    SR_NATIVE_ERROR_INVALID = 7,
    SR_NATIVE_ERROR_IO = 8
};
/* Interpret native errno ONLY inside the matching provider-header ABI. */
uint32_t sr_native_directory_error_class(int native_code);

/* error is the selected provider's native errno, zero on success/end.
 * Exactly one thread owns a handle. Null error pointers are allowed.
 * open rejects a final root symlink; next skips '.' and '..'.
 * next: 1 entry / 0 end / -1 failure. Names are NUL terminated, at most255 bytes.
 * Caller gives name[256], info storage; all are reset on end/failure.
 */
/* Optional step receives the failing operation, zero on success. Values
 * 40–46 are reserved startup exit codes (see docs/PS5-CURRENT.md). */
void *sr_native_directory_open(const char *path, int *error, uint32_t *step);
int sr_native_directory_next(void *handle, char name[256], SrNativeDirectoryInfo *info, int *error);
int sr_native_directory_close(void *handle, int *error);
int sr_native_stat_nofollow(const char *path, SrNativeDirectoryInfo *info, int *error);
#endif
