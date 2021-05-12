/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
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

/*! @brief Console server's system call table. */
extern uintptr_t __vsyscall_ptr;

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
    /* Future Work 4:
       Eventually RefOS should be changed so that processes that are started
       by the process server do not require that the their system call table be
       explicitly referenced in the code like this. Without expliciting referencing
       __vsyscall_ptr in main(), the compiler optimizes away __vsyscall_ptr
       and then processes started by the process server can't find their system call
       table. Each of the four places in RefOS where this explicit reference is
       required is affected by a custom linker script (linker.lds), so it is possible
       that the custom linker script (and then likely other things too) needs to be
       modified. Also note that the ROS_ERROR() and assert() inside this if statement
       would not actually be able to execute if __vsyscall_ptr() were ever not set.
       The purpose of these calls to ROS_ERROR() and assert() is to show future
       developers that __vsyscall_ptr needs to be defined.
    */
    if (! __vsyscall_ptr) {
        ROS_ERROR("Console server could not find system call table.");
        assert("!Console server could not find system call table.");
        return 0;
    }

    dprintf("Initialising RefOS Console server.\n");
    refosio_setup_morecore_override(conServMMapRegion, CONSERV_MMAP_REGION_SIZE);
    refos_initialise_os_minimal();
    conserv_init();

    console_server_mainloop();

    return 0;
}
