/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <sel4/sel4.h>
#include "data_syscall.h"
#include <refos-rpc/data_server.h>
#include <refos/refos.h>

#include "../system/memserv/window.h"
#include "../system/memserv/dataspace.h"
#include "../system/memserv/ringbuffer.h"
#include "../system/process/process.h"
#include "../system/process/pid.h"

/*! @file
   @brief Process server anon dataspace syscall handler.

   This module is reponsible for implementing the dataspace interface in the generated
   <refos-rpc/data_server.h>, for anon dataspaces.
*/

seL4_CPtr
data_open_handler(void *rpc_userptr , char* rpc_name , int rpc_flags , int rpc_mode , int rpc_size ,
                  int* rpc_errno)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    (void) pcb;
    (void) rpc_mode;

    if (rpc_size <= 0) {
        SET_ERRNO_PTR(rpc_errno, EINVALIDPARAM);
        return 0;
    }

    /* Name must either be NULL, empty string or anon. */
    if (rpc_name != NULL) {
        if (strlen(rpc_name) > 0 && strcmp(rpc_name, "anon") != 0) {
            SET_ERRNO_PTR(rpc_errno, EFILENOTFOUND);
            return 0;
        }
    }

    /* Create a ram dataspace of the given size. */
    struct ram_dspace *newDataspace = ram_dspace_create(&procServ.dspaceList, rpc_size);
    if (!newDataspace) {
        ROS_ERROR("Failed to create new_dataspace.\n");
        SET_ERRNO_PTR(rpc_errno, ENOMEM);
        return 0;
    }

    /* Set physical address mode, if required. */
    if ((rpc_flags & PROCSERV_DSPACE_FLAG_DEVICE_PADDR) != 0) {
        int error = EACCESSDENIED;
        if (pcb->systemCapabilitiesMask & PROCESS_PERMISSION_DEVICE_MAP) {
            error = ram_dspace_set_to_paddr(newDataspace, (uint32_t) rpc_mode);
        }
        if (error) {
            ram_dspace_unref(&procServ.dspaceList, newDataspace->ID);
            SET_ERRNO_PTR(rpc_errno, error);
            return 0;
        }
    }

    SET_ERRNO_PTR(rpc_errno, ESUCCESS);
    assert(newDataspace->magic == RAM_DATASPACE_MAGIC);
    return newDataspace->capability.capPtr;
}

refos_err_t
data_close_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        ROS_ERROR("data_close EINVALIDPARAM: bad dataspace capability.\n");
        return EINVALIDWINDOW;
    }

    /* Verify and find the RAM dataspace. */
    if (!dispatcher_badge_dspace(rpc_dspace_fd)) {
        ROS_ERROR("EINVALIDPARAM: invalid RAM dataspace badge..\n");
        return EINVALIDPARAM;
    }
    struct ram_dspace *dspace = ram_dspace_get_badge(&procServ.dspaceList, rpc_dspace_fd);
    if (!dspace) {
        ROS_ERROR("EINVALIDPARAM: dataspace not found.\n");
        return EINVALIDPARAM;
    }

    /* Purge the dataspace from all windows, unmapping every instance of it. */
    w_purge_dspace(&procServ.windowList, dspace);

    /* Purge the dataspace from all notification buffers and ring buffers. */
    pid_iterate(&procServ.PIDList, proc_dspace_delete_callback, (void*) dspace);

    /* Check that this is the last reference to the dataspace. */
    if (dspace->ref != 1) {
        ROS_WARNING("Dataspace reference is %d and not 1.", dspace->ref);
        ROS_WARNING("This is either a book-keeping bug or corruption.");
    }

    /* And finally destroy the RAM dataspace. */
    ram_dspace_unref(&procServ.dspaceList, dspace->ID);
    return ESUCCESS;
}

int
data_getc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_block)
{
    (void) rpc_userptr;
    (void) rpc_dspace_fd;
    return EUNIMPLEMENTED;
}

off_t
data_lseek_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , off_t rpc_offset ,
                   int rpc_whence)
{
    (void) rpc_userptr;
    (void) rpc_dspace_fd;
    (void) rpc_offset;
    (void) rpc_whence;
    return EUNIMPLEMENTED;
}

uint32_t
data_get_size_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        ROS_ERROR("bad dspace capability.\n");
        return 0;
    }

    /* Verify and find the RAM dataspace. */
    if (!dispatcher_badge_dspace(rpc_dspace_fd)) {
        ROS_ERROR("EINVALIDPARAM: invalid RAM dataspace badge..\n");
        return EINVALIDPARAM;
    }
    struct ram_dspace *dspace = ram_dspace_get_badge(&procServ.dspaceList, rpc_dspace_fd);
    if (!dspace) {
        ROS_ERROR("EINVALIDPARAM: dataspace not found.\n");
        return EINVALIDPARAM;
    }

    return dspace->npages * REFOS_PAGE_SIZE;
}

refos_err_t
data_expand_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_size)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDPARAM;
    }

    /* Verify and find the RAM dataspace. */
    if (!dispatcher_badge_dspace(rpc_dspace_fd)) {
        ROS_ERROR("EINVALIDPARAM: invalid RAM dataspace badge..\n");
        return EINVALIDPARAM;
    }
    struct ram_dspace *dspace = ram_dspace_get_badge(&procServ.dspaceList, rpc_dspace_fd);
    if (!dspace) {
        ROS_ERROR("EINVALIDPARAM: dataspace not found.\n");
        return EINVALIDPARAM;
    }

    return ram_dspace_expand(dspace, rpc_size);
}

/*! \brief Maps the given dataspace to the given memory window. */
refos_err_t
data_datamap_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , seL4_CPtr rpc_memoryWindow,
                     uint32_t rpc_offset)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000003, 2)) {
        return EINVALIDPARAM;
    }

    /* Retrieve and validate window badge. */
    if (!dispatcher_badge_window(rpc_memoryWindow)) { 
        return EINVALIDWINDOW;
    }
    struct w_window *window = w_get_window(&procServ.windowList, rpc_memoryWindow - W_BADGE_BASE);
    if (!window) {
        return EINVALIDWINDOW;
    }

    /* Verify and find the RAM dataspace. */
    if (!dispatcher_badge_dspace(rpc_dspace_fd)) {
        ROS_ERROR("EINVALIDPARAM: invalid RAM dataspace badge..\n");
        return EINVALIDPARAM;
    }
    struct ram_dspace *dspace = ram_dspace_get_badge(&procServ.dspaceList, rpc_dspace_fd);
    if (!dspace) {
        ROS_ERROR("EINVALIDPARAM: dataspace not found.\n");
        return EINVALIDPARAM;
    }

    /* Check that the offset is sane. */
    if (rpc_offset > (dspace->npages * REFOS_PAGE_SIZE)) {
        return EINVALIDPARAM;
    }

    /* Associate the dataspace with the window. This will release whatever the window was associated
       with beforehand. */
    w_set_anon_dspace(window, dspace, rpc_offset);
    return ESUCCESS;
}

refos_err_t
data_dataunmap_handler(void *rpc_userptr , seL4_CPtr rpc_memoryWindow)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDPARAM;
    }

    /* Retrieve and validate window badge. */
    if (!dispatcher_badge_window(rpc_memoryWindow)) { 
        return EINVALIDWINDOW;
    }
    struct w_window *window = w_get_window(&procServ.windowList, rpc_memoryWindow - W_BADGE_BASE);
    if (!window) {
        return EINVALIDWINDOW;
    }
    
    /* If window is already empty, then there's nothing to do here. */
    if (window->mode == W_MODE_EMPTY) {
        return ESUCCESS;
    }

    /* If window is mapped to something else, the un-do operation should not be data_unmap. */
    if (window->mode != W_MODE_ANONYMOUS) {
        return EINVALIDPARAM;
    }

    /* Reset the window back to empty. */
    w_set_anon_dspace(window, NULL, 0);
    return ESUCCESS;
}


refos_err_t
data_init_data_handler(void *rpc_userptr , seL4_CPtr rpc_destDataspace ,
                             seL4_CPtr rpc_srcDataspace , uint32_t rpc_srcDataspaceOffset)
{
    (void) rpc_userptr;
    (void) rpc_destDataspace;
    (void) rpc_srcDataspace;
    (void) rpc_srcDataspaceOffset;

    /* Process server doesn't support init_data syscall.
       Who on earth would want to initialise another dataspace by anonymous memory? */

    return EUNIMPLEMENTED;
}

/*! \brief Call from external dataserver asking to be the content initialiser for this dataspace. */
refos_err_t
data_have_data_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , seL4_CPtr rpc_faultNotifyEP ,
                       uint32_t* rpc_dataID)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!(check_dispatch_caps(m, 0x00000001, 2) || check_dispatch_caps(m, 0x00000001, 1))) {
        return EINVALIDPARAM;
    }

    if (rpc_dataID) {
        (*rpc_dataID) = 0;
    }

    /* Verify and find the RAM dataspace. */
    if (!dispatcher_badge_dspace(rpc_dspace_fd)) {
        ROS_ERROR("EINVALIDPARAM: invalid RAM dataspace badge..\n");
        return EINVALIDPARAM;
    }
    struct ram_dspace *dspace = ram_dspace_get_badge(&procServ.dspaceList, rpc_dspace_fd);
    if (!dspace) {
        ROS_ERROR("EINVALIDPARAM: dataspace not found.\n");
        return EINVALIDPARAM;
    }

    /* Special case - no fault notify EP, means unset content-init mode. */
    if (!rpc_faultNotifyEP) {
        cspacepath_t path;
        vka_cspace_make_path(&procServ.vka, 0, &path);
        return ram_dspace_content_init(dspace, path, PID_NULL);
    }

    /* Copyout the content-init fault notify EP. */
    seL4_CPtr faultNotifyEP = dispatcher_copyout_cptr(rpc_faultNotifyEP);
    if (!faultNotifyEP) {
        dvprintf("could not copy out faultNotifyEP.");
        return EINVALIDPARAM; 
    }
    cspacepath_t path;
    vka_cspace_make_path(&procServ.vka, faultNotifyEP, &path);

    /* Initialise the dataspace content with given dataserver EP. */
    int error = ram_dspace_content_init(dspace, path, pcb->pid);
    if (error != ESUCCESS) {
        dispatcher_release_copyout_cptr(faultNotifyEP);
        return error;
    }

    if (rpc_dataID) {
        (*rpc_dataID) = dspace->ID;
    }
    return ESUCCESS;
}

refos_err_t
data_unhave_data_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd)
{
    return data_have_data_handler(rpc_userptr, rpc_dspace_fd, 0, NULL);
}

/*! \brief Reply from another dataserver to provide the process server with content, in reply to a
           notification the process server has sent it which asked for content.
*/
refos_err_t
data_provide_data_from_parambuffer_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd ,
                                           uint32_t rpc_offset , uint32_t rpc_contentSize)
{
    struct proc_pcb *pcb = (struct proc_pcb*) rpc_userptr;
    struct procserv_msg *m = (struct procserv_msg*) pcb->rpcClient.userptr;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (!check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDPARAM;
    }

    /* Verify and find the RAM dataspace. */
    if (!dispatcher_badge_dspace(rpc_dspace_fd)) {
        ROS_ERROR("EINVALIDPARAM: invalid RAM dataspace badge..\n");
        return EINVALIDPARAM;
    }
    struct ram_dspace *dspace = ram_dspace_get_badge(&procServ.dspaceList, rpc_dspace_fd);
    if (!dspace) {
        ROS_ERROR("EINVALIDPARAM: dataspace not found.\n");
        return EINVALIDPARAM;
    }

    char *initContentBuffer = dispatcher_read_param(pcb, rpc_contentSize);
    if (!initContentBuffer) {
        ROS_WARNING("data_provide_data_from_parambuffer_handler: failed to read from paramBuffer.");
        return ENOPARAMBUFFER;
    }

    /* Initialise the page of the ram dataspace with these contents. */
    int error = ram_dspace_write(initContentBuffer, rpc_contentSize, dspace, rpc_offset);
    if (error != ESUCCESS) {
        return error;
    }

    /* Set the bit that says this page has been provided, and notify all waiters blocking on it. */
    rpc_offset = REFOS_PAGE_ALIGN(rpc_offset);
    int npages = (rpc_contentSize / REFOS_PAGE_SIZE) + (rpc_contentSize % REFOS_PAGE_SIZE ? 1 : 0);
    for (vaddr_t i = 0; i < npages; i++) {
        vaddr_t offset = rpc_offset + (i * REFOS_PAGE_SIZE);
        ram_dspace_set_content_init_provided(dspace, offset);
        ram_dspace_content_init_reply_waiters(dspace, offset);
    }

    return ESUCCESS;
}

int
check_dispatch_dataspace(struct procserv_msg *m, void **userptr)
{
    return check_dispatch_interface(m, userptr, RPC_DATA_LABEL_MIN, RPC_DATA_LABEL_MAX);
}

