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
#include <autoconf.h>
#include <assert.h>
#include <time.h>

#include "device_timer.h"
#include "state.h"
#include <platsupport/timer.h>
#include <platsupport/plat/timer.h>
#include <refos-util/dprintf.h>
#include <refos-util/device_io.h>

/*! @file
    @brief timer server timer device manager.

    This module is responsible for managing the timer state. IT is a thin wrapper on top of the
    timer device drivers provided by libplatsupport.

    When a client wants to sleep for some amount of seconds, we use seL4_CNode_SaveCaller in order
    to save its reply cap and reply to it when on the timer IRQ when its sleep period has expired.
    The timer module sets up frequent periodic timer IRQs in order to do this.

    This approach is relatively inefficient as it results in a lot of IRQs and wasted time
    processing them. A better approach would be to set up one-shot IRQs at the exact time of the
    next waking client, allowing less IRQ overhead and much better sleep accuracy. This may be
    implemented in the near future, but for now spamming IRQs is a simple approach which works.
*/

/* ---------------------- Platform specific timer device definitions ---------------------------- */

#if defined(PLAT_IMX31) || defined (PLAT_IMX6)

    #define TIMER_PADDR GPT1_DEVICE_PADDR
    #define TIMER_PERIODIC_MAX_SET false /* We don't explicitly set period for GPT. */
    #define TIMER_PERIODIC_MAX (((1ULL << 32) / 66UL) * 1000UL)

    #define TICK_TIMER_PADDR EPIT2_DEVICE_PADDR
    #define TICK_TIMER_IRQ EPIT2_INTERRUPT
    #define TICK_TIMER_PERIOD (2000000)
    #define TICK_TIMER_SCALE_NS 1

#elif defined(PLAT_AM335x)

    #define TIMER_ID DMTIMER2
    #define TICK_ID DMTIMER3
    #define TIMER_PERIODIC_MAX_SET true
    /* XXX am I doing this right? this is the largest period in nsec you can pass to the timer APIs */
    #define TIMER_PERIODIC_MAX 178956970666  // ((((1UL << 32) / 24UL) * 1000UL) - 1) 
    #define TICK_TIMER_PERIOD (2000000)
    #define TICK_TIMER_SCALE_NS 1

#elif defined(PLAT_PC99)

    /* PIT only needs IOPort operations. */
    #define TIMER_PERIODIC_MAX_SET true
    #define TIMER_PERIODIC_MAX 54925000

    #define TICK_TIMER_PERIOD (2000000)
    #define TICK_TIMER_SCALE_NS 1

    #include <platsupport/plat/rtc.h>

#else
    #error "Unsupported platform."
#endif

/* ------------------------------------ Timer functions ----------------------------------------- */

/* Forward declarations. We avoid including the whole state.h here due to errno.h definition
   conflicts. */
typedef void (*timeserv_irq_callback_fn_t)(void *cookie, uint32_t irq);
int timeserv_handle_irq(uint32_t irq, timeserv_irq_callback_fn_t callback, void *cookie);
void reply_data_write(void *rpc_userptr, int rpc___ret__);

/*! @brief Look at sleeper list and reply to any sleppers that have had their time requirements
           met.
    @param s The timer global state structure.
*/
static void
device_timer_update_sleepers(struct device_timer_state *s)
{
    uint64_t time = device_timer_get_time(s);

    /* Loop through and find any fired waiters to reply to. */
    int count = cvector_count(&s->waiterList);
    for (int i = 0; i < count; i++) {
        struct device_timer_waiter *waiter = (struct device_timer_waiter*)
                cvector_get(&s->waiterList, i);
        assert(waiter && waiter->magic == TIMESERV_DEVICE_TIMER_WAITER_MAGIC);
        assert(waiter->reply && waiter->client);

        if (waiter->time > time) {
            /* Not yet. */
            continue;
        }

        /* Reply to the waiter. */
        waiter->client->rpcClient.skip_reply = false;
        waiter->client->rpcClient.reply = waiter->reply;
        reply_data_write((void*) waiter->client, sizeof(uint32_t));

        /* Delete the saved reply cap, and free the structure. */
        waiter->client->rpcClient.reply = 0;
        csfree_delete(waiter->reply);
        waiter->magic = 0x0;
        free(waiter);

        /* Remove the item from the waiter list. */
        cvector_delete(&s->waiterList, i);
        count = cvector_count(&s->waiterList);
        i--;
    }
}

/*! @brief Callback function to handle GPT timer IRQs.

    GPT timer IRQs happen on GPT timer overflow. Note that the GPT device (used to get the actual
    time) may actually be the excat same device as the waiting-list IRQ generator device, depending
    on the platform.

    @param cookie The global timer state (struct device_timer_state *).
    @param irq The fired IRQ number.
*/
static void
device_timer_handle_irq(void *cookie, uint32_t irq)
{
    struct device_timer_state *s = (struct device_timer_state *) cookie;
    assert(s && s->magic == TIMESERV_DEVICE_TIMER_MAGIC);
    assert(s->timerIRQPeriod > 0);
    s->cumulativeTime += s->timerIRQPeriod;
    timer_handle_irq(s->timerDev, irq);
    device_timer_update_sleepers(s);
}

/*! @brief Callback function to handle waiter timer IRQs.
    
    Waiter-list IRQs happen very frequently, as opposed to the GPT overflow IRQs. This is used to
    wake up sleeping clients.

    @param cookie The global timer state (struct device_timer_state *).
    @param irq The fired IRQ number.
*/
static void
device_tick_handle_irq(void *cookie, uint32_t irq)
{
    struct device_timer_state *s = (struct device_timer_state *) cookie;
    assert(s && s->magic == TIMESERV_DEVICE_TIMER_MAGIC);
    timer_handle_irq(s->tickDev, irq);
    device_timer_update_sleepers(s);
}

static void
device_timer_init_rtc(struct device_timer_state *s, dev_io_ops_t *io)
{
    assert(s && io);
    s->cumulativeTime = 0;

    #if defined(PLAT_PC99)
    
    rtc_time_date_t rtcTimeDate;
    int error = rtc_get_time_date_reg(&io->opsIO.io_port_ops, 0, &rtcTimeDate);
    if (error) {
        ROS_ERROR("Could not read RTC.");
        return;
    }

    struct tm timeInfo = {0};
    timeInfo.tm_sec = rtcTimeDate.second;
    timeInfo.tm_min = rtcTimeDate.minute;
    timeInfo.tm_hour = rtcTimeDate.hour;
    timeInfo.tm_mday = rtcTimeDate.day;
    timeInfo.tm_mon = rtcTimeDate.month - 1;
    timeInfo.tm_year = rtcTimeDate.year - 1900;

    time_t epochSeconds = mktime(&timeInfo);
    dprintf("Time since epoch is %ld. RTC %s", epochSeconds, ctime(&epochSeconds));
    s->cumulativeTime = (uint64_t) epochSeconds * 1000000000ULL;

    #endif
}

void
device_timer_init(struct device_timer_state *s, dev_io_ops_t *io)
{
    assert(s && io);
    assert(!s->initialised);

    s->magic = TIMESERV_DEVICE_TIMER_MAGIC;
    s->io = io;
    s->timerDev = NULL;

#if defined(PLAT_IMX31) || defined (PLAT_IMX6)

    /* Map the GPT (General Purpose Timer) device to use as the clock. */
    gpt_config_t config;
    config.vaddr = ps_io_map(&io->opsIO.io_mapper, TIMER_PADDR, 0x1000,
                             false, PS_MEM_NORMAL);
    config.prescaler = 0;
    if (!config.vaddr) {
        ROS_ERROR("Could not map timer device.");
        assert(!"Could not map timer device.");
        return;
    }

    /* Get timer interface from given GPT device. */
    s->timerDev = gpt_get_timer(&config);

    /* Map the EPIT device to use as the tick source. */
    epit_config_t econfig;
    econfig.vaddr = ps_io_map(&io->opsIO.io_mapper, TICK_TIMER_PADDR, 0x1000,
                             false, PS_MEM_NORMAL);
    econfig.prescaler = 0;
    econfig.irq = TICK_TIMER_IRQ;
    if (!econfig.vaddr) {
        ROS_ERROR("Could not map tick timer device.");
        assert(!"Could not map tick timer device.");
        return;
    }

    /* Get the timer interface from EPIT device. */
    s->tickDev = epit_get_timer(&econfig);

#elif defined(PLAT_AM335x)

    timer_config_t config;
    config.vaddr = ps_io_map(&io->opsIO.io_mapper, dm_timer_paddrs[TIMER_ID],
                             0x1000, false, PS_MEM_NORMAL);
    config.irq = dm_timer_irqs[TIMER_ID];
    if (!config.vaddr) {
        ROS_ERROR("Could not map timer device.");
        assert(!"Could not map timer device.");
        return;
    }
    s->timerDev = ps_get_timer(TIMER_ID, &config);

    config.vaddr = ps_io_map(&io->opsIO.io_mapper, dm_timer_paddrs[TICK_ID],
                             0x1000, false, PS_MEM_NORMAL);
    config.irq = dm_timer_irqs[TICK_ID];
    if (!config.vaddr) {
        ROS_ERROR("Could not map tick timer device.");
        assert(!"Could not map tick timer device.");
        return;
    }
    s->tickDev = ps_get_timer(TICK_ID, &config);

#elif defined(PLAT_PC99)

    s->timerDev = pit_get_timer(&io->opsIO.io_port_ops);
    s->tickDev = s->timerDev;

#endif

    if (!s->timerDev) {
        ROS_ERROR("Could not initialise timer.");
        return;
    }

    /* Set up to recieve timer IRQs. */
    for (uint32_t i = 0; i < s->timerDev->properties.irqs; i++) {
        int irq = timer_get_nth_irq(s->timerDev, i);
        int error = dev_handle_irq(&timeServ.irqState, irq, device_timer_handle_irq, (void*) s);
        assert(!error);
        (void) error;
    }

    /* Set up to recieve tick timer IRQs. */
    if (s->tickDev != NULL && s->tickDev != s->timerDev) {
        for (uint32_t i = 0; i < s->tickDev->properties.irqs; i++) {
            int irq = timer_get_nth_irq(s->tickDev, i);
            int error = dev_handle_irq(&timeServ.irqState, irq, device_tick_handle_irq, (void*) s);
            assert(!error);
            (void) error;
        }

        int error = timer_start(s->tickDev);
        if (error) {
            ROS_ERROR("Could not start tick timer.");
            assert(!"Tick timer initialise but could not start.");
            return;
        }
    }

    /* Read the RTC. */
    device_timer_init_rtc(s, io);

    /* Start the timer. */
    int error = timer_start(s->timerDev);
    if (error) {
        ROS_ERROR("Could not start timer.");
        assert(!"Timer initialise but could not start.");
        return;
    }

    #if TIMER_PERIODIC_MAX_SET
    /* Set the timer for periodic overflow interrupts. */
    error = timer_periodic(s->timerDev, TIMER_PERIODIC_MAX);
    if (error) {
        ROS_ERROR("Could not configure periodic timer.");
        assert(!"Could not set periodic timer.");
        return;
    }
    #endif
    s->timerIRQPeriod = TIMER_PERIODIC_MAX;

    /* Set the tick timer for realy fast periodic ticks. Rather crude way of implementing sleep()
       functionality but works. In the future this should be improved
       (see module description above).
    */
    if (s->tickDev != NULL) {
        error = timer_periodic(s->tickDev, TICK_TIMER_PERIOD);
        if (error) {
            ROS_WARNING("Could not set periodic tick timer.");
            assert(!"Could not set periodic tick timer.");
        }
        if (s->tickDev == s->timerDev) {
            s->timerIRQPeriod = TICK_TIMER_PERIOD;
        }
    }

    /* Initialise the sleep timer waiter list. */
    cvector_init(&s->waiterList);

    s->initialised = true;
}

uint64_t
device_timer_get_time(struct device_timer_state *s)
{
    assert(s && s->magic == TIMESERV_DEVICE_TIMER_MAGIC);
    uint64_t time = timer_get_time(s->timerDev);
    if (!s->timerDev->properties.upcounter) {
        /* This is a downcounter timer. Invert the time. */
        assert(time <= s->timerIRQPeriod);
        time = s->timerIRQPeriod - time;
    }
    time += s->cumulativeTime;
    return time * TICK_TIMER_SCALE_NS;
}

int
device_timer_save_caller_as_waiter(struct device_timer_state *s, struct srv_client *c,
                                   uint64_t waitTime)
{
    assert(s && s->magic == TIMESERV_DEVICE_TIMER_MAGIC);
    assert(c && c->magic == TIMESERV_CLIENT_MAGIC);
    int error = EINVALID;

    /* Allocate and fill out waiter structure. */
    struct device_timer_waiter *waiter = malloc(sizeof(struct device_timer_waiter));
    if (!waiter) {
        ROS_ERROR("device_timer_save_caller_as_waiter failed to alloc waiter struct.");
        return ENOMEM;
    }
    waiter->magic = TIMESERV_DEVICE_TIMER_WAITER_MAGIC;
    waiter->client = c;
    waiter->time = (waitTime / TICK_TIMER_SCALE_NS) + device_timer_get_time(s);

    /* Allocate a cslot to save the reply cap into. */
    waiter->reply = csalloc();
    if (!waiter->reply) {
        ROS_ERROR("device_timer_save_caller_as_waiter failed to alloc cslot.");
        error = ENOMEM;
        goto exit1;
    }

    /* Save current caller into the reply cap. */
    error = seL4_CNode_SaveCaller(REFOS_CSPACE, waiter->reply, REFOS_CDEPTH);
    if (error != seL4_NoError) {
        ROS_ERROR("device_timer_save_caller_as_waiter failed to save caller.");
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
device_timer_purge_client(struct device_timer_state *client)
{
    assert(!"not implemented.");
}
