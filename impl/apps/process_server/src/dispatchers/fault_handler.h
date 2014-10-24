/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
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
