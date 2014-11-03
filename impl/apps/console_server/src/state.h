/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CONSOLE_SERVER_STATE_H_
#define _CONSOLE_SERVER_STATE_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <refos/vmlayout.h>
#include <refos-rpc/rpc.h>
#include <syscall_stubs_sel4.h>

#include <platsupport/chardev.h>
#include <platsupport/serial.h>
#include "device_input.h"
#include "device_screen.h"
#include "badge.h"

#include <data_struct/cvector.h>
#include <refos-util/serv_connect.h>
#include <refos-util/serv_common.h>
#include <refos-util/cspace.h>
#include <refos-util/device_io.h>
#include <refos-util/device_irq.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>
#include <refos/refos.h>

/*! @file
    @brief Console Server global state & helper functions. */

// Debug printing.
#include <refos-util/dprintf.h>

#ifndef CONFIG_REFOS_DEBUG
    #define printf(x,...)
#endif /* CONFIG_REFOS_DEBUG */

#define CONSERV_MMAP_REGION_SIZE 0x64000
#define CONSERV_MOUNTPOINT "dev_console"
#define CONSERV_CLIENT_MAGIC 0x12F5AA90

typedef void (*conserv_irq_callback_fn_t)(void *cookie, uint32_t irq);

/*! @brief Console Server IRQ callback state handler. */
struct conserv_irq_handler {
    seL4_CPtr handler;
    conserv_irq_callback_fn_t callback;
    void *cookie;
};

/*! @brief Console Server global state. */
struct conserv_state {
    srv_common_t commonState;
    dev_irq_state_t irqState;

    /* Main console server data structures. */
    dev_io_ops_t devIO;
    ps_chardevice_t devSerial;
    struct input_state devInput;
    struct device_screen_state devScreen;

    #ifdef PLAT_PC99
    ps_chardevice_t devKeyboard;
    bool keyboardEnabled;
    #endif

    seL4_CPtr serialBadgeEP;
    seL4_CPtr screenBadgeEP;
};

extern struct conserv_state conServ;
extern srv_common_t *conServCommon;

/*! @brief Initialise Console server state. */
void conserv_init(void);

#endif /* _CONSOLE_SERVER_STATE_H_ */