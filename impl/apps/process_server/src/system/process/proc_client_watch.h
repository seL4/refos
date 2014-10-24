/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_PROCESS_SERVER_PROCESS_CLIENT_WATCH_H_
#define _REFOS_PROCESS_SERVER_PROCESS_CLIENT_WATCH_H_

#include <sel4/sel4.h>
#include <data_struct/cvector.h>

/*! @file
    @brief Process server death notification book-keeping module. */

#define REFOS_PCB_WATCHLIST_MAGIC 0x191B44E7

/*! @brief Client death notify watch list structure. */
struct proc_watch_list {
    uint32_t magic;
    cvector_t clientList; /* uint32_t PIDs, no ownership. */
    cvector_t endpointNotifyList; /* cspacepath_t, has ownership. */
};

struct proc_pcb;

/* --------------------------- Proc watch client interface functions ---------------------------- */

/*! @brief Initialise the given client death notify watch list structure.
    @param wl The watch list to initialise. (No ownership transfer)
*/
void client_watch_init(struct proc_watch_list *wl);

/*! @brief De-initialise the given client death notify watch list structure, releasing all its owned
           resources. Does <b>not</b> free the actual structure.
    @param wl The watch list to de-initialise. (No ownership transfer)
*/
void client_watch_release(struct proc_watch_list *wl);

/*! @brief Get the current death notification notify EP for a given pid.
    @param wl The watch list to get from.
    @param pid The PID of client being watched.
    @return CPtr to the notify end point if someone is watching the given PID, 0 otherwise.
            (No ownership given).
*/
seL4_CPtr client_watch_get(struct proc_watch_list *wl, uint32_t pid);

/*! @brief Watch the given PID with the given notification async endpoint.
    @param wl The watch list to add to.
    @param pid The PID of client to watch.
    @param notifyEP The watcher notify async EP.
    @return ESUCCESS if success, refos_err_t otherwise.
*/
int client_watch(struct proc_watch_list *wl, uint32_t pid, seL4_CPtr notifyEP);

/*! @brief Stop watching the given PID.
    @param wl The watch list to add to.
    @param pid The PId of client to stop watching.
*/
void client_unwatch(struct proc_watch_list *wl, uint32_t pid);

/*! @brief Notify listening watchers that a client has exited. This is a callback function intended
           to be used with pid_iterate() in <process/pid.h>.
    @param pcb The current iterating PCB containign watchlist to go through.
    @param cookie The PID of dying client.
*/
void client_watch_notify_death_callback(struct proc_pcb *pcb, void *cookie);

#endif /* _REFOS_PROCESS_SERVER_PROCESS_CLIENT_WATCH_H_ */