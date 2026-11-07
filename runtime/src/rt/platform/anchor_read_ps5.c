/* SPDX-License-Identifier: GPL-2.0-or-later
 * Original read-only candidate. Compile exclusively with the selected public
 * RADV SDK headers. _openat is an allowed-libkernel export candidate, NOT an
 * assertion that target ABI, flag semantics or confinement have been exercised.
 */
#include "anchor_read_ps5.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
/* The pinned public libc namespace maps openat -> _openat. Let the owning
 * header declare that exact signature, instead of inventing a foreign ABI. */
#define openat _openat
#include <sys/fcntl.h>
#undef openat
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct SrReadAnchor { int fd; };
struct SrReadFile { int fd; uint64_t size; };
_Static_assert(sizeof(off_t) == sizeof(int64_t), "selected provider requires 64-bit offsets");
_Static_assert(sizeof(SrAnchorReadError) == 8, "scalar error wire size");

static uint32_t error_class(int code) {
    if (code == ENOENT) return SR_NATIVE_ERROR_MISSING;
    if (code == EACCES || code == EPERM) return SR_NATIVE_ERROR_DENIED;
    if (code == ENOTDIR) return SR_NATIVE_ERROR_NOT_DIRECTORY;
    if (code == ENAMETOOLONG) return SR_NATIVE_ERROR_TOO_LONG;
    if (code == ENOMEM) return SR_NATIVE_ERROR_NOMEM;
    if (code == ELOOP || code == EMLINK || code == ENOSYS || code == EOPNOTSUPP)
        return SR_NATIVE_ERROR_UNSUPPORTED;
    if (code == EINVAL || code == EFAULT || code == EBADF) return SR_NATIVE_ERROR_INVALID;
    return SR_NATIVE_ERROR_IO; /* unknown errors never mean absence/success */
}
static int set_error(SrAnchorReadError *error, uint32_t category, int code) {
    if (error) { error->error_class = category; error->native_code = (int32_t)code; }
    return -1;
}
static void clear_error(SrAnchorReadError *error) {
    if (error) { error->error_class = SR_NATIVE_ERROR_OK; error->native_code = 0; }
}
static int native_failure(SrAnchorReadError *error, int result) {
    /* Clear errno before each native call. If a failed candidate uses a foreign
     * return convention instead, retain its raw failure and classify unknown I/O.
     * No claim is made that this detects every possible ABI mismatch. */
    int code = errno ? errno : result;
    return set_error(error, error_class(code), code);
}
static int component_valid(const char *name, size_t length) {
    if (!length || length > SR_ANCHOR_READ_COMPONENT_MAX ||
        (length == 1 && name[0] == '.') ||
        (length == 2 && name[0] == '.' && name[1] == '.')) return 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c < 32 || c == 127 || c == '/' || c == '\\' ||
            c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return 0;
    }
    return name[length - 1] != '.' && name[length - 1] != ' ';
}
static int validate_path(const char *path, SrAnchorReadError *error) {
    if (!path) return set_error(error, SR_NATIVE_ERROR_INVALID, 0);
    size_t length = strnlen(path, SR_ANCHOR_READ_PATH_MAX + 1u);
    if (!length) return set_error(error, SR_NATIVE_ERROR_INVALID, 0);
    if (length > SR_ANCHOR_READ_PATH_MAX) return set_error(error, SR_NATIVE_ERROR_TOO_LONG, 0);
    const char *component = path;
    unsigned depth = 0;
    for (size_t i = 0; i <= length; ++i) if (!path[i] || path[i] == '/') {
        size_t count = (size_t)(path + i - component);
        if (count > SR_ANCHOR_READ_COMPONENT_MAX) return set_error(error, SR_NATIVE_ERROR_TOO_LONG, 0);
        if (!component_valid(component, count)) return set_error(error, SR_NATIVE_ERROR_INVALID, 0);
        if (++depth > SR_ANCHOR_READ_DEPTH_MAX) return set_error(error, SR_NATIVE_ERROR_TOO_LONG, 0);
        component = path + i + 1;
    }
    return 0;
}
SrReadAnchor *sr_anchor_read_adopt(int existing_fd, SrAnchorReadError *error) {
    clear_error(error);
    if (existing_fd < 0) { set_error(error, SR_NATIVE_ERROR_INVALID, 0); return NULL; }
    struct stat status = {0};
    errno = 0;
    if (fstat(existing_fd, &status) != 0) { native_failure(error, -1); return NULL; }
    if (!S_ISDIR(status.st_mode)) { set_error(error, SR_NATIVE_ERROR_NOT_DIRECTORY, 0); return NULL; }
    SrReadAnchor *anchor = malloc(sizeof(*anchor));
    if (!anchor) { set_error(error, SR_NATIVE_ERROR_NOMEM, 0); return NULL; }
    anchor->fd = existing_fd;
    return anchor;
}
SrReadFile *sr_anchor_read_open(SrReadAnchor *anchor, const char *relative_path,
                               uint64_t *size, SrAnchorReadError *error) {
    clear_error(error);
    if (size) *size = 0;
    if (!anchor || !size) { set_error(error, SR_NATIVE_ERROR_INVALID, 0); return NULL; }
    if (validate_path(relative_path, error) != 0) return NULL;
    int parent = anchor->fd, owned_parent = 0;
    const char *component = relative_path;
    for (;;) {
        const char *separator = strchr(component, '/');
        size_t length = separator ? (size_t)(separator - component) : strlen(component);
        char leaf[SR_ANCHOR_READ_COMPONENT_MAX + 1u];
        memcpy(leaf, component, length); leaf[length] = 0;
        int flags = O_RDONLY | O_NOFOLLOW;
        if (separator) flags |= O_DIRECTORY;
        else flags |= O_NONBLOCK; /* reject a FIFO/device after fstat without FIFO-open blocking */
        errno = 0;
        int next = _openat(parent, leaf, flags);
        if (next < 0) {
            native_failure(error, next);
            if (owned_parent) close(parent);
            return NULL;
        }
        if (owned_parent) {
            errno = 0;
            if (close(parent) != 0) {
                native_failure(error, -1); close(next); return NULL;
            }
        }
        if (separator) { parent = next; owned_parent = 1; component = separator + 1; continue; }
        struct stat status = {0};
        errno = 0;
        if (fstat(next, &status) != 0) { native_failure(error, -1); close(next); return NULL; }
        if (!S_ISREG(status.st_mode) || status.st_size < 0) {
            set_error(error, SR_NATIVE_ERROR_UNSUPPORTED, 0); close(next); return NULL;
        }
        SrReadFile *file = malloc(sizeof(*file));
        if (!file) { set_error(error, SR_NATIVE_ERROR_NOMEM, 0); close(next); return NULL; }
        file->fd = next; file->size = (uint64_t)status.st_size; *size = file->size;
        return file;
    }
}
int sr_anchor_read_at(SrReadFile *file, uint64_t offset, void *buffer,
                      uint32_t requested, uint32_t *bytes_read, SrAnchorReadError *error) {
    clear_error(error);
    if (bytes_read) *bytes_read = 0;
    if (!file || !bytes_read || (!buffer && requested) || requested > SR_ANCHOR_READ_REQUEST_MAX ||
        offset > INT64_MAX || requested > (uint64_t)INT64_MAX - offset)
        return set_error(error, SR_NATIVE_ERROR_INVALID, 0);
    if (!requested || offset >= file->size) return 0;
    if (requested > file->size - offset) requested = (uint32_t)(file->size - offset);
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        errno = 0;
        ssize_t got = pread(file->fd, buffer, requested, (off_t)offset);
        if (got >= 0) {
            if ((uint64_t)got > requested) return set_error(error, SR_NATIVE_ERROR_IO, 0);
            *bytes_read = (uint32_t)got;
            return 0;
        }
        if (errno != EINTR) return native_failure(error, (int)got);
    }
    return set_error(error, SR_NATIVE_ERROR_IO, EINTR);
}
int sr_anchor_read_file_close(SrReadFile **file, SrAnchorReadError *error) {
    clear_error(error);
    if (!file) return set_error(error, SR_NATIVE_ERROR_INVALID, 0);
    if (!*file) return 0;
    SrReadFile *owned = *file; *file = NULL;
    errno = 0;
    int result = close(owned->fd);
    if (result != 0) native_failure(error, result);
    free(owned);
    return result == 0 ? 0 : -1;
}
int sr_anchor_read_release(SrReadAnchor **anchor, SrAnchorReadError *error) {
    clear_error(error);
    if (!anchor) return set_error(error, SR_NATIVE_ERROR_INVALID, 0);
    if (!*anchor) return 0;
    SrReadAnchor *owned = *anchor; *anchor = NULL;
    errno = 0;
    int result = close(owned->fd);
    if (result != 0) native_failure(error, result);
    free(owned);
    return result == 0 ? 0 : -1;
}
