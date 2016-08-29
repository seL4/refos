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
    @brief RefOS sharing library.
    
    Helper ring buffer library which can be used to implement sharing. For details on how sharing
    is done on RefOS, please refer to the protocol documentation. Uses a standard ringbuffer
    implementation.
    
    Assumes that the sharing has already been set up, and mapped into windows on both processes.
*/

#ifndef _REFOS_SHARE_H_
#define _REFOS_SHARE_H_

#include <stddef.h>

#include "refos.h"

/*! @brief Read from a shared buffer.
    @param dest Buffer in which to store the read data. (output, no ownership)
    @param len Maximum length of the destination buffer.
    @param bufVaddr The shared ringbuffer address. (input, no ownership)
    @param bufSize The shared ringbuffer size.
    @param start Handle to the associated start address (input, output, no ownership)
    @param bytesRead The number of bytes that has been successfully read from the shared buffer.
                     (output, no ownership)
    @return 0 if success, -1 otherwise.
 */
int refos_share_read(char *dest, size_t len, char *bufVaddr, size_t bufSize,
        unsigned int *start, unsigned int *bytesRead);

/*! @brief Write to a shared buffer.
    @param src Buffer containing content to write. (input, no ownership)
    @param len Length of the content source.
    @param bufVaddr The shared ringbuffer address. (input, no ownership)
    @param bufSize The shared ringbuffer size.
    @param end Handle to the associated end address (input, output, no ownership)
    @return 0 if success, -1 otherwise.
 */
int refos_share_write(char *src, size_t len, char *bufVaddr, size_t bufSize,
        unsigned int *end);

#endif /* _REFOS_SHARE_H_ */

