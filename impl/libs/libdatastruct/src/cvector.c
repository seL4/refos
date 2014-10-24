/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <data_struct/cvector.h>
#include <assert.h>

static void
cvector_delete_resize(cvector_t *v) {
    if (v->nextResize-- > 0) {
        return;
    }
    // Condition to decrease v->data size: size > (c * 2) + k
    if ((v->count + 2) * 2 < v->size) {
        v->size = (v->count + 2);
        v->data = krealloc(v->data, sizeof(cvector_item_t) * v->size);
        v->nextResize = CVECTOR_RESIZE_CHECK_FREQ;
    }
}

void
cvector_init(cvector_t *v)
{
    assert(v);
    v->data = NULL;
    v->size = 0;
    v->count = 0;
    v->nextResize = CVECTOR_RESIZE_CHECK_FREQ;
}

int
cvector_add(cvector_t *v, cvector_item_t e)
{
    assert(v);
    if (v->size == 0) {
        v->size = CVECTOR_INIT_SIZE;
        v->data = kmalloc(sizeof(cvector_item_t) * v->size);
        assert(v->data);
    }

    // Condition to increase v->data: last slot exhausted
    if (v->size <= v->count) {
        v->size *= 2;
        v->data = krealloc(v->data, sizeof(cvector_item_t) * v->size);
        assert(v->data);
    }

    v->data[v->count] = e;
    return v->count++;
}

void
cvector_set(cvector_t *v, uint32_t index, cvector_item_t e)
{
    assert(v);
    assert(index < v->count);
    v->data[index] = e;
}

cvector_item_t
cvector_get(cvector_t *v, int index)
{
    assert(v);
    assert(index < v->count);
    return v->data[index];
}

void
cvector_delete(cvector_t *v, int index)
{
    assert(v);
    assert(index < v->count);
    for(int i = index; i < (v->count - 1); i++) {
        v->data[i] = v->data[i + 1];
    }
    v->count--;
    cvector_delete_resize(v);
}

void
cvector_free(cvector_t *v)
{
    assert(v);
    if (v->data) kfree(v->data);
    cvector_init(v);
}

void
cvector_reset(cvector_t *v)
{
    assert(v);
    v->count = 0;
}
