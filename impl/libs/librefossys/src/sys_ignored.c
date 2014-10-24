/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>

long sys_getpid(va_list ap) {
    /* Ignored stub. */
    return 0;
}

long sys_umask(va_list ap) {
    /* Ignored stub. */
    return (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
}

long sys_rt_sigaction(va_list ap) {
    /* Ignored stub. */
    return 0;
}

long sys_fcntl64(va_list ap) {
    /* Ignored stub. */
    return 0;
}