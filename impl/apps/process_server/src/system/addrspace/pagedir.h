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
    @brief Static page directory allocator.

    Pre-allocates a static array of PDs, and provides interface to allocate and reuse them as
    needed. This is done to avoid fragmentation as kernel PD objects are quite big. Also manages
    root CNodes objects associated with the PDs address spaces, as they are quite big and also cause
    fragmentation.

    This module has ownership of the underlying PDs and CNodes, and will manage their creation /
    deletion. Uses a free-list internally to bookkeep re-assignment of previously deleted PDs.
*/

#ifndef _REFOS_PROCESS_SERVER_SYSTEM_ADDRSPACE_PAGE_DIRECTORY_H_
#define _REFOS_PROCESS_SERVER_SYSTEM_ADDRSPACE_PAGE_DIRECTORY_H_

#include <stdio.h>
#include <stdlib.h>
#include <data_struct/cpool.h>
#include <autoconf.h>
#include <allocman/allocman.h>
#include <allocman/vka.h>
#include <sel4utils/util.h>
#include <sel4utils/vspace.h>
#include "../../common.h"

/*! @file
    @brief VSpace Page Directory module. */

#define PD_MAX CONFIG_PROCSERV_MAX_VSPACES

/*! @brief Page directory list structure. */
struct pd_list {
    cpool_t PDpool;
    vka_object_t pd[PD_MAX]; /* Has ownership. */
    vka_object_t cnode[PD_MAX]; /* Has ownership. */
};

/*! @brief Page directory output structure. */
struct pd_info {
    seL4_CPtr kpdObject;
    seL4_CPtr kcnodeObject;
};

/*! @brief Initialises a PD list.
    @param pdlist The allocated struct pd_list structure to initialise.
 */
void pd_init(struct pd_list *pdlist);

/*! @brief Assigns a free PD to use.
    @param pdlist The PD list to assign from. (No ownership)
    @return pd_info struct containing: cptr to an empty PD (No ownership), 
            cptr to empty Root CNode associated with the PD (No ownership), if success.
            Returns NULL if no more PDs left.
 */
struct pd_info pd_assign(struct pd_list *pdlist);

/*! @brief Frees a previously assigned PD from use.
    @param pdlist The PD list to delete from. (No ownership)
    @param pdPtr Reference to PD. Assumes the PD came from a pd_assign from the same list. 
           (Keeps previous ownership)
 */
void pd_free(struct pd_list *pdlist, seL4_CPtr pdPtr);

#endif /* _REFOS_PROCESS_SERVER_SYSTEM_ADDRSPACE_PAGE_DIRECTORY_H_ */
