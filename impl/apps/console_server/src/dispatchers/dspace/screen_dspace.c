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
#include "../dispatch.h"
#include "screen_dspace.h"
#include "stdio_dspace.h"
#include <refos/refos.h>

/*! @file
    @brief Timer dataspace interface functions.

    This module implements a subset of the dataspace interface (<refos-rpc/data_server.h>),
    for screen devices. The common dataspace dispatcher module delegates calls to us if it has
    decided that the recieved message is a timer dataspace call.

    This is a thin layer basically wrapping the device_screen module, which has the concrete
    implementations.
*/


seL4_CPtr
screen_open_handler(void *rpc_userptr , char* rpc_name , int rpc_flags , int rpc_mode ,
                    int rpc_size , int* rpc_errno)
{
    /* Return the serial dataspace badged EP. */
    assert(conServ.screenBadgeEP);
    if (conServ.devScreen.initialised) {
        SET_ERRNO_PTR(rpc_errno, ESUCCESS);
        return conServ.screenBadgeEP;
    }
    SET_ERRNO_PTR(rpc_errno, EFILENOTFOUND);
    return 0;
}

int
screen_write_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                     rpc_buffer_t rpc_buf , uint32_t rpc_count)
{
    assert(rpc_dspace_fd == CONSERV_DSPACE_BADGE_SCREEN);
    if (!conServ.devScreen.initialised) {
        ROS_WARNING("Screen dataspace recieved but no screen available.");
        return -1;
    }
    device_screen_write(&conServ.devScreen, (char*) rpc_buf.data, (int) rpc_buf.count);
    return rpc_buf.count;
}

int
screen_getc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_block)
{
    return serial_getc_handler(rpc_userptr, rpc_dspace_fd, rpc_block);
}

refos_err_t
screen_putc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_c)
{
    assert(rpc_dspace_fd == CONSERV_DSPACE_BADGE_SCREEN);
    if (!conServ.devScreen.initialised) {
        ROS_WARNING("Screen dataspace recieved but no screen available.");
        return -1;
    }
    char c = (char) rpc_c;
    device_screen_write(&conServ.devScreen, &c, 1);
    return ESUCCESS;
}