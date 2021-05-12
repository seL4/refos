/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*! @file
    @brief Process server fault dispatcher which handles VM faults.

    Handles faults which the kernel sends to the processs server IPC endpoint pretending to be the
    faulting client, handling internal ram dataspace faults or delegating to the corresponding
    pager / content initialiser accordingly.
*/

#ifndef _REFOS_PROCESS_SERVER_DISPATCHER_FAULT_HANDLER_H_
#define _REFOS_PROCESS_SERVER_DISPATCHER_FAULT_HANDLER_H_

#include "dispatcher.h"
#include "../state.h"

/*! @brief Check whether the given recieved message is a VM fault message.
    @param m Struct containing info about the recieved message.
    @param userptr Output user pointer. Pass this into the generated dispatcher function.
    @return DISPATCH_SUCCESS if message is a VM fault syscall, DISPATCH_PASS otherwise.
*/
int check_dispatch_fault(struct procserv_msg *m, void **userptr);

/*! \brief Dispatcher for client VM faults. */
int dispatch_vm_fault(struct procserv_msg *m, void **userptr);

#endif /* _REFOS_PROCESS_SERVER_DISPATCHER_FAULT_HANDLER_H_ */
