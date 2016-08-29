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

/*! @file
    @brief Common helper functions and global definitions.

    Useful global definitions and general inline helper functions that every process server module
    should know about.
*/

#ifndef _REFOS_PROCESS_SERVER_COMMON_H_
#define _REFOS_PROCESS_SERVER_COMMON_H_

#include <autoconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <sel4/sel4.h>
#include <cpio/cpio.h>
#include <refos/refos.h>

// Macros.

#define MiB(x) (1024*1024*(x))
#define die( ... ) while (1) { printf(__VA_ARGS__); seL4_DebugHalt(); }

/* Debug printing. */
#include <refos-util/dprintf.h>

#include "badge.h"

typedef seL4_Word vaddr_t;
typedef seL4_Word paddr_t;

#define kmalloc malloc
#define krealloc realloc
#define kfree free

#endif /* _REFOS_PROCESS_SERVER_COMMON_H_ */