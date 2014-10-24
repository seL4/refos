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
    @brief CPIO Fileserver pager RAM frame block module. */

#ifndef _FILE_SERVER_FRAME_PAGER_H_
#define _FILE_SERVER_FRAME_PAGER_H_

#include <refos/refos.h>
#include <sel4/types.h>
#include <sel4/sel4.h>
#include <data_struct/cpool.h>

typedef seL4_Word vaddr_t;

/*! @brief CPIO File server RAM frame block

    CPIO Fileserver frame block structure, stores book-keeping data for allocation of frames used
    for paging clients.
 */
struct fs_frame_block {
    bool initialised;
    cpool_t framePool;
    seL4_CPtr dataspace;
    seL4_CPtr window;
    vaddr_t frameBlockVAddr;
    uint32_t frameBlockNumPages;
};

/*! @brief Initialises pager frame block table.
    @param fb The frame block to initialise.
    @param framesSize The size of the pager frame block in bytes. This number must be a multiple
                      of PAGE_SIZE (4k).
    @return A pager frame block if success, NULL otherwise.
 */
void pager_init(struct fs_frame_block* fb, uint32_t framesSize);

/*! @brief Tear downs a pager frame block table and releases all associated memory.
    @param fb The frame block to de-initialise.
*/
void pager_release(struct fs_frame_block* fb);

/*! @brief Allocates a frame from the pager frame block.
    @param fb Pager frame block table to allocate from.
    @return Virtual addr of a pager frame if success, NULL otherwise.
 */
vaddr_t pager_alloc_frame(struct fs_frame_block *fb);

/*! @brief Frees a frame.
    @param fb Pager frame block table to return the frame to.
    @param frame VAddr to the frame to be freed.
 */
void pager_free_frame(struct fs_frame_block *fb, vaddr_t frame);

#endif /* _FILE_SERVER_FRAME_PAGER_H_ */
