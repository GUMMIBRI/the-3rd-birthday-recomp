/* SPDX-License-Identifier: GPL-3.0-or-later
 * Original scalar ABI adapter to Mihawk's public ps5platform directory API.
 * Provider: PS5_PayloadSDK b83202be73e930050e00de0f1b4d9ec46c0391bf,
 * platform/src/directory.c (Copyright2026 Mihawk, GPL-3.0-or-later).
 * Compile this translation unit with that provider's retained RADV SDK headers.
 */
#include "native_directory_ps5.h"
#include <ps5platform/libc.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <string.h>

#define PROVIDER_PATH_CAPACITY 1024
typedef struct {
    DIR *stream;
    dev_t device;
    ino_t inode;
    char path[PROVIDER_PATH_CAPACITY];
} NativeDirectory;

_Static_assert(sizeof(SrNativeDirectoryInfo) == 16, "directory scalar ABI size");
uint32_t sr_native_directory_error_class(int native_code) {
    if (!native_code) return SR_NATIVE_ERROR_OK;
    if (native_code == ENOENT) return SR_NATIVE_ERROR_MISSING;
    if (native_code == EACCES || native_code == EPERM) return SR_NATIVE_ERROR_DENIED;
    if (native_code == ENOTDIR) return SR_NATIVE_ERROR_NOT_DIRECTORY;
    if (native_code == ENAMETOOLONG) return SR_NATIVE_ERROR_TOO_LONG;
    if (native_code == ENOMEM) return SR_NATIVE_ERROR_NOMEM;
    if (native_code == ELOOP || native_code == ENOSYS || native_code == EOPNOTSUPP)
        return SR_NATIVE_ERROR_UNSUPPORTED;
    if (native_code == EINVAL || native_code == EFAULT) return SR_NATIVE_ERROR_INVALID;
    return SR_NATIVE_ERROR_IO;
}
static int fail(int *error, int code) {
    if (error) *error = code;
    return -1;
}
static void clear_info(SrNativeDirectoryInfo *info) {
    if (info) memset(info, 0, sizeof(*info));
}
static void describe(const struct stat *status, SrNativeDirectoryInfo *info) {
    info->kind = S_ISREG(status->st_mode) ? SR_NATIVE_REGULAR :
                 S_ISDIR(status->st_mode) ? SR_NATIVE_DIRECTORY :
                 S_ISLNK(status->st_mode) ? SR_NATIVE_SYMLINK : SR_NATIVE_OTHER;
    info->size = status->st_size > 0 ? (uint64_t)status->st_size : 0;
    info->reserved = 0;
}
/* This provider tracks paths in1024-byte records. Fail before it could silently
 * stop recording one; no path truncation. Ancestor authority remains caller-owned.
 */
static int copy_path(const char *input, char output[PROVIDER_PATH_CAPACITY]) {
    size_t length;
    if (!input || !(length = strnlen(input, PROVIDER_PATH_CAPACITY))) return EINVAL;
    if (length >= PROVIDER_PATH_CAPACITY) return ENAMETOOLONG;
    while (length > 1 && input[length - 1] == '/') --length;
    memcpy(output, input, length); output[length] = 0;
    const char *component = output;
    for (const char *p = output;; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c && (c < 32 || c == 127 || c == '\\' || strchr(":*?\"<>|", c))) return EINVAL;
        if (!c || c == '/') {
            size_t count = (size_t)(p - component);
            if (count == 2 && component[0] == '.' && component[1] == '.') return EINVAL;
            if (count == 1 && component[0] == '.' && !(component == output && !c)) return EINVAL;
            component = p + 1;
        }
        if (!c) break;
    }
    return 0;
}
static int leaf_valid(const char *name, size_t length) {
    if (!length || length > 255) return 0;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char c = (unsigned char)name[i];
        if (c < 32 || c == 127 || c == '/' || c == '\\' || strchr(":*?\"<>|", c)) return 0;
    }
    return name[length - 1] != '.' && name[length - 1] != ' ';
}
void *sr_native_directory_open(const char *path, int *error) {
    char normalized[PROVIDER_PATH_CAPACITY];
    struct stat before, opened;
    NativeDirectory *directory;
    int code = copy_path(path, normalized);
    if (error) *error = 0;
    if (code) { fail(error, code); return NULL; }
    if (lstat(normalized, &before) != 0) { fail(error, errno); return NULL; }
    if (S_ISLNK(before.st_mode)) { fail(error, ELOOP); return NULL; }
    if (!S_ISDIR(before.st_mode)) { fail(error, ENOTDIR); return NULL; }
    directory = calloc(1, sizeof(*directory));
    if (!directory) { fail(error, ENOMEM); return NULL; }
    directory->stream = ps5_opendir(normalized);
    if (!directory->stream) { code = errno; free(directory); fail(error, code); return NULL; }
    int fd = ps5_dirfd(directory->stream);
    if (fd < 0 || fstat(fd, &opened) != 0) {
        code = errno; ps5_closedir(directory->stream); free(directory); fail(error, code); return NULL;
    }
    if (opened.st_dev != before.st_dev || opened.st_ino != before.st_ino || !S_ISDIR(opened.st_mode)) {
        ps5_closedir(directory->stream); free(directory); fail(error, EIO); return NULL;
    }
    directory->device = opened.st_dev; directory->inode = opened.st_ino;
    memcpy(directory->path, normalized, strlen(normalized) + 1);
    return directory;
}
int sr_native_directory_next(void *handle, char name[256], SrNativeDirectoryInfo *info, int *error) {
    NativeDirectory *directory = handle;
    struct stat root_status;
    if (error) *error = 0;
    if (name) name[0] = 0;
    clear_info(info);
    if (!directory || !name || !info) return fail(error, EINVAL);
    if (lstat(directory->path, &root_status) != 0) return fail(error, errno);
    if (!S_ISDIR(root_status.st_mode) || root_status.st_dev != directory->device || root_status.st_ino != directory->inode)
        return fail(error, EIO);
    for (;;) {
        errno = 0;
        struct dirent *entry = ps5_readdir(directory->stream);
        if (!entry) return errno ? fail(error, errno) : 0;
        /* The pinned provider does not populate d_namlen or d_fileno. Use its
         * validated terminated name, then no-follow scalar metadata instead.
         */
        const char *end = memchr(entry->d_name, 0, sizeof(entry->d_name));
        if (!end) return fail(error, EIO);
        size_t length = (size_t)(end - entry->d_name);
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (!leaf_valid(entry->d_name, length)) return fail(error, EIO);
        struct stat status;
        if (ps5_fstatat(ps5_dirfd(directory->stream), entry->d_name, &status, AT_SYMLINK_NOFOLLOW) != 0)
            return fail(error, errno);
        describe(&status, info);
        memcpy(name, entry->d_name, length + 1);
        return 1;
    }
}
int sr_native_directory_close(void *handle, int *error) {
    NativeDirectory *directory = handle;
    if (error) *error = 0;
    if (!directory) return fail(error, EINVAL);
    int result = ps5_closedir(directory->stream);
    int code = result < 0 ? errno : 0;
    free(directory);
    return code ? fail(error, code) : 0;
}
int sr_native_stat_nofollow(const char *path, SrNativeDirectoryInfo *info, int *error) {
    char normalized[PROVIDER_PATH_CAPACITY];
    struct stat status;
    if (error) *error = 0;
    clear_info(info);
    if (!info) return fail(error, EINVAL);
    int code = copy_path(path, normalized);
    if (code) return fail(error, code);
    if (lstat(normalized, &status) != 0) return fail(error, errno);
    describe(&status, info);
    return 0;
}
