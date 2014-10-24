/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "proc_syscall.h"

#include <vka/kobject_t.h>
#include <refos-rpc/proc_common.h>
#include <refos-rpc/proc_server.h>

#include "../system/process/pid.h"
#include "../system/process/process.h"
#include "../system/process/proc_client_watch.h"
#include "../system/addrspace/vspace.h"
#include "../system/memserv/window.h"
#include "../system/memserv/dataspace.h"
#include "../system/memserv/ringbuffer.h"

/*! @file
    @brief Dispatcher for the procserv syscalls.

    Handles calls to process server syscall interface. The process server interface provides process
    abstraction, simple naming, server registration, memory windows...etc. (For more details, refer
    to the protocol design document.). The methods here implement the functions in the generated
    header file <refos-rpc/proc_server.h>.

    The memory related process server syscalls resides in mem_syscall.c and mem_syscall.h.
*/

/* ---------------------------- Proc Server syscall helper functions ---------------------------- */

/*! @brief Creates a kernel endpoint object for the given process.

    Creates a kernel endpoint object for the given process. Also does the book-keeping underneath so
    that the created objects can be destroyed when the process exits.

    @param pcb Handle to the process to create the object for. (no ownership)
    @param type The kernel endpoint object type to create for the process; sync or async.
    @return Handle to the created object if successful (no ownership), NULL if there was an
            error. (eg. ran out of heap or untyped memory).
 */
static seL4_CPtr
proc_syscall_allocate_endpoint(struct proc_pcb *pcb, kobject_t type)
{
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    /* Allocate the kernel object. */
    vka_object_t endpoint;
    int error = -1;
    if (type == KOBJECT_ENDPOINT_SYNC) {
        error = vka_alloc_endpoint(&procServ.vka, &endpoint);
    } else if (type == KOBJECT_ENDPOINT_ASYNC) {
        error = vka_alloc_async_endpoint(&procServ.vka, &endpoint);
    } else {
        assert(!"Invalid endpoint type.");
    }
    if (error || endpoint.cptr == 0) {
        ROS_ERROR("failed to allocate endpoint for process. Procserv out of memory.\n");
        return 0;
    }

    /* Track this allocation, so it may be freed along with the process vspace. */
    vs_track_obj(&pcb->vspace, endpoint);
    return endpoint.cptr;
}

/* -------------------------------- Proc Server syscall handlers -------------------------------- */

/*! @brief Handles ping syscalls. */
refos_err_t
proc_ping_handler(void *rpc_userptr) {
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb->magic == REFOS_PCB_MAGIC);

    (void) pcb;
    (void) m;

    dprintf(COLOUR_G "PROCESS SERVER RECIEVED PING!!! HELLO THERE! (´・ω・)っ由" COLOUR_RESET "\n");
    return ESUCCESS;
}

/*! @brief Handles sync endpoint creation syscalls. */
seL4_CPtr
proc_new_endpoint_internal_handler(void *rpc_userptr , refos_err_t* rpc_errno)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb->magic == REFOS_PCB_MAGIC);
    dprintf("Process server creating endpoint!\n");
    seL4_CPtr ep = proc_syscall_allocate_endpoint(pcb, KOBJECT_ENDPOINT_SYNC);
    if (!ep) {
        SET_ERRNO_PTR(rpc_errno, ENOMEM);
        return 0;
    }
    SET_ERRNO_PTR(rpc_errno, ESUCCESS);
    return ep;
}

/*! @brief Handles async endpoint creation syscalls. */
seL4_CPtr
proc_new_async_endpoint_internal_handler(void *rpc_userptr , refos_err_t* rpc_errno)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb->magic == REFOS_PCB_MAGIC);
    dprintf("Process server creating async endpoint!\n");
    seL4_CPtr ep = proc_syscall_allocate_endpoint(pcb, KOBJECT_ENDPOINT_ASYNC);
    if (!ep) {
        SET_ERRNO_PTR(rpc_errno, ENOMEM);
        return 0;
    }
    SET_ERRNO_PTR(rpc_errno, ESUCCESS);
    return ep;
}

/*! @brief Handles client watching syscalls.

    Most servers would need to call this in order to be notified of client death in order to be able
    to delete any internal book-keeping for the dead client.
 */
refos_err_t
proc_watch_client_handler(void *rpc_userptr , seL4_CPtr rpc_liveness , seL4_CPtr rpc_deathEP ,
                          int32_t* rpc_deathID)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 2)) {
        return EINVALIDPARAM;
    }

    /* Retrieve the corresponding client's ASID unwrapped from its liveness cap. */
    if (!dispatcher_badge_liveness(rpc_liveness)) {
        return EINVALIDPARAM;
    }
    
    /* Verify the corresponding client. */
    struct proc_pcb *client = pid_get_pcb(&procServ.PIDList,
                                          rpc_liveness - PID_LIVENESS_BADGE_BASE);
    if (!client) {
        return EINVALIDPARAM; 
    }
    assert(client->magic == REFOS_PCB_MAGIC);

    /* Copy out the death notification endpoint. */
    seL4_CPtr deathNotifyEP = dispatcher_copyout_cptr(rpc_deathEP);
    if (!deathNotifyEP) {
        ROS_ERROR("could not copy out deathNotifyEP.");
        return ENOMEM; 
    }

    /* Add the new client to the watch list of the calling process. */
    int error = client_watch(&pcb->clientWatchList, client->pid, deathNotifyEP);
    if (error) {
        ROS_ERROR("failed to add to watch list. Procserv possibly out of memory.");
        dispatcher_release_copyout_cptr(deathNotifyEP);
        return error;
    }
    if (rpc_deathID) {
        (*rpc_deathID) = client->pid;
    }

    return ESUCCESS;
}


/*! @brief Handles client un-watching syscalls. */
refos_err_t
proc_unwatch_client_handler(void *rpc_userptr , seL4_CPtr rpc_liveness) 
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDPARAM;
    }

    /* Retrieve the corresponding client's ASID unwrapped from its liveness cap. */
    if (!dispatcher_badge_liveness(rpc_liveness)) {
        return EINVALIDPARAM;
    }
    
    /* Verify the corresponding client. */
    struct proc_pcb *client = pid_get_pcb(&procServ.PIDList,
                                          rpc_liveness - PID_LIVENESS_BADGE_BASE);
    if (!client) {
        return EINVALIDPARAM; 
    }
    assert(client->magic == REFOS_PCB_MAGIC);

    /* Remove the given client PID from the watch list. */
    client_unwatch(&pcb->clientWatchList, client->pid);
    return ESUCCESS;
}

/*! @brief Sets the process's parameter buffer.

    Sets the process's parameter buffer to the given RAM dataspace. Only support a RAM dataspace
    which orginated from the process server's own dataspace implementation, does not support
    an external dataspace.
*/
refos_err_t
proc_set_parambuffer_handler(void *rpc_userptr , seL4_CPtr rpc_dataspace , uint32_t rpc_size)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb->magic == REFOS_PCB_MAGIC);
    struct ram_dspace *dataspace;

    /* Special case zero size and NULL parameter buffer - means unset the parameter buffer. */
    if (rpc_size == 0 && rpc_dataspace == 0) {
        proc_set_parambuffer(pcb, NULL);
        return ESUCCESS;
    }

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDPARAM;
    }

    /* Check if the given badge is a RAM dataspace. */
    if (!dispatcher_badge_dspace(rpc_dataspace)) {
        return EINVALIDPARAM;
    }

    /* Retrieve RAM dataspace structure. */
    dataspace = ram_dspace_get_badge(&procServ.dspaceList, rpc_dataspace);
    if (!dataspace) {
        dvprintf("No such dataspace!.\n");
        return EINVALID;
    }

    /* Set new parameter buffer. */
    proc_set_parambuffer(pcb, dataspace);
    return ESUCCESS;
}


/*! @brief Starts a new process. */
refos_err_t
proc_new_proc_handler(void *rpc_userptr , char* rpc_name , char* rpc_params , bool rpc_block ,
                      int32_t rpc_priority , int32_t* rpc_status)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb->magic == REFOS_PCB_MAGIC);
    
    if (!rpc_name) {
        return EINVALIDPARAM;
    }

    /* Kick off an instance of selfloader, which will do the actual process loading work. */
    int error = proc_load_direct("selfloader", rpc_priority, rpc_name, pcb->pid, 0x0);
    if (error != ESUCCESS) {
        ROS_WARNING("failed to run selfloader for new process [%s].", rpc_name);
        return error;
    }

    /* Optionally block parent process until child process has finished. */
    if (rpc_block) {
        /* Save the reply endpoint. */
        proc_save_caller(pcb);
        pcb->parentWaiting = true;
        pcb->rpcClient.skip_reply = true;
        return ESUCCESS;
    }

    /* Immediately resume the parent process. */
    return ESUCCESS;
}

/*! @brief Exits and deletes the process which made this call. */
refos_err_t
proc_exit_handler(void *rpc_userptr , int32_t rpc_status)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb->magic == REFOS_PCB_MAGIC);
    dprintf("Process PID %u exiting with status %d !!!\n", pcb->pid, rpc_status);
    pcb->exitStatus = rpc_status;
    pcb->rpcClient.skip_reply = true;
    proc_queue_release(pcb);
    return ESUCCESS;
}

int
proc_clone_internal_handler(void *rpc_userptr , seL4_Word rpc_entryPoint , seL4_Word rpc_childStack
        , int rpc_flags , seL4_Word rpc_arg , refos_err_t* rpc_errno)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb->magic == REFOS_PCB_MAGIC);

    int threadID = -1;
    int error = proc_clone(pcb, &threadID, (vaddr_t) rpc_childStack, (vaddr_t) rpc_entryPoint);
    SET_ERRNO_PTR(rpc_errno, error);
    return threadID;
}

refos_err_t
proc_nice_handler(void *rpc_userptr , int rpc_threadID , int rpc_priority)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb->magic == REFOS_PCB_MAGIC);
    return proc_nice(pcb, rpc_threadID, rpc_priority);
}

seL4_CPtr
proc_get_irq_handler_handler(void *rpc_userptr , int rpc_irq)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb->magic == REFOS_PCB_MAGIC);
    if ((pcb->systemCapabilitiesMask & PROCESS_PERMISSION_DEVICE_IRQ) == 0) {
        return 0;
    }
    dprintf("Process %d (%s) getting IRQ number %d...\n", pcb->pid, pcb->debugProcessName, rpc_irq);
    return procserv_get_irq_handler(rpc_irq);
}


/* ------------------------------------ Dispatcher functions ------------------------------------ */

int
check_dispatch_syscall(struct procserv_msg *m, void **userptr) {
    return check_dispatch_interface(m, userptr, RPC_PROC_LABEL_MIN, RPC_PROC_LABEL_MAX);
}