/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <data_struct/coat.h>

void coat_init(coat_t *t, int start, int end) {
    assert(t);
    assert(start != 0); /* zero idx is reserved. */
    cpool_init(&t->pool, start, end);
    cvector_init(&t->table);
}

void coat_release(coat_t *t) {
    assert(t);
    size_t count = cvector_count(&t->table);
    for (int i = 0; i < count; i++) {
        cvector_item_t obj = cvector_get(&t->table, i);
        if (!obj) continue;
        if (t->oat_delete) {
            t->oat_delete(t, obj);
        }
    }
    cvector_free(&t->table);
    cpool_release(&t->pool);
}

int coat_alloc(coat_t *t, uint32_t arg[COAT_ARGS], cvector_item_t *out_obj) {
    assert(t);

    // Allocate new ID.
    int id = cpool_alloc(&t->pool);
    if (!id || id == COAT_INVALID_ID) {
        goto error; 
    }

    // Potentially expand ID table vector.
    while (cvector_count(&t->table) <= id) {
        if (t->oat_expand) {
            t->oat_expand(&t->table);
            continue;
        }
        // Defaults to adding NULL pointers to fill ID table.
        cvector_add(&t->table, (cvector_item_t) NULL);
    }

    cvector_item_t obj = cvector_get(&t->table, id);
    if (!obj && t->oat_create) {
        // Create object structure and store it.
        obj = t->oat_create(t, id, arg);
        if (!obj) {
            goto error;
        }
        cvector_set(&t->table, id, obj);
    }

    if (out_obj) {
        (*out_obj) = obj;
    }

    return id;

error:
    if (id) cpool_free(&t->pool, id);
    return COAT_INVALID_ID; 
}

cvector_item_t coat_get(coat_t *t, int id) {
    if (!t || id < t->pool.start || id >= t->pool.end || id >= cvector_count(&t->table)) {
        return (cvector_item_t) NULL;
    }
    return cvector_get(&t->table, id);
}

int coat_free(coat_t *t, int id) {
    assert(t);
    if (cpool_check(&t->pool, id)) {
        // Free but should have been allocated.
        return -EBADF;
    }
    if (t->oat_delete) {
        cvector_item_t obj = coat_get(t, id);
        if (!obj) {
            // No such object.
            return -EBADF;
        }
        t->oat_delete(t, obj);
    }
    cvector_set(&t->table, id, (cvector_item_t) NULL);
    cpool_free(&t->pool, id);
    return 0;
}
