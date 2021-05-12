/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*! @file
    @brief Common interface for process server.
    
    Common interface a process server server and clients.

    Author: kevine
    Created on: Jul 29, 2012
*/

#ifndef PROC_COMMON_H_
#define PROC_COMMON_H_

#include <assert.h>
#include <refos/refos.h>

#define PROCSERV_NOTIFICATION_MAGIC 0xB0BA11CE

enum proc_notify_types {
    PROCSERV_NOTIFY_FAULT_DELEGATION,
    PROCSERV_NOTIFY_CONTENT_INIT,
    PROCSERV_NOTIFY_DEATH
};

struct proc_notification {
    seL4_Word magic;
    seL4_Word label;
    seL4_Word arg[7];
};

#endif /* PROC_COMMON_H_ */













