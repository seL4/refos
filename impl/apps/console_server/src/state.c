/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>
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
#include <refos-util/serv_common.h>
#include <refos-util/device_irq.h>
#include "state.h"
#include "badge.h"

/*! @file
    @brief Console Server global state & helper functions. */

struct conserv_state conServ;
srv_common_t *conServCommon;
const char* dprintfServerName = "CONSERV";
int dprintfServerColour = 33;

/*! @brief Console Server stdout writev override.

    The console server needs to override stdout, because in the console server itself is the serial
    driver, and obviously cannot IPC itself for stdout. Instead, we override writev here to directly
    output to the serial driver. If EGA screen is detected to be enabled, we write to that as well.

    @param data The data buffer to write out to serial.
    @param count Num of characters in writev.
    @return The number of bytes written. Always returns count.
*/
static size_t
conserv_writev_override(void *data, size_t count)
{
    for (int i = 0; i < count; i++) {
        ps_cdev_putchar(&conServ.devSerial, ((char*) data)[i]);
    }
    if (conServ.devScreen.initialised) {
        device_screen_write(&conServ.devScreen, (char*) data, (int) count);
    }
    return count;
}

static seL4_CPtr
conserv_get_irq_handler_endpoint(void *cookie, int irq)
{
    return proc_get_irq_handler(irq);
}

void
conserv_init(void)
{
    refos_seL4_debug_override_stdout();
    dprintf("RefOS Console server initialising...\n");
    conServCommon = &conServ.commonState;

    /* Initialise device IO manager. */
    dprintf("    Initialising conserv device IO manager...\n");
    devio_init(&conServ.devIO);

    /* Initialise the serial device. */
    dprintf("    Initialising conserv serial device...\n");
    ps_chardevice_t *devSerialRet = ps_cdev_init(PS_SERIAL_DEFAULT, &conServ.devIO.opsIO,
                                                 &conServ.devSerial);
    if (!devSerialRet || devSerialRet != &conServ.devSerial) {
        ROS_ERROR("Console Server could not initialise serial device.");
        assert(!"conserv_init failed.");
        exit(1);
    }
    dprintf("    Serial device initialised at vaddr 0x%x\n", (uint32_t) devSerialRet->vaddr);
    refos_override_stdio(NULL, conserv_writev_override);

    /* Set up the server common config. */
    srv_common_config_t cfg = {
        .maxClients = SRV_DEFAULT_MAX_CLIENTS,
        .clientBadgeBase = CONSERV_CLIENT_BADGE_BASE,
        .clientMagic = CONSERV_CLIENT_MAGIC,
        .notificationBufferSize = SRV_DEFAULT_NOTIFICATION_BUFFER_SIZE,
        .paramBufferSize = SRV_DEFAULT_PARAM_BUFFER_SIZE, 
        .serverName = "conserver",
        .mountPointPath = CONSERV_MOUNTPOINT,
        .nameServEP = REFOS_NAMESERV_EP,
        .faultDeathNotifyBadge = CONSERV_ASYNC_NOTIFY_BADGE | CONSERV_ASYNC_BADGE_MASK
    };

    /* Set up file server common state. */
    srv_common_init(conServCommon, cfg);

    /* Set up irq handler state config. */
    dev_irq_config_t irqConfig = {
        .numIRQChannels = CONSERV_IRQ_BADGE_NCHANNELS,
        .badgeBaseBit = CONSERV_IRQ_BADGE_BASE_POW,
        .badgeTopBit = CONSERV_IRQ_BADGE_BASE_POW_TOP,
        .badgeMaskBits = CONSERV_ASYNC_BADGE_MASK,
        .notifyAsyncEP = conServCommon->notifyAsyncEP,
        .getIRQHandlerEndpoint = conserv_get_irq_handler_endpoint,
        .getIRQHandlerEndpointCookie = NULL
    };

    /* Set up IRQ handler state. */
    dev_irq_init(&conServ.irqState, irqConfig);
    assert(conServ.irqState.magic == DEVICE_IRQ_MAGIC);

    /* Create the badged EP to represent the serial, screen & timer dataspaces. */
    conServ.serialBadgeEP = srv_mint(CONSERV_DSPACE_BADGE_STDIO, conServCommon->anonEP);
    assert(conServ.serialBadgeEP);
    conServ.screenBadgeEP = srv_mint(CONSERV_DSPACE_BADGE_SCREEN, conServCommon->anonEP);
    assert(conServ.screenBadgeEP);

    /* Set up keyboard device. */
    #if defined(PLAT_PC99) && defined(CONFIG_REFOS_ENABLE_KEYBOARD)
    ps_chardevice_t *devKeyboardRet;
    dprintf("ps_cdev_init keyboard...\n");
    devKeyboardRet = ps_cdev_init(PC99_KEYBOARD_PS2, &conServ.devIO.opsIO, &conServ.devKeyboard);
    if (!devKeyboardRet || devKeyboardRet != &conServ.devKeyboard) {
        ROS_ERROR("Console Server could not initialise keyboard device.");
        assert(!"conserv_init failed.");
        exit(1);
    }
    conServ.keyboardEnabled = true;
    #endif

    /* Set up input device. */
    input_init(&conServ.devInput);

    /* Set up screen device. */
    device_screen_init(&conServ.devScreen, &conServ.devIO);

    dprintf("conserv_init over...\n");
}
