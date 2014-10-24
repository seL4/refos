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
    @brief Manages and keeps track of memory windows.

    \image html memwindow.png

    In RefOS, memory segment windows (just called windows for short), are virtual address  <a href =
    'http://en.wikipedia.org/wiki/Memory_segmentation'>memory segments</a> which belong in clients'
    vspaces. Each window keeps track of its read / write permissions, cachability, mapped contents,
    and any other state about the memory segment, and are represented by badged procserv endpoint
    capabilities. A window represents the permission to handle and map memory into a segment of a
    client's virtual address space.

    This module manages and keeps track of client memory window segments and their associations.
    Used to implement the memory window segment interface. Each window is assigned a unique
    windowID, dynamically allocated.

    Note that when operating on client memory windows, the higher-level vs_*
    window functions in <addrspace/vspace.h> should be used instead of the functions here. This
    module simply provides the low-level management functions for global window lists and window
    associations, does not provide any vspace functionality underneath.
*/

#ifndef _REFOS_PROCESS_SERVER_SYSTEM_MEMSERV_WINDOW_SEGMENTS_H_
#define _REFOS_PROCESS_SERVER_SYSTEM_MEMSERV_WINDOW_SEGMENTS_H_

#include <stdint.h>
#include <stdbool.h>
#include <sel4/sel4.h>
#include <data_struct/cvector.h>
#include <data_struct/coat.h>
#include <vspace/vspace.h>
#include "../../common.h"

#define W_INVALID_WINID 0
#define W_MAGIC 0x02B16401
#define W_LIST_MAGIC 0x63CA5250 

/* Should mirror definitions in proc_client_helper.h. */
#define W_PERMISSION_WRITE 0x1
#define W_PERMISSION_READ 0x2
#define W_FLAGS_UNCACHED 0x1

struct ram_dspace;
struct w_list;
struct vs_vspace;

/* Forward declaration for vspace unmapping function. This is called whenever a window mode is
   changed in order to unmap any old pages that were there, for the new content to take its place.
*/
void vs_unmap_window(struct vs_vspace *vs, int wID);

/*! @brief Memory window operation mode.
    A window may be backed by nothing, by a process server dataspace, or by an external
    fileserver dataspace.
*/
enum w_window_mode {
    W_MODE_EMPTY = 0, /*!< The window is currently unmapped. Accessing it will result in segfault.*/
    W_MODE_ANONYMOUS, /*!< The window is mapped to a procserv anonymous memory dataspace. */
    W_MODE_PAGER,     /*!< The window is mapped to an external pager. */
};

/*! @brief Memory window structure.

    A memory window structure, keeping track of all the information about one specific memory
    window. This structure resides as a global list; each client's vspace contains indexes into this
    global list.
 */
struct w_window {
    uint32_t magic;
    int wID;
    int clientOwnerPID;
    struct w_list* parentList;

    vaddr_t size;
    enum w_window_mode mode;
    seL4_Word permissions;
    bool cacheable;

    vspace_t *vspace; /* No ownership. */
    reservation_t reservation; /* Has ownership. */
    cspacepath_t capability;

    /*! External pager endpoint. Has ownership. Valid only if mode is W_MODE_PAGER */
    cspacepath_t pager;
    uint32_t pagerPID;

    /*! Ram dataspace. Shared ownership. Valid only if mode is W_MODE_ANONYMOUS */
    struct ram_dspace *ramDataspace;
    vaddr_t ramDataspaceOffset;
};

/*! @brief Window list.

    A window list structure that stores and amanges allocation for a window list. Keeps a
    dynamic object allocation table of windows.
 */
struct w_list {
    coat_t windows; /* struct w_window* */
    uint32_t magic;
};

/*! @brief Window association entry.

    A structure which records a client's association to a window, and stores the base addr offset
    to which the memory wnddow is mapped to in the corresponding client's vspace.
 */
struct w_associated_window {
    int winID;
    vaddr_t offset;
    vaddr_t size;
};

/*! @brief Window association list.

    A list of window associations, used to keep track of the list of windows a client has in its
    vspace.
 */
struct w_associated_windowlist {
    struct w_associated_window *associated;
    uint32_t associatedVectorSize;
    int numIndex;
    bool updated;
};

/* --------------------------------------- Window functions ------------------------------------- */

/*! @brief Initialises the window list.
    @param wlist The window list to initialise.
*/
void w_init(struct w_list *wlist);

/*! @brief Deinitialises the window list, and release all windows in it.
    @param wlist The window list to deinitialise.
*/
void w_deinit(struct w_list *wlist);

/*! @brief Creates a new window.
    @param wlist The window list to allocate window from.
    @param size The window size in bytes.
    @param ownerPID The owner client's PID (who's address space is this window in?).
    @param permissions The window permissions.
    @param vspace The vspace corresponding to the reservation.
    @param reservation The sel4utils vspace reservation.
    @param cacheable Whether to create the window flagged as cached.
    @return The assigned window structure if success, NULL otherwise. No ownership transfer.
 */
struct w_window* w_create_window(struct w_list *wlist, vaddr_t size, int ownerPID,
        seL4_Word permissions, vspace_t *vspace, reservation_t reservation, bool cacheable);

/*! @brief Delete a window at given index from list, along with any stored caps.
    @param wlist The window list to delete from.
    @param windowID The windowID to delete.
    @return ESUCCESS if success, RefOS error otherwise.
 */
int w_delete_window(struct w_list *wlist, int windowID);

/*! @brief Retrieve the window structure associated with given windowID.
    @param wlist The window list to delete from.
    @param windowID The windowID to delete.
    @return The corresponding window structure if success, NULL otherwise.
 */
struct w_window* w_get_window(struct w_list *wlist, int windowID);

/*! @brief Helper function to convert window permission to seL4 capRights.
    @param permission Window permission bitmask.
    @return corresponding seL4_CapRights to given window permission bitmask.
*/
static inline seL4_CapRights
w_convert_permission_to_caprights(seL4_Word permission)
{
    seL4_CapRights capr = seL4_CanGrant;
    if (permission | W_PERMISSION_WRITE) {
        capr |= seL4_CanWrite;
    }
    if (permission | W_PERMISSION_READ) {
        capr |= seL4_CanRead;
    }
    return capr;
}

/*! @brief Set the window up to be paged by an external pager. Potentially does VSpace unmapping
           operations.
    @param window The window to set in pager mode.
    @param endpoint The pager asynchronous notification endpoint of the window. Takes ownership.
    @param pid The pager PID. (No ownership)
*/
void w_set_pager_endpoint(struct w_window *window, cspacepath_t endpoint, uint32_t pid);

/*! @brief Set an internal RAM dataspace as the window's source of data. Potentially does VSpace
           unmapping operations.
    @param window The window to set to anonymous dataspace mode.
    @param dspace The internal RAM dataspace to set the window to (Shares ownership).
                  NULL means clear this window back to empty.
    @param offset Offset into the dataspace at which this window is mapped. Alignment restrictions
                  apply.
*/
void w_set_anon_dspace(struct w_window *window, struct ram_dspace *dspace, vaddr_t offset);

/*! @brief Notify of the window list of the death of a dataspace.

    Notify of the window list of the death of a dataspace. If any windows in the list are found to
    be set to be initialised by the given dataspace, that window will be reset to W_MODE_EMPTY, and
    the dataspace dereferenced. Potentially does VSpace unmapping operations.

    @param wlist The window list to purge from.
    @param dspace The internal RAM dataspace to purge all references to. (No ownership)
*/
void w_purge_dspace(struct w_list *wlist, struct ram_dspace *dspace);

/*! @brief Resize a window. Note that this does not perform any window associate updates or checks,
           nor any vspace operations, simply updates the field in the global window list entry,
           and re-reserves the owned reservation.
    @param window The window to update size for.
    @param vaddr The vaddr of the window. Should _not_ change from what it was before.
    @param size The new window size.
    @return ESUCCESS if success, RefOS error otherwise.
 */
int w_resize_window(struct w_window *window, vaddr_t vaddr, vaddr_t size);

/* -------------------------------- Window Association functions -------------------------------- */

/*! @brief Initialises window association list. Same as w_associate_clear. */
void w_associate_init(struct w_associated_windowlist *aw);

/*! @brief Creates a new window association.
    @param aw The window association list of a process.
    @param winID The window ID of the window to be associated. (No ownership transfer).
    @param offset The base address of the window in the address space of the process.
    @param size The size of the window.
    @return ESUCCESS if success, refos_error otherwise.
 */
int w_associate(struct w_associated_windowlist *aw, int winID, vaddr_t offset, vaddr_t size);

/*! @brief Prints the window association list.
    @param aw The window association list of a process.
 */
void w_associate_print(struct w_associated_windowlist *aw);

/*! @brief Unassociate a window from a process.
    @param aw The window association list of a process.
    @param winID The window ID of the window to be unassociated.
 */
void w_unassociate(struct w_associated_windowlist *aw, int winID);

/*! @brief Clears window association list.
    @param aw The window association list of a process to be cleared.
 */
void w_associate_clear(struct w_associated_windowlist *aw);

/*! @brief Clears window association list, and releases any associated windows from the global
           window list. This will not only unassociate all the associated windows, but will actually
           delete them too.
           Note that calling this function transfers ownership of _all_ the winIDs in the given
           association list. So please double check window object ownership before calling this, to
           avoid double free-ing of windows.
    @param wlist The window list to delete from.
    @param aw The window association list to delete windows from.
 */
void w_associate_release_associated_all_windows(struct w_list *wlist,
        struct w_associated_windowlist *aw);

/*! @brief Finds a window from a window association list using base address of the window.
    @param aw The window association list of a process.
    @param addr The base address of the target window.
    @return Pointer of the window association entry of the target window if found, NULL otherwise.
 */
struct w_associated_window *w_associate_find(struct w_associated_windowlist *aw, vaddr_t addr);

/*! @brief Finds a window from a window association list using window ID of the window.
    @param aw The window association list of a process.
    @param winID The window ID of the target window.
    @return Pointer of the window association entry of the target window if found, NULL otherwise.
 */
struct w_associated_window *w_associate_find_winID(struct w_associated_windowlist *aw, int winID);

/*! @brief Checks if a window can be added to the window association list of a process.
    @param aw The window association list of a process.
    @param offset The base address of the window to be checked.
    @param size The size of the window to be checked.
    @return TRUE if window can be added, FALSE window conflicts.
 */
bool w_associate_check(struct w_associated_windowlist *aw, vaddr_t offset, vaddr_t size);

/*! @brief Finds a window that entirely contains the given range.
    @param aw The window association list of a process.
    @param offset The base address of the range to look for.
    @param size The size of the range to look for.
    @return Pointer of the window association entry  if found, NULL otherwise.
 */
struct w_associated_window *w_associate_find_range(struct w_associated_windowlist *aw,
        vaddr_t offset, vaddr_t size);

#endif /* _REFOS_PROCESS_SERVER_SYSTEM_MEMSERV_WINDOW_SEGMENTS_H_ */
