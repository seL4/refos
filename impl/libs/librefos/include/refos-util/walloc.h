/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_UTIL_MEMORY_WINDOW_ALLOCATOR_H_
#define _REFOS_UTIL_MEMORY_WINDOW_ALLOCATOR_H_

#include <stdint.h>
#include <refos/refos.h>
#include <sel4/sel4.h>
#include <data_struct/cbpool.h>
#include <data_struct/chash.h>

/*! @file
    @brief Simple window allocator for RefOS client data mappings.

    This module provides a simple vspace window allocator for quick dataspace mappings. If the
    client simply wants to map and read a dataspace (eg. as a parameter buffer) and manage this
    dynamically without defining static vspace regions, then this module does the job.    
*/

#define WALLOC_MAGIC 0x4E667012
#define WALLOC_WINDOW_CPTR_MAP_HASHSIZE 1024

typedef struct walloc_state_s {
    bool initialised;
    uint32_t magic;

    seL4_Word startAddr;
    seL4_Word endAddr;
    seL4_Word npages;
    cbpool_t pool;

    chash_t windowCptrMap;
} walloc_state_t;

/* --------------------------- Userland simplified walloc interface  -----------------------------*/

/*! @brief Initialise window allocator.
    @param startAddr The starting address of the window allocator.
    @param endAddr The ending address of the allocator region.
*/
void walloc_init(seL4_Word startAddr, seL4_Word endAddr);

/*! @brief De-initialise window allocator, and release all allocated resources. */
void walloc_deinit(void);

/*! @brief Allocate a window. Defaults to read/write permission, no flags.
    @param npages The size of the window (in number of pages, page size is REFOS_PAGE_SIZE, usually
                  4096 bytes).
    @param[out] window Output cptr to store the allocated window capability, if success.
                (No ownership)
    @return VAddr of allocated window if success, 0 otherwise.
*/
seL4_Word walloc(int npages, seL4_CPtr *window);

/*! @brief Allocate a window, with extra options.

    Much like walloc(), but with extra control over permission and flag values of the window.
    eg. used to implement device mappings, which should be mapped uncached.

    @param npages The size of the window (in number of pages, page size is REFOS_PAGE_SIZE, usually
                  4096 bytes).
    @param[out] window Output cptr to store the allocated window capability, if success.
                (No ownership)
    @param permission The window permission flags, defined in proc_client_helper.h.
    @param flags Controls window flags such as cached/uncached. See proc_client_helper.h.
    @return VAddr of allocated window if success, 0 otherwise.
*/
seL4_Word walloc_ext(int npages, seL4_CPtr *window, uint32_t permission, uint32_t flags);

/*! @brief Retrieve the window capability in the current client's vspace at a given vaddr.

    This is a helper function used to avoid the need for double-book-keeping of vspace to window
    capabilities, used for releasing allocated windows without book-keeping them again. Used for
    RefOS userland dynamic mmap implementation.

    @param vaddr The virtual address to retrieve window for.
    @return Copy of capability to window at the given vaddr if any, 0 otherwise. Ownership is given
            for the cslot and cap, so caller must delete those and release the cslot.
*/
seL4_CPtr walloc_get_window_at_vaddr(seL4_Word vaddr);

/*! @brief Free an allocated window.
    @param addr The base vaddr of the allocated window.
    @param npages The size in pages of the allocated window.
*/
void walloc_free(uint32_t addr, int npages);

#endif /* _REFOS_UTIL_MEMORY_WINDOW_ALLOCATOR_H_ */
