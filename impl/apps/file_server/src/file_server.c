/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*! @file
    @brief CPIO File Server main source file.
    
    The main role of the fileserver in the system is to provide the executable file contents via
    the dataspace interface. It does some book-keeping and simple management of clients.

    @image html fileserv.png
    
    The CPIO file server:
    <ul>
        <li>Supports providing pager service to clients.</li>
        <li>Supports providing content-initialisation for another dataserver.</li>
        <li>Supports parameter buffers using an external dataspace (i.e. a dataspace provided by
            another dataserver).</li>
        <li>Does NOT support parameter buffers using an internal dataspace (i.e. a dataspace
            provided the fileserver itself).</li>
        <li>Does NOT support having its dataspace content-initalised by an external dataspace.</li>
        <li>Ignores nBytes parameter in open() method (actual CPIO file size is used).</li>
    </ul>

    The CPIO file server provides access to files stored in a CPIO archive inside the server.
    The CPIO archive is a simple format file archive stored inside a parent program's ELF section.
    This is a similar idea to something like creating a
    > const char data[] = { 0x3F, 0xFF, 0x23 ...etc}

    The CPIO file server is used to store the RefOS userland app ELF binaries, and any additional
    data files. It provides a simple example of a dataserver, and may serve as the means to boot
    more complex dataspace servers involving flash memory / SATA drivers and so forth.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <cpio/cpio.h>

#include <refos/refos.h>
#include <refos-io/morecore.h>
#include <refos-io/stdio.h>
#include <refos-util/init.h>
#include <refos-util/serv_connect.h>

#include "state.h"
#include "dispatchers/dispatch.h"
#include "dispatchers/serv_dispatch.h"
#include "dispatchers/cpio_dspace.h"
#include "dispatchers/fault_notify.h"

#define MMAP_SIZE 0x200000 /*!< 32MB.. */
static char mmapRegion[MMAP_SIZE];

/*! @brief Fake time generator used by dprintf(). */
uint32_t faketime() {
    static uint32_t _faketime = 0;
    return _faketime++;
}

/*! @brief Handle messages received by the CPIO file server.
    @param s The global file server state. (No ownership transfer)
    @param msg The received message. (No ownership transfer)
    @return DISPATCH_SUCCESS if message dispatched, DISPATCH_ERROR if unknown message.
*/
static int
fileserv_handle_message(struct fs_state *s, srv_msg_t *msg)
{
    int result;
    int label = seL4_GetMR(0);
    void *userptr;
    (void) result;

    if (dispatch_notification(msg) == DISPATCH_SUCCESS) {
        return DISPATCH_SUCCESS;
    }

    if (check_dispatch_serv(msg, &userptr) == DISPATCH_SUCCESS) {
        result = rpc_sv_serv_dispatcher(userptr, label);
        assert(result == DISPATCH_SUCCESS);
        return DISPATCH_SUCCESS;
    }

    if (check_dispatch_data(msg, &userptr) == DISPATCH_SUCCESS) {
        result = rpc_sv_data_dispatcher(userptr, label);
        assert(result == DISPATCH_SUCCESS);
        return DISPATCH_SUCCESS;
    }

    dprintf("Unknown message (badge = %d msgInfo = %d label = %d (0x%x)).\n",
            msg->badge, seL4_MessageInfo_get_label(msg->message), label, label);
    ROS_ERROR("File server unknown message.");
    assert(!"File server unknown message.");

    return DISPATCH_ERROR;
}

/*! @brief Main CPIO file server message loop. Simply loops through recieving and dispatching
           messages repeatedly. */
static void
fileserv_mainloop(void)
{
    struct fs_state *s = &fileServ;
    srv_msg_t msg;
    
    while (1) {
        dvprintf("Fileserver blocking for message...\n");
        msg.message = seL4_Recv(fileServCommon->anonEP, &msg.badge);
        fileserv_handle_message(s, &msg);
        client_table_postaction(&fileServCommon->clientTable);
    }
}

/*! @brief Main CPIO file server entry point. */
int
main() {
    SET_MUSLC_SYSCALL_TABLE;
    refosio_setup_morecore_override(mmapRegion, MMAP_SIZE);
    refos_initialise_os_minimal();
    refos_setup_dataspace_stdio(REFOS_DEFAULT_STDIO_DSPACE);

    fileserv_init();
    fileserv_mainloop();

    return 0;
}
