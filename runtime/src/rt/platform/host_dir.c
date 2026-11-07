/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include "host_dir.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(SR_PS5_DIRECTORY_PROVIDER)
#include "native_directory_ps5.h"
#else
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#endif

struct SrHostDir {
    char root[SR_DIR_PATH_CAPACITY];
    int ended;
#ifdef _WIN32
    HANDLE handle;
    WIN32_FIND_DATAA data;
    int pending;
#elif defined(SR_PS5_DIRECTORY_PROVIDER)
    void *handle;
#else
    DIR *handle;
#endif
};

static int result(SrDirError *error, SrDirStatus status, uint32_t code) {
    if (error) { error->status = status; error->native_code = code; }
    return status;
}
static int separator(char c) { return c == '/' || c == '\\'; }
static int root_valid(const char *root) {
    const char *p, *component;
    if (!root || !root[0]) return 0;
    component = root;
    for (p = root; ; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c && (c < 32 || c == 127 || c == '*' || c == '?' || c == '"' ||
                c == '<' || c == '>' || c == '|')) return 0;
        if (c == ':') {
#ifdef _WIN32
            if (p != root + 1 || !((root[0] >= 'A' && root[0] <= 'Z') ||
                    (root[0] >= 'a' && root[0] <= 'z')) || !separator(p[1])) return 0;
#else
            return 0;
#endif
        }
        if (!c || separator((char)c)) {
            if (p - component == 2 && component[0] == '.' && component[1] == '.') return 0;
            if (p - component == 1 && component[0] == '.' && component != root) return 0;
            component = p + 1;
        }
        if (!c) break;
    }
    return 1;
}
static int leaf_valid(const char *leaf) {
    const unsigned char *p;
    if (!leaf || !leaf[0] || !strcmp(leaf, ".") || !strcmp(leaf, "..")) return 0;
    for (p = (const unsigned char *)leaf; *p; ++p)
        if (*p < 32 || *p == 127 || separator((char)*p) || strchr(":*?\"<>|", *p)) return 0;
    /* Windows strips trailing dots/spaces, creating ambiguous component aliases. */
    return p[-1] != '.' && p[-1] != ' ';
}
int sr_dir_join(const char *root, const char *leaf, char *out, size_t capacity,
                SrDirError *error) {
    size_t root_len, leaf_len, slash;
    if (!out || !capacity || !root_valid(root) || !leaf_valid(leaf))
        return result(error, SR_DIR_INVALID, 0);
    root_len = strlen(root); leaf_len = strlen(leaf);
    slash = !separator(root[root_len - 1]);
    if (root_len >= SR_DIR_PATH_CAPACITY || leaf_len >= SR_DIR_NAME_CAPACITY ||
            root_len + slash + leaf_len >= capacity ||
            root_len + slash + leaf_len >= SR_DIR_PATH_CAPACITY)
        return result(error, SR_DIR_TOO_LONG, 0);
    /* out must not overlap either input. */
    memcpy(out, root, root_len);
    if (slash) out[root_len] = '/';
    memcpy(out + root_len + slash, leaf, leaf_len + 1);
    return result(error, SR_DIR_OK, 0);
}

#if defined(SR_PS5_DIRECTORY_PROVIDER)
static int native_error(SrDirError *error, int code) {
    SrDirStatus status;
    switch (sr_native_directory_error_class(code)) {
    case SR_NATIVE_ERROR_OK: status = SR_DIR_OK; break;
    case SR_NATIVE_ERROR_MISSING: status = SR_DIR_MISSING; break;
    case SR_NATIVE_ERROR_DENIED: status = SR_DIR_DENIED; break;
    case SR_NATIVE_ERROR_NOT_DIRECTORY: status = SR_DIR_NOT_DIRECTORY; break;
    case SR_NATIVE_ERROR_TOO_LONG: status = SR_DIR_TOO_LONG; break;
    case SR_NATIVE_ERROR_NOMEM: status = SR_DIR_NOMEM; break;
    case SR_NATIVE_ERROR_UNSUPPORTED: status = SR_DIR_UNSUPPORTED; break;
    case SR_NATIVE_ERROR_INVALID: status = SR_DIR_INVALID; break;
    default: status = SR_DIR_IO; break;
    }
    return result(error, status, (uint32_t)code);
}
int sr_dir_open(const char *root, SrHostDir **out, SrDirError *error) {
    SrHostDir *dir;
    int code = 0;
    size_t length;
    if (!out) return result(error, SR_DIR_INVALID, 0);
    *out = NULL;
    if (!root_valid(root)) return result(error, SR_DIR_INVALID, 0);
    length = strlen(root);
    if (length >= SR_DIR_PATH_CAPACITY) return result(error, SR_DIR_TOO_LONG, 0);
    dir = calloc(1, sizeof(*dir));
    if (!dir) return result(error, SR_DIR_NOMEM, 0);
    while (length > 1 && separator(root[length - 1])) --length;
    memcpy(dir->root, root, length); dir->root[length] = 0;
    dir->handle = sr_native_directory_open(dir->root, &code);
    if (!dir->handle) { free(dir); return native_error(error, code); }
    *out = dir;
    return result(error, SR_DIR_OK, 0);
}
int sr_dir_next(SrHostDir *dir, SrDirEntry *entry, SrDirError *error) {
    SrNativeDirectoryInfo info;
    char name[256];
    int code = 0, status;
    if (!dir || !entry) return result(error, SR_DIR_INVALID, 0);
    memset(entry, 0, sizeof(*entry));
    if (dir->ended) return result(error, SR_DIR_OK, 0);
    status = sr_native_directory_next(dir->handle, name, &info, &code);
    if (status <= 0) {
        dir->ended = 1;
        return status < 0 ? native_error(error, code) : result(error, SR_DIR_OK, 0);
    }
    status = sr_dir_join(dir->root, name, entry->path, sizeof(entry->path), error);
    if (status != SR_DIR_OK) return status;
    memcpy(entry->name, name, strlen(name) + 1);
    switch (info.kind) {
    case SR_NATIVE_REGULAR: entry->kind = SR_DIR_REGULAR; break;
    case SR_NATIVE_DIRECTORY: entry->kind = SR_DIR_DIRECTORY; break;
    case SR_NATIVE_SYMLINK: entry->kind = SR_DIR_SYMLINK; break;
    default: entry->kind = SR_DIR_OTHER; break;
    }
    result(error, SR_DIR_OK, 0);
    return 1;
}
int sr_dir_close(SrHostDir *dir, SrDirError *error) {
    int code = 0, status;
    if (!dir) return result(error, SR_DIR_INVALID, 0);
    status = sr_native_directory_close(dir->handle, &code);
    free(dir);
    return status < 0 ? native_error(error, code) : result(error, SR_DIR_OK, 0);
}
#else
#ifdef _WIN32
static int native_error(SrDirError *error, DWORD code) {
    SrDirStatus status = SR_DIR_IO;
    if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) status = SR_DIR_MISSING;
    else if (code == ERROR_ACCESS_DENIED || code == ERROR_SHARING_VIOLATION) status = SR_DIR_DENIED;
    else if (code == ERROR_DIRECTORY) status = SR_DIR_NOT_DIRECTORY;
    else if (code == ERROR_FILENAME_EXCED_RANGE) status = SR_DIR_TOO_LONG;
    else if (code == ERROR_NOT_ENOUGH_MEMORY || code == ERROR_OUTOFMEMORY) status = SR_DIR_NOMEM;
    return result(error, status, (uint32_t)code);
}
#else
static int native_error(SrDirError *error, int code) {
    SrDirStatus status = SR_DIR_IO;
    if (code == ENOENT) status = SR_DIR_MISSING;
    else if (code == EACCES || code == EPERM) status = SR_DIR_DENIED;
    else if (code == ENOTDIR) status = SR_DIR_NOT_DIRECTORY;
    else if (code == ENAMETOOLONG) status = SR_DIR_TOO_LONG;
    else if (code == ENOMEM) status = SR_DIR_NOMEM;
    else if (code == ELOOP) status = SR_DIR_UNSUPPORTED;
    return result(error, status, (uint32_t)code);
}
#endif

int sr_dir_open(const char *root, SrHostDir **out, SrDirError *error) {
    SrHostDir *dir;
    size_t length;
    char normalized[SR_DIR_PATH_CAPACITY];
    if (!out) return result(error, SR_DIR_INVALID, 0);
    *out = NULL;
    if (!root_valid(root)) return result(error, SR_DIR_INVALID, 0);
    length = strlen(root);
    if (length >= SR_DIR_PATH_CAPACITY - 2) return result(error, SR_DIR_TOO_LONG, 0);
    /* lstat("link/") follows the link on POSIX. Remove trailing separators before
     * checking the final component, retaining filesystem/Windows drive roots. */
    while (length > 1 && separator(root[length - 1])) {
#ifdef _WIN32
        if (length == 3 && root[1] == ':') break;
#endif
        --length;
    }
    memcpy(normalized, root, length); normalized[length] = 0;
    root = normalized;
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(root);
    if (attributes == INVALID_FILE_ATTRIBUTES) return native_error(error, GetLastError());
    if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) return result(error, SR_DIR_UNSUPPORTED, 0);
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) return result(error, SR_DIR_NOT_DIRECTORY, 0);
#else
    struct stat attributes;
    if (lstat(root, &attributes) != 0) return native_error(error, errno);
    if (S_ISLNK(attributes.st_mode)) return result(error, SR_DIR_UNSUPPORTED, 0);
    if (!S_ISDIR(attributes.st_mode)) return result(error, SR_DIR_NOT_DIRECTORY, 0);
#endif
    dir = (SrHostDir *)calloc(1, sizeof(*dir));
    if (!dir) return result(error, SR_DIR_NOMEM, 0);
    memcpy(dir->root, root, length + 1);
#ifdef _WIN32
    char pattern[SR_DIR_PATH_CAPACITY];
    memcpy(pattern, root, length);
    if (!separator(root[length - 1])) pattern[length++] = '/';
    pattern[length++] = '*'; pattern[length] = 0;
    dir->handle = FindFirstFileA(pattern, &dir->data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        DWORD code = GetLastError();
        if (code != ERROR_FILE_NOT_FOUND && code != ERROR_NO_MORE_FILES) {
            free(dir); return native_error(error, code);
        }
        dir->ended = 1;
    } else dir->pending = 1;
#else
    dir->handle = opendir(root);
    if (!dir->handle) { int code = errno; free(dir); return native_error(error, code); }
#endif
    *out = dir;
    return result(error, SR_DIR_OK, 0);
}

int sr_dir_next(SrHostDir *dir, SrDirEntry *entry, SrDirError *error) {
    if (!dir || !entry) return result(error, SR_DIR_INVALID, 0);
    memset(entry, 0, sizeof(*entry));
    if (dir->ended) return result(error, SR_DIR_OK, 0);
    for (;;) {
        const char *name;
#ifdef _WIN32
        if (dir->pending) dir->pending = 0;
        else if (!FindNextFileA(dir->handle, &dir->data)) {
            DWORD code = GetLastError();
            dir->ended = 1;
            if (code == ERROR_NO_MORE_FILES) return result(error, SR_DIR_OK, 0);
            return native_error(error, code);
        }
        name = dir->data.cFileName;
#else
        struct dirent *found;
        errno = 0;
        found = readdir(dir->handle);
        if (!found) {
            int code = errno;
            dir->ended = 1;
            return code ? native_error(error, code) : result(error, SR_DIR_OK, 0);
        }
        name = found->d_name;
#endif
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        int status = sr_dir_join(dir->root, name, entry->path, sizeof(entry->path), error);
        if (status != SR_DIR_OK) return status;
        memcpy(entry->name, name, strlen(name) + 1);
#ifdef _WIN32
        DWORD attributes = dir->data.dwFileAttributes;
        entry->kind = (attributes & FILE_ATTRIBUTE_REPARSE_POINT) ? SR_DIR_SYMLINK :
            (attributes & FILE_ATTRIBUTE_DIRECTORY) ? SR_DIR_DIRECTORY :
            (attributes & FILE_ATTRIBUTE_DEVICE) ? SR_DIR_OTHER : SR_DIR_REGULAR;
#else
        struct stat attributes;
        if (lstat(entry->path, &attributes) != 0) return native_error(error, errno);
        entry->kind = S_ISLNK(attributes.st_mode) ? SR_DIR_SYMLINK :
            S_ISDIR(attributes.st_mode) ? SR_DIR_DIRECTORY :
            S_ISREG(attributes.st_mode) ? SR_DIR_REGULAR : SR_DIR_OTHER;
#endif
        result(error, SR_DIR_OK, 0);
        return 1;
    }
}

int sr_dir_close(SrHostDir *dir, SrDirError *error) {
    int status = SR_DIR_OK;
    if (!dir) return result(error, SR_DIR_INVALID, 0);
#ifdef _WIN32
    if (dir->handle != INVALID_HANDLE_VALUE && !FindClose(dir->handle))
        status = native_error(error, GetLastError());
#else
    if (closedir(dir->handle) != 0) status = native_error(error, errno);
#endif
    free(dir);
    if (status == SR_DIR_OK) result(error, SR_DIR_OK, 0);
    return status;
}

#endif /* SR_PS5_DIRECTORY_PROVIDER */
