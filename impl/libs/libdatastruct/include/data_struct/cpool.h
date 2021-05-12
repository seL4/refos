/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
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
