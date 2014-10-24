/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_IO_STDIO_H_
#define _REFOS_IO_STDIO_H_

/*! @file
    @brief There is a copy of this file in librefos/src/refos-util/stdio_copy.h.
           This unfortunate duplication is done to avoid cyclic dependency problems between
           the libraries.
*/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

/*! @brief Whether to translate the return key into a '\n' from a '\r'. */
extern bool refos_stdio_translate_stdin_cr;

typedef size_t (*stdio_read_fn_t)(void *data, size_t count);

typedef size_t (*stdio_write_fn_t)(void *data, size_t count);

void refos_override_stdio(stdio_read_fn_t readfn, stdio_write_fn_t writefn);

void refos_seL4_debug_override_stdout(void);

void refos_setup_dataspace_stdio(char *dspacePath);

int refos_async_getc(void);

int refos_getc(void);

#endif /* _REFOS_IO_STDIO_H_ */
