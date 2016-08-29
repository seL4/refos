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

#ifndef _CONSOLE_SERVER_DISPATCHER_DSPACE_SCREEN_H_
#define _CONSOLE_SERVER_DISPATCHER_DSPACE_SCREEN_H_

/*! @file
    @brief Screen / keyboard STDIO dataspace interface functions. */

/*! @brief Similar to data_open_handler, for screen / keyboard dataspaces. */
seL4_CPtr screen_open_handler(void *rpc_userptr , char* rpc_name , int rpc_flags , int rpc_mode ,
                              int rpc_size , int* rpc_errno);

/*! @brief Similar to data_write_handler, for screen / keyboard dataspaces. */
int screen_write_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                         rpc_buffer_t rpc_buf , uint32_t rpc_count);

/*! @brief Similar to data_getc_handler, for screen / keyboard dataspaces. */
int screen_getc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_block);

/*! @brief Similar to data_putc_handler, for screen / keyboard dataspaces. */
refos_err_t screen_putc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_c);

#endif /* _CONSOLE_SERVER_DISPATCHER_DSPACE_SCREEN_H_ */