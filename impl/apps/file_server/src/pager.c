/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*! @file
    @brief CPIO Fileserver pager RAM frame block module.
    
    Creates big anon RAM dataspace and uses it to manage allocation/deallocation of the frames of
    the RAM dataspace to be used for paging clients. The frame block works by using a frame pool
    (cpool_t structure), with the big dataspace mapped into a window. We then call data_datamap
    in order to page frames into faulting clients, after initialising them with the requested
    CPIO contents.
*/

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <refos/refos.h>
#include <refos/vmlayout.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>
#include <refos-util/cspace.h>
#include <refos-util/walloc.h>

#include "pager.h"
#include "state.h"

void
pager_init(struct fs_frame_block* fb, uint32_t framesSize)
{
    assert(fb);
    fb->initialised = false;

    /* Initialise the frame pool allocator. */
    dprintf("        Initialising frame block allocator pool...\n");
    assert(framesSize % REFOS_PAGE_SIZE == 0);
    fb->frameBlockNumPages = framesSize / REFOS_PAGE_SIZE;
    cpool_init(&fb->framePool, 1, fb->frameBlockNumPages);

    /* Initialise the anonymouse RAM dataspace to allocate from. */
    dprintf("        Creating pager frame block...\n");
    int error = EINVALID;
    fb->dataspace = data_open(REFOS_PROCSERV_EP, "anon", O_CREAT | O_WRONLY  |O_TRUNC, O_RDWR,
        framesSize, &error);
    if (error != ESUCCESS || !fb->dataspace) {
        ROS_ERROR("page_init failed to open anon dataspace.");
        assert(!"page_init failed to open anon dataspace.");
        return;
    }

    /* Allocate the window to map things into. */
    dprintf("        Allocating frame block window...\n");
    fb->frameBlockVAddr = walloc(fb->frameBlockNumPages, &fb->window);
    if (!fb->frameBlockVAddr || !fb->window) {
        ROS_ERROR("page_init failed to allocate window.");
        assert(!"page_init failed to allocate window.");
        return;
    }
    dprintf("        Allocated frame block window 0x%x --> 0x%x...\n",
        fb->frameBlockVAddr, fb->frameBlockVAddr + framesSize);

    /* Map the dataspace into the window. */
    dprintf("        Datamapping frame block...\n");
    error = data_datamap(REFOS_PROCSERV_EP, fb->dataspace, fb->window, 0);
    if (error != ESUCCESS) {
        ROS_ERROR("page_init failed to datamap dataspace to window.");
        assert(!"page_init failed to datamap dataspace to window.");
        return;
    }

    fb->initialised = true;
}

void
pager_release(struct fs_frame_block* fb)
{
    fb->initialised = false;

    /* Destroy the dataspace and memory window. */
    data_close(REFOS_PROCSERV_EP, fb->dataspace);
    seL4_CNode_Delete(REFOS_CSPACE, fb->dataspace, seL4_WordBits);
    proc_delete_mem_window(fb->window);
    csfree(fb->dataspace);
    csfree(fb->window);

    /* Release the allocator pool. */
    fb->frameBlockVAddr = 0;
    fb->frameBlockNumPages = 0;
    cpool_release(&fb->framePool);
}

vaddr_t
pager_alloc_frame(struct fs_frame_block *fb)
{
    assert(fb && fb->initialised);
    if (!fb->initialised) {
        return (vaddr_t) 0;
    }
    vaddr_t pagen = (vaddr_t) cpool_alloc(&fb->framePool);
    if (pagen == 0 || pagen >= fb->frameBlockNumPages) {
        /* Allocation failed. */
        return (vaddr_t) 0;
    }
    return (vaddr_t) (fb->frameBlockVAddr + (pagen * REFOS_PAGE_SIZE));
}

void
pager_free_frame(struct fs_frame_block *fb, vaddr_t frame)
{
    assert(fb && fb->initialised);
    if (frame < fb->frameBlockVAddr) {
        ROS_WARNING("pager_free_frame: invalid frame vaddr.");
        return;
    }
    int pagen = (frame - fb->frameBlockVAddr) / REFOS_PAGE_SIZE;
    if (pagen <= 0 || pagen >= fb->frameBlockNumPages) {
        ROS_WARNING("pager_free_frame: invalid frame vaddr.");
        return;
    }
    if (cpool_check(&fb->framePool, pagen)) {
        ROS_WARNING("pager_free_frame: frame already freed.");
        return;
    }
    cpool_free(&fb->framePool, pagen);
}
