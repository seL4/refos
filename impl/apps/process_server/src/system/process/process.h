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
    @brief Process management module for process server.

    Module which manages everything to do with a process and its threads; provides a basic process
    interface, handling the virtual memory address spaces, kernel objects, ELF-loading....etc. The
    sel4utils library is used here to easily implement direct booting of executables. The process
    server needs to be able to directly boot the system executables needed for the selfloader
    to function. The selfloader app then takes over the job of ELF-loading RefOS user-land apps
    once the system is up and running.

    Executable files are stored on the process server's own CPIO archive.
*/

#ifndef _REFOS_PROCESS_SERVER_SYSTEM_PROCESS_PROCESS_H_
#define _REFOS_PROCESS_SERVER_SYSTEM_PROCESS_PROCESS_H_

#include <refos/vmlayout.h>
#include <refos-rpc/rpc.h>
#include <data_struct/cvector.h>

#include "../../common.h"
#include "../addrspace/vspace.h"
#include "../memserv/dataspace.h"
#include "../memserv/ringbuffer.h"
#include "proc_client_watch.h"

#define REFOS_PCB_MAGIC 0xB33FFEED
#define REFOS_PCB_DEBUGNAME_LEN 32

#define PROCESS_PERMISSION_DEVICE_MAP 0x0001
#define PROCESS_PERMISSION_DEVICE_IRQ 0x0002
#define PROCESS_PERMISSION_DEVICE_IOPORT 0x0004

/*! @brief Process control block structure.

    It stores process related information. It is able to own up to PROCESS_MAX_THREADS threads
    as well as having sessions with up to PROCESS_MAX_SESSIONS servers. authenticateEP is for
    the process server to notify death of a client when this pcb is for a server.
 */
struct proc_pcb {
    rpc_client_state_t rpcClient; /* Inherited structure (must be 1st). */
    uint32_t magic;
    uint32_t pid; /* No ownership. Must free separately. */
    char debugProcessName[REFOS_PCB_DEBUGNAME_LEN];

    struct vs_vspace vspace;
    cvector_t threads; /* proc_tcb */

    struct proc_watch_list clientWatchList;
    struct ram_dspace *paramBuffer; /* Shared ownership. */
    struct rb_buffer *notificationBuffer; /* Has ownership. */
    uint32_t systemCapabilitiesMask;

    cspacepath_t faultReply;
    int32_t exitStatus;

    uint32_t parentPID; /* No ownership. */
    bool parentWaiting;
};

/* ---------------------------------- Proc interface functions ---------------------------------- */

/*! @brief Set up and configure a process.
    
    Set up and configure a process. Note that this does <b>not</b> set up the process's environment
    to be a RefOS client environment, not does it allocate any new processes. It simply configures
    the process to use its vspace and loads its ELF regions into the vspace.

    @param p The process to configure (No ownership).
    @param pid The allocated PID of the process.
    @param priority The priority that the process runs at.
    @param imageName The ELF file name, in the process server's CPIO archive.
    @param systemCapabilitiesMask The system capabilities mask, which allows access to additional
                                  syscalls.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int proc_config_new(struct proc_pcb *p, uint32_t pid, uint8_t priority, char *imageName,
                    uint32_t systemCapabilitiesMask);

/*! @brief Start a process' thread running.
    @param p The process to start.
    @param tindex The index of the thread to start in given process.
    @param arg0 The first argument to pass to the thread's entry point. (unimplemented)
    @param arg1 The second argument to pass to the thread's entry point. (unimplemented)
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int proc_start_thread(struct proc_pcb *p, int tindex, void* arg0, void* arg1);

/*! @brief Directly load an ELF into a process.

    This function does everything a process needs to run and runs it. It allocates a PID, creates a
    vspace, loads the ELF segments into it, creates an initial thread, and starts it.

    @param name The ELF file name, in the process server's CPIO archive.
    @param priority The priority of the initial thread of the process to load.
    @param param The static parameter to give to the process.
    @param parentPID The PId of the parent that has started this process.
    @param systemCapabilitiesMask The system capabilities mask, which allows access to additional
                                  syscalls.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int proc_load_direct(char *name, int priority, char *param, unsigned int parentPID,
                     uint32_t systemCapabilitiesMask);

/*! @brief Release a process, and delete all its owned resources.
    
    This function destroys a process and deletes everything that it has ownership to. Note that the
    actual vspace may not be destroyed, as it could have more shared references. When a client
    process exit call is recieved, do <b>NOT</b> use this function, as we are not able to clean up
    the IPC state mid-way through an IPC. Instead, handle the IPC state properly, skipping replying
    to block the client, and use proc_queue_release() instead to release the client once the IPC is
    over.

    @param p The process to release.
*/
void proc_release(struct proc_pcb *p);

/*! @brief Queues a process to be released at the next proc_syscall_postaction() call. 

    This is done to avoid leaving IPC state inconsistent. See proc_release() documentation for
    details.
    
    @param p The process to queue for release.
*/
void proc_queue_release(struct proc_pcb *p);

/* ------------------------------- Proc interface helper functions ------------------------------ */

/*! @brief Change the priority for a given process' thread.
    @param p The process to change priority for.
    @param tindex The thread index to change priority for.
    @param priority The new priority to change to.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int proc_nice(struct proc_pcb *p, int tindex, int priority);

/*! @brief Set the parameter buffer for a process.
    @param p The process to set parameter buffer for.
    @param paramBuffer The parameter buffer anon dataspace structure. (Shared ownership)
    @return ESUCCESS on success, refos_err_t otherwise.
*/
void proc_set_parambuffer(struct proc_pcb *p, struct ram_dspace *paramBuffer);

/*! @brief Set the async notification buffer for a process.
    @param p The process to set notification buffer for.
    @param notifBuffer The notification buffer anon dataspace structure. (Shared ownership)
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int proc_set_notificationbuffer(struct proc_pcb *p, struct ram_dspace *notifBuffer);

/*! @brief Purge all references to a dataspace.

    Purge all references to a dataspace (called on dataspace deletion). This will unset any
    parameter or notification buffers that have been set to that dataspace. This is to be used with
    the pid_iterate() function in <process/pid.h>.

    @param p The process check and unset and dataspace from.
    @param cookie The dataspace ID to purge all references to.
*/
void proc_dspace_delete_callback(struct proc_pcb *p, void *cookie);

/*! @brief Get the thread TCB of process at the given threadID.
    @param p The process to get TCB from.
    @param tindex The thread index to of the thread TCB to get.
    @returns TCB at the given threadID of the given process. (No ownership transfer)
*/
struct proc_tcb *proc_get_thread(struct proc_pcb *p, int tindex);

/*! @brief Save the current caller's reply cap. This should only be called when the current
           reply cap is for a message fro mthe given process.
    @param p The process to save current caller cap into.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int proc_save_caller(struct proc_pcb *p);

/*! @brief Create another thread for the given process, sharing the process' address space.
    @param p The process to clone another thread for.
    @param threadID Optional output pointer to store the created threadID in.
    @param stackAddr The stack address of the new thread, in the given process' vspace.
    @param entryPoint The entry point of the new thread, in the given process' vspace.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int proc_clone(struct proc_pcb *p, int *threadID, vaddr_t stackAddr, vaddr_t entryPoint);

/*! @brief Reply to the saved cap previous saved by proc_save_caller().
    @param p The process to reply to.
*/
void proc_fault_reply(struct proc_pcb *p);

/*! @brief Perform any process book-kepping postactions.
    
    This is used to neatly release an exiting process, without leaving an inconsistent IPC state.
    See the documentation under proc_release() for details.
*/
void proc_syscall_postaction(void);

#endif /* _REFOS_PROCESS_SERVER_SYSTEM_PROCESS_PROCESS_H_ */