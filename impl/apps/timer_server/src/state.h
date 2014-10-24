/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _TIMER_SERVER_STATE_H_
#define _TIMER_SERVER_STATE_H_

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
#include "device_timer.h"
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
    @brief timer server global state & helper functions. */

// Debug printing.
#include <refos-util/dprintf.h>

#ifndef CONFIG_REFOS_DEBUG
    #define printf(x,...)
#endif /* CONFIG_REFOS_DEBUG */

#define TIMESERV_MMAP_REGION_SIZE 0x64000
#define TIMESERV_MOUNTPOINT "dev_timer"
#define TIMESERV_CLIENT_MAGIC 0xEE340912

/*! @brief timer server global state. */
struct timeserv_state {
    srv_common_t commonState;
    dev_irq_state_t irqState;

    dev_io_ops_t devIO;
    struct device_timer_state devTimer;
    seL4_CPtr timerBadgeEP;
};

extern struct timeserv_state timeServ;
extern srv_common_t *timeServCommon;

/*! @brief Initialise timer server state. */
void timeserv_init(void);

#endif /* _TIMER_SERVER_STATE_H_ */