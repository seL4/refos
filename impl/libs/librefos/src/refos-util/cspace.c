/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
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




