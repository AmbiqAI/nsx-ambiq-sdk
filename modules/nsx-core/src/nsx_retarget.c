/**
 * @file nsx_retarget.c
 * @brief Newlib `__wrap_*` retarget stubs for the nsx-system bring-up layer.
 *
 * GCC and ATfE (clang + newlib overlay) link with `-Wl,--wrap=_write_r` etc.
 * and need these symbols defined. armclang uses its own retarget mechanism
 * via __stdout/__stdin and does not compile this file (see CMakeLists.txt
 * newlib gate, and the NSX_HAS_NEWLIB guard below).
 *
 * These are intentionally minimal stubs: stdio output is routed through the
 * nsx-runtime printf path (am_util_stdio), not through newlib file I/O, so the
 * default file descriptors are inert. Override any of these in an application
 * if real newlib file semantics are required.
 */

#include "nsx_compiler.h"

#if NSX_HAS_NEWLIB

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <reent.h>

int __wrap__write_r(struct _reent *r, int fd, const void *ptr, size_t len) {
    (void)r; (void)fd; (void)ptr;
    return len;
}

int __wrap__read_r(struct _reent *r, int fd, void *ptr, size_t len) {
    (void)r; (void)fd; (void)ptr;
    return len;
}

int __wrap__close_r(struct _reent *r, int fd) {
    (void)r; (void)fd;
    return 0;
}

int __wrap__lseek_r(struct _reent *r, int fd, int ptr, int dir) {
    (void)r; (void)fd; (void)ptr; (void)dir;
    return 0;
}

int __wrap__kill_r(struct _reent *r, int pid, int sig) {
    (void)r; (void)pid; (void)sig;
    return 0;
}

int __wrap__getpid_r(struct _reent *r) {
    (void)r;
    return 0;
}

int __wrap__isatty_r(struct _reent *r, int fd) {
    (void)r; (void)fd;
    return 1;
}

int __wrap__fstat_r(struct _reent *r, int fd, struct stat *st) {
    (void)r; (void)fd; (void)st;
    return 0;
}

#endif /* NSX_HAS_NEWLIB */
