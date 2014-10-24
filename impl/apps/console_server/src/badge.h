/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CONSOLE_SERVER_BADGE_H_
#define _CONSOLE_SERVER_BADGE_H_

#include <refos/refos.h>

/*! @file
    @brief Console Server badge space definitions.

    Since Console server needs to recieve both asynchronous device IRQ and synchronous syscall
    messages. This presents an interesting badge space problem, as device IRQs are XOR-ed with each
    other but synchronous call values are distinct. The Console server needs to tell one from the
    other.

    Console Server badgespace distinguishes between async notifications and sync notifications using
    an async signature bit (bit 19). If this signature bit is set, it means the first 18-bit bitmask
    preceding it contains IRQ async notification data. If this signature bit is not set, it means
    the 18-bit value preceding it contains numbered badges (which contain both StdIO / timer badges,
    and client session badges).

    Visualisation of Console server badgespace bits:

    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    CONSERV_DSPACE_BADGE_STDIO:
                        ◀――― lower bits               higher bits ―――▶
                        0 0 0 1 0 1    0 0 0 0 0 0 0 0 0 0 0 0 0     0
                        ⎣_________⎦                                  ▲ ASYNC_BADGE_MASK = false
                             ▲ 0x28 

    CONSERV_DSPACE_BADGE_SCREEN
                        ◀――― lower bits               higher bits ―――▶
                        1 1 1 0 0 1    0 0 0 0 0 0 0 0 0 0 0 0 0     0
                        ⎣_________⎦                                  ▲ ASYNC_BADGE_MASK = false
                             ▲ 0x27

    CONSERV_CLIENT_BADGE badges:
                        ◀――― lower bits               higher bits ―――▶
                        0 0 0 0 0 0    1 0 1 1 0 1 0 0 0 0 0 0 0     0
                                       ⎣_______________________⎦     ▲ ASYNC_BADGE_MASK = false
                                                  ▲ Client ID value

    CONSERV_ASYNC_NOTIFY_BADGE:
                        ◀――― lower bits               higher bits ―――▶
                        1    0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0     1
                        ▲                                            ▲ ASYNC_BADGE_MASK = true
                     0x1 bit set

    CONSERV_IRQ_BADGE badges:
                        ◀――― lower bits               higher bits ―――▶
                        0    0 1 0 0 0 0 1 1 0 0 0 0 0 1 0 1 0 0     1
                             ⎣_________________________________⎦     ▲ ASYNC_BADGE_MASK = true
                                             ▲
                        Bitmask indicating which IRQ channels happened.

    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

#define CONSERV_DSPACE_BADGE_STDIO 0x28    /*!< @brief StdIO dataspace badge value. */
#define CONSERV_DSPACE_BADGE_SCREEN 0x27    /*!< @brief Screen dataspace badge value. */

/* ---- BadgeID 48 to 4143 : Clients ---- */
#define CONSERV_CLIENT_BADGE_BASE 0x30

/* ---- Async BadgeIDs  ---- */

#define CONSERV_ASYNC_BADGE_MASK (1 << 19)
#define CONSERV_ASYNC_NOTIFY_BADGE (1 << 0)

#define CONSERV_IRQ_BADGE_BASE_POW 1
#define CONSERV_IRQ_BADGE_BASE_POW_TOP 18
#define CONSERV_IRQ_BADGE_NCHANNELS \
        ((CONSERV_IRQ_BADGE_BASE_POW_TOP - CONSERV_IRQ_BADGE_BASE_POW) + 1)

#endif /* _CONSOLE_SERVER_BADGE_H_ */
