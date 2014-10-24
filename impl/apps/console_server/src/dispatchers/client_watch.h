/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
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