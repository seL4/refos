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
