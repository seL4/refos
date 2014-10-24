/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils/arith.h>
#include "refos/share.h"

/*! @file
    @brief RefOS sharing library. */

#define SHARE_METADATA_SIZE (sizeof(seL4_Word) * 2)

static inline char *
refos_share_get_base(char *bufVaddr) {
    return bufVaddr + SHARE_METADATA_SIZE;
}

static inline int
refos_share_get_start(char *bufVaddr) {
    return *((int*)bufVaddr);
}

static inline int
refos_share_get_end(char *bufVaddr) {
    return *((int*)(bufVaddr + sizeof(seL4_Word)));
}

static inline int
refos_share_write_remaining_size(unsigned int start, unsigned int end, size_t bufSize)
{
    return start > end ? ((start - end) - 1) :
            ((bufSize - 1) - (end - start));
}

static inline int
refos_share_validate_params(size_t bufSize, int start, int end) {
    if (bufSize < SHARE_METADATA_SIZE + 1) return -1;
    if (start > bufSize || end > bufSize) return -1;
    return 0;
}

int
refos_share_read(char *dest, size_t len, char *bufVaddr, size_t bufSize,
        unsigned int *start, unsigned int *bytesRead)
{
    assert(dest != NULL);
    assert(bufVaddr != NULL);
    char *bufBase = refos_share_get_base(bufVaddr);
    unsigned int end = refos_share_get_end(bufVaddr);
    unsigned int ringBufSize = bufSize - SHARE_METADATA_SIZE;

    if (!dest || !bufVaddr || !start || !bytesRead) {
        printf("ERROR(refos_share_read): NULL parameter.\n");
        return -1;
    }

    if (refos_share_validate_params(bufSize, *start, end)) {
        return -1;
    }

    if (*start <= end) {
        /* Non-wrapping case, read block of data straight in. */
        *bytesRead = MIN(end - *start, len);
        memcpy(dest, bufBase + *start, *bytesRead);
    } else {
        /* Wrapping case - read the end bit first. */
        *bytesRead = MIN(ringBufSize - *start, len);
        memcpy(dest, bufBase + *start, *bytesRead);
        unsigned int bytesRemain = MIN(len - *bytesRead, end);
        /* Read the wrapped start bit if it exists. */
        if (bytesRemain > 0) {
            memcpy(dest + *bytesRead, bufBase, bytesRemain);
            *bytesRead += bytesRemain;
        }
    }

    *start = (*start + *bytesRead) % ringBufSize;
    *((seL4_Word*)bufVaddr) = *start;

    return 0;
}

int
refos_share_write(char *src, size_t len, char *bufVaddr, size_t bufSize,
        unsigned int *end)
{
    assert(src != NULL);
    assert(bufVaddr != NULL);
    char *bufBase = refos_share_get_base(bufVaddr);
    unsigned int start = refos_share_get_start(bufVaddr);
    unsigned int ringBufSize = bufSize - SHARE_METADATA_SIZE;

    if (!src || !bufVaddr || !end) {
        printf("ERROR(refos_share_write): NULL parameter.\n");
        return -1;
    }

    if (refos_share_validate_params(bufSize, start, *end)) {
        return -1;
    }
    if (len > refos_share_write_remaining_size(start, *end, ringBufSize)) {
        return -1;
    }
    if (*end < start) {
        /* Non-wrapping case, copy block of data straight in. */
        memcpy(bufBase + *end, src, len);
    } else {
        /* Wrapping case - copy the end bit first. */
        unsigned int endBytes = MIN(len, ringBufSize - *end);
        memcpy(bufBase + *end, src, endBytes);
        assert(endBytes <= len);
        /* Copy the wrapped start bit if it exists. */
        if (endBytes < len) {
            memcpy(bufBase, src + endBytes, len - endBytes);
        }
    }

    *end = (*end + len) % ringBufSize;
    *((seL4_Word*)(bufVaddr + sizeof(seL4_Word))) = *end;

    return 0;
}
