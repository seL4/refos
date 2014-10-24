/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "thread.h"
#include "../../state.h"
#include "../../common.h"
#include "../addrspace/vspace.h"

/* Forward declaration here, to work around the assumption by sel4utils_start_thread that the thread
   stack is in same vspace as the caller. If someone lifts that assumption at some point, this code
   should be fixed accordingly.
*/
int sel4utils_internal_start_thread(sel4utils_thread_t *thread, void *entry_point, void *arg0,
        void *arg1, int resume, void *local_stack_top, void *dest_stack_top);
int
thread_config(struct proc_tcb *thread, uint8_t priority, vaddr_t entryPoint,
                   struct vs_vspace *vspace)
{
    assert(thread);
    if (!entryPoint || !vspace) {
        memset(thread, 0, sizeof(struct proc_tcb));
        return EINVALIDPARAM;
    }

    /* Configure the new thread struct. */
    dprintf("Configuring new thread 0x%x..\n", (uint32_t) thread);
    memset(thread, 0, sizeof(struct proc_tcb));
    thread->magic = REFOS_PROCESS_THREAD_MAGIC;
    thread->priority = priority;
    thread->entryPoint = entryPoint;
    thread->vspaceRef = vspace;
    vs_ref(vspace);

    /* Configure the thread object. */
    int error = sel4utils_configure_thread(
            &procServ.vka, &procServ.vspace, &vspace->vspace, REFOS_PROCSERV_EP,
            priority, vspace->cspace.capPtr, vspace->cspaceGuardData,
            &thread->sel4utilsThread
    );
    if (error) {
        ROS_ERROR("Failed to configure thread for new process, error: %d.\n", error);
        memset(thread, 0, sizeof(struct proc_tcb));
        vs_unref(vspace);
        return EINVALID;
    }

    return ESUCCESS;
}

int
thread_start(struct proc_tcb *thread, void *arg0, void *arg1)
{
    assert(thread);
    assert(thread->entryPoint);

    /* Start the thread using seL4utils library. */
    int error = sel4utils_internal_start_thread (
            &thread->sel4utilsThread,
            (void *) thread->entryPoint,
            arg0, arg1,
            true, NULL, thread->sel4utilsThread.stack_top
    );
    if (error) {
        ROS_ERROR("sel4utils_start_thread failed. error: %d.", error);
        return EINVALID;
    }

    return ESUCCESS;
}

void
thread_release(struct proc_tcb *thread)
{
    assert(thread);
    assert(thread->vspaceRef);
    cspacepath_t path;
    vka_cspace_make_path(&procServ.vka, thread_tcb_obj(thread), &path);
    vka_cnode_revoke(&path);
    sel4utils_clean_up_thread(&procServ.vka, &thread->vspaceRef->vspace, &thread->sel4utilsThread);
    vs_unref(thread->vspaceRef);
    memset(thread, 0, sizeof(struct proc_tcb));
}