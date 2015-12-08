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
    @brief Top-level main module for process server.

    The top level main module of the process server, containing the main function which runs as
    the initial kernel thread. Starts up the process server, bootstraps itself, initialises the
    submodules, and and starts the rest of system.

    The process server is responsible for implementing threads & processes, manage memory window
    segments, expose anon memory dataspaces, and hand off other system resources to the system
    processes available (such as device and IRQ caps device servers).

    @image html procserv.png

    The process server's anonymous dataspace implementation:
    <ul>
        <li>Assumes the connection session is set up, so therefore does not support connection
            establishment.</li>
        <li>Ignores the fileName parameter of the open method; makes no sense for anon memory.</li>
        <li>Reads the nBytes parameter of the open method, as the max. size of the ram dataspace
            created. </li>
        <li>Does NOT implement set_parambuffer, it shares the parambuffer from procserv interface
            and reads that instead. </li>
        <li>Supports the parambuffer (set in the procserv interface) coming from a ram dataspace
            provided by the process server itself. </li>
        <li>Does not support content initialisation via init_data. Process server cannot provide
            content for another dataserver in the current implementation.</li>
        <li>Does support have_data and provide_data. In other words, process server RAM dataspace
            can have its content initialised by an external dataserver.</li>
    </ul>
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"
#include "state.h"
#include "test/test.h"
#include "dispatchers/proc_syscall.h"
#include "dispatchers/mem_syscall.h"
#include "dispatchers/data_syscall.h"
#include "dispatchers/name_syscall.h"
#include "dispatchers/fault_handler.h"
#include "system/process/process.h"

/*! @brief Process server IPC message handler.
    
    Handles dispatching of all process server IPC messages. Calls each individual dispatcher until
    the correct dispatcher for the message type has been found.

    @param s The process server global state.
    @param msg The process server recieved message info.
 */
static void
proc_server_handle_message(struct procserv_state *s, struct procserv_msg *msg)
{
    int result;
    int label = seL4_GetMR(0);
    void *userptr = NULL;
    (void) result;

    /* Attempt to dispatch to procserv syscall dispatcher. */
    if (check_dispatch_syscall(msg, &userptr) == DISPATCH_SUCCESS) {
        result = rpc_sv_proc_dispatcher(userptr, label);
        assert(result == DISPATCH_SUCCESS);
        mem_syscall_postaction();
        proc_syscall_postaction();
        return;
    }

    /* Attempt to dispatch to VM fault dispatcher. */
    if (check_dispatch_fault(msg, &userptr) == DISPATCH_SUCCESS) {
        result = dispatch_vm_fault(msg, &userptr);
        assert(result == DISPATCH_SUCCESS);
        return;
    }

    /* Attempt to dispatch to RAM dataspace syscall dispatcher. */
    if (check_dispatch_dataspace(msg, &userptr) == DISPATCH_SUCCESS) {
        result = rpc_sv_data_dispatcher(userptr, label);
        assert(result == DISPATCH_SUCCESS);
        mem_syscall_postaction();
        return;
    }

    /* Attempt to dispatch to nameserv syscall dispatcher. */
    if (check_dispatch_nameserv(msg, &userptr) == DISPATCH_SUCCESS) {
        result = rpc_sv_name_dispatcher(userptr, label);
        assert(result == DISPATCH_SUCCESS);
        return;
    }

    /* Unknown message. Block calling client indefinitely. */
    dprintf("Unknown message (badge = %d msgInfo = %d syscall = 0x%x).\n",
            msg->badge, seL4_MessageInfo_get_label(msg->message), label);
    ROS_ERROR("Process server unknown message. ¯＼(º_o)/¯");
}

/*! @brief Main process server loop.

    The main loop that the process server goes into and keeps looping until the process server
    is to exit and the whole system is to by shut down (which is possibly never). It blocks on the
    process server endpoint and waits for an IPC message, and then handles the dispatching of
    the message when it recieves one, before looping around and waiting for the next IPC message.

    @return Does not return, runs endlessly.
*/
static int
proc_server_loop(void)
{
    struct procserv_state *s = &procServ;
    struct procserv_msg msg = { .state = s };

    while (1) {
        dvprintf("procserv blocking for new message...\n");
        msg.message = seL4_Recv(s->endpoint.cptr, &msg.badge);
        proc_server_handle_message(s, &msg);
        s->faketime++;
    }

    return 0;
}

/*! @brief Process server main entry point. */
int
main(void)
{
    SET_MUSLC_SYSCALL_TABLE;
    initialise(seL4_GetBootInfo(), &procServ);
    dprintf("======== RefOS Process Server ========\n");

    // -----> Run Root Task Testing.
    #ifdef CONFIG_REFOS_RUN_TESTS
        test_run_all();
    #endif

    // -----> Start RefOS system processes.
    int error;

    error = proc_load_direct("console_server", 252, "", PID_NULL, 
            PROCESS_PERMISSION_DEVICE_IRQ | PROCESS_PERMISSION_DEVICE_MAP |
            PROCESS_PERMISSION_DEVICE_IOPORT);
    if (error) {
        ROS_WARNING("Procserv could not start console_server.");
        assert(!"RefOS system startup error.");
    }

    error = proc_load_direct("file_server", 250, "", PID_NULL, 0x0);
    if (error) {
        ROS_WARNING("Procserv could not start file_server.");
        assert(!"RefOS system startup error.");
    }

    // -----> Start OS level tests.
    #ifdef CONFIG_REFOS_RUN_TESTS
        error = proc_load_direct("test_os", 245, "", PID_NULL, 0x0);
        if (error) {
            ROS_WARNING("Procserv could not start test_os.");
            assert(!"RefOS system startup error.");
        }
    #endif

    // -----> Start RefOS timer server.
    error = proc_load_direct("selfloader", 245, "fileserv/timer_server", PID_NULL,
            PROCESS_PERMISSION_DEVICE_IRQ | PROCESS_PERMISSION_DEVICE_MAP |
            PROCESS_PERMISSION_DEVICE_IOPORT);
    if (error) {
        ROS_WARNING("Procserv could not start timer_server.");
        assert(!"RefOS system startup error.");
    }

    // -----> Start initial task.
    if (strlen(CONFIG_REFOS_INIT_TASK) > 0) {
        error = proc_load_direct("selfloader", CONFIG_REFOS_INIT_TASK_PRIO, CONFIG_REFOS_INIT_TASK,
                                 PID_NULL, 0x0);
        if (error) {
            ROS_WARNING("Procserv could not start initial task.");
            assert(!"RefOS system startup error.");
        }
    }

    return proc_server_loop();
}
