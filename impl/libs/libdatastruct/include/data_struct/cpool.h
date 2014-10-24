/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CALLOCPOOL_H_
#define _CALLOCPOOL_H_

#include <stdbool.h>
#include <data_struct/cvector.h>

typedef struct cpool_s {
    uint32_t start;
    uint32_t end;
    uint32_t mx;
    cvector_t freelist;
} cpool_t;

void cpool_init(cpool_t *p, uint32_t start, uint32_t end);

void cpool_release(cpool_t *p);

uint32_t cpool_alloc(cpool_t *p);

void cpool_free(cpool_t *p, uint32_t obj);

bool cpool_check(cpool_t *p, uint32_t obj);

#endif /* _CALLOCPOOL_H_ */
