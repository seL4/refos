/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _REFOS_PROCESS_SERVER_DISPATCHER_RAM_DATASPACE_SYSCALL_H_
#define _REFOS_PROCESS_SERVER_DISPATCHER_RAM_DATASPACE_SYSCALL_H_

#include "../common.h"
#include "dispatcher.h"

#define PROCSERV_DSPACE_FLAG_DEVICE_PADDR 0x10000000

/*! @file
   @brief Process server anon dataspace syscall handler. */

int rpc_sv_data_dispatcher(void *rpc_userptr, uint32_t label);

/*! @brief Check whether the given recieved message is a data syscall.
    @param m Struct containing info about the recieved message.
    @param userptr Output user pointer. Pass this into the generated dispatcher function.
    @return DISPATCH_SUCCESS if message is a dataspace syscall, DISPATCH_PASS otherwise.
*/
int check_dispatch_dataspace(struct procserv_msg *m, void **userptr);

#endif /* _REFOS_PROCESS_SERVER_DISPATCHER_RAM_DATASPACE_SYSCALL_H_ */