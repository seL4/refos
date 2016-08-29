/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */

#ifndef _CONSOLE_SERVER_DISPATCHER_CLIENT_WATCH_HANDLER_H_
#define _CONSOLE_SERVER_DISPATCHER_CLIENT_WATCH_HANDLER_H_

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

#endif /* _CONSOLE_SERVER_DISPATCHER_CLIENT_WATCH_HANDLER_H_ */