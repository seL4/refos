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
#include <stdbool.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <refos/refos.h>
#include <refos/error.h>
#include <refos/vmlayout.h>
#include <refos-util/walloc.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <data_struct/cbpool.h>
#include <data_struct/chash.h>
#include <refos-util/dprintf.h>

static walloc_state_t _walloc_state;

/* ------------------------------ Walloc-list internal interface  --------------------------------*/

static void
walloc_list_init(walloc_state_t *ws, seL4_Word startAddr, seL4_Word endAddr)
{
    uint32_t sz = endAddr - startAddr;
    if (startAddr % REFOS_PAGE_SIZE != 0) {
        assert(!"walloc_init startAddr is not page aligned.");
        return;
    }
    if (sz % REFOS_PAGE_SIZE != 0) {
        assert(!"walloc_init size is not a multiple of PAGE_SIZE.");
        return;
    }

    /* Initialise the bitmap pool which keeps track of allocated portions of vspace. */
    ws->startAddr = startAddr;
    ws->endAddr = endAddr;
    ws->npages = sz / REFOS_PAGE_SIZE;
    cbpool_init(&ws->pool, ws->npages);

    /* Initialise the windows list. */
    chash_init(&ws->windowCptrMap, WALLOC_WINDOW_CPTR_MAP_HASHSIZE);

    ws->initialised = true;
    ws->magic = WALLOC_MAGIC;
}

static void
walloc_list_deinit(walloc_state_t *ws)
{
    if (!ws->initialised) return;
    chash_release(&ws->windowCptrMap);
    cbpool_release(&ws->pool);
    ws->startAddr = 0;
    ws->endAddr = 0;
    ws->npages = 0;
    ws->initialised = false;
    ws->magic = 0;
}

static seL4_Word
walloc_list_ext(walloc_state_t *ws, int npages, seL4_CPtr *window, uint32_t permission,
                uint32_t flags)
{
    assert(ws->initialised && ws->magic == WALLOC_MAGIC);
    if (!npages) return 0;

    // Allocate window.
    uint32_t startPage = cbpool_alloc(&ws->pool, npages);
    if (startPage == CBPOOL_INVALID) {
        printf("WARNING: walloc out of windows.\n");
        return 0;
    }
    assert(startPage >= 0 && startPage < ws->npages);

    // Calculate the allocated window region address.
    uint32_t regionAddr = ws->startAddr + (startPage * REFOS_PAGE_SIZE);
    assert(regionAddr % REFOS_PAGE_SIZE == 0);

    // Allocate a window at this address.
    seL4_CPtr windowCap = proc_create_mem_window_ext(regionAddr, npages * REFOS_PAGE_SIZE,
            permission, flags);
    if (windowCap == 0 || REFOS_GET_ERRNO() != ESUCCESS) {
        cbpool_free(&ws->pool, startPage, npages);
        printf("WARNING: walloc could not create memory window.\n");
        assert(!"WARNING: walloc could not create procserv memory window.\n");
        return 0;
    }

    // Book keep this allocated window cap.
    int err = chash_set(&ws->windowCptrMap, startPage, (chash_item_t) windowCap);
    assert(!err);
    (void) err;

    if (window != NULL) {
        (*window) = windowCap;
    }
    return regionAddr;
}

static seL4_Word
walloc_list(walloc_state_t *ws, int npages, seL4_CPtr *window)
{
    return walloc_ext(npages, window, PROC_WINDOW_PERMISSION_READWRITE, 0x0);
}

static uint32_t
walloc_list_get_start_page(walloc_state_t *ws, seL4_Word vaddr)
{
    assert(ws->initialised && ws->magic == WALLOC_MAGIC);
    assert(vaddr >= ws->startAddr && vaddr <= ws->endAddr);
    uint32_t startPage = (vaddr - ws->startAddr) / REFOS_PAGE_SIZE;
    assert(startPage >= 0 && startPage < ws->npages);
    return startPage;
}

static seL4_CPtr
walloc_list_get_window_at_vaddr(walloc_state_t *ws, seL4_Word vaddr)
{
    assert(ws->initialised && ws->magic == WALLOC_MAGIC);
    seL4_CPtr windowCap = (seL4_CPtr)
            chash_get(&ws->windowCptrMap, walloc_list_get_start_page(ws, vaddr));
    return windowCap;
}

static void
walloc_list_free(walloc_state_t *ws, uint32_t addr, int npages)
{
    assert(ws->initialised && ws->magic == WALLOC_MAGIC);
    if (!npages) return;
    cbpool_free(&ws->pool, walloc_list_get_start_page(ws, addr), npages);
    seL4_CPtr windowCap = walloc_get_window_at_vaddr(addr);
    if (windowCap) {
        proc_delete_mem_window(windowCap);
        seL4_CNode_Revoke(REFOS_CSPACE, windowCap, REFOS_CSPACE_DEPTH);
        csfree_delete(windowCap);
        chash_remove(&ws->windowCptrMap, walloc_list_get_start_page(ws, addr));
    }
}

/* --------------------------- Userland simplified walloc interface  -----------------------------*/

void
walloc_init(seL4_Word startAddr, seL4_Word endAddr)
{
    walloc_list_init(&_walloc_state, startAddr, endAddr);
}

void
walloc_deinit(void)
{
    walloc_list_deinit(&_walloc_state);
}

seL4_Word
walloc(int npages, seL4_CPtr *window)
{
    return walloc_list(&_walloc_state, npages, window);
}

seL4_Word
walloc_ext(int npages, seL4_CPtr *window, uint32_t permission, uint32_t flags)
{
    return walloc_list_ext(&_walloc_state, npages, window, permission, flags);
}

seL4_CPtr
walloc_get_window_at_vaddr(seL4_Word vaddr)
{
    return walloc_list_get_window_at_vaddr(&_walloc_state, vaddr);
}

void
walloc_free(uint32_t addr, int npages)
{
    walloc_list_free(&_walloc_state, addr, npages);
}