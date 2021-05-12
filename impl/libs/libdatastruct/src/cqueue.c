/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <data_struct/cqueue.h>
#include <assert.h>
#include <stdio.h>  

void cqueue_init(cqueue_t *q, uint32_t maxSize) {
    assert(q);
    q->data = kmalloc(sizeof(cqueue_item_t) * maxSize);
    assert(q->data);
    q->top = 0;
    q->count = 0;
    q->maxSize = maxSize;
}

bool cqueue_push(cqueue_t *q, cqueue_item_t e) {
    assert(q && q->data);
    while (q->count >= q->maxSize) {
        return false;
    }
    q->data[q->top] = e;
    q->top = (q->top + 1) % q->maxSize;
    q->count++;
    return true;
}

cqueue_item_t cqueue_pop(cqueue_t *q) {
    assert(q && q->data);
    if (q->count == 0) {
        return (cqueue_item_t) NULL;
    }
    uint32_t bot = (q->top >= q->count) ? (q->top - q->count) : (q->maxSize - (q->count - q->top));
    assert(bot < q->maxSize);
    cqueue_item_t item =  q->data[bot];
    q->count--;
    return item;
}

void cqueue_free(cqueue_t *q) {
    if (q->data) {
        kfree(q->data);
        q->data = NULL;
    }
    q->top = 0;
    q->count = 0;
    q->maxSize = 0;
}