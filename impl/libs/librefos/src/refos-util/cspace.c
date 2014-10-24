/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <refos-util/cspace.h>
#include <refos/vmlayout.h>

/*! @file
    @brief RefOS client cspace allocator. */

static seL4_CPtr *cspaceFreeList = NULL;
static size_t cspaceFreeListNum = 0;
static bool cspaceStaticAllocated;

void
csalloc_init(seL4_CPtr start, seL4_CPtr end)
{
    size_t sz = end - start;
    csalloc_deinit();
    cspaceFreeList = malloc(sizeof(seL4_CPtr) * sz);
    assert(cspaceFreeList);
    for (int i = 0; i < sz; i++) {
        cspaceFreeList[i] = (seL4_CPtr)(start + i);
    }
    cspaceFreeListNum = sz;
    cspaceStaticAllocated = false;
}

void
csalloc_init_static(seL4_CPtr start, seL4_CPtr end, char* buffer, uint32_t bufferSz)
{
    assert(buffer);

    size_t sz = end - start;
    assert(sizeof(seL4_CPtr) * sz <= bufferSz);

    csalloc_deinit();
    cspaceFreeList = (seL4_CPtr *) buffer;
    for (int i = 0; i < sz; i++) {
        cspaceFreeList[i] = (seL4_CPtr)(start + i);
    }
    cspaceFreeListNum = sz;
    cspaceStaticAllocated = true;
}

void
csalloc_deinit(void)
{
    if (cspaceFreeList && !cspaceStaticAllocated) {
        free(cspaceFreeList);
    }
    cspaceFreeList = NULL;
    cspaceFreeListNum = 0;
}

seL4_CPtr
csalloc(void)
{
    assert(cspaceFreeList);
    if (cspaceFreeListNum == 0) {
        return 0;
    }
    return cspaceFreeList[--cspaceFreeListNum];
}

void
csfree(seL4_CPtr c)
{
    assert(cspaceFreeList);
    cspaceFreeList[cspaceFreeListNum++] = c;
}

void
csfree_delete(seL4_CPtr c)
{
    assert(cspaceFreeList);
    seL4_CNode_Delete(REFOS_CSPACE, c, REFOS_CDEPTH);
    cspaceFreeList[cspaceFreeListNum++] = c;
}




