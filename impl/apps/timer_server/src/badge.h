/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _TIMER_SERVER_BADGE_H_
#define _TIMER_SERVER_BADGE_H_

#include <refos/refos.h>

/*! @file
    @brief Timer Server badge space definitions.

    Since Timer server needs to recieve both asynchronous device IRQ and synchronous syscall
    messages, similar to the console server. The solution is thus also similar to console server.
    Please look in @ref console_server/badge.h for a more detailed explanation.
*/

#define TIMESERV_DSPACE_BADGE_TIMER 0x26    /*!< @brief Timer dataspace badge value. */

/* ---- BadgeID 48 to 4143 : Clients ---- */
#define TIMESERV_CLIENT_BADGE_BASE 0x30

/* ---- Async BadgeIDs  ---- */

#define TIMESERV_ASYNC_BADGE_MASK (1 << 19)
#define TIMESERV_ASYNC_NOTIFY_BADGE (1 << 0)

#define TIMESERV_IRQ_BADGE_BASE_POW 1
#define TIMESERV_IRQ_BADGE_BASE_POW_TOP 18
#define TIMESERV_IRQ_BADGE_NCHANNELS \
        ((TIMESERV_IRQ_BADGE_BASE_POW_TOP - TIMESERV_IRQ_BADGE_BASE_POW) + 1)

#endif /* _TIMER_SERVER_BADGE_H_ */
