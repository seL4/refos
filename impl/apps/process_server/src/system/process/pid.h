/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_PROCESS_SERVER_PROCESS_ID_ALLOCATION_TABLE_H_
#define _REFOS_PROCESS_SERVER_PROCESS_ID_ALLOCATION_TABLE_H_

#include "../../common.h"
#include "../../badge.h"
#include <data_struct/cpool.h>

/*! @file
    @brief Process server PID allocation. */

#define PID_NULL 0x0

struct proc_pcb;

/*! @brief PID / ASID table structure

    The structure keeps a reference to a pcb with no ownership, PID is used as index to locate
    the corresponding pcb.
 */
struct pid_list {
    cpool_t pids;
    struct proc_pcb* pcbs[PID_MAX];
};

/*! @brief Callback function type, used for iteration through all PIDs. */
typedef void (*pid_iterate_callback_fn_t)(struct proc_pcb *p, void *cookie);

/*! @brief Initialise process server ProcessID allocation pool.
    @param p The processID list structure to initialise.
*/
void pid_init(struct pid_list *p);

/*! @brief Assign a new ASID and allocate its corresponding proc_pcb structure.
    
    Note that this does not create any resources for the PCB whatsoever; only the PCB struct itself
    is allocated. The memory containing the PCB structure is zeroed.

    @param p The processID list structure to allocate from.
    @return a new PID if success, PID_NULL otherwise.
 */
uint32_t pid_alloc(struct pid_list *p);

/*! @brief Free an entry from the PID pool and recycle the ID with PID free list.
    
    Note that this does not free the resources that the PCB structure points to; only frees
    the PCB struct itself. All the resources that the PCB owns and points to must be released
    before calling this in order to avoid leakage.

    @param p The processID list structure to allocate from.
    @param pid PID intended to be freed and recycled.
 */
void pid_free(struct pid_list *p, uint32_t pid);

/*! @brief Look up a PCB structure given a PID.
    @param p The processID list structure to allocate from.
    @param pid PID of process to look up.
    @return the proc_pcb struct corresponding to pid if success, NULL otherwise.
 */
struct proc_pcb* pid_get_pcb(struct pid_list *p, uint32_t pid);

/*! @brief Look up a PCB structure given a PID Badge.
    @param p The processID list structure to allocate from.
    @param badge PID badge of process to look up.
    @return the proc_pcb struct corresponding to pid if success, NULL otherwise.
 */
struct proc_pcb* pid_get_pcb_from_badge(struct pid_list *p, uint32_t badge);

/*! @brief Get the badge number given a PID.
    @param pid PID of valid process.
    @return corresponding badge of PID if success, UINT_MAX otherwise.
 */
uint32_t pid_get_badge(uint32_t pid);

/*! @brief Get the liveness badge number given a PID.
    @param pid PID of valid process.
    @return corresponding liveness badge of PID if success, UINT_MAX otherwise.
 */
uint32_t pid_get_liveness_badge(uint32_t pid);

/*! @brief Iterate through all valid PIDs, and invoke the iteration callback.
    @param p The processID list structure to iterate through.
    @param callback The callback function to call on each PID.
    @param cookie The cookie to pass to the callback function.
*/
void pid_iterate(struct pid_list *p, pid_iterate_callback_fn_t callback, void *cookie);

#endif /* _REFOS_PROCESS_SERVER_PROCESS_ID_ALLOCATION_TABLE_H_ */