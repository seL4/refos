/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _TIMER_SERVER_DEVICE_TIMER_H_
#define _TIMER_SERVER_DEVICE_TIMER_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sel4/sel4.h>
#include <data_struct/cvector.h>
#include <refos-util/device_io.h>
#include <platsupport/timer.h>
#include <platsupport/plat/timer.h>

/*! @file
    @brief timer server timer device manager. */

#define TIMESERV_DEVICE_TIMER_MAGIC 0x54F1A770
#define TIMESERV_DEVICE_TIMER_WAITER_MAGIC 0x2F4401A9

/*! @brief Timer device waiter structure. */
struct device_timer_waiter {
    uint32_t magic;
    uint64_t time;
    seL4_CPtr reply;
    struct srv_client *client; /* No ownership. */
};

/*! @brief Timer device state structure. */
struct device_timer_state {
    uint32_t magic;
    dev_io_ops_t *io; /* No ownership, weak reference. */
    bool initialised;

    /*! This should point to a timer device. */
    pstimer_t *timerDev; /* No ownership. Weak ref to static. */

    /*! This should point to a timer device that is capable of generating periodic interrupts.
        Note that this may point to the exact same device as timerDev. */
    pstimer_t *tickDev; /* No ownership. Weak ref to static. */

    cvector_t waiterList; /* struct device_timer_waiter */
    uint64_t cumulativeTime; /*!< Current cumulative time. */
    uint64_t timerIRQPeriod;
};

/*! @brief Initialies the timer device management module.
    @param s The global timer device state structure (No ownership).
    @param io The initialised device IO manager. (No ownership).
*/
void device_timer_init(struct device_timer_state *s, dev_io_ops_t *io);

/*! @brief Get the current time since epoch in nanoseconds.
    @param s The global timer device state structure (No ownership).
    @return The current time in nanoseconds.
*/
uint64_t device_timer_get_time(struct device_timer_state *s);

/*! @brief Save the current caller client's reply cap, and reply to it when its sleep time has
           passed.
    @param s The global timer device state structure (No ownership).
    @param c The client structure of the waiting client.
    @param waitTime The amount of time in nanoseconds that the client wishes to wait, relative to
                    now.
    @return ESUCCESS if success, refos_err_t otherwise.
*/
int device_timer_save_caller_as_waiter(struct device_timer_state *s, struct srv_client *c,
        uint64_t waitTime);

/*! @brief Purge all weak references to client form waiting list. Used when client dies.
    @param client The dying client to be purged.
*/
void device_timer_purge_client(struct device_timer_state *client);

#endif /* _TIMER_SERVER_DEVICE_TIMER_H_ */