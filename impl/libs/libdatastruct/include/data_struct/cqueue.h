/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CQUEUE_H_
#define _CQUEUE_H_

#include <stdint.h>
#include <stdbool.h>
#include "cvector.h"

typedef void* cqueue_item_t;

typedef struct cqueue_s {
    cqueue_item_t* data;
    uint32_t top;
    uint32_t count;
    uint32_t maxSize;
} cqueue_t;

void cqueue_init(cqueue_t *q, uint32_t maxSize);

bool cqueue_push(cqueue_t *q, cqueue_item_t e);

cqueue_item_t cqueue_pop(cqueue_t *q);

inline size_t cqueue_size(cqueue_t *q) {
    return q->count;
}

void cqueue_free(cqueue_t *q);

#endif /* _CQUEUE_H_ */