/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CONSOLE_SERVER_DEVICE_INPUT_H_
#define _CONSOLE_SERVER_DEVICE_INPUT_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sel4/sel4.h>
#include <data_struct/cvector.h>
#include <data_struct/cqueue.h>

/*! @file
    @brief Console Server input device implementation. */

#define CONSERV_DEVICE_INPUT_MAGIC 0x54F1A770
#define CONSERV_DEVICE_INPUT_BACKLOG_MAXSIZE 2
#define CONSERV_DEVICE_INPUT_WAITER_MAGIC 0x341A8321

#define INPUT_WAITERTYPE_GETC 0x0
#define INPUT_WAITERTYPE_READ 0x1

struct srv_client;

struct input_waiter {
    uint32_t magic;
    seL4_CPtr reply;
    struct srv_client *client; /*!< No ownership, Weak Reference. */
    bool type; /*!< Whether getc or read. */
};

struct input_state {
    uint32_t magic;
    cqueue_t inputBacklog; /*!< char */
    cvector_t waiterList; /*!< input_waiter */
};

/*! @brief Initialise input state manager and waiter list.
    @param s The input state structure. (No ownership transfer)
*/
void input_init(struct input_state *s);

/*! @brief Read from the input device backlog.
    @param s The input state structure. (No ownership transfer)
    @param dest Output buffer which the inputted chars will be written to. (No ownership transfer)
    @param count Maximum output buffer length in bytes.
    @return Number of characters read successfully. 0 means no characters to be read, and if this is
            a blocking syscall, need to use input_save_caller_as_waiter() to block the calling
            client.
*/
int input_read(struct input_state *s, int *dest, uint32_t count);

/*! @brief Block current calling client and save its reply cap for when there is input available.
    @param s The input state structure. (No ownership transfer)
    @param c The client to be blocked. (No ownership transfer)
    @param type The syscall type. Currently must be INPUT_WAITERTYPE_GETC.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int input_save_caller_as_waiter(struct input_state *s, struct srv_client *c, bool type);

/*! @brief Purge all weak references to client form waiting list. Used when client dies.
    @param client The dying client to be purged.
*/
void input_purge_client(struct srv_client *client);

#endif /* _CONSOLE_SERVER_DEVICE_INPUT_H_ */