/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "../../common.h"
#include "../../state.h"
#include "pagedir.h"
#include <sel4utils/mapping.h>

/*! @file
    @brief Static page directory allocator.

    This procserv module is responsible for allocation and deallocation of client kernel page
    directory objects, and root cnode objects. Because these objects are really big, continually
    allocating and re-allocating them could fail due to memory fragmentation.
*/

#define PD_ID_BASE 1

void
pd_init(struct pd_list *pdlist)
{
    assert(pdlist);

    /* Initialise the allocation pool. */
    dprintf("Initialising static Page Directory pool (sized %d)...\n", PD_MAX);
    cpool_init(&pdlist->PDpool, PD_ID_BASE, PD_ID_BASE + PD_MAX);

    /* Statically allocate the page directories. */
    for (int i = 0; i < PD_MAX; i++) {

        /* Allocate the kernel PD object. */
        int error = vka_alloc_page_directory(&procServ.vka, &pdlist->pd[i]);
        if (error) {
            ROS_ERROR("Failed to allocate page directory. error %d\n", error);
            assert(!"Procserv initialisation failed. Try lowering CONFIG_PROCSERV_MAX_VSPACES.");
            return;
        }
        assert(pdlist->pd[i].cptr != 0);

        /* If we are on mainline, we need to assign a kernel ASID. ASID Pools have been removed
           from the newer experimental kernels. */
        #ifndef CONFIG_KERNEL_STABLE
        #ifndef CONFIG_X86_64
            error = seL4_ARCH_ASIDPool_Assign(seL4_CapInitThreadASIDPool, pdlist->pd[i].cptr);
            assert(error == seL4_NoError);
        #endif
        #endif

        /* Allocate the kernel Root CNode object. */
        error = vka_alloc_cnode_object(&procServ.vka, REFOS_CSPACE_RADIX, &pdlist->cnode[i]);
        if (error) {
            ROS_ERROR("Failed to allocate Root CNode. error %d\n", error);
            assert(!"Procserv initialisation failed. Try lowering CONFIG_PROCSERV_MAX_VSPACES.");
            return;
        }
        assert(pdlist->cnode[i].cptr != 0);
    }
}

struct pd_info 
pd_assign(struct pd_list *pdlist)
{
    assert(pdlist);
    struct pd_info info;
    uint32_t pdID = cpool_alloc(&pdlist->PDpool);
    if (!pdID) {
        info.kpdObject = 0;
        info.kcnodeObject = 0;
        return info;
    }
    info.kpdObject = pdlist->pd[pdID - 1].cptr;
    info.kcnodeObject = pdlist->cnode[pdID - 1].cptr;
    return info;
}

void
pd_free(struct pd_list *pdlist, seL4_CPtr pdPtr)
{
    assert(pdlist);

    /* First, we loop through our list and find the associated vka object with this given cptr. */
    int idx = -1;
    for (int i = 0; i < PD_MAX; i++) {
        if (pdlist->pd[i].cptr == pdPtr) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        /* Could not find the given cptr. */
        ROS_WARNING("pd_free failed: no such page directory cptr %d\n", pdPtr);
        return;
    }

    if (cpool_check(&pdlist->PDpool, idx + 1)) {
        /* Already free. */
        ROS_WARNING("pd_free failed: page directory cptr %d is already free.\n", pdPtr);
        return;
    }

    /* Delete all derived caps from this PD. */
    cspacepath_t cpath;
    vka_cspace_make_path(&procServ.vka, pdPtr, &cpath);
    vka_cnode_revoke(&cpath);

    /* Delete this PD's associated root CNode. */
    vka_cspace_make_path(&procServ.vka, pdlist->cnode[idx].cptr, &cpath);
    vka_cnode_revoke(&cpath);
    vka_cnode_delete(&cpath);
    vka_free_object(&procServ.vka, &pdlist->cnode[idx]);

    /* Allocate a new kernel Root CNode object. */
    int error = vka_alloc_cnode_object(&procServ.vka, REFOS_CSPACE_RADIX, &pdlist->cnode[idx]);
    if (error) {
        ROS_ERROR("Failed to re-allocate Root CNode. error %d\n", error);
        return;
    }

    /* Put it back into the allocation pool. */
    cpool_free(&pdlist->PDpool, idx + 1);
}