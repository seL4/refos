/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
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