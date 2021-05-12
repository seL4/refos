/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
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