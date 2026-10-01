#ifndef AEGIS_PACKAGE_REQUEST_H
#define AEGIS_PACKAGE_REQUEST_H
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define AEGIS_PACKAGE_REQUEST_MAX_BYTES 131072u

/* Seals protect contents, not a descriptor's access mode. SELinux checks that
 * mode again across exec/SCM_RIGHTS. Receivers need no write authority over the
 * broker's bounded request object. This is transport validation, not admission. */
static inline int aegis_package_request_readonly(int fd, size_t bytes) {
    struct stat st;
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fstat(fd, &st) < 0) return -1;
    if (!bytes || bytes > AEGIS_PACKAGE_REQUEST_MAX_BYTES || !S_ISREG(st.st_mode) || st.st_nlink
            || st.st_size != (off_t)bytes || (flags & (O_ACCMODE | O_PATH)) != O_RDONLY
            || fcntl(fd, F_GET_SEALS) != (F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL)) {
        errno = EPERM; return -1;
    }
    return 0;
}

/* Return a new independent readonly description of this already sealed memfd.
 * Retain the source during reopening and compare identity; never accept a path
 * or an unsealed input, and do not consume the caller's original descriptor. */
static inline int aegis_package_reopen_request(int source, size_t bytes) {
    struct stat before, after;
    if (fstat(source, &before) < 0) return -1;
    if (!bytes || bytes > AEGIS_PACKAGE_REQUEST_MAX_BYTES || !S_ISREG(before.st_mode) || before.st_nlink
            || before.st_size != (off_t)bytes
            || fcntl(source, F_GET_SEALS) != (F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL)) {
        errno = EPERM; return -1;
    }
    char path[64];
    int length = snprintf(path, sizeof(path), "/proc/self/fd/%d", source);
    if (length <= 0 || (size_t)length >= sizeof(path)) { errno = EOVERFLOW; return -1; }
    int result = open(path, O_RDONLY | O_CLOEXEC);
    if (result < 0) return -1;
    int error = 0;
    if (aegis_package_request_readonly(result, bytes) < 0 || fstat(result, &after) < 0) error = errno;
    else if (before.st_dev != after.st_dev || before.st_ino != after.st_ino) error = ESTALE;
    if (error) { close(result); errno = error; return -1; }
    return result;
}
#endif
