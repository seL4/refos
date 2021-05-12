/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _RPC_INTERFACE_PROC_CLIENT_HELPER_H_
#define _RPC_INTERFACE_PROC_CLIENT_HELPER_H_

#include <refos-rpc/rpc.h>
#include <refos/refos.h>
#include <refos/error.h>
#include <refos-rpc/proc_client.h>
#include <refos-util/cspace.h>

/*! @file
    @brief Helper functions for the process server interface.

    This file contains a simple layer of helper functions that make using the procserv interface
    much easier, but are too complex to have been generated by the stub generator. 
*/

/* Should mirror definitions in window.h. */
#define PROC_WINDOW_PERMISSION_WRITE 0x1
#define PROC_WINDOW_PERMISSION_READ 0x2
#define PROC_WINDOW_PERMISSION_READWRITE \
        (PROC_WINDOW_PERMISSION_WRITE | PROC_WINDOW_PERMISSION_READ)
#define PROC_WINDOW_FLAGS_UNCACHED 0x1

seL4_CPtr rpc_copyout_cptr(seL4_CPtr v);

/*! @brief Create a new sync endpoint. Helper function for proc_new_endpoint_internal().
    @return Recieved cap if success, 0 otherwise.
*/
static inline seL4_CPtr
proc_new_endpoint(void) {
    refos_err_t errnoRetVal = EINVALID;
    seL4_CPtr tcap = proc_new_endpoint_internal(&errnoRetVal);
    if (errnoRetVal != ESUCCESS || tcap == 0) {
        REFOS_SET_ERRNO(errnoRetVal);
        return 0;
    }
    REFOS_SET_ERRNO(ESUCCESS);
    return tcap;
}

/*! @brief Delete a created endpoint. */
static inline void
proc_del_endpoint(seL4_CPtr ep) { 
    seL4_CNode_Delete(REFOS_CSPACE, ep, REFOS_CSPACE_DEPTH);
    csfree(ep);
}

/*! @brief Create a new sync endpoint. Helper function for proc_new_async_endpoint_internal().
    @return Recieved cap if success, 0 otherwise.
*/
static inline seL4_CPtr
proc_new_async_endpoint(void) {
    refos_err_t errnoRetVal = EINVALID;
    seL4_CPtr tcap = proc_new_async_endpoint_internal(&errnoRetVal);
    if (errnoRetVal != ESUCCESS || tcap == 0) {
        REFOS_SET_ERRNO(errnoRetVal);
        return 0;
    }
    REFOS_SET_ERRNO(ESUCCESS);
    return tcap;
}

/*! @brief Delete a created async endpoint. */
static inline void
proc_del_async_endpoint(seL4_CPtr ep) { 
    seL4_CNode_Delete(REFOS_CSPACE, ep, REFOS_CSPACE_DEPTH);
    csfree(ep);
}

/*! @brief Create a new sync endpoint, and then mints a badged async endpoint from it, before
           discarding the capability to the original.
    @param badge The badge to mint endpoint with.
    @return Minted cap if success, 0 otherwise.
*/
static inline seL4_CPtr
proc_new_async_endpoint_badged(int badge) {
    seL4_CPtr tcap = proc_new_async_endpoint();
    if (!tcap) {
        return 0;
    }
    seL4_CPtr bgcap = csalloc();
    if (!bgcap) {
        proc_del_async_endpoint(tcap);
        REFOS_SET_ERRNO(ENOMEM);
        return 0;
    }
    /* Mint the badged cap. */
    int error = seL4_CNode_Mint (
        REFOS_CSPACE, bgcap, REFOS_CDEPTH,
        REFOS_CSPACE, tcap, REFOS_CDEPTH,
        seL4_AllRights, seL4_CapData_Badge_new(badge)
    );
    if (error != seL4_NoError) {
        proc_del_async_endpoint(tcap);
        REFOS_SET_ERRNO(EINVALID);
        return 0;
    }
    proc_del_async_endpoint(tcap);
    REFOS_SET_ERRNO(ESUCCESS);
    return bgcap;
}

/*! @brief Create a new memory window segment (with extra parameters). Helper function for
           proc_create_mem_window_internal().
    @param vaddr The window base address in the calling client's VSpace.
    @param size The size of the mem window.
    @param permission The read / write permission bitmask.
    @param flags The flags bitmask (cached / uncached).
    @return Capability to created window if success, 0 otherwise (errno will be set).
*/
static inline seL4_CPtr
proc_create_mem_window_ext(uint32_t vaddr, uint32_t size, uint32_t permission, uint32_t flags)
{
    refos_err_t errnoRetVal = EINVALID;
    seL4_CPtr tcap = proc_create_mem_window_internal(vaddr, size, permission, flags, &errnoRetVal);
    if (errnoRetVal != ESUCCESS || tcap == 0) {
        REFOS_SET_ERRNO(errnoRetVal);
        return 0;
    }
    REFOS_SET_ERRNO(ESUCCESS);
    return tcap;
}

/*! @brief Create a new memory window segment. Helper function for
           proc_create_mem_window_internal(). Permission is set to PROC_WINDOW_PERMISSION_READWRITE,
           and flags 0x0.
    @param vaddr The window base address in the calling client's VSpace.
    @param size The size of the mem window.
    @return Capability to created window if success, 0 otherwise (errno will be set).
*/
static inline seL4_CPtr
proc_create_mem_window(uint32_t vaddr, uint32_t size)
{
    return proc_create_mem_window_ext(vaddr, size, PROC_WINDOW_PERMISSION_READWRITE, 0x0);
}

/*! @brief Clones a new thread for process. Helper function for proc_clone_internal().
    @param func The entry point function of the new thread.
    @param childStack The stack vaddr of the new thread.
    @param flags Unused, must be 0.
    @param arg Unused, must be 0.
    @return threadID if success, negative if error occured (errno will be set).
*/
static inline int
proc_clone(int (*func)(void *), void *childStack, int flags, void *arg)
{
    (void) flags;
    refos_err_t errnoRetVal = EINVALID;
    int threadID = proc_clone_internal((seL4_Word) func, (seL4_Word) childStack, flags,
            (seL4_Word) arg, &errnoRetVal);
    REFOS_SET_ERRNO(errnoRetVal);
    return threadID;
}

#endif /* _RPC_INTERFACE_PROC_CLIENT_HELPER_H_ */
