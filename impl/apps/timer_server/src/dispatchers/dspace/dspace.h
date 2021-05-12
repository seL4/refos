/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _TIMESERV_DATASPACE_SYSCALL_DISPATCHER_H_
#define _TIMESERV_DATASPACE_SYSCALL_DISPATCHER_H_

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

#endif /* _TIMESERV_DATASPACE_SYSCALL_DISPATCHER_H_ */
