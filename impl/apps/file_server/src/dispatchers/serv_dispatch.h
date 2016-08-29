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

#ifndef _FILESERV_SERV_SYSCALL_DISPATCHER_H_
#define _FILESERV_SERV_SYSCALL_DISPATCHER_H_

#include "../state.h"
#include "dispatch.h"
#include <refos-util/serv_connect.h>

/*! @file
    @brief Handles server connection and session establishment syscalls. */

/*! @brief Check whether the given recieved message is a server syscall.
    @param m Struct containing info about the recieved message.
    @param userptr Output user pointer. Pass this into the generated dispatcher function.
    @return DISPATCH_SUCCESS if message is a dataspace syscall, DISPATCH_PASS otherwise.
*/
int check_dispatch_serv(srv_msg_t *m, void **userptr);

int rpc_sv_serv_dispatcher(void *rpc_userptr, uint32_t label);

#endif /* _FILESERV_SERV_SYSCALL_DISPATCHER_H_ */
