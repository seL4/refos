/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
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