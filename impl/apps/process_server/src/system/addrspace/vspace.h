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
    @brief Client address space objects. 

    This module is responsible for managing a client's vspace (and cspace included). Exposes memory
    segment window management, mapping & unmapping, along with seL4utils reservations. vs_vspace
    structures do not actually own the kernel PD / root CNode objects they wrap, they are owned by
    the PD. Also does not allocate ow deallocated the vspace's associated PIDs. VSpace objects
    support shared strong references through refcounting. This is done mostly to support COW (copy-
    on-write) of vspaces.
*/

#ifndef _REFOS_PROCESS_SERVER_SYSTEM_ADDRSPACE_VSPACE_H_
#define _REFOS_PROCESS_SERVER_SYSTEM_ADDRSPACE_VSPACE_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <data_struct/cvector.h>
#include <allocman/allocman.h>
#include <allocman/vka.h>
#include <sel4utils/util.h>
#include <sel4utils/vspace.h>

#include "../../common.h"
#include "../memserv/window.h"
#include "pagedir.h"

#define REFOS_VSPACE_MAGIC 0x03FFED14

struct proc_pcb;

/*! @brief Client VSpace structure. Each process is assigned one. */
struct vs_vspace {
    uint32_t magic;
    uint32_t ref;

    seL4_Word pid; /* No ownership. */
    struct w_associated_windowlist windows; /* Has ownership of the associated windows. */

    /* VSpace. */
    seL4_CPtr kpd;
    vspace_t vspace;
    sel4utils_alloc_data_t vspaceData;

    /* CSpace. */
    seL4_CPtr cspaceUnguarded;
    cspacepath_t cspace;
    uint32_t cspaceSize;
    seL4_CapData_t cspaceGuardData;

    /*! List of objects allocated for book keeping. Should free all this when
        this vspace is deleted. Contains list of vka_object_t*s. */
    cvector_t  kobjVSpaceAllocatedFreelist; /* vka_object_t */
};

/* ---------------------------------- VSpace struct ----------------------------------------------*/

/*! @brief Initialise a VSpace structure.
    @param vs The VSpace structure to intialise (No ownership)
    @param pid The allocated unique PID to assign this vspace to. Each process has exactly one
               addrspace, so the ASID is the PID.
    @return ESUCCESS if success, refos-err_t otherwise.
*/
int vs_initialise(struct vs_vspace *vs, uint32_t pid);

/*! @brief Add a shared reference to the VSpaces' reference count.
    @param vs The valid vspace to add a shared reference to. (No ownership)
*/
void vs_ref(struct vs_vspace *vs);

/*! @brief Remove a shared reference to the VSpaces' reference count. If this is the last reference
           to a vspace, then it is deleted and the underlying vspace deleted.
    @param vs The valid vspace to add a shared reference to. (Takes ownership)
*/
void vs_unref(struct vs_vspace *vs);

/*! @brief Add tracked kernel VKA object to be owned by this vspace. The VKA object will be
           deleted when the vspace is deleted.
    @param vs The valid vspace to add a tracked object to.
    @param object The VKA object to add to the vspaces' ownership (Takes ownership).
*/
void vs_track_obj(struct vs_vspace *vs, vka_object_t object);

/* ---------------------------------- VSpace windows ---------------------------------------------*/

/*! @brief Create a memory segment window in this vspace.

    A wrapper / helper fucntion which creates a global window object, associates it with this
    vspace, and creates the sel4utils reservation. When managing a client's memory, these window
    management functions should be used instead of the lower level functions in the
    <memserv/window.h> module.

    @param vs The vspace to create a memory segment window in.
    @param vaddr The vaddr where the memory segment window starts.
    @param size The size of the memory segment to create.
    @param permissions Permissions flags bitmask. (W_PERMISSION_WRITE / W_PERMISSION_READ)
    @param cacheable Whether this memory segment is cacheable.
    @param winID Optional output containing the ID of window assigned to the created segment.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int vs_create_window(struct vs_vspace *vs, vaddr_t vaddr, vaddr_t size, seL4_Word permissions,
                     bool cacheable, int *winID);

/*! @brief Delete a memory segment window in this vspace.

    A wrapper / helper function which unmaps the segment region, unassociated and deletes the window
    object. When managing a client's memory, these window management functions should be used
    instead of the lower level functions in the <memserv/window.h> module.

    @param vs The vspace to delete given memory segment window in.
    @param wID The windowID of window to delete. This window should belong in the vspace given.
*/
void vs_delete_window(struct vs_vspace *vs, int wID);

/*! @brief Resize a memory segment window in this vspace.

    A wrapper / helper function which resizes a memory segment. Currently, window shrinking is not
    supported yet, only expanding. When managing a client's memory, these window management
    functions should be used instead of the lower level functions in the <memserv/window.h> module.

    @param vs The vspace to resize given memory segment window in.
    @param wID The windowID of window to resize. This window should belong in the vspace given.
    @param size The new window segment size.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int vs_resize_window(struct vs_vspace *vs, int wID, vaddr_t size);

/* ---------------------------------- VSpace mapping ---------------------------------------------*/

/*! @brief Map an array of frames into vspace. Needs a valid window to be covering that
           address range.
    @param vs The vspace to map frames into.
    @param vaddr The starting destination vaddr into vspace to map frames into.
    @param frames Array of frames to map.
    @param nFrames Number of frames in given frame array.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int vs_map(struct vs_vspace *vs, vaddr_t vaddr, seL4_CPtr frames[], int nFrames);

/*! @brief Map an array of frames that have been mapped into one vspace, into another vspace.
    @param vsSrc The source vspace to map from.
    @param vaddrSrc The vaddr in the source vspace to map from.
    @param windowDest Destination window to map into.
    @param windowDestOffset Offset into destination window.
    @param outClientPCB Optional destination client PCB which uses this vspace.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int vs_map_across_vspace(struct vs_vspace *vsSrc, vaddr_t vaddrSrc, struct w_window *windowDest,
                         uint32_t windowDestOffset, struct proc_pcb **outClientPCB);

/*! @brief Find & map a device frame into client's vspace. 
    @param vs The vspace to map device frame into.
    @param window The window in which to map the device.
    @param windowOffset Offset into the window at which the device frame(s) will be mapped.
    @param paddr The physical address of the device to map.
    @param size Size of the device frames.
    @param cached Whether to map the device frame cached.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int vs_map_device(struct vs_vspace *vs, struct w_window *window, uint32_t windowOffset,
                  uint32_t paddr , uint32_t size, bool cached);

/*! @brief Unmap a series of frames from vspace. Needs a valid window to be covering that
           address range.
    @param vs The vspace to unmap frames from.
    @param vaddr The vaddr to unmap frames from.
    @param nFrames The number of 4k frames from given vaddr to unmap.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int vs_unmap(struct vs_vspace *vs, vaddr_t vaddr, int nFrames);

/*! @brief Unmap the frames covered by given window, in a vspace.

    Unmap the frames covered by given window, in a vspace. Note that this does not do anything
    to the actual window (doesn't unassociate the window nor deletes the window). It merely
    unmaps the frames under it.

    @param vs The vspace to unmap window from.
    @param wID The windowID of the window to unmap frames under.
*/
void vs_unmap_window(struct vs_vspace *vs, int wID);

/*! @brief Get the frame mapped in a vaddr, if any.
    @param vs The vspace to get mapped from.
    @param vaddr The vaddr to get map from.
    @return The mapped from, if any.
*/
cspacepath_t vs_get_frame(struct vs_vspace *vs, vaddr_t vaddr);

#endif /* _REFOS_PROCESS_SERVER_SYSTEM_ADDRSPACE_VSPACE_H_ */
