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
#include <stdlib.h>
#include <refos/refos.h>
#include <refos-rpc/name_client.h>
#include <refos-rpc/name_client_helper.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>
#include <refos-util/serv_connect.h>
#include <refos-util/serv_common.h>

#include "badge.h"
#include "state.h"
#include "dataspace.h"
#include "pager.h"

 /*! @file
     @brief CPIO Fileserver global state & helper functions. */

struct fs_state fileServ;
srv_common_t *fileServCommon;
const char* dprintfServerName = "FILESERV";
int dprintfServerColour = 35;

void
fileserv_init(void) {
    dprintf("RefOS Fileserver initialising...\n");
    struct fs_state *s = &fileServ;
    fileServCommon = &fileServ.commonState;

    /* Set up the server common config. */
    srv_common_config_t cfg = {
        .maxClients = SRV_DEFAULT_MAX_CLIENTS,
        .clientBadgeBase = FS_CLIENT_BADGE_BASE,
        .clientMagic = FS_CLIENT_MAGIC,
        .notificationBufferSize = FILESERVER_NOTIFICATION_BUFFER_SIZE,
        .paramBufferSize = SRV_DEFAULT_PARAM_BUFFER_SIZE, 
        .serverName = "fileserver",
        .mountPointPath = FILESERVER_MOUNTPOINT,
        .nameServEP = REFOS_NAMESERV_EP,
        .faultDeathNotifyBadge = FS_ASYNC_NOTIFY_BADGE
    };

    /* Set up file server common state. */
    srv_common_init(fileServCommon, cfg);

    /* Set up file server book keeping data structures. */

    dprintf("    initialising pager frame block...\n");
    pager_init(&s->pageFrameBlock, FILESERVER_MAX_PAGE_FRAMES * REFOS_PAGE_SIZE);

    dprintf("    initialising dataspace allocation table...\n");
    dspace_table_init(&s->dspaceTable);
}
