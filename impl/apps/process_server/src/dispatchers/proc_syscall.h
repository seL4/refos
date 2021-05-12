/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*! @file
    @brief Dispatcher for the procserv syscalls. */

#ifndef _REFOS_PROCESS_SERVER_DISPATCHER_PROCSERV_SYSCALL_H_
#define _REFOS_PROCESS_SERVER_DISPATCHER_PROCSERV_SYSCALL_H_

#include "dispatcher.h"

/*! @brief Check whether the given recieved message is a procserv syscall.
    @param m Struct containing info about the recieved message.
    @param userptr Output user pointer. Pass this into the generated dispatcher function.
    @return DISPATCH_SUCCESS if message is a procserv syscall, DISPATCH_PASS otherwise.
*/
int check_dispatch_syscall(struct procserv_msg *m, void **userptr);

int rpc_sv_proc_dispatcher(void *rpc_userptr, uint32_t label);

#endif /* _REFOS_PROCESS_SERVER_DISPATCHER_PROCSERV_SYSCALL_H_ */
