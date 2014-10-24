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
#include "../../device_input.h"
#include "../dispatch.h"
#include "stdio_dspace.h"

/*! @file
    @brief Serial STDIO dataspace interface functions.

    This module implements a subset of the dataspace interface (<refos-rpc/data_server.h>),
    for serial devices. The common dataspace dispatcher module delegates calls to us if it has
    decided that the recieved message is a serial dataspace call.

    This is a thin layer basically wrapping the device_input module, which has the concrete
    implementations.
*/

seL4_CPtr
serial_open_handler(void *rpc_userptr , char* rpc_name , int rpc_flags , int rpc_mode ,
                    int rpc_size , int* rpc_errno)
{
    /* Return the serial dataspace badged EP. */
    assert(conServ.serialBadgeEP);
    SET_ERRNO_PTR(rpc_errno, ESUCCESS);
    return conServ.serialBadgeEP;
}

int
serial_write_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                     rpc_buffer_t rpc_buf , uint32_t rpc_count)
{
    assert(rpc_dspace_fd == CONSERV_DSPACE_BADGE_STDIO);
    for (int i = 0; i < rpc_buf.count; i++) {
        ps_cdev_putchar(&conServ.devSerial, ((char*) rpc_buf.data)[i]);
    }
    return rpc_buf.count;
}

int
serial_getc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_block)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    assert(c && c->magic == CONSERV_CLIENT_MAGIC);
    assert(rpc_dspace_fd == CONSERV_DSPACE_BADGE_STDIO ||
           rpc_dspace_fd == CONSERV_DSPACE_BADGE_SCREEN);
    int ch = -1;

    int nread = input_read(&conServ.devInput, &ch, 1);
    if (nread == 0) {
        if (rpc_block) {
            c->rpcClient.skip_reply = true;
            int error = input_save_caller_as_waiter(&conServ.devInput, c, INPUT_WAITERTYPE_GETC);
            if (error != ESUCCESS) {
                ROS_ERROR("Could not save caller.");
            }
            return -1;
        } else {
            return -1;
        }
    }

    return ch;
}

refos_err_t
serial_putc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_c)
{
    assert(rpc_dspace_fd == CONSERV_DSPACE_BADGE_STDIO);
    ps_cdev_putchar(&conServ.devSerial, rpc_c);
    return ESUCCESS;
}