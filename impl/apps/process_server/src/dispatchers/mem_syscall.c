/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*! \file mem_syscall.c
    @brief Dispatcher for the procserv memory related syscalls.
*/

#include "proc_syscall.h"
#include "mem_syscall.h"

#include <refos/vmlayout.h>
#include <refos-rpc/proc_server.h>

#include "../system/process/process.h"
#include "../system/memserv/window.h"
#include "../system/addrspace/vspace.h"

/*! @file
    @brief Handles process server memory-related syscalls.

    This module handles memory-related process server syscalls, functions defined in the generated
    file <refos-rpc/proc_server.h>. Although technically these syscalls are still process server
    interface syscalls and should belong in proc_syscall module, there is enough code here to be
    separted into two files for better code organisation.
*/

/*! @brief Handles memory window creation syscalls.

    The window must not be overlapping with an existing window in the client's VSpace, or
    EINVALIDPARAM will be the returned.
    
    When mapping a dataspace to a non-page aligned window, the dataspace will actually be mapped to
    the page-aligned address of the window base due to technical restrictions. Thus, the first B -
    PAGE_ALIGN(B) bytes of the mapped dataspace is unaccessible. This can have unintended effects
    when two process map the same dataspace for sharing purposes. In other words, when sharing
    dataspaces, it's easiest for the window bases for BOTH processes to be page-aligned.
 */
seL4_CPtr
proc_create_mem_window_internal_handler(void *rpc_userptr , uint32_t rpc_vaddr , uint32_t rpc_size ,
                                        uint32_t rpc_permissions, uint32_t flags,
                                        refos_err_t* rpc_errno)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    /* Check that this window does not override protected kernel memory. */
    if (rpc_vaddr >= PROCESS_KERNEL_RESERVED || PROCESS_KERNEL_RESERVED < (rpc_size + rpc_vaddr)) {
        dvprintf("memory window out of bounds, overlaps kernel reserved.\n");
        SET_ERRNO_PTR(rpc_errno, EINVALIDPARAM);
        return 0;
    }

    /* Create the window. */
    int windowID = W_INVALID_WINID;
    bool cached = (flags & W_FLAGS_UNCACHED) ? false : true;
    int error = vs_create_window(&pcb->vspace, rpc_vaddr, rpc_size, rpc_permissions, cached,
            &windowID);
    if (error != ESUCCESS || windowID == W_INVALID_WINID) {
        dvprintf("Could not create window.\n");
        SET_ERRNO_PTR(rpc_errno, error);
        return 0;
    }

    /* Find the window and return the window capability. */
    struct w_window* window = w_get_window(&procServ.windowList, windowID);
    if (!window) {
        assert(!"Successfully allocated window failed to be found. Process server bug.");
        /* Cannot recover from this situation cleanly. Shouldn't ever happen. */
        SET_ERRNO_PTR(rpc_errno, EINVALID);
        return 0;
    }

    assert(window->magic == W_MAGIC);
    assert(window->capability.capPtr);
    SET_ERRNO_PTR(rpc_errno, ESUCCESS);
    return window->capability.capPtr;
}

/*! @brief Handles memory window resize syscalls. */
refos_err_t
proc_resize_mem_window_handler(void *rpc_userptr , seL4_CPtr rpc_window , uint32_t rpc_size)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        dvprintf("Warning: proc_resize_mem_window invalid window cap.\n");
        return EINVALIDWINDOW;
    }

    if (!dispatcher_badge_window(rpc_window)) {
        dvprintf("Warning: proc_resize_mem_window invalid window badge.\n");
        return EINVALIDWINDOW;
    }

    /* Perform the actual window resize operation. */
    return vs_resize_window(&pcb->vspace, rpc_window - W_BADGE_BASE, rpc_size);
}

/*! @brief Handles memory window deletion syscalls. */
refos_err_t
proc_delete_mem_window_handler(void *rpc_userptr , seL4_CPtr rpc_window)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDWINDOW;
    }

    if (!dispatcher_badge_window(rpc_window)) {
        return EINVALIDWINDOW;
    }

    /* Perform the actual window deletion. Also unmaps the window. */
    vs_delete_window(&pcb->vspace, rpc_window - W_BADGE_BASE);
    return ESUCCESS;
}

seL4_CPtr
proc_get_mem_window_handler(void *rpc_userptr , uint32_t rpc_vaddr)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    /* Find the window at the vaddr. */
    struct w_associated_window *aw = w_associate_find(&pcb->vspace.windows, rpc_vaddr);
    if (!aw) {
        return 0;
    }

    /* Retrieve the window from global window list. */
    struct w_window *window = w_get_window(&procServ.windowList, aw->winID);
    if (!window) {
        ROS_ERROR("Failed to find associated window in global list. Procserv book-keeping bug.");
        return 0;
    }

    assert(window->capability.capPtr);
    return window->capability.capPtr;
}

seL4_CPtr
proc_get_mem_window_dspace_handler(void *rpc_userptr , seL4_CPtr rpc_window ,
                                   refos_err_t* rpc_errno)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        SET_ERRNO_PTR(rpc_errno, EINVALIDWINDOW);
        return 0;
    }

    if (!dispatcher_badge_window(rpc_window)) {
        SET_ERRNO_PTR(rpc_errno, EINVALIDWINDOW);
        return 0;
    }
    
    /* Retrieve the window from global window list. */
    struct w_window *window = w_get_window(&procServ.windowList, rpc_window - W_BADGE_BASE);
    if (!window) {
        ROS_ERROR("Failed to find associated window in global list. Procserv book-keeping bug.");
        SET_ERRNO_PTR(rpc_errno, EINVALIDWINDOW);
        return 0;
    }

    if (window->mode != W_MODE_ANONYMOUS) {
        SET_ERRNO_PTR(rpc_errno, ESUCCESS);
        return 0;
    }
    assert(window->ramDataspace && window->ramDataspace->magic == RAM_DATASPACE_MAGIC);

    SET_ERRNO_PTR(rpc_errno, ESUCCESS);
    return window->ramDataspace->capability.capPtr;
}


/*! @brief Handles server pager setup syscalls.

    A dataserver calls the process server with this call in order to set up to be the pager of
    one of its client processes, for a particular window. The client process is identified by the
    passing of its liveliness cap. All faults for the client's process which happen at that window
    will then be delegated to the dataserver to be handled.
*/
refos_err_t
proc_register_as_pager_handler(void *rpc_userptr , seL4_CPtr rpc_window ,
                               seL4_CPtr rpc_faultNotifyEP , seL4_Word* rpc_winID) 
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 2)) {
        return EINVALIDPARAM;
    }

    /* Retrieve and verify the window cap. */
    if (!dispatcher_badge_window(rpc_window)) {
        ROS_WARNING("Invalid window badge.");
        return EINVALIDPARAM;
    }
    struct w_window *win = w_get_window(&procServ.windowList, rpc_window - W_BADGE_BASE);
    if (!win) {
        ROS_ERROR("invalid window ID.");
        return EINVALIDPARAM;
    }
    assert(win->magic == W_MAGIC);

    /* Copy out the fault endpoint. */
    seL4_CPtr faultNotifyEP = dispatcher_copyout_cptr(rpc_faultNotifyEP);
    if (!faultNotifyEP) {
        dvprintf("could not copy out faultNotifyEP.");
        return EINVALIDPARAM; 
    }

    /* Set the pager endpoint. If there was anything else mapped to this window, it will be
       unmapped. */
    cspacepath_t faultNotifyEPPath;
    vka_cspace_make_path(&procServ.vka, faultNotifyEP, &faultNotifyEPPath);
    w_set_pager_endpoint(win, faultNotifyEPPath, pcb->pid);

    if (rpc_winID) {
        (*rpc_winID) = win->wID;
    }
    return ESUCCESS;
}

refos_err_t
proc_unregister_as_pager_handler(void *rpc_userptr , seL4_CPtr rpc_window)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDPARAM;
    }

    /* Retrieve and verify the window cap. */
    if (!dispatcher_badge_window(rpc_window)) {
        ROS_WARNING("Invalid window badge.");
        return EINVALIDPARAM;
    }
    struct w_window *win = w_get_window(&procServ.windowList, rpc_window - W_BADGE_BASE);
    if (!win) {
        ROS_ERROR("invalid window ID.");
        return EINVALIDPARAM;
    }
    assert(win->magic == W_MAGIC);

    /* Unset the pager endpoint. If there was anything else mapped to this window, it will be
       unmapped. */
    cspacepath_t emptyPath;
    memset(&emptyPath, 0, sizeof(cspacepath_t));
    w_set_pager_endpoint(win, emptyPath, PID_NULL);
    return ESUCCESS;
}

/*! @brief Handles server notification buffer setup syscalls.

    A server calls this on the process server in order to set up its notification buffer, used
    for notification messages such as content-initialisation and pager fault-delegation.
 */
refos_err_t
proc_notification_buffer_handler(void *rpc_userptr , seL4_CPtr rpc_dataspace)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDPARAM;
    }

    /* Verify & find the dataspace. */
    if (!dispatcher_badge_dspace(rpc_dataspace)) {
        return EINVALIDPARAM;
    }
    struct ram_dspace *dspace = ram_dspace_get_badge(&procServ.dspaceList, rpc_dataspace);
    if (!dspace) {
        return EINVALIDPARAM;
    }
 
    /* Set the notification buffer. */
    return proc_set_notificationbuffer(pcb, dspace);
}

/*! @brief Handles server window map syscalls.

    A server calls this on the process server in response to a prior fault delegation notification
    made by the process server, in order to map the given frame in the dataserver's VSpace into
    the faulting address frame, resolving the fault.
 */
refos_err_t
proc_window_map_handler(void *rpc_userptr , seL4_CPtr rpc_window , uint32_t rpc_windowOffset ,
                        uint32_t rpc_srcAddr)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDPARAM;
    }

    /* Retrieve and verify the window cap. */
    if (!dispatcher_badge_window(rpc_window)) {
        return EINVALIDPARAM;
    }
    struct w_window *window = w_get_window(&procServ.windowList, rpc_window - W_BADGE_BASE);
    if (!window) {
        ROS_ERROR("window does not exist!\n");
        return EINVALIDWINDOW;
    }
    
    /* Map the frame from src vspace to dest vspace. */
    struct proc_pcb *clientPCB = NULL;
    int error = vs_map_across_vspace(&pcb->vspace, rpc_srcAddr, window, rpc_windowOffset,
                                    &clientPCB);
    if (error) {
        return error;
    }
    assert(clientPCB != NULL && clientPCB->magic == REFOS_PCB_MAGIC);

    /* Resume the blocked faulting thread if there is one. */
    assert(procServ.unblockClientFaultPID == PID_NULL);
    procServ.unblockClientFaultPID = clientPCB->pid;
    return ESUCCESS;
}

/*! @brief Handles device server device map syscalls. */
refos_err_t
proc_device_map_handler(void *rpc_userptr , seL4_CPtr rpc_window , uint32_t rpc_windowOffset ,
        uint32_t rpc_paddr , uint32_t rpc_size , int rpc_cached)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDPARAM;
    }

    if ((pcb->systemCapabilitiesMask & PROCESS_PERMISSION_DEVICE_MAP) == 0) {
        dvprintf("Process needs device map permissions to perform this.\n");
        return EACCESSDENIED;
    }

    /* Retrieve and verify window. */
    struct w_window *window = w_get_window(&procServ.windowList, rpc_window - W_BADGE_BASE);
    if (!window) {
        ROS_ERROR("window does not exist!\n");
        return EINVALIDWINDOW;
    }

    /* Get the client PCB that owns the given window. */
    struct proc_pcb* clientPCB = pid_get_pcb(&procServ.PIDList, window->clientOwnerPID);
    if (!clientPCB) {
        ROS_ERROR("invalid window owner!\n");
        return EINVALID;
    }
    assert(clientPCB->magic == REFOS_PCB_MAGIC);

    return vs_map_device(&clientPCB->vspace, window, rpc_windowOffset, rpc_paddr, rpc_size,
                         rpc_cached ? true : false);
}

void
mem_syscall_postaction()
{
    if (procServ.unblockClientFaultPID != PID_NULL) {
        struct proc_pcb *clientPCB = pid_get_pcb(&procServ.PIDList, procServ.unblockClientFaultPID);
        if (!clientPCB) {
            ROS_WARNING("mem_syscall_postaction error: No such PID!");
            procServ.unblockClientFaultPID = PID_NULL;
            return;
        }

        /* Resume the blocked faulting thread if there is one. */
        proc_fault_reply(clientPCB);
        procServ.unblockClientFaultPID = PID_NULL;
    }
}
