/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_IO_STDIO_COPY_H_
#define _REFOS_IO_STDIO_COPY_H_

/*! @file
    @brief This file should be a copy of librefossys/include/refos-io/stdio.h.
           This unfortunate duplication is done to avoid cyclic dependency problems between
           the libraries, that isn't handled too nicely by our build system.
*/

#include <stdio.h>
#include <stdlib.h>

typedef size_t (*stdio_read_fn_t)(void *data, size_t count);

typedef size_t (*stdio_write_fn_t)(void *data, size_t count);

void refos_override_stdio(stdio_read_fn_t readfn, stdio_write_fn_t writefn);

void refos_seL4_debug_override_stdout(void);

void refos_setup_dataspace_stdio(char *dspacePath);

int refos_async_getc(void);

int refos_getc(void);

#endif /* _REFOS_IO_STDIO_H_ */
