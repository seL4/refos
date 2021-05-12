/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <sel4/sel4.h>
#include <stdlib.h>
#include <stdarg.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>

long
sys_exit(va_list ap)
{
    int status = va_arg(ap, int);
    proc_exit(status);
    while (1); /* We don't return after this */
    return 0;
}

long
sys_exit_group(va_list ap)
{
    int status = va_arg(ap, int);
    proc_exit(status);
    while (1); /* We don't return after this */
    return 0;
}
