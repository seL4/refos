/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CBITMAPALLOCPOOL_H_
#define _CBITMAPALLOCPOOL_H_

#include <stdbool.h>
#include <data_struct/cvector.h>

#define CBPOOL_INVALID ((uint32_t) (-1))

typedef struct cbpool_s {
    uint32_t size;
    uint32_t size_ntiles;
    uint32_t *bitmap;
} cbpool_t;

void cbpool_init(cbpool_t *p, uint32_t size);

void cbpool_init_static(cbpool_t *p, uint32_t size, char *buffer, int bufferSize);

void cbpool_release(cbpool_t *p);

uint32_t cbpool_alloc(cbpool_t *p, uint32_t size);

void cbpool_free(cbpool_t *p, uint32_t obj, uint32_t size);

bool cbpool_check_single(cbpool_t *p, uint32_t obj);

void cbpool_set_single(cbpool_t *p, uint32_t obj, bool val);

#endif /* _CBITMAPALLOCPOOL_H_ */
