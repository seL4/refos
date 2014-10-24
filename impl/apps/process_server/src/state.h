/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_PROCESS_SERVER_STATE_H_
#define _REFOS_PROCESS_SERVER_STATE_H_

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <autoconf.h>
#include <allocman/vka.h>
#include <allocman/allocman.h>
#include <allocman/bootstrap.h>
#include <allocman/cspaceops.h>
#include <vka/vka.h>
#include <vka/object.h>
#include <vka/capops.h>
#include <vspace/vspace.h>
#include <sel4utils/vspace.h>
#include <syscall_stubs_sel4.h>
#include <refos-util/nameserv.h>
#include <sel4platsupport/platsupport.h>
#include <data_struct/chash.h>

#include <simple/simple.h>
#ifdef CONFIG_KERNEL_STABLE
    #include <simple-stable/simple-stable.h>
#else
    #include <simple-default/simple-default.h>
#endif

#include "common.h"
#include "system/process/pid.h"
#include "system/addrspace/vspace.h"
#include "system/addrspace/pagedir.h"
#include "system/memserv/window.h"
#include "system/memserv/dataspace.h"

/*! @file
    @brief Global environment struct & helper functions for process server. */

/*! @brief A list of global process server objects; represents an instance of the process server. */
struct procserv_state {
    /* Allocator information. */
    seL4_BootInfo                     *bootinfo;
    vka_t                              vka;
    vka_t                             *vkaPtr;
    vspace_t                           vspace;
    sel4utils_alloc_data_t             vspaceData;
    allocman_t                         *allocman;
    simple_t                           simpleEnv;

    /* Process server endpoints. */
    vka_object_t                       endpoint;
    cspacepath_t                       IPCCapRecv;

    /* Process server global lists. */
    struct pid_list                    PIDList;
    struct pd_list                     PDList;
    struct w_list                      windowList;
    struct ram_dspace_list             dspaceList;
    nameserv_state_t                   nameServRegList;
    chash_t                            irqHandlerList;

    /* Misc states. */
    uint32_t                           faketime;
    uint32_t                           unblockClientFaultPID;
    uint32_t                           exitProcessPID;
};

/*! @brief Process server message structure. */
struct procserv_msg {
    seL4_MessageInfo_t message;
    seL4_Word badge;
    struct procserv_state *state;
};

/*! @brief Process server CPIO archive. */
extern char _refos_process_server_archive[];

/*! @brief Process server global state. */
extern struct procserv_state procServ;

/*! @brief Fake time, useful for debugging. */
uint32_t faketime();

/*! @brief Initialises the process server.
    
    High-level initialisation function which calls the other initialisation functions and starts the
    process server up and running to the point where it is ready to dispatch all messages, and is
    able to start processes.

    @param info The BootInfo struct passed in from the kernel.
    @param s The process server global state.
 */
void initialise(seL4_BootInfo *info, struct procserv_state *s);

/*! @brief Creates a generic minted badge using the process server's EP.

    Creates a generic minted badge using the process server's EP. Ownership is transferred, so the
    resulting capability must be revoked, deleted and the cslot freed by the caller. Permissions are
    set to canWrite | canGrant, as the resulting object badge should be passable and invokable but
    we don't really want anyone else listening in to that EP and pretending to be the process
    server.

    @param badge the badge number to create.
    @return cslot to minted badge (ownership transfer).
*/
cspacepath_t procserv_mint_badge(int badge);

/*! @brief Temporary map a page frame and write data to it.
    
    In order to read / write data to / from a 

    @param frame CPtr to destination frame.
    @param src Data source buffer.
    @param len Data source buffer length.
    @param offset Offset into frame to write to.
    @return ESUCCESS if write successful, refos error otherwise.
*/
int procserv_frame_write(seL4_CPtr frame, const char* src, size_t len, size_t offset);

/*! @brief Temporary map a page frame and read data from it.
    @param frame CPtr to source frame.
    @param dst Data destination buffer.
    @param len Data destination buffer max length.
    @param offset Offset into frame to read from.
    @return ESUCCESS if read successful, refos error otherwise.
*/
int procserv_frame_read(seL4_CPtr frame, const char* dst, size_t len, size_t offset);

/*! @brief Helper function to finds a MMIO device frame.
    @param paddr Physical address of the device MMIO frame.
    @param size Size of device frame in bytes.
    @return Path to device frame on success (gives ownership to caller), 0 otherwise.
*/
cspacepath_t procserv_find_device(void *paddr, int size);

/*! @brief Helper Function to TLB flush on a series of frames.
    @param frame Pointer to array of frame caps to unmap.
    @param nFrames Number of frames in given frame array.
*/
void procserv_flush(seL4_CPtr *frame, int nFrames);

/*! @brief Helper function to retrieve an IRQ handler for the given IRQ number. Uses a hash table
           in order to avoid creating the same IRQ handler twice.
    @param irq The IRQ number to create the handler for.
    @return The corresponding IRQ handler cap if successful, 0 otherwise.
*/
seL4_CPtr procserv_get_irq_handler(int irq);

#endif /* _REFOS_PROCESS_SERVER_STATE_H_ */
