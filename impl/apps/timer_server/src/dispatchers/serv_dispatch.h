/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _TIMER_SERVER_DISPATCHER_SERV_INTERFACE_H_
#define _TIMER_SERVER_DISPATCHER_SERV_INTERFACE_H_

#include "../state.h"
#include "dispatch.h"
#include <refos-util/serv_connect.h>

/*! @file
    @brief Handles server connection and session establishment syscalls. */

int rpc_sv_serv_dispatcher(void *rpc_userptr, uint32_t label);

/*! @brief Check whether the given recieved message is a server syscall.
    @param m Struct containing info about the recieved message.
    @param userptr Output user pointer. Pass this into the generated dispatcher function.
    @return DISPATCH_SUCCESS if message is a dataspace syscall, DISPATCH_PASS otherwise.
*/
int check_dispatch_serv(srv_msg_t *m, void **userptr);

#endif /* _TIMER_SERVER_DISPATCHER_SERV_INTERFACE_H_ */