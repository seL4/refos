/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*! \file mem_syscall.h
    \brief Dispatcher for the procserv memory related syscalls.
*/

#ifndef _REFOS_PROCESS_SERVER_DISPATCHER_PROCSERV_MEMORY_SYSCALL_H_
#define _REFOS_PROCESS_SERVER_DISPATCHER_PROCSERV_MEMORY_SYSCALL_H_

#include "dispatcher.h"
#include "../state.h"

/*! @file
    @brief Handles process server memory-related syscalls. */

/*! @brief Memory syscall post-action.

    Replies to any processes that are waiting to be resumed from a dataspace VM fault.
*/
void mem_syscall_postaction();

#endif /* _REFOS_PROCESS_SERVER_DISPATCHER_PROCSERV_MEMORY_SYSCALL_H_ */