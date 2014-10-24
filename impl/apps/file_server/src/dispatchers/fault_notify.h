/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _FILESERV_FAULT_NOTIFY_DISPATCHER_H_
#define _FILESERV_FAULT_NOTIFY_DISPATCHER_H_

#include "../state.h"
#include "dispatch.h"

/*! @file
    @brief Handles process server client notifications. */

/*! @brief Notification dispatcher, responsible for dispatching async notifications.
    @param m The fileserver message info struct.
    @return DISPATCH_SUCCESS if success, DISPATCHER_ERROR of and error occured, DISPATCH_PASS if the
            message is not a async notification.
*/
int dispatch_notification(srv_msg_t *m);

#endif /* _FILESERV_FAULT_NOTIFY_DISPATCHER_H_ */
