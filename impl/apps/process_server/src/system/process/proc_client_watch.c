/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "process.h"
#include "proc_client_watch.h"
#include "../../common.h"
#include "../../state.h"
#include "../../dispatchers/dispatcher.h"
#include <refos-rpc/proc_common.h>

/*! @file
    @brief Process server death notification book-keeping module. */

static void
client_watch_free_ep_cslot(cspacepath_t *ep)
{
    assert(ep && ep->capPtr);

    /* Free the endpoint and its cslot. */
    vka_cnode_revoke(ep);
    vka_cnode_delete(ep);
    vka_cspace_free(&procServ.vka, ep->capPtr);
    memset(ep, 0, sizeof(cspacepath_t));
    kfree(ep);
}

static int
client_watch_find(struct proc_watch_list *wl, uint32_t pid)
{
    assert(wl && wl->magic == REFOS_PCB_WATCHLIST_MAGIC);
    int count = cvector_count(&wl->clientList);
    for (int i = 0; i < count; i++) {
        uint32_t _pid = (uint32_t) cvector_get(&wl->clientList, i);
        if (_pid == pid) {
            return i;
        }
    }
    return -1;
}

/* --------------------------- Proc watch client interface functions ---------------------------- */

void
client_watch_init(struct proc_watch_list *wl)
{
    assert(wl);
    wl->magic = REFOS_PCB_WATCHLIST_MAGIC;
    cvector_init(&wl->clientList);
    cvector_init(&wl->endpointNotifyList);
}

void
client_watch_release(struct proc_watch_list *wl)
{
    assert(wl && wl->magic == REFOS_PCB_WATCHLIST_MAGIC);
    int count = cvector_count(&wl->endpointNotifyList);
    for (int i = 0; i < count; i++) {
        cspacepath_t *ep = (cspacepath_t *) cvector_get(&wl->endpointNotifyList, i);
        client_watch_free_ep_cslot(ep);
    }
    cvector_free(&wl->clientList);
    cvector_free(&wl->endpointNotifyList);
    wl->magic = 0;
}

seL4_CPtr
client_watch_get(struct proc_watch_list *wl, uint32_t pid)
{
    int idx = client_watch_find(wl, pid);
    if (idx == -1) {
        return 0;
    }
    cspacepath_t *ep = (cspacepath_t *) cvector_get(&wl->endpointNotifyList, idx);
    assert(ep && ep->capPtr);
    return ep->capPtr;
}

int
client_watch(struct proc_watch_list *wl, uint32_t pid, seL4_CPtr notifyEP)
{
    if (!notifyEP) {
        return EINVALIDPARAM;
    }

    /* Allocate cspacepath_t structure. */
    cspacepath_t *cslot = kmalloc(sizeof(cspacepath_t));
    if (!cslot) {
        ROS_ERROR("client_watch failed to malloc cslot structure. Procserv out of memory.");
        return ENOMEM;
    }
    memset(cslot, 0, sizeof(cspacepath_t));

    /* Save the cap path, and take overship of the given notify EP. */
    vka_cspace_make_path(&procServ.vka, notifyEP, cslot);

    int idx = client_watch_find(wl, pid);
    if (idx != -1) {
        /* We are already watching this client. Unwatch first. */
        client_unwatch(wl, pid);
    }

    /* And finally, add to our watch list. */
    dprintf("Adding client_watch pid %d notifyEP 0x%x\n", pid, notifyEP);
    cvector_add(&wl->clientList, (cvector_item_t) pid);
    cvector_add(&wl->endpointNotifyList, (cvector_item_t) cslot);
    assert(cvector_count(&wl->clientList) == cvector_count(&wl->endpointNotifyList));

    return ESUCCESS;
}

void
client_unwatch(struct proc_watch_list *wl, uint32_t pid)
{
    int idx = client_watch_find(wl, pid);
    if (idx == -1) {
        /* Not watched. Nothing to do. */
        return;
    }

    /* Free the endpoint and remove from watch list. */
    cspacepath_t *ep = (cspacepath_t *) cvector_get(&wl->endpointNotifyList, idx);
    client_watch_free_ep_cslot(ep);
    cvector_delete(&wl->endpointNotifyList, idx);
    cvector_delete(&wl->clientList, idx);
}

void
client_watch_notify_death_callback(struct proc_pcb *pcb, void *cookie)
{
    uint32_t deathPID = (int) cookie;
    assert(pcb && pcb->magic == REFOS_PCB_MAGIC);

    if (pcb->pid == deathPID) {
        /* Skip this PID; no point notifying the dying process of its own death. */
        return;
    }

    /* Retrieve the watch notify endpoint, if any. */
    seL4_CPtr ep = client_watch_get(&pcb->clientWatchList, deathPID);
    if (!ep) {
        /* This process isn't watching the dying client. */
        return;
    }

    dprintf("Notifying client %d [%s] of the death of %d with EP 0x%x...\n",
        pcb->pid, pcb->debugProcessName, deathPID, ep);
    if (!pcb->notificationBuffer) {
        ROS_WARNING("Process %d [%s] is watching client with no notification buffer!",
                pcb->pid, pcb->debugProcessName);
        ROS_WARNING("This death notification will be ignored.");
        return;
    }

    /* Construct the notification. */
    struct proc_notification exitNotification;
    exitNotification.magic = PROCSERV_NOTIFICATION_MAGIC;
    exitNotification.label = PROCSERV_NOTIFY_DEATH;
    exitNotification.arg[0] = deathPID;

    /* Append notification to watcher's notification buffer. */
    int error = rb_write(pcb->notificationBuffer, (char*)(&exitNotification),
            sizeof(exitNotification));
    if (error) {
        ROS_WARNING("Failed to write death fault notification to buffer.");
        return;
    }

    /* Notify the pager of this fault. */
    dispatcher_notify(ep);
    client_unwatch(&pcb->clientWatchList, deathPID);
}