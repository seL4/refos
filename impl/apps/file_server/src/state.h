/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _FILE_SERVER_STATE_H_
#define _FILE_SERVER_STATE_H_

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <cpio/cpio.h>
#include <syscall_stubs_sel4.h>

#include <refos/refos.h>
#include <refos-util/cspace.h>
#include <refos-util/walloc.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <refos-rpc/data_client_helper.h>
#include <cpio/cpio.h>
#include <refos-util/serv_connect.h>
#include <refos-util/serv_common.h>

#include "dataspace.h"
#include "pager.h"

 /*! @file
     @brief CPIO Fileserver global state & helper functions. */

/* Debug printing. */
#include <refos-util/dprintf.h>

#define FILESERVER_MAX_PAGE_FRAMES 128
#define FILESERVER_NOTIFICATION_BUFFER_SIZE 0x2000 /* 2 Frames. */
#define FILESERVER_MOUNTPOINT "fileserv"
#define FS_CLIENT_MAGIC 0x3FA3EF6E

/*! @brief Global CPIO file server state structure. */
struct fs_state {
    srv_common_t commonState;

    /* Main file server data structures. */
    struct fs_frame_block pageFrameBlock;
    struct fs_dataspace_table dspaceTable;
};

/*! @brief Global CPIO file server state. */
extern struct fs_state fileServ;

/*! @brief Weak pointer to CPIO file server common state. */
extern srv_common_t *fileServCommon;

/*! @brief Initialise the file server state. */
void fileserv_init(void);

#endif /* _FILE_SERVER_STATE_H_ */