/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "dispatch.h"
#include "serv_dispatch.h"
#include "../badge.h"
#include <refos/error.h>
#include <refos-rpc/serv_server.h>

/*! @file
    @brief Handles server connection and session establishment syscalls.

    This file contains the handlers for serv interface syscalls. It should implement the
    declarations in the generated <refos-rpc/serv_server.h>.
*/

seL4_CPtr
serv_connect_direct_handler(void *rpc_userptr , seL4_CPtr rpc_liveness , int* rpc_errno)
{
    struct srv_client *anonc = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) anonc->rpcClient.userptr;
    assert(anonc->magic == FS_DISPATCH_ANON_CLIENT_MAGIC);
    struct srv_client *c = fileServCommon->ctable_connect_direct_handler(
            fileServCommon, m, rpc_liveness, rpc_errno);
    return c ? c->session : (seL4_CPtr) 0;
}

refos_err_t
serv_ping_handler(void *rpc_userptr)
{
    dprintf(COLOUR_M "FILE SERVER RECIEVED PING!!! HI THERE! (╯°□°）╯︵ ┻━┻\n" COLOUR_RESET);
    return ESUCCESS;
}

refos_err_t
serv_set_param_buffer_handler(void *rpc_userptr , seL4_CPtr rpc_parambuffer_dataspace ,
                              uint32_t rpc_parambuffer_size)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c->magic == FS_CLIENT_MAGIC);
    return fileServCommon->ctable_set_param_buffer_handler(fileServCommon, c, m,
        rpc_parambuffer_dataspace, rpc_parambuffer_size);
}

void
serv_disconnect_direct_handler(void *rpc_userptr)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    assert(c->magic == FS_CLIENT_MAGIC);
    dprintf("Fileserver disconnecting client cID = %d. Bye! (D:)\n", c->cID);
    return fileServCommon->ctable_disconnect_direct_handler(fileServCommon, c);
}

int
check_dispatch_serv(srv_msg_t *m, void **userptr)
{
    int label = seL4_GetMR(0);
    if (label == RPC_SERV_CONNECT_DIRECT && m->badge != SRV_UNBADGED) {
        return DISPATCH_PASS;
    }
    return check_dispatch_interface(m, userptr, RPC_SERV_LABEL_MIN, RPC_SERV_LABEL_MAX);
}