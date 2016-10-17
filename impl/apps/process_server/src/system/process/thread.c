/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */

#include "thread.h"
#include "../../state.h"
#include "../../common.h"
#include "../addrspace/vspace.h"

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

    /* Future Work 1:
       How the process server creates and starts new processes and threads should be modified.
       Currently the process server creates new processes by creating a 'dummy'
       sel4utils_process_t structure and then making a call to sel4utils_spawn_process_v()
       with the sel4utils_process_t structure. The existing vspace, sel4utilsThread and entryPoint
       are copied into the sel4utils_process_t structure. Instead of using sel4utils_process_t the
       conventional way, RefOS implements its own structure for managing processes and threads. The
       RefOS defined proc_pcb structure performs overall the same functionality as the (now existing)
       sel4utils_process_t which most seL4 projects rely on. Further RefOS work is to modify RefOS
       so that it does not use its proc_pcb structure and instead uses the sel4utils_process_t structure
       entirely.
    */
    /* Start the thread using seL4_TCB_Resume() */
    int error = seL4_TCB_Resume(thread->sel4utilsThread.tcb.cptr);
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
