/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _TIMER_SERVER_DISPATCHER_CLIENT_WATCH_HANDLER_H_
#define _TIMER_SERVER_DISPATCHER_CLIENT_WATCH_HANDLER_H_

#include "../state.h"
#include "dispatch.h"

/*! @file
    @brief Client watch dispatcher module. */

/*! @brief Dispatch a client death notification message.
    @param m The recieved interrupt message.
    @return DISPATCH_SUCCESS if successfully dispatched, DISPATCH_ERROR if there was an unexpected
            error, DISPATCH_PASS if the given message is not an interrupt message.
*/
int dispatch_client_watch(srv_msg_t *m);

#endif /* _TIMER_SERVER_DISPATCHER_CLIENT_WATCH_HANDLER_H_ */