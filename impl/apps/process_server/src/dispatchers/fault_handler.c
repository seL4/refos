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
#include <stdbool.h>
#include <stdint.h>
#include <sel4/sel4.h>
#include <sel4/messages.h>
#include <sel4utils/arch/util.h>
#include <refos-rpc/proc_common.h>
#include <refos/error.h>

#include "fault_handler.h"
#include "dispatcher.h"
#include "../state.h"
#include "../badge.h"
#include "../system/addrspace/vspace.h"
#include "../system/process/process.h"
#include <refos/refos.h>

/*! @file
    @brief Process server fault dispatcher which handles VM faults. */

/*! @brief Temporary internal VM fault message info struct. */
struct procserv_vmfault_msg {
    /*! The faulting program's process control block. */
    struct proc_pcb *pcb;
    /*! The program counter at the faulting instruction. */
    vaddr_t pc;
    /* The faulting address. */
    vaddr_t faultAddr;
    /*! Whether this fault was caused by a data access or an instruction.  */
    seL4_Word instruction;
    /*! The arch-dependant fault status register passed in from the kernel. */
    seL4_Word fsr;
    /*! True if this fault is a read fault, false if it is a write fault. */
    bool read;
};

/*! @brief Helper function to print a rather colourful and verbose segmentation fault message.
    @param message Text message containing fault reason.
    @param f The fault message info structure.
*/
static void
output_segmentation_fault(const char* message, struct procserv_vmfault_msg *f)
{
    ROS_ERROR("▷▶▷ RefOS Program PID %d [%s] blocked with Segmentation fault ಠ_ಠ ◁◀◁",
            f->pcb->pid, f->pcb->debugProcessName);
    ROS_ERROR("    %s %s fault at 0x%x, Instruction Pointer 0x%x, Fault Status Register 0x%x",
            f->instruction ? "Instruction" : "Data", f->read ? "read" : "write",
            f->faultAddr, f->pc, f->fsr);
    ROS_ERROR("    Error Message: " COLOUR_R "%s" COLOUR_RESET, message);
    ROS_ERROR("    Thread will be blocked indefinitely.");
}

/*! @brief Helper function to delegate fault message to an external endpoint.
    
    Writes the given message to the delegator's notification ring buffer,  and then sends an async
    notification along the EP the delegator has set up previously.  This helper function is used for
    every kind of notification.

    @param f The fault message info structure.
    @param delegationPCB The PCB structure of the delegator recieving the notification.
    @param delegationEP The async endpoint of delegator to notify.
    @param vmFaultNotification The VM fault noficfiation message contents.
    @param saveReply Whether to save the falunt client's reply EP.
*/
static void
fault_delegate_notification(struct procserv_vmfault_msg *f, struct proc_pcb *delegationPCB,
        cspacepath_t delegationEP, struct proc_notification vmFaultNotification, bool saveReply)
{
    assert(f && f->pcb);
    assert(delegationPCB && delegationPCB->magic == REFOS_PCB_MAGIC);

    if (!delegationPCB->notificationBuffer) {
        printf("PID notif buffer NULL! %d\n", delegationPCB->pid);
        output_segmentation_fault("Delegator dataspace server has no notification buffer.", f);
        return;
    }

    /* Save the caller reply cap to reply to later. */
    if (saveReply) {
        int error = proc_save_caller(f->pcb);
        if (error) {
            output_segmentation_fault("Failed to save caller reply cap.", f);
            return;
        }
    }

    /* Append notification to pager's notification buffer. */
    int error = rb_write(delegationPCB->notificationBuffer, (char*)(&vmFaultNotification),
            sizeof(vmFaultNotification));
    if (error) {
        output_segmentation_fault("Failed to write VM fault notification to buffer.", f);
        return;
    }

    /* Notify the pager of this fault. */
    dispatcher_notify(delegationEP.capPtr);
}

/* ----------------------------- Proc Server fault handler functions ---------------------------- */

/*! @brief Handles faults on windows mapped to anonymous memory.

    This function is responsible for handling VM faults on windows which have been mapped to the
    process server's own anonymous dataspaces, including ones that have been content-initialised.

    If the dataspace has been set to content-initialised, then we will need to delegate and save the
    reply cap to reply to it once the content has been initialised. If it has not been initialised
    we simply map the dataspace page and reply.

    @param m The recieved IPC fault message from the kernel.
    @param f The VM fault message info struct.
    @param aw Found associated window of the faulting address & client.
    @param window The window structure of the faulting address & client.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
static int
handle_vm_fault_dspace(struct procserv_msg *m, struct procserv_vmfault_msg *f,
        struct w_associated_window *aw, struct w_window *window)
{
    assert(f && f->pcb);
    assert(aw && window && window->mode == W_MODE_ANONYMOUS);

    vaddr_t dspaceOffset = (f->faultAddr + window->ramDataspaceOffset) -
                           REFOS_PAGE_ALIGN(aw->offset);
    struct ram_dspace *dspace = window->ramDataspace;
    assert(dspace && dspace->magic == RAM_DATASPACE_MAGIC);

    dvprintf("# PID %d VM fault ―――――▶ anon RAM dspace %d\n", f->pcb->pid, dspace->ID);

    if (dspace->contentInitEnabled) {
        /* Data space is backed by external content. Content initialisation delegation. */
        int contentInitState =  ram_dspace_need_content_init(dspace, dspaceOffset);
        if (contentInitState < 0) {
            output_segmentation_fault("Failed to retrieve content-init state.", f);
            return EINVALID;
        }
        if (contentInitState == true) {
            /* Content has not yet been initialised so we delegate. */
            if (f->faultAddr + window->ramDataspaceOffset >= aw->offset + aw->size) {
                output_segmentation_fault("Fault address out of range!", f);
                return EINVALID;
            }

            /* Find the pager's PCB. */
            assert(dspace->contentInitPID != PID_NULL);
            struct proc_pcb* cinitPCB = pid_get_pcb(&procServ.PIDList, dspace->contentInitPID);
            if (!cinitPCB) {
                output_segmentation_fault("Invalid content initialiser PID.", f);
                return EINVALID;
            }
            if (!dspace->contentInitEP.capPtr) {
                output_segmentation_fault("Invalid content-init endpoint!", f);
                return EINVALID;
            }

            /* Save the reply endpoint. */
            int error = ram_dspace_add_content_init_waiter_save_current_caller(dspace,
                    dspaceOffset);
            if (error != ESUCCESS) {
                output_segmentation_fault("Failed to save reply cap as dspace waiter!", f);
                return EINVALID;
            }

            /* Set up and send the fault notification. */
            struct proc_notification vmFaultNotification;
            vmFaultNotification.magic = PROCSERV_NOTIFICATION_MAGIC;
            vmFaultNotification.label = PROCSERV_NOTIFY_CONTENT_INIT;
            vmFaultNotification.arg[0] = dspace->ID;
            vmFaultNotification.arg[1] = REFOS_PAGE_ALIGN(dspaceOffset);

            fault_delegate_notification(f, cinitPCB, dspace->contentInitEP, vmFaultNotification,
                                        false);

            /* Return an error here to avoid resuming the client. */
            return EDELEGATED;
        }

        /* Fallthrough to normal dspace mapping if content-init state is set to already provided. */
    }

    /* Get the page at the dataspaceOffset into the dataspace. */
    seL4_CPtr frame = ram_dspace_get_page(dspace, dspaceOffset);
    if (!frame) {
        output_segmentation_fault("Out of memory to allocate page or read off end of dspace.", f);
        return ENOMEM;
    }

    /* Map this frame into the client process's page directory. */
    int error = vs_map(&f->pcb->vspace, f->faultAddr, &frame, 1);
    if (error != ESUCCESS) {
        output_segmentation_fault("Failed to map frame into client's vspace at faultAddr.", f);
        return error;
    }

    return ESUCCESS;
}

/*! \brief Handles faults on windows set to external pager.

    This functions handles the VM faults which results in a window that has been set up to be in
    external paged mode. we simply delegate to the external pager and they'll call data_datamap
    back on us and take are of the rest.
    
    @param m The recieved IPC fault message from the kernel.
    @param f The VM fault message info struct.
    @param aw Found associated window of the faulting address & client.
    @param window The window structure of the faulting address & client.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
static int
handle_vm_fault_pager(struct procserv_msg *m, struct procserv_vmfault_msg *f,
        struct w_associated_window *aw, struct w_window *window)
{
    assert(f && f->pcb);
    assert(aw && window && window->mode == W_MODE_PAGER);

    /* Set up and send the fault notification. */
    struct proc_notification vmFaultNotification;
    vmFaultNotification.magic = PROCSERV_NOTIFICATION_MAGIC;
    vmFaultNotification.label = PROCSERV_NOTIFY_FAULT_DELEGATION;
    vmFaultNotification.arg[0] = window->wID;
    vmFaultNotification.arg[1] = window->size;
    vmFaultNotification.arg[2] = f->faultAddr;
    vmFaultNotification.arg[3] = aw->offset;
    vmFaultNotification.arg[4] = f->instruction;
    vmFaultNotification.arg[5] = window->permissions;
    vmFaultNotification.arg[6] = f->pc;

    /* Find the pager's PCB. */
    assert(window->pagerPID != PID_NULL);
    struct proc_pcb* pagerPCB = pid_get_pcb(&procServ.PIDList, window->pagerPID);
    if (!pagerPCB) {
        output_segmentation_fault("Invalid pager PID.", f);
        return EINVALID;
    }

    /* Send the delegation notification. */    
    fault_delegate_notification(f, pagerPCB, window->pager, vmFaultNotification, true);

    /* Return an error here to avoid resuming the client. */
    return EDELEGATED;
}

/*! @brief Handles client VM fault messages sent by the kernel.

    Handles the VM fault message by looking up the details of the window that it faulted in, and
    decides whether this fault should be delegated to an external dataspace server for paging
    or content initalisation, or be handled internally by the process server's own dataspace
    implementation for RAM, or is an invalid memory access.
    
    In the case of an invalid memory access, or if the process server runs out of RAM, then
    the fault is unable to be handled and the faulting process is blocked indefinitely.

    @param m The recieved IPC fault message from the kernel.
    @param f The VM fault message info struct.
*/
static void
handle_vm_fault(struct procserv_msg *m, struct procserv_vmfault_msg *f)
{
    assert(f && f->pcb);
    dvprintf("# Process server recieved PID %d VM fault\n", f->pcb->pid);
    dvprintf("# %s %s fault at 0x%x, Instruction Pointer 0x%x, Fault Status Register 0x%x\n",
            f->instruction ? "Instruction" : "Data", f->read ? "read" : "write",
            f->faultAddr, f->pc, f->fsr);

    /* Thread should never be fault blocked (or else how did this VM fault even happen?). */
    if (f->pcb->faultReply.capPtr != 0) {
        ROS_ERROR("(how did this VM fault even happen? Check book-keeping.\n");
        output_segmentation_fault("Process should already be fault-blocked.", f);
        return;
    }

    /* Check faulting vaddr in segment windows. */
    struct w_associated_window *aw = w_associate_find(&f->pcb->vspace.windows, f->faultAddr);
    if (!aw) {
        output_segmentation_fault("invalid memory window segment", f);
        return;
    }

    /* Retrieve the associated window. */
    struct w_window *window = w_get_window(&procServ.windowList, aw->winID);
    if (!window) {
        output_segmentation_fault("invalid memory window - procserv book-keeping error.", f);
        assert(!"Process server could not find window - book-keeping error.");
        return;
    }

    /* Check window permissions. */
    if (f->read && !(window->permissions | W_PERMISSION_READ)) {
        output_segmentation_fault("no read access permission to window.", f);
        return;
    }
    if (!f->read && !(window->permissions | W_PERMISSION_WRITE)) {
        output_segmentation_fault("no write access permission to window.", f);
        return;
    }

    /* Check that there isn't a page entry already mapped. */
    cspacepath_t pageEntry = vs_get_frame(&f->pcb->vspace, f->faultAddr);
    if (pageEntry.capPtr != 0) {
        output_segmentation_fault("entry already occupied; book-keeping error.", f);
        return;
    }

    /* Handle the dispatch request depending on window mode. */
    int error = EINVALID;
    switch (window->mode) {
        case W_MODE_EMPTY:
            output_segmentation_fault("fault in empty window.", f);
            break;
        case W_MODE_ANONYMOUS:
            error = handle_vm_fault_dspace(m, f, aw, window);
            break;
        case W_MODE_PAGER:
            error = handle_vm_fault_pager(m, f, aw, window);
            break;
        default:
            assert(!"Invalid window mode. Process server bug.");
            break;
    }

    /* Reply to the faulting process to unblock it. */
    if (error == ESUCCESS) {
        seL4_Reply(_dispatcherEmptyReply);
    }
}

/* ------------------------------------ Dispatcher functions ------------------------------------ */

int
check_dispatch_fault(struct procserv_msg *m, void **userptr) {
    if (seL4_MessageInfo_get_label(m->message) != seL4_VMFault ||
            !dispatcher_badge_PID(m->badge)) {
        /* Not a VM fault, pass onto next dispatcher. */
        return DISPATCH_PASS;
    }
    (void) userptr;
    return DISPATCH_SUCCESS;
}

int
dispatch_vm_fault(struct procserv_msg *m, void **userptr) {
    if (check_dispatch_fault(m,  userptr) != DISPATCH_SUCCESS) {
        return DISPATCH_PASS;
    }
    (void) userptr;

    /* Find the faulting client's PCB. */
    struct proc_pcb *pcb = pid_get_pcb_from_badge(&procServ.PIDList, m->badge);
    if (!pcb) {
        ROS_WARNING("Unknown client.");
        return DISPATCH_ERROR;
    }
    assert(pcb->magic == REFOS_PCB_MAGIC);
    assert(pcb->pid == m->badge - PID_BADGE_BASE);

    /* Fill out the VM fault message info structure. */
    struct procserv_vmfault_msg vmfault;
    vmfault.pcb = pcb;
    vmfault.pc = seL4_PF_FIP();
    vmfault.faultAddr = seL4_PF_Addr();
    vmfault.instruction = seL4_GetMR(SEL4_PFIPC_PREFETCH_FAULT);
    vmfault.fsr = seL4_GetMR(SEL4_PFIPC_FSR);
    vmfault.read = sel4utils_is_read_fault();

    /* Handle the VM fault. */
    handle_vm_fault(m, &vmfault);
    return DISPATCH_SUCCESS;
}