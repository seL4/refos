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
