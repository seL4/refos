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
#include <assert.h>
#include "dispatcher.h"
#include "../system/process/process.h"
#include <refos/refos.h>

 /*! @file
     @brief Common Process server dispatcher helper functions. */

#define PROCSERV_SYSCALL_PARAM_SIZE_MAX (REFOS_PAGE_SIZE * 8)
static char _paramBuffer[PROCSERV_SYSCALL_PARAM_SIZE_MAX];

seL4_MessageInfo_t _dispatcherEmptyReply;

int
check_dispatch_interface(struct procserv_msg *m, void **userptr, int labelMin, int labelMax)
{
    assert(userptr);
    if (seL4_MessageInfo_get_label(m->message) != seL4_NoFault ||
            !dispatcher_badge_PID(m->badge)) {
        /* Not a Syscall, pass onto next dispatcher. */
        return DISPATCH_PASS;
    }

    struct proc_pcb *pcb = pid_get_pcb_from_badge(&procServ.PIDList, m->badge);
    if (!pcb) {
        ROS_WARNING("Unknown client.");
        return DISPATCH_ERROR;
    }

    seL4_Word syscallFunc = seL4_GetMR(0);
    if (syscallFunc <= labelMin || syscallFunc >= labelMax) {
        /* Not our type of syscall to handle. */
        return DISPATCH_PASS;
    }

    pcb->rpcClient.userptr = (void*) m;
    pcb->rpcClient.minfo = m->message;
    (*userptr) = (void*) pcb;

    return DISPATCH_SUCCESS;
}

bool
check_dispatch_caps(struct procserv_msg *m, seL4_Word unwrappedMask, int numExtraCaps)
{
    assert(m);
    if (seL4_MessageInfo_get_capsUnwrapped(m->message) != unwrappedMask ||
            seL4_MessageInfo_get_extraCaps(m->message) != numExtraCaps) {
        return false;
    }
    return true;
}

seL4_CPtr
dispatcher_copyout_cptr(seL4_CPtr c)
{
    cspacepath_t cslot;
    int error = vka_cspace_alloc_path(&procServ.vka, &cslot);
    if (error || cslot.capPtr == 0) {
        return (seL4_CPtr) 0;
    }
    cspacepath_t src;
    vka_cspace_make_path(&procServ.vka, c, &src);
    error = vka_cnode_copy(&cslot, &src, seL4_AllRights);
    if (error) {
        assert(!"dispatcher_copyout_cptr could not copy cap. Most likely a procserv bug.");
        vka_cspace_free(&procServ.vka, cslot.capPtr);
        return (seL4_CPtr) 0;
    }
    return cslot.capPtr;
}

void
dispatcher_release_copyout_cptr(seL4_CPtr c)
{
    if (!c) {
        /* No capability here, nothing to do. */
        return;
    }
    cspacepath_t cslot;
    vka_cspace_make_path(&procServ.vka, c, &cslot);
    vka_cnode_revoke(&cslot);
    vka_cnode_delete(&cslot);
    vka_cspace_free(&procServ.vka, cslot.capPtr);
}

char*
dispatcher_read_param(struct proc_pcb *pcb, uint32_t readLen)
{
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    /* Check that client actually has a paramBuffer set. */
    if (!pcb->paramBuffer && readLen) {
        dvprintf("WARNING: no param buffer exists.")
        return NULL;
    }

    /* Check readLen is not too big. */
    if (readLen > PROCSERV_SYSCALL_PARAM_SIZE_MAX) {
        ROS_ERROR("Parameter buffer too large to be read.")
        return NULL;
    }
    
    /* Reset the static temp buffer. */
    char *tempBuffer = _paramBuffer;
    memset(tempBuffer, 0, PROCSERV_SYSCALL_PARAM_SIZE_MAX);

    /* Read bytes out of the paramBuffer dataspace. */
    if (readLen > 0) {
        int error = ram_dspace_read(tempBuffer, readLen, pcb->paramBuffer, 0);
        if (error != ESUCCESS) {
            ROS_ERROR("Parameter buffer failed to read from parameter buffer.");
            assert(!"Failed to read from parameter buffer. This shouldn't happen; Procserv OOM.");
            return NULL;
        }
    }

    return tempBuffer;
}

