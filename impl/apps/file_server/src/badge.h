/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _FILE_SERVER_BADGE_H_
#define _FILE_SERVER_BADGE_H_

#include <refos/refos.h>
#include <refos-util/serv_common.h>

/*! @file
    @brief CPIO File Server badge space definitions. */

/* ---- BadgeID 49 : Async Notify ---- */

#define FS_ASYNC_NOTIFY_BADGE 0x31

/* ---- BadgeID 50 to 4145 : Clients ---- */

#define FS_CLIENT_BADGE_BASE 0x32

/* ---- BadgeID 4146 to 10319 : Dataspaces ---- */

#define FS_MAX_DATASPACES 8192
#define FS_DSPACE_BADGE_BASE (FS_CLIENT_BADGE_BASE + SRV_DEFAULT_MAX_CLIENTS)

#endif /* _FILE_SERVER_BADGE_H_ */
