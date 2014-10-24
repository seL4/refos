/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _FILESERV_CPIO_DATASPACE_SYSCALL_DISPATCHER_H_
#define _FILESERV_CPIO_DATASPACE_SYSCALL_DISPATCHER_H_

#include "../state.h"
#include "dispatch.h"
#include <refos-util/serv_connect.h>

 /*! @file
    @brief Handles CPIO file server dataspace calls. */

int rpc_sv_data_dispatcher(void *rpc_userptr, uint32_t label);

/*! @brief Check whether the given recieved message is a data syscall.
    @param m Struct containing info about the recieved message.
    @param userptr Output user pointer. Pass this into the generated dispatcher function.
    @return DISPATCH_SUCCESS if message is a dataspace syscall, DISPATCH_PASS otherwise.
*/
int check_dispatch_data(srv_msg_t *m, void **userptr);

#endif /* _FILESERV_CPIO_DATASPACE_SYSCALL_DISPATCHER_H_ */
