/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "pid.h"
#include "process.h"

/*! @file
    @brief Process server PID allocation.

    Simple PID / ASID allocation module. Uses simple free-list based allocation defined in
    <data_struct/cpool.h>. The PID module owns the PCBs it contains.
*/

#define PID_START 1

void
pid_init(struct pid_list *p)
{
    dprintf("Initialising process ID allocation table...\n");
    assert(p);
    cpool_init(&p->pids, PID_START, PID_MAX);
    memset(p->pcbs, 0, sizeof(struct proc_pcb*) * PID_MAX);
}

uint32_t
pid_alloc(struct pid_list *p)
{
    uint32_t pid = cpool_alloc(&p->pids);

    if (pid < PID_START || pid >= PID_MAX) {
        /* Allocation failed due to too many processes. */
        ROS_ERROR("PID allocation failed. Too many processes!\n");
        return PID_NULL;
    }
    /* Should never allocate a pID that is currently active. */
    assert(p->pcbs[pid] == NULL);

    /* Allocate new PCB for this pID. */
    p->pcbs[pid] = kmalloc(sizeof(struct proc_pcb));
    if (p->pcbs[pid] == NULL) {
        ROS_ERROR("Could not allocate PCB structure. Procserv out of memory.\n");
        cpool_free(&p->pids, pid);
        return PID_NULL;
    }
    memset(p->pcbs[pid], 0, sizeof(struct proc_pcb));
    return pid;
}

void
pid_free(struct pid_list *p, uint32_t pid)
{
    if (pid < PID_START || pid >= PID_MAX) {
        ROS_ERROR("PID out of range!\n");
        return;
    }
    if (p->pcbs[pid] == NULL) {
        ROS_ERROR("PID already freed!\n");
        return;
    }
    kfree(p->pcbs[pid]);
    p->pcbs[pid] = NULL;
    cpool_free(&p->pids, pid);
}

struct proc_pcb*
pid_get_pcb(struct pid_list *p, uint32_t pid)
{
    /* Does _NOT_ grant ownership of returned PCB object. */
    if (pid < PID_START || pid >= PID_MAX) {
        return NULL;
    }
    return p->pcbs[pid];
}

uint32_t
pid_get_badge(uint32_t pid)
{
    if (pid < PID_START || pid >= PID_MAX) {
        ROS_ERROR("PID out of range!\n");
        assert(!"PID out of range.");
        return (uint32_t) -1;
    }
    return (pid + PID_BADGE_BASE);
}

struct proc_pcb*
pid_get_pcb_from_badge(struct pid_list *p, uint32_t badge)
{
    if (badge < PID_BADGE_BASE) {
        return NULL;
    }
    return pid_get_pcb(p, badge - PID_BADGE_BASE);
}

uint32_t
pid_get_liveness_badge(uint32_t pid)
{
    if (pid < PID_START || pid >= PID_MAX) {
        ROS_ERROR("PID out of range!\n");
        assert(!"PID out of range.");
        return (uint32_t) -1;
    }
    return (pid + PID_LIVENESS_BADGE_BASE);
}

void
pid_iterate(struct pid_list *p, pid_iterate_callback_fn_t callback, void *cookie)
{
    assert(callback);
    for (int i = PID_START; i < PID_MAX; i++) {
        struct proc_pcb* pcb = pid_get_pcb(p, i);
        if (pcb != NULL) {
            callback(pcb, cookie);
        }
    }
}




