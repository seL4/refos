/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_PROCESS_SERVER_SYSTEM_PROCESS_THREAD_H_
#define _REFOS_PROCESS_SERVER_SYSTEM_PROCESS_THREAD_H_

#include <stdint.h>
#include "../../common.h"
#include <sel4utils/thread.h>

/*! @file
    @brief Thread module for process server. */

#define REFOS_PROCESS_THREAD_MAGIC 0x1003C44C

struct vs_vspace;

/*! @brief Process thread control block structure.

    It stores thread related information and keeps a reference of the address space it is
    associated to.
 */
struct proc_tcb {
    uint32_t magic;
    uint8_t priority;
    struct vs_vspace *vspaceRef; /* Shared ownership. */
    sel4utils_thread_t sel4utilsThread;
    vaddr_t entryPoint;
};

/*! @brief Helper function to get the underlying kernel TCB object that a procserv TCB
           structure wraps.
    @param thread The procserv TCB structure.
    @return The underlying kernel TCB object (No ownership transfer).
*/
static inline seL4_CPtr
thread_tcb_obj(struct proc_tcb *thread)
{
    return thread->sel4utilsThread.tcb.cptr;
}

/*! @brief Configure a thread, and set it up for use.
    @param thread The TCB structure to set up and configure. (No ownership transfer).
    @param priority Priority of the thread.
    @param entryPoint The entry point of the thread, in the thread's vspace.
    @param vspace The thread's new vspace.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int thread_config(struct proc_tcb *thread, uint8_t priority, vaddr_t entryPoint,
                  struct vs_vspace *vspace);

/*! @brief Start a thread running.
    @param thread The TCB of the thread to start.
    @param arg0 The first argument. 
    @param arg1 The second argument. 
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int thread_start(struct proc_tcb *thread, void *arg0, void *arg1);

/*! @brief Release a thread and all the resources it owns. Does not free the struct itself.
    @param thread The TCB of the thread to remove. (No ownership transfer)
*/
void thread_release(struct proc_tcb *thread);

#endif /* _REFOS_PROCESS_SERVER_SYSTEM_PROCESS_THREAD_H_ */
