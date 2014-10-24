/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _TIMER_SERVER_DISPATCHER_DSPACE_TIMER_H_
#define _TIMER_SERVER_DISPATCHER_DSPACE_TIMER_H_

#include "../../badge.h"

/*! @file
    @brief Timer dataspace interface functions. */

/*! @brief Similar to data_open_handler, for timer dataspaces. */
seL4_CPtr timer_open_handler(void *rpc_userptr , char* rpc_name , int rpc_flags , int rpc_mode ,
                              int rpc_size , int* rpc_errno);

/*! @brief Similar to data_write_handler, for timer dataspaces.

    Writing a uint64_t to the timer dspace will be interpreted as sleeping for that many nanoseconds
    akin to a nanosleep() syscall, before returning.
*/
int timer_write_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                         rpc_buffer_t rpc_buf , uint32_t rpc_count);

/*! @brief Similar to data_read_handler, for timer dataspaces.

    Reading a uint64_t from the timer dspace will be interpreted as reading the current time in
    nanoseconds.
*/
int timer_read_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                        rpc_buffer_t rpc_buf , uint32_t rpc_count);

#endif /* _TIMER_SERVER_DISPATCHER_DSPACE_TIMER_H_ */