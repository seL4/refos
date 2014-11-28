/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <autoconf.h>

#include "device_input.h"
#include "state.h"

#include <refos-util/serv_connect.h>
#include <refos-rpc/data_server.h>

/*! @file
    @brief Console Server input device implementation.

    Most of the actual input drivers work is fortunately done by the libplatsupport library. This
    module simply provides a client waiting list layer on top of getchar, managing client getc() and
    read() syscalls into stdin.

    When a character is typed but there is no one to listen, it goes into the backlog, buffered for
    the next read() or getc() call. If the backlog is full, the oldest character is lost.

    When a client wants to getc() with blocking enabled and there isn't a character already waiting
    to be recieved, the Console server must block the calling client until an RX irq comes in from the
    input device. This is implemented using seL4_SaveCaller functionality, saving the reply endpoint
    capability into a 'waiting list'. Whenever we recieve an IRQ, we go through this waiting list
    and reply to any waiters, unblocking their syscall.
*/

/*! @brief Add a new character onto the getchar queue.
    @param s The input state structure (struct input_state*)
    @param c The new character to push.
*/
static void
input_push_char(struct input_state *s, int c)
{
    /* If backlog is too big, prune it. */
    while (cqueue_size(&s->inputBacklog) >= CONSERV_DEVICE_INPUT_BACKLOG_MAXSIZE) {
        cqueue_item_t item = cqueue_pop(&s->inputBacklog);
        (void) item;
    }

    /* Push new character onto the queue. */
    bool success = cqueue_push(&s->inputBacklog, (cqueue_item_t) c);
    if (!success) {
        ROS_ERROR("input backlog queue should not be full. conserv book-keeping error.");
    }
}

/*! @brief The IRQ handling callback function.
   
    This callback function gets called from the interrupt dispatcher module to handle RX irqs.
    It adds the inputted character to the backlog, and then goes through the waiting list and
    replies to any waiters.

    @param cookie The input state structure (struct input_state*)
    @param irq The IRQ number.
*/
static void
input_handle_irq(void *cookie, uint32_t irq)
{
    struct input_state *s = (struct input_state *) cookie;
    assert(s && s->magic == CONSERV_DEVICE_INPUT_MAGIC);
    ps_cdev_handle_irq(&conServ.devSerial, irq);

    while (1) {
        int c = ps_cdev_getchar(&conServ.devSerial);
        if (c == -1) {
            break;
        }
        dvprintf("You typed [%c]\n", c);
        input_push_char(s, c);
    }

    #ifdef PLAT_PC99
    while (conServ.keyboardEnabled) {
        int c = ps_cdev_getchar(&conServ.devKeyboard);
        if (c == -1) {
            break;
        }
        dvprintf("You typed on keyboard [%c]\n", c);
        input_push_char(s, c);
    }
    #endif

    /* Notify any waiters. */
    for (int i = 0; i < cvector_count(&s->waiterList); i++) {
        struct input_waiter *waiter = (struct input_waiter*) cvector_get(&s->waiterList, i);
        assert(waiter && waiter->magic == CONSERV_DEVICE_INPUT_WAITER_MAGIC);
        assert(waiter->reply && waiter->client);

        if (cqueue_size(&s->inputBacklog) <= 0) {
            /* No more backlog to reply to. Cannot reply to more waiters. */
            break;
        }

        waiter->client->rpcClient.skip_reply = false;
        waiter->client->rpcClient.reply = waiter->reply;

        /* Reply to the waiter. */
        if (waiter->type == INPUT_WAITERTYPE_GETC) {
            int ch = (int) cqueue_pop(&s->inputBacklog);
            reply_data_getc((void*) waiter->client, ch);
        } else {
            assert(!"Not implemented.");
        }

        /* Delete the saved reply cap, and free the structure. */
        waiter->client->rpcClient.reply = 0;
        csfree_delete(waiter->reply);
        waiter->magic = 0x0;
        free(waiter);
        cvector_set(&s->waiterList, i, (cvector_item_t) NULL);
        cvector_delete(&s->waiterList, i);
        i--;
    }
}

void
input_init(struct input_state *s)
{
    assert(s);
    s->magic = CONSERV_DEVICE_INPUT_MAGIC;

    /* Initialise the input backlog and waiting list. */
    cqueue_init(&s->inputBacklog, CONSERV_DEVICE_INPUT_BACKLOG_MAXSIZE);
    cvector_init(&s->waiterList);

    /* Loop through every possible IRQ, and get the ones that the input device needs to
       listen to. */
    for (uint32_t i = 0; i < DEVICE_MAX_IRQ; i++) {
        if (ps_cdev_produces_irq(&conServ.devSerial, i)) {
            dev_handle_irq(&conServ.irqState, i, input_handle_irq, (void*) s);
            input_handle_irq((void*) s, i);
        }
        #ifdef PLAT_PC99
        if (conServ.keyboardEnabled) {
            if (ps_cdev_produces_irq(&conServ.devKeyboard, i)) {
                dev_handle_irq(&conServ.irqState, i, input_handle_irq, (void*) s);
                input_handle_irq((void*) s, i);
            }
        }
        #endif
    }
}

int
input_read(struct input_state *s, int *dest, uint32_t count)
{
    assert(s && s->magic == CONSERV_DEVICE_INPUT_MAGIC);
    if (!dest || count == 0) {
        return 0;
    }

    if (cqueue_size(&s->inputBacklog) == 0) {
        /* Nothing in the backlog. We're going to have to block. */
        return 0;
    }

    /* Read in from backlog. */
    int i = 0;
    while (cqueue_size(&s->inputBacklog) > 0) {
        int c = (int) cqueue_pop(&s->inputBacklog);
        dest[i++] = c;
        if (i >= count) {
            break;
        }
    }

    return i;
}

int
input_save_caller_as_waiter(struct input_state *s, struct srv_client *c, bool type)
{
    assert(s && s->magic == CONSERV_DEVICE_INPUT_MAGIC);
    assert(c && c->magic == CONSERV_CLIENT_MAGIC);
    int error = EINVALID;

    /* Allocate and fill out waiter structure. */
    struct input_waiter *waiter = malloc(sizeof(struct input_waiter));
    if (!waiter) {
        ROS_ERROR("input_save_caller_as_waiter failed to alloc waiter struct.");
        return ENOMEM;
    }
    waiter->magic = CONSERV_DEVICE_INPUT_WAITER_MAGIC;
    waiter->client = c;
    waiter->type = type;

    /* Allocate a cslot to save the reply cap into. */
    waiter->reply = csalloc();
    if (!waiter->reply) {
        ROS_ERROR("input_save_caller_as_waiter failed to alloc cslot.");
        error = ENOMEM;
        goto exit1;
    }

    /* Save current caller into the reply cap. */
    error = seL4_CNode_SaveCaller(REFOS_CSPACE, waiter->reply, REFOS_CDEPTH);
    if (error != seL4_NoError) {
        ROS_ERROR("input_save_caller_as_waiter failed to save caller.");
        error = EINVALID;
        goto exit2;
    }

    /* Add to waiter list. (Takes ownership) */
    cvector_add(&s->waiterList, (cvector_item_t) waiter);

    return ESUCCESS;

    /* Exit stack. */
exit2:
    csfree(waiter->reply);
exit1:
    waiter->magic = 0;
    free(waiter);
    return error;
}

void
input_purge_client(struct srv_client *client)
{
    assert(!"not implemented.");
}
