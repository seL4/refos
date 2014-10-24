/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_UTIL_CSPACE_H_
#define _REFOS_UTIL_CSPACE_H_

/*! @file
    @brief RefOS client cspace allocator.

    Simple RefOS cspace allocator, used to manage RefOS client cslots. We avoid using the vka
    interface here as most of it would not be relevant; we are not allocating kernel objects, just
    managing cslots. Uses a simple free-list allocator.
*/


#include <refos/refos.h>

/*! @brief Initialise the cspace allocator. Uses malloc() heap memory.
    @param start The start of the cslot range to allocate from.
    @param end The end of the cslot range to allocate from.
*/
void csalloc_init(seL4_CPtr start, seL4_CPtr end);

/*! @brief Initialise the cspace allocator, using a static buffer.

    Like csalloc_init(), but uses a static buffer instead of malloc(). This is used for RefOS user-
    land processes which use a dynamic heap which itself needs a cspace allocator. Thus, we use a
    static cspace allocator buffer in order to avoid this circular dependency. RefOS system proceses
    use a static heap / mmap region and thus do not require static cspace allocator.

    @param start The start of the cslot range to allocate from.
    @param end The end of the cslot range to allocate from.
    @param buffer The static buffer to use. Must be at least ( sizeof(seL4_CPtr) * (start - end) )
                  bytes large. (No ownership)
    @param buffersz The size of the static buffer.
*/
void csalloc_init_static(seL4_CPtr start, seL4_CPtr end, char* buffer, uint32_t bufferSz);

/*! @brief De-initialise the cspace allocator. If the allocator uses a static buffer, it would NOT
           be released. */
void csalloc_deinit(void);

/*! @brief Allocate a cslot.
    @return CPtr to the allocated cslot. (Ownership given)
*/
seL4_CPtr csalloc(void);

/*! @brief Free an allocated cslot. Does NOT actually delete or revoke the cap, so do not do this
           if there is still a capability at the given cslot. Use csfree_delete() in that case.
    @param c The allocate cslot to free.
*/
void csfree(seL4_CPtr c);

/*! @brief Free an allocated cslot, and delete the capability in it. Does NOT revoke the capability.
    @param c The allocate cslot to delete and free.
*/
void csfree_delete(seL4_CPtr c);

#endif /* _REFOS_UTIL_CSPACE_H_ */
