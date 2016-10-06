/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */

#include <stdio.h>
#include <assert.h>
#include <refos/refos.h>
#include <refos-util/init.h>
#include <refos-io/morecore.h>
#include "state.h"
#include "dispatchers/dspace/dspace.h"
#include "dispatchers/serv_dispatch.h"
#include "dispatchers/client_watch.h"

/*! @file
    @brief Timer Server main source file.

    The RefOS Timer server acts as the timer driver, and exposes a timer dataspace, which implements
    gettime and sleep functionality.

    @image html timeserv.png

    The Timer server:
    <ul>
        <li>Does NOT support providing pager service to clients.</li>
        <li>Does NOT support providing content-initialisation for another dataserver.</li>
        <li>Does NOT supports parameter buffers using an external dataspace (i.e. a dataspace
            provided by another dataserver).</li>
        <li>Does NOT support parameter buffers using an internal dataspace (i.e. a dataspace
            provided the fileserver itself).</li>
        <li>Does NOT support having its dataspace content-initalised by an external dataspace.</li>
        <li>Ignores nBytes parameter in open() method (timer dspace doesn't have a file size).</li>
    </ul>

    The timer server provides the dataspace `/dev_timer/timer`.
    <ul>
        <li>Writing a uint64_t to `/dev/timer` results a sleep() call for that many ns.</li>
        <li>Reading a uint64_t from `/dev/timer` results in getting the current time in ns.</li>
    </ul>
*/

/*! @brief timer server's static morecore region. */
static char timeServMMapRegion[TIMESERV_MMAP_REGION_SIZE];

/*! @brief Handle messages recieved by the timer server.
    @param s The global timer server state. (No ownership transfer)
    @param msg The recieved message. (No ownership transfer)
    @return DISPATCH_SUCCESS if message dispatched, DISPATCH_ERROR if unknown message.
*/
static int
timer_server_handle_message(struct timeserv_state *s, srv_msg_t *msg)
{
    int result = DISPATCH_PASS;
    int label = seL4_GetMR(0);
    void *userptr;

    if (dispatch_client_watch(msg) == DISPATCH_SUCCESS) {
        result = DISPATCH_SUCCESS;
    }

    if (dev_dispatch_interrupt(&timeServ.irqState, msg) == DISPATCH_SUCCESS) {
        result = DISPATCH_SUCCESS;
    }

    if (result == DISPATCH_SUCCESS) {
        return result;
    }

    if (check_dispatch_data(msg, &userptr) == DISPATCH_SUCCESS) {
        result = rpc_sv_data_dispatcher(userptr, label);
        assert(result == DISPATCH_SUCCESS);
        return DISPATCH_SUCCESS;
    }

    if (check_dispatch_serv(msg, &userptr) == DISPATCH_SUCCESS) {
        result = rpc_sv_serv_dispatcher(userptr, label);
        assert(result == DISPATCH_SUCCESS);
        return DISPATCH_SUCCESS;
    }

    dprintf("Unknown message (badge = %d msgInfo = %d label = %d).\n",
            msg->badge, seL4_MessageInfo_get_label(msg->message), label);
    ROS_ERROR("timer server unknown message.");
    assert(!"timer server unknown message.");

    return DISPATCH_ERROR;
}

/*! @brief Main timer server message loop. Simply loops through recieving and dispatching messages
           repeatedly. */
static void
timer_server_mainloop(void)
{
    struct timeserv_state *s = &timeServ;
    srv_msg_t msg;

    while (1) {
        msg.message = seL4_Recv(s->commonState.anonEP, &msg.badge);
        timer_server_handle_message(s, &msg);
        client_table_postaction(&s->commonState.clientTable);
    }
}

uint32_t faketime() {
    static uint32_t faketime = 0;
    return faketime++;
}

/*! @brief Main timer server entry point. */
int
main()
{
    /* Future Work 3:
       How the selfloader bootstraps user processes needs to be modified further. Changes were
       made to accomodate the new way that muslc expects process's stacks to be set up when
       processes start, but the one part of this that still needs to changed is how user processes
       find their system call table. Currently the selfloader sets up user processes so that
       the selfloader's system call table is used by user processes by passing the address of the
       selfloader's system call table to the user processes via the user process's environment
       variables. Ideally, user processes would use their own system call table.
    */

    uintptr_t address = strtoll(getenv("SYSTABLE"), NULL, 16);
    refos_init_selfload_child(address);
    dprintf("Initialising RefOS timer server.\n");
    refosio_setup_morecore_override(timeServMMapRegion, TIMESERV_MMAP_REGION_SIZE);
    refos_initialise_timer();
    timeserv_init();

    timer_server_mainloop();

    return 0;
}
