/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CHASH_H_
#define _CHASH_H_

#include <data_struct/cvector.h>

#ifndef kmalloc
    #include <stdlib.h>
    #include <stdint.h>
    #define kmalloc malloc
    #define krealloc realloc
    #define kfree free
#endif

typedef void* chash_item_t;

typedef struct chash_entry_s {
    uint32_t key;
    chash_item_t item;
} chash_entry_t;

typedef struct chash_s {
    cvector_t* table;
    size_t tableSize;
} chash_t;

void chash_init(chash_t *t, size_t sz);

void chash_release(chash_t *t);

chash_item_t chash_get(chash_t *t, uint32_t key);

int chash_set(chash_t *t, uint32_t key, chash_item_t obj);

void chash_remove(chash_t *t, uint32_t key);

int chash_find_free(chash_t *t, uint32_t rangeStart, uint32_t rangeEnd);

#endif /* _CHASH_H_ */
