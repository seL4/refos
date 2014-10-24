/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*! @file
    @brief Dispatcher for the procserv name server syscalls.

    Handles calls to process server name server interface. The process server acts as the root
    name server.
*/

#ifndef _REFOS_PROCESS_SERVER_DISPATCHER_NAMESERV_SYSCALL_H_
#define _REFOS_PROCESS_SERVER_DISPATCHER_NAMESERV_SYSCALL_H_

#include "../common.h"
#include "dispatcher.h"

/*! @brief Check whether the given recieved message is a nameserv syscall.
    @param m Struct containing info about the recieved message.
    @param userptr Output user pointer. Pass this into the generated dispatcher function.
    @return DISPATCH_SUCCESS if message is a nameserv syscall, DISPATCH_PASS otherwise.
*/
int check_dispatch_nameserv(struct procserv_msg *m, void **userptr);

int rpc_sv_name_dispatcher(void *rpc_userptr, uint32_t label);

#endif /* _REFOS_PROCESS_SERVER_DISPATCHER_NAMESERV_SYSCALL_H_ */