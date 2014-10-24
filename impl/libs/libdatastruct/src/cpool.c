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
#include <data_struct/cpool.h>
#include <assert.h>
#include <errno.h>

void
cpool_init(cpool_t *p, uint32_t start, uint32_t end)
{
    assert(p);
    p->start = start;
    p->end = end;
    p->mx = start;
    cvector_init(&p->freelist);
}

void
cpool_release(cpool_t *p) {
    if (!p) {
        return;
    }
    cvector_free(&p->freelist);
    cpool_init(p, 0, 0);
}

uint32_t
cpool_alloc(cpool_t *p)
{
    assert(p);

    // First try to allocate from the free list.
    size_t fSz = cvector_count(&p->freelist);
    if (fSz > 0) {
        // Allocate the last item available on the free list.
        cvector_item_t obj = cvector_get(&p->freelist, fSz - 1);
        cvector_delete(&p->freelist, fSz - 1);
        return (uint32_t) obj;
    }

    // Free list exhausted, allocate by increasing max obj ID..
    if (p->mx <= p->end) {
        return (uint32_t) p->mx++;
    }

    // Out of object to allocate.
    return 0;
}

void
cpool_free(cpool_t *p, uint32_t obj)
{
    assert(p);
    if (obj < p->start || obj > p->end || obj >= p->mx) {
        return;
    }
    if (obj == p->mx) {
        // Decrease max obj ID.
        // Note that this implementation doesn't decrease mx for all cases;
        // decreasing mx properly would require a periodic sort of the freelist,
        // and a O(n) loop from the end. Doesn't save _that_ much memory.
        p->mx = (obj - 1);
        return;
    }
    // Add to free list.
    cvector_add(&p->freelist, (cvector_item_t)obj);
}

bool cpool_check(cpool_t *p, uint32_t obj) {
    if (obj < p->start || obj > p->end) {
        // Not free if out of range.
        return false;
    }
    if (obj > p->mx) {
        // Free if in the not-allocated-yet range.
        return true;
    }
    size_t sz = cvector_count(&p->freelist);
    for (int i = 0; i < sz; i++) {
        uint32_t x = (uint32_t)cvector_get(&p->freelist, i);
        if (x == obj) {
            return true;
        }
    }
    return false;
}
