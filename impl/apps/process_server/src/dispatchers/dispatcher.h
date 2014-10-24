/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*! \file dispatcher.h
    @brief Common shared dispatcher definitions and helper functions.

    Shared helper functions and constant definitions for dispatchers, which every dispatcher should
    most likely needs and should include to reduce code duplication.
*/

#ifndef _REFOS_PROCESS_SERVER_DISPATCHER_DISPATCHER_COMMON_H_
#define _REFOS_PROCESS_SERVER_DISPATCHER_DISPATCHER_COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sel4/sel4.h>

#include "../common.h"
#include "../state.h"
#include "../badge.h"
#include "../system/process/pid.h"
#include "../system/process/process.h"
#include "../system/memserv/ringbuffer.h"
#include "../system/memserv/dataspace.h"

 /*! @file
     @brief Common Process server dispatcher helper functions. */

#define DISPATCH_SUCCESS 0
#define DISPATCH_PASS -1
#define DISPATCH_ERROR -2

#define SET_ERRNO_PTR(e, ev) if (e) {(*e) = (ev);}

extern seL4_MessageInfo_t _dispatcherEmptyReply;

/*! @brief Notify an async endpoint with a message. (non-blocking)
    @param endpoint The endpoint to notify through.
 */
static inline void
dispatcher_notify(seL4_CPtr endpoint)
{
    seL4_MessageInfo_t notifyTag = seL4_MessageInfo_new(PROCSERV_NOTIFY_TAG, 0, 0, 0);
    seL4_Send(endpoint, notifyTag);
}

/*! @brief Checks whether a badge is an PID badge.
    @param badge The badge to check.
    @return true if the badge is an PID badge, false otherwise.
 */
static inline bool
dispatcher_badge_PID(seL4_Word badge)
{
    return (badge >= PID_BADGE_BASE && badge < PID_BADGE_END);
}

/*! @brief Checks whether a badge is a client liveliness badge.
    @param badge The badge to check.
    @return true if the badge is a liveliness badge, false otherwise.
 */
static inline bool
dispatcher_badge_liveness(seL4_Word badge)
{
    return (badge >= PID_LIVENESS_BADGE_BASE && badge < PID_LIVENESS_BADGE_END);
}

/*! @brief Checks whether a given badge is a ram dataspace.
    @param badge The badge to check.
    @return true if the given badge is a valid ram dataspace ID, false otherwise.
 */
static inline bool
dispatcher_badge_dspace(seL4_Word badge)
{
    return (badge >= RAM_DATASPACE_BADGE_BASE && badge < RAM_DATASPACE_BADGE_END);
}

/*! @brief Checks whether a given badge is a window.
    @param badge The badge to check.
    @return true if the given badge is a valid windowID, false otherwise.
 */
static inline bool
dispatcher_badge_window(seL4_Word badge)
{
    return (badge >= W_BADGE_BASE && badge < W_BADGE_END);
}

/*! @brief Helper function to check for an interface.

    Most of the other check_dispatcher_*_interface functions use call this helper function, that
    does most of the real work. It generates a usable userptr containing the client_t structure of
    the calling process. If the calling syscall label enum is outside of given range,  DISPATCH_PASS
    is returned.

    @param m The recieved message structure.
    @param userptr Output userptr containing corresponding client, to be passed into generated
                   interface dispatcher function.
    @param labelMin The minimum syscall label to accept. 
    @param labelMax The maximum syscall label to accept. 
*/
int check_dispatch_interface(struct procserv_msg *m, void **userptr, int labelMin, int labelMax);

/*! @brief Helper function to check recieved caps and unwrapped mask.
    @param m The recieved message.
    @param unwrappedMask The expected capability unwrap mask.
    @param numExtraCaps The expected nubmer of recieved capabilities.
    @return true if the recieved message capabilities match the unwrapped mask and number, false
            if there is a mismatck and the recieved caps are invalid.
*/
bool check_dispatch_caps(struct procserv_msg *m, seL4_Word unwrappedMask, int numExtraCaps);

/*! @brief Helper function to copy out a given capability. Used by dispatchers to quickly copy out
           a recieved capability.
    @param c The capability to copy out.
    @return A copy of the given capability if success, 0 otherwise. (Ownership transferred.)
*/
seL4_CPtr dispatcher_copyout_cptr(seL4_CPtr c);

/*! @brief Helper function to release a copied-out capability and free its cslot.
    @param c The copied out capability to release (Takes ownership).
*/
void dispatcher_release_copyout_cptr(seL4_CPtr c);

/*! @brief Reads the contents of the given process's param buffer.
    @param pcb The PCB of the process to read parameter buffer from.
    @param readLen The length in bytes to read from the parambuffer.
    @return A static temporary buffer with contents of parameter buffer. (No ownership)
*/
char* dispatcher_read_param(struct proc_pcb *pcb, uint32_t readLen);

#endif /* _REFOS_PROCESS_SERVER_DISPATCHER_DISPATCHER_COMMON_H_ */