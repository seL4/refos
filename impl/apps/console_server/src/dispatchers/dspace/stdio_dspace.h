/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CONSOLE_SERVER_DISPATCHER_DSPACE_STDIO_H_
#define _CONSOLE_SERVER_DISPATCHER_DSPACE_STDIO_H_

#include "../../badge.h"

/*! @file
    @brief Serial STDIO dataspace interface functions. */

/*! @brief Similar to data_open_handler, for serial dataspaces. */
seL4_CPtr serial_open_handler(void *rpc_userptr , char* rpc_name , int rpc_flags , int rpc_mode ,
                              int rpc_size , int* rpc_errno);

/*! @brief Similar to data_write_handler, for serial dataspaces. */
int serial_write_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                         rpc_buffer_t rpc_buf , uint32_t rpc_count);

/*! @brief Similar to data_getc_handler, for serial dataspaces. */
int serial_getc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_block);

/*! @brief Similar to data_putc_handler, for serial dataspaces. */
refos_err_t serial_putc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_c);

#endif /* _CONSOLE_SERVER_DISPATCHER_DSPACE_STDIO_H_ */