/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
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
