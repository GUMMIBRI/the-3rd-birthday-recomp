/* SPDX-License-Identifier: MIT
 * Adapted from the public PE native application's MIT unsupported.c adapter.
 * PS5 titles have no approved readlink import. Mesa uses this for optional
 * Linux /proc/self/fd diagnostic labels, not game file resolution. Return the
 * real unsupported-operation error instead of linking an inline syscall.
 */
#include <errno.h>
#include <sys/types.h>
#include <stddef.h>
ssize_t sr_native_readlink(const char *path, char *buffer, size_t size) {
    (void)path; (void)buffer; (void)size;
    errno = ENOSYS;
    return -1;
}
