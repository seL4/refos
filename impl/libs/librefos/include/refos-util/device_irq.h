/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_UTIL_DEVICE_IRQ_HANDLER_HELPER_LIBRARY_H_
#define _REFOS_UTIL_DEVICE_IRQ_HANDLER_HELPER_LIBRARY_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <refos/refos.h>
#include <refos-util/serv_common.h>
#include <refos-util/serv_connect.h>
#include <data_struct/cvector.h>

#define DEVICE_MAX_IRQ 256
#define DEVICE_IRQ_BADGE_MAX_CHANNELS 28
#define DEVICE_IRQ_MAGIC 0x6220E130

/*! @file
    @brief Device IRQ handling library.

    Unfortunately seL4 async device IRQ badges are quite complicated.

    Firstly, a badgeMaskBits mask is used to distinguish between device IRQ messages and messages of
    other type that the server may recieve, so endpoint sharing is supported. If the server does not
    share the device IRQ endpoint with anything else then simply set this badgeMaskBits to 0.
    In the example below, bit 19 has been used as the device IRQ badge flag.

    Then, out of the remaining available bits, a bit range is used to indicate an IRQ happening.
    In the example below, bits 1 - 18 has been used as the range. Every IRQ that the server needs
    to be able to handle is allocated a bit. If the server needs to handle more than 18 different
    IRQ then channel conflict occurs and we reuse an allocated channel, resulting in both IRQ
    handlers getting called, spurious IRQs. This is a limitation of a single endpoint IRQ reciever
    design.
    
    IRQ badges:
                        ◀――― lower bits               higher bits ―――▶
                        0    0 1 0 0 0 0 1 1 0 0 0 0 0 1 0 1 0 0     1
                             ⎣_________________________________⎦     ▲ badgeMaskBits = 0x80000
                             ▲               ▲                 ▲
                       badgeBaseBit   Bitmask indicating   badgeTopBit
                                      which IRQ channels
                                          happened.

                        numIRQChannels = badgeTopBit - badgeBaseBit - 1

*/

typedef void (*dev_irq_callback_fn_t)(void *cookie, uint32_t irq);

/*! @brief IRQ handler struct. */
typedef struct dev_irq_handler {
    seL4_CPtr handler;
    dev_irq_callback_fn_t callback;
    void *cookie;
} dev_irq_handler_t;

/*! @brief IRQ state configuration struct.
*/
typedef struct dev_irq_config {
    int numIRQChannels;
    int badgeBaseBit;
    int badgeTopBit;
    uint32_t badgeMaskBits;

    seL4_CPtr notifyAsyncEP;
    seL4_CPtr (*getIRQHandlerEndpoint)(void *cookie, int irq);
    void *getIRQHandlerEndpointCookie;
} dev_irq_config_t;

/*! @brief IRQ handler state struct. */
typedef struct dev_irq_state {
    int magic;
    dev_irq_config_t cfg;
    dev_irq_handler_t handler[DEVICE_MAX_IRQ];
    cvector_t channel[DEVICE_IRQ_BADGE_MAX_CHANNELS];
    uint32_t nextIRQChannel;
} dev_irq_state_t;

/*! @brief Initialise IRQ handler helper library state.
    @param irqState The IRQ state struct.
    @param config The config struct.
*/
void dev_irq_init(dev_irq_state_t *irqState, dev_irq_config_t config);

/*! @brief Helper function to set up a callback function to the given IRQ number.
           Each IRQ number by only have one callback set.
    @param irqState The IRQ state struct.
    @param irq The IRQ number to set callback for.
    @param callback The callback function to set.
    @param cookie The cookie that will be passed onto the callback function.
    @return ESUCCESS if success, refos_err_t otherwise.
*/
int dev_handle_irq(dev_irq_state_t *irqState, uint32_t irq,
                   dev_irq_callback_fn_t callback, void *cookie);

/*! @brief Dispatch a device interrupt message.
    @param irqState The IRQ state struct.
    @param m The recieved interrupt message.
    @return DISPATCH_SUCCESS if successfully dispatched, DISPATCH_ERROR if there was an unexpected
            error, DISPATCH_PASS if the given message is not an interrupt message.
*/
int dev_dispatch_interrupt(dev_irq_state_t *irqState, srv_msg_t *m);

#endif /* _REFOS_UTIL_DEVICE_IRQ_HANDLER_HELPER_LIBRARY_H_ */
