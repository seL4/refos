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
#include <data_struct/cbpool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

void cbpool_init_static(cbpool_t *p, uint32_t size, char *buffer, int bufferSize) {
    assert(p);
    memset(p, 0, sizeof(cbpool_t));
    p->size = size;
    p->size_ntiles = (size / 32) + 1;
    assert(p->size_ntiles  * sizeof(uint32_t) <= bufferSize);
    p->bitmap = (uint32_t*) buffer;
    assert(p->bitmap);
    memset(p->bitmap, 0, p->size_ntiles * sizeof(uint32_t));
}

void cbpool_init(cbpool_t *p, uint32_t size) {
    assert(p);
    memset(p, 0, sizeof(cbpool_t));
    p->size = size;
    p->size_ntiles = (size / 32) + 1;
    p->bitmap = kmalloc(p->size_ntiles * sizeof(uint32_t));
    assert(p->bitmap);
    memset(p->bitmap, 0, p->size_ntiles * sizeof(uint32_t));
}

void cbpool_release(cbpool_t *p) {
    if (!p) return;
    if (p->bitmap) {
        kfree(p->bitmap);
    }
}

uint32_t cbpool_alloc_internal(cbpool_t *p, uint32_t size, int st, int ed, int inc, bool lt) {
    if (!size) {
        return CBPOOL_INVALID;
    }
    uint32_t freesz = 0;
    for (int i = st; (lt ? (i < ed) : (i >= ed)); i+=inc) {
        if (!cbpool_check_single(p, i)) {
            freesz++;
            if (freesz >= size) {
                uint32_t sta = i - (freesz - 1);
                for (uint32_t j = sta; j <= i; j++) {
                    cbpool_set_single(p, j, true);
                }
                return sta;
            }
        } else {
            freesz = 0;
        }
    }
    return CBPOOL_INVALID;
}

uint32_t cbpool_alloc(cbpool_t *p, uint32_t size) {
    return cbpool_alloc_internal(p, size, 0, p->size, 1, true);
}

void cbpool_free(cbpool_t *p, uint32_t obj, uint32_t size) {
    if (!size) {
        return;
    }
    uint32_t end = obj + size;
    if (end >= p->size) end = (p->size - 1);
    for (int i = obj; i < end; i++) {
        cbpool_set_single(p, i, false);
    }
}

bool cbpool_check_single(cbpool_t *p, uint32_t obj) {
    assert(p && p->bitmap);
    uint32_t idx = obj / 32;
    assert(idx < p->size_ntiles);
    uint32_t b = p->bitmap[idx];
    if (b & (1 << (obj % 32))) {
        return true;
    }
    return false;
}

void cbpool_set_single(cbpool_t *p, uint32_t obj, bool val) {
    assert(p && p->bitmap);
    uint32_t idx = obj / 32;
    assert(idx < p->size_ntiles);
    if (val) {
        p->bitmap[idx] |= (1 << (obj % 32));
    } else {
        p->bitmap[idx] &= ~(1 << (obj % 32));
    }
}

