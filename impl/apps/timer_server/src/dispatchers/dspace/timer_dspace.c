/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "../../state.h"
#include "../../device_timer.h"
#include "../dispatch.h"
#include "timer_dspace.h"
#include <refos/refos.h>

/*! @file
    @brief Timer dataspace interface functions.

    This module implements a subset of the dataspace interface (<refos-rpc/data_server.h>),
    for timer devices. The common dataspace dispatcher module delegates calls to us if it has
    decided that the recieved message is a timer dataspace call.

    This is a thin layer basically wrapping the device_timer module, which has the concrete
    implementations.
*/

seL4_CPtr
timer_open_handler(void *rpc_userptr , char* rpc_name , int rpc_flags , int rpc_mode ,
                              int rpc_size , int* rpc_errno)
{
    /* Return the timer dataspace badged EP. */
    assert(timeServ.timerBadgeEP);
    SET_ERRNO_PTR(rpc_errno, ESUCCESS);
    return timeServ.timerBadgeEP;
}

int
timer_write_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                         rpc_buffer_t rpc_buf , uint32_t rpc_count)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    assert(c && c->magic == TIMESERV_CLIENT_MAGIC);
    assert(rpc_dspace_fd == TIMESERV_DSPACE_BADGE_TIMER);

    if (rpc_buf.count < sizeof(uint64_t)) {
        return -EINVALIDPARAM;
    }

    /* Writing to the timer dataspace results in a sleep call. */
    uint64_t timeWait = *( (uint64_t*) (rpc_buf.data) );
    dvprintf("timer_write_handler client waiting for %llu nanoseconds.\n", timeWait);

    int error = device_timer_save_caller_as_waiter(&timeServ.devTimer, c, timeWait);
    if (error == ESUCCESS) {
        c->rpcClient.skip_reply = true;
    }
    return -error;
}

int
timer_read_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                        rpc_buffer_t rpc_buf , uint32_t rpc_count)
{
    if (!rpc_buf.count) {
        return -EINVALIDPARAM;
    }
    if (rpc_buf.count < sizeof(uint64_t)) {
        ROS_WARNING("Buffer not large enough: need %d only got %d.", sizeof(uint64_t),
                    rpc_buf.count);
        return 0;
    }

    assert(rpc_dspace_fd == TIMESERV_DSPACE_BADGE_TIMER);

    /* Reading from the timer dataspace results in a sys_get_time call. */
    uint64_t time = device_timer_get_time(&timeServ.devTimer);
    memcpy(rpc_buf.data, &time, sizeof(uint64_t));
    return sizeof(uint64_t);
}