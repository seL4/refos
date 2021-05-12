/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <refos-util/cspace.h>
#include <refos/vmlayout.h>
#include <refos/refos.h>
#include <refos-io/stdio.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <refos-rpc/name_client.h>
#include <refos-rpc/name_client_helper.h>
#include "state.h"
#include "badge.h"

/*! @file
    @brief timer server global state & helper functions. */

struct timeserv_state timeServ;
srv_common_t *timeServCommon;
const char* dprintfServerName = "TIMESERV";
int dprintfServerColour = 34;

static seL4_CPtr
timeserv_get_irq_handler_endpoint(void *cookie, int irq)
{
    return proc_get_irq_handler(irq);
}

void
timeserv_init(void)
{
    /* Initialise device IO manager. */
    dprintf("    Initialising timeserv device IO manager...\n");
    devio_init(&timeServ.devIO);

    /* Set up the server common config. */
    srv_common_config_t cfg = {
        .maxClients = SRV_DEFAULT_MAX_CLIENTS,
        .clientBadgeBase = TIMESERV_CLIENT_BADGE_BASE,
        .clientMagic = TIMESERV_CLIENT_MAGIC,
        .notificationBufferSize = SRV_DEFAULT_NOTIFICATION_BUFFER_SIZE,
        .paramBufferSize = SRV_DEFAULT_PARAM_BUFFER_SIZE, 
        .serverName = "timeserver",
        .mountPointPath = TIMESERV_MOUNTPOINT,
        .nameServEP = REFOS_NAMESERV_EP,
        .faultDeathNotifyBadge = TIMESERV_ASYNC_NOTIFY_BADGE | TIMESERV_ASYNC_BADGE_MASK
    };

    /* Set up file server common state. */
    timeServCommon = &timeServ.commonState;
    srv_common_init(timeServCommon, cfg);

    /* Set up irq handler state config. */
    dev_irq_config_t irqConfig = {
        .numIRQChannels = TIMESERV_IRQ_BADGE_NCHANNELS,
        .badgeBaseBit = TIMESERV_IRQ_BADGE_BASE_POW,
        .badgeTopBit = TIMESERV_IRQ_BADGE_BASE_POW_TOP,
        .badgeMaskBits = TIMESERV_ASYNC_BADGE_MASK,
        .notifyAsyncEP = timeServCommon->notifyAsyncEP,
        .getIRQHandlerEndpoint = timeserv_get_irq_handler_endpoint,
        .getIRQHandlerEndpointCookie = NULL
    };

    /* Set up IRQ handler state. */
    dev_irq_init(&timeServ.irqState, irqConfig);
    assert(timeServ.irqState.magic == DEVICE_IRQ_MAGIC);

    /* Create the badged EP to represent the timer dataspace. */
    timeServ.timerBadgeEP = srv_mint(TIMESERV_DSPACE_BADGE_TIMER, timeServCommon->anonEP);
    assert(timeServ.timerBadgeEP);

    /* Set up timer device. */
    device_timer_init(&timeServ.devTimer, &timeServ.devIO);
}