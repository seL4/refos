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
#include <assert.h>
#include <refos/refos.h>
#include <refos-util/init.h>
#include <refos-io/morecore.h>
#include "state.h"
#include "dispatchers/dspace/dspace.h"
#include "dispatchers/dspace/stdio_dspace.h"
#include "dispatchers/serv_dispatch.h"
#include "dispatchers/client_watch.h"

/*! @file
    @brief Console Server main source file.

    The RefOS Console server acts as a simple device driver server for basic system console input /
    output functionality (eg. serial I/O for seeing things on the screen, EGA console output). It is
    booted as a special system app in order for the system to to gain output functionality as early
    as possible in the booting process. The only way to get output functionality before the console
    server is up is to enable debug kernel mode and use seL4_DebugPutChar().

    @image html conserv.png

    The Console server:
    <ul>
        <li>Does NOT support providing pager service to clients.</li>
        <li>Does NOT support providing content-initialisation for another dataserver.</li>
        <li>Does NOT supports parameter buffers using an external dataspace (i.e. a dataspace
            provided by another dataserver).</li>
        <li>Does NOT support parameter buffers using an internal dataspace (i.e. a dataspace
            provided the fileserver itself).</li>
        <li>Does NOT support having its dataspace content-initalised by an external dataspace.</li>
        <li>Ignores nBytes parameter in open() method (device IO doesn't have a file size).</li>
    </ul>

    The Console server provides the dataspaces: `/dev_console/serial` and `/dev/_console/screen`.
    <ul>
        <li>Writing to `/dev_console/serial` results in outputting to serial device.</li>
        <li>Reading from `/dev_console/serial` results in reading from serial device.</li>
        <li>Writing to `/dev_console/screen` results in outputting to EGA screen buffer.</li>
        <li>Reading from `/dev_console/screen` is unsupported.</li>
    </ul>
*/

/*! @brief Console server's static morecore region. */
static char conServMMapRegion[CONSERV_MMAP_REGION_SIZE];

/*! @brief Handle messages recieved by the Console server.
    @param s The global Console server state. (No ownership transfer)
    @param msg The recieved message. (No ownership transfer)
    @return DISPATCH_SUCCESS if message dispatched, DISPATCH_ERROR if unknown message.
*/
static int
console_server_handle_message(struct conserv_state *s, srv_msg_t *msg)
{
    int result = DISPATCH_PASS;
    int label = seL4_GetMR(0);
    void *userptr;

    if (dispatch_client_watch(msg) == DISPATCH_SUCCESS) {
        result = DISPATCH_SUCCESS;
    }

    if (dev_dispatch_interrupt(&s->irqState, msg) == DISPATCH_SUCCESS) {
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
    ROS_ERROR("Console server unknown message.");
    assert("!Console server unknown message.");

    return DISPATCH_ERROR;
}

/*! @brief Main console server message loop. Simply loops through recieving and dispatching messages
           repeatedly. */
static void
console_server_mainloop(void)
{
    struct conserv_state *s = &conServ;
    srv_msg_t msg;

    while (1) {
        msg.message = seL4_Recv(conServCommon->anonEP, &msg.badge);
        console_server_handle_message(s, &msg);
        client_table_postaction(&conServCommon->clientTable);
    }
}

uint32_t faketime() {
    static uint32_t faketime = 0;
    return faketime++;
}

/*! @brief Main Console server entry point. */
int
main(void)
{
    SET_MUSLC_SYSCALL_TABLE;
    dprintf("Initialising RefOS Console server.\n");
    refosio_setup_morecore_override(conServMMapRegion, CONSERV_MMAP_REGION_SIZE);
    refos_initialise_os_minimal();
    conserv_init();

    console_server_mainloop();

    return 0;
}
