/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CONSOLE_SERVER_DATASPACE_SYSCALL_DISPATCHER_H_
#define _CONSOLE_SERVER_DATASPACE_SYSCALL_DISPATCHER_H_

#include "../../state.h"
#include "../dispatch.h"
#include <refos-rpc/data_server.h>
#include <refos-util/serv_connect.h>

 /*! @file
     @brief Common dataspace interface functions. */

int rpc_sv_data_dispatcher(void *rpc_userptr, uint32_t label);

/*! @brief Check whether the given recieved message is a data syscall.
    @param m Struct containing info about the recieved message.
    @param userptr Output user pointer. Pass this into the generated dispatcher function.
    @return DISPATCH_SUCCESS if message is a dataspace syscall, DISPATCH_PASS otherwise.
*/
int check_dispatch_data(srv_msg_t *m, void **userptr);

#endif /* _CONSOLE_SERVER_DATASPACE_SYSCALL_DISPATCHER_H_ */
