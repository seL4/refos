/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <autoconf.h>
#include <sel4utils/elf.h>
#include <refos/vmlayout.h>
#include <refos/refos.h>
#include <refos-rpc/proc_server.h>
#include <sel4utils/process.h>

#include "../../state.h"
#include "pid.h"
#include "process.h"
#include "thread.h"
#include "proc_client_watch.h"
#include <refos/refos.h>

/*! @file
    @brief Process management module for process server. */

/* ------------------------------ Proc Helper functions ------------------------------------------*/

static int
proc_staticparam_create_and_set(struct proc_pcb *p, char *param)
{
    assert(p && param);
    size_t paramLen = strlen(param);
    assert(paramLen <= PROCESS_STATICPARAM_SIZE && paramLen <= REFOS_PAGE_SIZE);
    struct vs_vspace *vs = &p->vspace;
    int error = EINVALID;

    /* Allocate the frame to map. */
    vka_object_t frame;
    error = vka_alloc_frame(&procServ.vka, seL4_PageBits, &frame);
    if (error != ESUCCESS || !frame.cptr) {
        ROS_ERROR("Could not allocate frame kobj. Procserv out of memory.");
        return ENOMEM;
    }

    /* Write param data to frame. */
    error = procserv_frame_write(frame.cptr, param, paramLen, 0);
    if (error) {
        ROS_ERROR("Could not write to param frame.");
        error = ENOMEM;
        goto exit0;
    }

    /* Create the static parameter window. */
    int windowID = W_INVALID_WINID;
    error = vs_create_window(vs, PROCESS_STATICPARAM_ADDR, PROCESS_STATICPARAM_SIZE,
            W_PERMISSION_WRITE | W_PERMISSION_READ, true, &windowID);
    if (error) {
        ROS_ERROR("Could not create static param window.");
        goto exit0;
    }

    /* Map the static parameter window. */
    error = vs_map(vs, PROCESS_STATICPARAM_ADDR, &frame.cptr, 1);
    if (error) {
        ROS_ERROR("Could not map static param window.");
        goto exit1;
    }

    return ESUCCESS;

    /* Exit stack. */
exit1:
    if (windowID != W_INVALID_WINID) {
        vs_delete_window(vs, windowID);
    }
exit0:
    vka_free_object(&procServ.vka, &frame);
    return error;
}

static void
proc_pass_badge(struct proc_pcb *p, seL4_CPtr destCSlot, seL4_CPtr ep, seL4_CapRights_t rights,
                seL4_CapData_t badge)
{
    cspacepath_t pathSrc, pathDest;
    vka_cspace_make_path(&procServ.vka, ep, &pathSrc);

    pathDest.root = p->vspace.cspace.capPtr;
    pathDest.capPtr = destCSlot;
    pathDest.capDepth = seL4_WordBits;

    /* Mint a badged endpoint into the client's vspace. */
    int error = vka_cnode_mint(&pathDest, &pathSrc, seL4_AllRights, badge);
    if (error) {
        dprintf("WARNING: proc_pass_badge to dest cslot %d failed.\n", destCSlot);
        assert(!"proc_pass_badge failed to pass badge.");
    }
}

static void
proc_copy_badge(struct proc_pcb *p, seL4_CPtr destCSlot, seL4_CPtr ep, seL4_CapRights_t rights)
{
    cspacepath_t pathSrc, pathDest;
    vka_cspace_make_path(&procServ.vka, ep, &pathSrc);

    pathDest.root = p->vspace.cspace.capPtr;
    pathDest.capPtr = destCSlot;
    pathDest.capDepth = seL4_WordBits;

    /* Move an object into the client's vspace. */
    int error = vka_cnode_copy(&pathDest, &pathSrc, rights);
    if (error) {
        dprintf("WARNING: proc_copy_badge to dest cslot %d failed.\n", destCSlot);
        assert(!"proc_copy_badge failed to pass badge.");
    }
}

static void
proc_setup_environment(struct proc_pcb *p, char *param)
{
    assert(p);

    /* Pass the process its static parameter contents. */
    proc_staticparam_create_and_set(p, param);

    /* Tell the process about ourself, the process server. */
    proc_pass_badge (
            p, REFOS_PROCSERV_EP, procServ.endpoint.cptr,
            seL4_NoRead,
            seL4_CapData_Badge_new(pid_get_badge(p->pid))
    );

    /* Tell the process about its own liveness cap. */
    proc_pass_badge (
            p, REFOS_LIVENESS, procServ.endpoint.cptr,
            seL4_NoRead,
            seL4_CapData_Badge_new(pid_get_liveness_badge(p->pid))
    );

    /* Tell the process about its initial thread. */
    struct proc_tcb *t0 = proc_get_thread(p, 0);
    assert(t0);
    proc_pass_badge (
            p, REFOS_THREAD_TCB, thread_tcb_obj(t0), seL4_AllRights, 
            seL4_CapData_Badge_new(0)
    );

    #if defined(PLAT_PC99)
    /* Give any process with IO port permission to read / write to IO ports. */
    if (p->systemCapabilitiesMask & PROCESS_PERMISSION_DEVICE_IOPORT) {
        proc_copy_badge (
            p, REFOS_DEVICE_IO_PORTS, seL4_CapIOPort, seL4_AllRights
        );
    }
    #else
    (void) proc_copy_badge;
    #endif
}

static void
proc_purge_pid_callback(struct proc_pcb *pcb, void *cookie)
{
    uint32_t deathPID = (int) cookie;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);
    if (pcb->pid == deathPID) {
        return;
    }

    /* Orphan the child PID. */
    if (pcb->parentPID == deathPID) {
        pcb->parentPID = PID_NULL;
        pcb->parentWaiting = false;
    }
}

/* ----------------------------- Proc interface functions ----------------------------------------*/

int
proc_config_new(struct proc_pcb *p, uint32_t pid, uint8_t priority, char *imageName,
                uint32_t systemCapabilitiesMask)
{
    assert(p);
    if (!imageName) {
        return EINVALIDPARAM;
    }
    memset(p, 0, sizeof(struct proc_pcb));
    int error = EINVALID;

    /* Set initial info. */
    p->magic = REFOS_PCB_MAGIC;
    p->systemCapabilitiesMask = systemCapabilitiesMask;
    p->pid = pid;
    p->paramBuffer = NULL;
    p->notificationBuffer = NULL;

    /* Allocate a vspace. */
    dvprintf("Initialising vspace for %s...\n", imageName);
    error = vs_initialise(&p->vspace, pid);
    if (error != ESUCCESS) {
        dprintf("Failed vspace allocation.\n");
        goto exit0;
    }

    /* Create thread. */
    dvprintf("Allocating thread structure for %s...\n", imageName);
    cvector_init(&p->threads);
    struct proc_tcb *thread = kmalloc(sizeof(struct proc_tcb));
    if (!thread) {
        ROS_ERROR("Failed to malloc thread structure.\n");
        error = ENOMEM;
        goto exit1;
    }

    /* Load ELF image from CPIO archive. */
    dvprintf("Loading ELF file %s...\n", imageName);
    void *entryPoint = sel4utils_elf_load (
            &p->vspace.vspace, &procServ.vspace, &procServ.vka,
            &procServ.vka, imageName
    );
    if (entryPoint == NULL) {
        ROS_ERROR("Failed to load ELF file %s.", imageName);
        error = EINVALIDPARAM;
        goto exit2;
    }

    uintptr_t sysInfo = sel4utils_elf_get_vsyscall(imageName);

    /* Configure initial thread. Note that we do this after loading the ELF into vspace, to
       avoid potentially clobbering the vspace ELF regions. */
    error = thread_config(thread, priority, (vaddr_t) entryPoint, &p->vspace);
    if (error) {
        ROS_ERROR("Failed to configure thread for %s.", imageName);
        goto exit2;
    }

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
    sel4utils_process_t n_process;
    n_process.vspace = p->vspace.vspace;
    n_process.thread = thread->sel4utilsThread;
    n_process.sysinfo = sysInfo;
    n_process.entry_point = entryPoint;

    error = sel4utils_spawn_process_v(&n_process, &procServ.vka, &procServ.vspace, 0, NULL, 0);
    if (error) {
        ROS_ERROR("Failed to spawn process for %s.", imageName);
        goto exit2;
    }

    thread->sel4utilsThread = n_process.thread;

    /* Add thread to list. */
    dvprintf("Adding to threads list...\n");
    cvector_add(&p->threads, (cvector_item_t) thread);
    assert(cvector_count(&p->threads) == 1);

    /* Initialise miscellaneous process state. */
    client_watch_init(&p->clientWatchList);
    strcpy(p->debugProcessName, imageName);

    return ESUCCESS;

    /* Exit stack. */
exit2:
    kfree(thread);
exit1:
    vs_unref(&p->vspace);
exit0:
    memset(p, 0, sizeof(struct proc_pcb));
    return error;
}

int
proc_start_thread(struct proc_pcb *p, int tindex, void* arg0, void* arg1)
{
    dvprintf("Starting PID %d thread %d!!!\n", p->pid, tindex);
    assert(cvector_count(&p->threads) >= 1);
    struct proc_tcb *t = (struct proc_tcb *) cvector_get(&p->threads, tindex);
    assert(t);
    assert(t->entryPoint);
    return thread_start(t, arg0, arg1);
}

int
proc_load_direct(char *name, int priority, char *param, unsigned int parentPID,
                 uint32_t systemCapabilitiesMask)
{
    /* Allocate a PID. */
    dprintf("Allocating PID and PCB...\n");
    uint32_t npid = pid_alloc(&procServ.PIDList);
    if (npid == PID_NULL) {
        dprintf("Failed PID allocation.\n");
        return ENOMEM;
    }
    dprintf("Allocated PID %d!...\n", npid);
    struct proc_pcb *pcb = pid_get_pcb(&procServ.PIDList, npid);
    assert(pcb);

    /* Configure the process. */
    dprintf("Configuring process for PID %d!...\n", npid);
    int error = proc_config_new(pcb, npid, priority, name, systemCapabilitiesMask);
    if (error != ESUCCESS) {
        pid_free(&procServ.PIDList, npid);
        return error;
    }
    pcb->parentPID = parentPID;

    /* If we are selfloading this process, then the actual image name is in the param string.
       This is a bit of a hacky way, but the debug name is only used for debugging so its not too
       bad. */
    if (!strcmp(name, "selfloader")) {
        strcpy(pcb->debugProcessName, param);
    }

    /* Configure the process' vspace and cspace for the RefOS userland environment. */
    proc_setup_environment(pcb, param);

    /* Start the initial thread (thread 0). */
    error = proc_start_thread(pcb, 0, NULL, NULL);
    if (error) {
        ROS_ERROR("Could not start thread 0!");
        pid_free(&procServ.PIDList, npid);
        return error;
    }

    return ESUCCESS;
}

int
proc_nice(struct proc_pcb *p, int tindex, int priority)
{
    assert(cvector_count(&p->threads) >= 1);
    struct proc_tcb *t = (struct proc_tcb *) cvector_get(&p->threads, tindex);
    if (!t) {
        ROS_WARNING("proc_nice warning: no such thread %d!", tindex);
        return EINVALIDPARAM;
    }
    assert(thread_tcb_obj(t));
    int error = seL4_TCB_SetPriority(thread_tcb_obj(t), priority);
    if (error) {
        ROS_ERROR("proc_nice failed to set TCB priority.");
        return EINVALID;
    }
    t->priority = priority;
    return ESUCCESS;
}

static void proc_parent_reply(struct proc_pcb *p);

void
proc_release(struct proc_pcb *p)
{
    assert(p && p->magic == REFOS_PCB_MAGIC);
    memset(&p->rpcClient, 0, sizeof(rpc_client_state_t));

    /* For anybody that's watching us, they've got to be notified, and then unwatched. */
    pid_iterate(&procServ.PIDList, client_watch_notify_death_callback, (void*) p->pid);

    /* For any children PID under us, they are now orphaned. */
    dvprintf("    orphaning children...\n");
    pid_iterate(&procServ.PIDList, proc_purge_pid_callback, (void*) p->pid);

    /* If there's potentially a process waiting for us to exit, then notify that process. */
    dvprintf("    replying parent...\n");
    if (p->parentPID != PID_NULL) {
        proc_parent_reply(p);
    }

    /* Unreference the parameter buffer. */
    dvprintf("    unreffing parameter buffer...\n");
    if (p->paramBuffer) {
        ram_dspace_unref(p->paramBuffer->parentList, p->paramBuffer->ID);
        p->paramBuffer = NULL;
    }

    /* Release notification buffer. */
    dvprintf("    releasing notification buffer...\n");
    if (p->notificationBuffer) {
        rb_delete(p->notificationBuffer);
        p->notificationBuffer = NULL;
    }

    /* Release fault reply cap. */
    dvprintf("    releasing caller EP...\n");
    if (p->faultReply.capPtr) {
        vka_cnode_delete(&p->faultReply);
        vka_cspace_free(&procServ.vka, p->faultReply.capPtr);
        p->faultReply.capPtr = 0;
    }

    /* Unreference vspace. */
    dvprintf("    Unref vspace...\n");
    vs_unref(&p->vspace);

    /* Clean up threads. */
    dvprintf("    Cleaning up threads...\n");
    int nthreads = cvector_count(&p->threads);
    for (int i = 0; i < nthreads; i++) {
        dvprintf("       Cleaning up thread %d...\n", i);
        struct proc_tcb *thread = (struct proc_tcb *) cvector_get(&p->threads, i);
        assert(thread && thread->magic == REFOS_PROCESS_THREAD_MAGIC);
        thread_release(thread);
        kfree(thread);
    }
    cvector_free(&p->threads);

    dvprintf("    process released OK.\n");
    p->magic = 0;
    p->pid = 0;
    p->systemCapabilitiesMask = 0x0;
}

void
proc_queue_release(struct proc_pcb *p)
{
    assert(p && p->magic == REFOS_PCB_MAGIC);
    assert(procServ.exitProcessPID == PID_NULL);
    procServ.exitProcessPID = p->pid;
}

/* ------------------------------- Proc interface helper functions ------------------------------ */

void
proc_set_parambuffer(struct proc_pcb *p, struct ram_dspace *paramBuffer)
{
    assert(p && p->magic == REFOS_PCB_MAGIC);
    if (p->paramBuffer == paramBuffer) {
        /* Same parameter buffer, nothing to do here. */
        return;
    } else if (p->paramBuffer != NULL) {
        /* We need to undeference the previous parameter buffer. */
        ram_dspace_unref(p->paramBuffer->parentList, p->paramBuffer->ID);
        p->paramBuffer = NULL;
    }
    if (paramBuffer != NULL) {
        /* Now reference the new parameter buffer. */
        ram_dspace_ref(paramBuffer->parentList, paramBuffer->ID);
    }
    p->paramBuffer = paramBuffer;
}

int
proc_set_notificationbuffer(struct proc_pcb *p, struct ram_dspace *notifBuffer)
{
    /* Release old notification buffer. */
    if (p->notificationBuffer) {
        rb_delete(p->notificationBuffer);
        p->notificationBuffer = NULL;
    }
    if (!notifBuffer) {
        return ESUCCESS;
    }
    p->notificationBuffer = rb_create(notifBuffer, RB_WRITEONLY);
    if (!p->notificationBuffer) {
        ROS_ERROR("Could not create notification buffer");
        return ENOMEM;
    }
    return ESUCCESS;
}

void
proc_dspace_delete_callback(struct proc_pcb *p, void *cookie)
{
    assert(p && p->magic == REFOS_PCB_MAGIC);
    struct ram_dspace *dspace = (struct ram_dspace *) cookie;
    assert(dspace && dspace->magic == RAM_DATASPACE_MAGIC);

    if (p->paramBuffer) {
        if (p->paramBuffer == dspace || (p->paramBuffer->parentList == dspace->parentList &&
                p->paramBuffer->ID == dspace->ID)) {
            /* The process's parameter buffer just got deleted. Unset the process's parambuffer. */
            proc_set_parambuffer(p, NULL);
            assert(p->paramBuffer == NULL);
        }
    }

    if (p->notificationBuffer) {
        if (p->notificationBuffer->dataspace == dspace ||
                (p->notificationBuffer->dataspace->parentList == dspace->parentList &&
                 p->notificationBuffer->dataspace->ID == dspace->ID) ) {
            rb_delete(p->notificationBuffer);
            p->notificationBuffer = NULL;
        }
    }
    
}

struct proc_tcb *
proc_get_thread(struct proc_pcb *p, int tindex)
{
    assert(p && p->magic == REFOS_PCB_MAGIC);
    if (tindex >= cvector_count(&p->threads)) {
        return NULL;
    }
    struct proc_tcb *t = (struct proc_tcb *) cvector_get(&p->threads, tindex);
    assert(t && t->magic == REFOS_PROCESS_THREAD_MAGIC);
    assert(t->entryPoint);
    return t;
}

int
proc_save_caller(struct proc_pcb *p)
{
    assert(p && p->magic == REFOS_PCB_MAGIC);

    /* Release any previous fault reply cap. */
    if (p->faultReply.capPtr) {
        vka_cnode_delete(&p->faultReply);
        vka_cspace_free(&procServ.vka, p->faultReply.capPtr);
        p->faultReply.capPtr = 0;
    }

    /* Allocate the cslot for reply cap to be saved to. */
    int error = vka_cspace_alloc_path(&procServ.vka, &p->faultReply);
    if (error != seL4_NoError) {
        ROS_ERROR("Could not allocate path to save caller. PRocserv outof cslots.");
        return ENOMEM;
    }
    assert(p->faultReply.capPtr);

    /* Save the caller cap. */
    error = vka_cnode_saveCaller(&p->faultReply);
    if (error != seL4_NoError) {
        ROS_ERROR("Could not copy out reply cap.");
        return EINVALID;
    }

    return ESUCCESS;
}

int
proc_clone(struct proc_pcb *p, int *threadID, vaddr_t stackAddr, vaddr_t entryPoint)
{
    assert(p && p->magic == REFOS_PCB_MAGIC);
    if (threadID) {
        (*threadID) = -1;
    }

    /* No support for fork() behaviour. */
    if (!stackAddr || !entryPoint) {
        return EINVALIDPARAM;
    }

    /* Get the main thread of process to read its priority. */
    struct proc_tcb * t = proc_get_thread(p, 0);
    if (!t) {
        ROS_ERROR("Failed to retrieve main thread.\n");
        return EINVALID;
    }

    /* Create the TCB struct for the clone thread. */
    dvprintf("Allocating thread structure...\n");
    struct proc_tcb *thread = kmalloc(sizeof(struct proc_tcb));
    if (!thread) {
        ROS_ERROR("Failed to malloc thread structure.\n");
        return ENOMEM;
    }

    /* Configure new thread, sharing the process's address space */
    int error = thread_config(thread, t->priority, (vaddr_t) entryPoint, &p->vspace);
    if (error) {
        ROS_ERROR("Failed to configure thread for new thread.");
        goto exit1;
    }

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
    sel4utils_process_t n_process;
    n_process.vspace = p->vspace.vspace;
    n_process.thread = thread->sel4utilsThread;
    n_process.sysinfo = 0;
    n_process.entry_point = (void *) entryPoint;

    error = sel4utils_spawn_process_v(&n_process, &procServ.vka, &procServ.vspace, 0, NULL, 0);
    if (error) {
        ROS_ERROR("Failed to spawn process.");
        goto exit1;
    }

    thread->sel4utilsThread = n_process.thread;

    /* Add thread to list. */
    dvprintf("Adding to threads list...\n");
    int tID = cvector_count(&p->threads);
    if (threadID) {
        (*threadID) = tID;
    }
    cvector_add(&p->threads, (cvector_item_t) thread);
    assert(cvector_count(&p->threads) >= 1);

    /* Start the new child thread. */
    error = proc_start_thread(p, tID, NULL, NULL);
    if (error) {
        ROS_ERROR("Could not start child thread %d!", tID);
        goto exit2;
    }

    return ESUCCESS;

    /* Exit stack. */
exit2:
    cvector_delete(&p->threads, tID);
    thread_release(thread);
exit1:
    kfree(thread);
    assert(error != ESUCCESS);
    return error;
}

extern seL4_MessageInfo_t _dispatcherEmptyReply;

void
proc_fault_reply(struct proc_pcb *p)
{
    if (!p->faultReply.capPtr) {
        return;
    }
    seL4_Send(p->faultReply.capPtr, _dispatcherEmptyReply);
    vka_cnode_delete(&p->faultReply);
    vka_cspace_free(&procServ.vka, p->faultReply.capPtr);
    p->faultReply.capPtr = 0;
}

static void
proc_parent_reply(struct proc_pcb *p)
{
    assert(p->parentPID != PID_NULL);

    /* Retrieve the parent PCB. */
    struct proc_pcb* parentPCB = pid_get_pcb(&procServ.PIDList, p->parentPID);
    if (!parentPCB) {
        ROS_WARNING("proc_parent_reply Could not get parent PID.");
        return;
    }
    if (!parentPCB->parentWaiting) {
        /* Nothing to do here. */
        return;
    }
    if (!parentPCB->faultReply.capPtr) {
        ROS_WARNING("proc_parent_reply No reply endpoint! Book-keeping error.");
        return;
    }

    dprintf("proc_parent_reply childPID %d parentPID %d.\n", p->pid, parentPCB->pid);
    parentPCB->rpcClient.skip_reply = false;
    parentPCB->rpcClient.reply = parentPCB->faultReply.capPtr;
    reply_proc_new_proc((void*) parentPCB, &p->exitStatus, (refos_err_t) EXIT_SUCCESS);

    vka_cnode_delete(&parentPCB->faultReply);
    vka_cspace_free(&procServ.vka, parentPCB->faultReply.capPtr);
    parentPCB->faultReply.capPtr = 0;
}

void
proc_syscall_postaction(void)
{
     if (procServ.exitProcessPID != PID_NULL) {
        struct proc_pcb *pcb = pid_get_pcb(&procServ.PIDList, procServ.exitProcessPID);
        if (!pcb) {
            ROS_WARNING("proc_syscall_postaction error: No such PID!");
            procServ.exitProcessPID = PID_NULL;
            return;
        }

        /* Delete the actual process. */
        uint32_t pid = pcb->pid;
        pcb->rpcClient.skip_reply = true;
        dprintf("    Releasing process %d [%s]...\n", pcb->pid, pcb->debugProcessName);
        proc_release(pcb);

        /* Kill the PID. */
        dvprintf("    Freeing PID...\n");
        pid_free(&procServ.PIDList, pid);
        dvprintf("    Process exit OK!\n");

        procServ.exitProcessPID = PID_NULL;
    }
}
