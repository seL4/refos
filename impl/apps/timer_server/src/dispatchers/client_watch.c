/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <stdbool.h>
#include <refos/share.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_common.h>
#include "dispatch.h"
#include "../state.h"
#include "../badge.h"

/*! @file
    @brief Client watch dispatcher module. */

/*! @brief Handles client death notifications.
    @param notification Structure containing the notification message, read from the notification
                        ring buffer.
    @return DISPATCH_SUCCESS if success, DISPATCHER_ERROR otherwise.
*/
static int
handle_timeserver_death_notification(struct proc_notification *notification)
{
    dprintf(COLOUR_B "## Timer server Handling death notification...\n" COLOUR_RESET);
    dprintf("     Label: PROCSERV_NOTIFY_DEATH\n");
    dprintf("     deathID: %d\n", notification->arg[0]);

    /* Find the client and queue it for deletion. */
    int error = client_queue_delete_deathID(&timeServCommon->clientTable, notification->arg[0]);

    if (error) {
        ROS_ERROR("Unknown deathID. timer server book-keeping error.");
        assert(!"timer server book-keeping bug.");
        return DISPATCH_ERROR;
    }
    return DISPATCH_SUCCESS;
}

int dispatch_client_watch(srv_msg_t *m)
{
    if ((m->badge & TIMESERV_ASYNC_BADGE_MASK) == 0) {
        return DISPATCH_PASS;
    }
    if ((m->badge & TIMESERV_ASYNC_NOTIFY_BADGE) == 0) {
        return DISPATCH_PASS;
    }

    srv_common_notify_handler_callbacks_t cb = {
        .handle_server_fault = NULL,
        .handle_server_content_init = NULL,
        .handle_server_death_notification = handle_timeserver_death_notification
    };

    return srv_dispatch_notification(timeServCommon, cb);
}
