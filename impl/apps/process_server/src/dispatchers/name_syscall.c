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
#include <refos-rpc/name_server.h>
#include <refos-util/nameserv.h>
#include "../state.h"
#include "name_syscall.h"

/*! @file
    @brief Dispatcher for the procserv name server syscalls.

    Handles calls to process server name server interface. The process server acts as the root
    name server. The methods here implement the methods defined in the generated file header
    <refos-rpc/name_server.h>.
*/

refos_err_t
nsv_register_handler(void *rpc_userptr , char* rpc_name , seL4_CPtr rpc_ep) {
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    dprintf("Process server name register [%s]! :D \n", rpc_name);

    if (!check_dispatch_caps(m, 0x00000000, 1)) {
        ROS_ERROR("nsv_register cap is not transferred properly.\n");
        return EINVALIDPARAM;
    }

    /* Register the new server into the server list. */
    if (!rpc_name) {
        return EINVALIDPARAM;
    }

    /* Copy out the anonymous cap. */
    seL4_CPtr anonCap = dispatcher_copyout_cptr(rpc_ep);
    if (!anonCap) {
        dvprintf("could not copy out anonCap.");
        return ENOMEM; 
    }

    /* Register the server under given name. */
    int error = nameserv_add(&procServ.nameServRegList, rpc_name, anonCap);
    if (error != ESUCCESS) {
        ROS_ERROR("failed to register server.\n");
        dispatcher_release_copyout_cptr(anonCap);
        return error;
    }

    return ESUCCESS;
}

refos_err_t
nsv_unregister_handler(void *rpc_userptr , char* rpc_name)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);
    (void) pcb;

    dprintf("Process server name unregister! D:\n");
    if (!rpc_name) {
        return EINVALIDPARAM;
    }

    nameserv_delete(&procServ.nameServRegList, rpc_name);
    return ESUCCESS;
}

seL4_CPtr
nsv_resolve_segment_internal_handler(void *rpc_userptr , char* rpc_path , int* rpc_resolvedBytes ,
                                     refos_err_t* rpc_errno)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);
    (void) pcb;

    /* Quick check that the path actually exists. */
    if (!rpc_path) {
        SET_ERRNO_PTR(rpc_errno, EINVALIDPARAM);
        return 0;
    }

    /* Perform the resolvation, resolving the next segment in given path. */
    seL4_CPtr anonCap;
    int resolvedBytes = nameserv_resolve(&procServ.nameServRegList, rpc_path, &anonCap);

    if (rpc_resolvedBytes) {
        (*rpc_resolvedBytes) = resolvedBytes;
    }
    SET_ERRNO_PTR(rpc_errno, ESUCCESS);
    return anonCap;
}

int
check_dispatch_nameserv(struct procserv_msg *m, void **userptr)
{
    return check_dispatch_interface(m, userptr, RPC_NAME_LABEL_MIN, RPC_NAME_LABEL_MAX);
}
