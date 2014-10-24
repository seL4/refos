/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "dspace.h"
#include "stdio_dspace.h"
#include "screen_dspace.h"

 /*! @file
     @brief Common dataspace interface functions.

     This module implements the actual functions defined in <refos-rpc/data_server.h>, and then
     analyses the parameters to decide which dataspace to delegate the message to. If the parameters
     indicate the message should be handed off to the serial / screen dataspace, this module then
     calls the corresponding dataspace interface function of the correct serial / screen dataspace.
*/

seL4_CPtr
data_open_handler(void *rpc_userptr , char* rpc_name , int rpc_flags , int rpc_mode , int rpc_size ,
                  int* rpc_errno)
{
    if (!rpc_name) {
        SET_ERRNO_PTR(rpc_errno, EFILENOTFOUND);
        return 0;
    }

    /* Handle stdio / serial dataspace open requests. */
    if (strcmp(rpc_name, "serial") == 0 || strcmp(rpc_name, "stdio") == 0) {
        return serial_open_handler(rpc_userptr, rpc_name, rpc_flags, rpc_mode, rpc_size, rpc_errno);
    }

    /* Handle screen dataspace open requests. */
    if (strcmp(rpc_name, "screen") == 0 || strcmp(rpc_name, "keyboard") == 0) {
        return screen_open_handler(rpc_userptr, rpc_name, rpc_flags, rpc_mode, rpc_size, rpc_errno);
    }

    SET_ERRNO_PTR(rpc_errno, EFILENOTFOUND);
    return 0;
}

refos_err_t
data_close_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c && (c->magic == CONSERV_DISPATCH_ANON_CLIENT_MAGIC ||
           c->magic == CONSERV_CLIENT_MAGIC));

    if (!srv_check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDPARAM;
    }

    /* No need to close stdio / serial dataspaces. */
    if (rpc_dspace_fd == CONSERV_DSPACE_BADGE_STDIO) {
        return ESUCCESS;
    }

    /* No need to close screen dataspaces. */
    if (rpc_dspace_fd == CONSERV_DSPACE_BADGE_SCREEN) {
        return ESUCCESS;
    }

    return EFILENOTFOUND;
}

int
data_read_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                  rpc_buffer_t rpc_buf , uint32_t rpc_count)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c && (c->magic == CONSERV_DISPATCH_ANON_CLIENT_MAGIC || c->magic == CONSERV_CLIENT_MAGIC));

    if (!srv_check_dispatch_caps(m, 0x00000001, 1)) {
        return -EINVALIDPARAM;
    }

    /* Handle read from stdio / serial dataspaces. */
    if (rpc_dspace_fd == CONSERV_DSPACE_BADGE_STDIO) {
        return -EACCESSDENIED;
    }

    /* Handle read from screen dataspaces. */
    if (rpc_dspace_fd == CONSERV_DSPACE_BADGE_SCREEN) {
        return -EACCESSDENIED;
    }

    return -EFILENOTFOUND;
}

int
data_write_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                   rpc_buffer_t rpc_buf , uint32_t rpc_count)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c && (c->magic == CONSERV_DISPATCH_ANON_CLIENT_MAGIC || c->magic == CONSERV_CLIENT_MAGIC));

    if (!srv_check_dispatch_caps(m, 0x00000001, 1)) {
        return -EINVALIDPARAM;
    }

    /* Handle write to stdio / serial dataspaces. */
    if (rpc_dspace_fd == CONSERV_DSPACE_BADGE_STDIO) {
        return serial_write_handler(rpc_userptr, rpc_dspace_fd, rpc_offset, rpc_buf, rpc_count);
    }

    /* Handle write to screen dataspaces. */
    if (rpc_dspace_fd == CONSERV_DSPACE_BADGE_SCREEN) {
        return screen_write_handler(rpc_userptr, rpc_dspace_fd, rpc_offset, rpc_buf, rpc_count);
    }

    return -EFILENOTFOUND;
}

int
data_getc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_block)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;

    if (c->magic == CONSERV_DISPATCH_ANON_CLIENT_MAGIC) {
        return -1;
    }
    if (!srv_check_dispatch_caps(m, 0x00000001, 1)) {
        return -1;
    }

    /* Handle getc stdio / serial dataspaces. */
    if (rpc_dspace_fd == CONSERV_DSPACE_BADGE_STDIO) {
        return serial_getc_handler(rpc_userptr, rpc_dspace_fd, rpc_block);
    }

    /* Handle getc screen dataspaces. */
    if (rpc_dspace_fd == CONSERV_DSPACE_BADGE_SCREEN) {
        return screen_getc_handler(rpc_userptr, rpc_dspace_fd, rpc_block);
    }

    return -1;
}

refos_err_t
data_expand_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_size)
{
    return EUNIMPLEMENTED;
}

refos_err_t
data_putc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_c)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c && (c->magic == CONSERV_DISPATCH_ANON_CLIENT_MAGIC || c->magic == CONSERV_CLIENT_MAGIC));

    if (!srv_check_dispatch_caps(m, 0x00000001, 1)) {
        return EINVALIDPARAM;
    }

    /* Handle close stdio / serial dataspaces. */
    if (rpc_dspace_fd == CONSERV_DSPACE_BADGE_STDIO) {
        return serial_putc_handler(rpc_userptr, rpc_dspace_fd, rpc_c);
    }

    /* Handle close screen dataspaces. */
    if (rpc_dspace_fd == CONSERV_DSPACE_BADGE_SCREEN) {
        return screen_putc_handler(rpc_userptr, rpc_dspace_fd, rpc_c);
    }

    return EFILENOTFOUND;
}

off_t
data_lseek_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , off_t rpc_offset , int rpc_whence)
{
    assert(!"data_lseek_handler unimplemented.");
    return 0;
}

uint32_t
data_get_size_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd)
{
    return 0;
}

refos_err_t
data_datamap_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , seL4_CPtr rpc_memoryWindow ,
                     uint32_t rpc_offset)
{
    assert(!"data_datamap_handler unimplemented.");
    return EUNIMPLEMENTED;
}

refos_err_t
data_dataunmap_handler(void *rpc_userptr , seL4_CPtr rpc_memoryWindow)
{
    assert(!"data_dataunmap_handler unimplemented.");
    return EUNIMPLEMENTED;
}

refos_err_t
data_init_data_handler(void *rpc_userptr , seL4_CPtr rpc_destDataspace , seL4_CPtr rpc_srcDataspace,
                       uint32_t rpc_srcDataspaceOffset)
{
    assert(!"data_init_data_handler unimplemented.");
    return EUNIMPLEMENTED;
}

refos_err_t
data_have_data_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , seL4_CPtr rpc_faultNotifyEP ,
                       uint32_t* rpc_dataID)
{
    assert(!"data_have_data_handler unimplemented.");
    return EUNIMPLEMENTED;
}

refos_err_t
data_unhave_data_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd)
{
    assert(!"data_unhave_data_handler unimplemented.");
    return EUNIMPLEMENTED;
}

refos_err_t
data_provide_data_from_parambuffer_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd ,
                                           uint32_t rpc_offset , uint32_t rpc_contentSize)
{
    assert(!"data_provide_data_from_parambuffer_handler unimplemented.");
    return EUNIMPLEMENTED;
}

int
check_dispatch_data(srv_msg_t *m, void **userptr)
{
    return check_dispatch_interface(m, userptr, RPC_DATA_LABEL_MIN, RPC_DATA_LABEL_MAX);
}