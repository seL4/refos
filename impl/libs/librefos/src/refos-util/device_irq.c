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
#include <string.h>
#include <assert.h>
#include <refos-util/device_irq.h>
#include <refos-util/dprintf.h>
#include <refos-util/serv_common.h>

/*! @file
    @brief Device IRQ handling library. */

void dev_irq_init(dev_irq_state_t *irqState, dev_irq_config_t config)
{
    assert(irqState);
    memset(irqState, 0, sizeof(dev_irq_state_t));
    dprintf("Initialising IRQ handler state...\n");

    if (config.numIRQChannels <= 0 || config.numIRQChannels >= DEVICE_IRQ_BADGE_MAX_CHANNELS) {
        ROS_ERROR("config.numIRQChannels is invalid.\n");
        ROS_ERROR("IRQ handler init failed.\n");
        return;
    }
    if (!config.getIRQHandlerEndpoint) {
        ROS_ERROR("Please provide a callback function for minting a IRQ handler cap.\n");
        ROS_ERROR("IRQ handler init failed.\n");
        return;
    }
    if (!config.notifyAsyncEP) {
        ROS_ERROR("Please provide an async endpoint to mint IRQ badges from.\n");
        ROS_ERROR("IRQ handler init failed.\n");
        return;
    }

    /* Initialise IRQ channel table. */
    for (int i = 0; i < config.numIRQChannels; i++) {
        cvector_init(&irqState->channel[i]);
    }

    irqState->cfg = config;
    irqState->magic = DEVICE_IRQ_MAGIC;
}

int dev_handle_irq(dev_irq_state_t *irqState, uint32_t irq,
                   dev_irq_callback_fn_t callback, void *cookie)
{
    assert(irqState && irqState->magic == DEVICE_IRQ_MAGIC);

    if (irq >= DEVICE_MAX_IRQ) {
        ROS_ERROR("dev_handle_irq IRQ num too high. Try raising DEVICE_MAX_IRQ.");
        assert(!"Try raising DEVICE_MAX_IRQ.");
        return EINVALIDPARAM;
    }

    /* Retrieve the handler, if necessary. */
    if (!irqState->handler[irq].handler) {
        assert(irqState->cfg.getIRQHandlerEndpoint);
        irqState->handler[irq].handler = irqState->cfg.getIRQHandlerEndpoint(
            irqState->cfg.getIRQHandlerEndpointCookie, irq
        );
        if (!irqState->handler[irq].handler) {
            ROS_WARNING("dev_handle_irq : could not get IRQ handler for irq %u.\n", irq);
            return EINVALID;
        }
    }

    /* Determine next round-robin channel to go in. */
    uint32_t nextChannel = irqState->cfg.badgeBaseBit + irqState->nextIRQChannel;
    cvector_add(&irqState->channel[irqState->nextIRQChannel], (cvector_item_t) irq);
    irqState->nextIRQChannel = (irqState->nextIRQChannel + 1) % irqState->cfg.numIRQChannels;

    /* Mint the badged AEP. */
    assert(irqState->cfg.notifyAsyncEP);
    seL4_CPtr irqBadge = srv_mint((1 << nextChannel) | irqState->cfg.badgeMaskBits,
                                  irqState->cfg.notifyAsyncEP);
    if (!irqBadge) {
        ROS_WARNING("dev_handle_irq : could not mint badged aep for irq %u.\n", irq);
        return EINVALID;
    }

    /* Assign AEP to the IRQ handler. */
    int error = seL4_IRQHandler_SetNotification(irqState->handler[irq].handler, irqBadge);
    if (error) {
        ROS_WARNING("dev_handle_irq : could not set notify aep for irq %u.\n", irq);
        csfree_delete(irqBadge);
        return EINVALID;
    }
    seL4_IRQHandler_Ack(irqState->handler[irq].handler);

    /* Set callback function and cookie. */
    irqState->handler[irq].callback = callback;
    irqState->handler[irq].cookie = cookie;

    csfree_delete(irqBadge);
    return ESUCCESS;
}

int dev_dispatch_interrupt(dev_irq_state_t *irqState, srv_msg_t *m)
{
    assert(irqState && irqState->magic == DEVICE_IRQ_MAGIC);

    if ((m->badge & irqState->cfg.badgeMaskBits) == 0) {
        return DISPATCH_PASS;
    }

    /* Loop through every bit in the bitmask to determine which IRQs fired. */
    bool IRQFound = false;
    for (int i = 0; i <  irqState->cfg.numIRQChannels; i++) {
        if (m->badge & (1 << (irqState->cfg.badgeBaseBit + i))) {
            IRQFound = true;

            /* Go through every IRQ clash. If the server needs to handle a lot of different IRQs
               then this will lead to false positives. This seems to be a kernel API limitation 
               of a single threaded server design. More threads listening to multiple endpoints
               will avoid this problem of IRQ clash.

               Hopefully in practical environments, this won't be a problem since we probably won't
               have a small system that needs a lot of different interrupts.
            */
            int nIRQs = cvector_count(&irqState->channel[i]);
            for (int j = 0; j < nIRQs; j++) {
                uint32_t irq = (uint32_t) cvector_get(&irqState->channel[i], j);
                assert (irq < DEVICE_MAX_IRQ);

                if (irqState->handler[irq].callback) {
                    irqState->handler[irq].callback(irqState->handler[irq].cookie, irq);
                }
                if (irqState->handler[irq].handler) {
                    seL4_IRQHandler_Ack(irqState->handler[irq].handler);
                }
            }

        }
    }

    if (!IRQFound) {
        return DISPATCH_PASS;
    }

    return DISPATCH_SUCCESS;
}
