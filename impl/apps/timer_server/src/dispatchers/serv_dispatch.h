/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
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