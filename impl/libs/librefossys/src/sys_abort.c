/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <autoconf.h>
#include <sel4/sel4.h>

#define DPRINTF_SERVER_NAME ""
#include <refos-util/dprintf.h>

void
sys_abort(void)
{
    seL4_DebugPrintf("RefOS HALT.\n");

#if defined(SEL4_DEBUG_KERNEL) 
    seL4_DebugHalt();
#endif

    while (1); /* We don't return after this */
}
