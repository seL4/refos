/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _C_OBJECT_ALLOCATION_TABLE_H_
#define _C_OBJECT_ALLOCATION_TABLE_H_

#include <stdint.h>
#include <stdbool.h>
#include <data_struct/cvector.h>
#include <data_struct/cpool.h>

#define COAT_INVALID_ID -1
#define COAT_ARGS 4

struct coat_s;
typedef struct coat_s coat_t;

// Not a bad idea to 'inherit' this struct.
struct coat_s {
    // Config
    void (*oat_expand)(cvector_t *table);
    cvector_item_t (*oat_create)(coat_t *oat, int id, uint32_t arg[COAT_ARGS]);
    void (*oat_delete)(coat_t *oat, cvector_item_t *obj);

    // Members.
    cpool_t /* indexes */ pool;
    cvector_t /* cvector_item_t */ table;
};

void coat_init(coat_t *t, int start, int end);

void coat_release(coat_t *t);

int coat_alloc(coat_t *t, uint32_t arg[COAT_ARGS], cvector_item_t *out_obj);

cvector_item_t coat_get(coat_t *t, int id);

int coat_free(coat_t *t, int id);

#endif /* _C_OBJECT_ALLOCATION_TABLE_H_ */
