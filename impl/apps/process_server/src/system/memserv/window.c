 /*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <sel4utils/vspace.h>

#include "window.h"
#include "../../badge.h"
#include "../../state.h"

#include "../addrspace/vspace.h"
#include "../process/pid.h"
#include "../process/process.h"

/*! @file
    @brief Manages and keeps track of memory windows. */

#define W_INITIAL_SIZE 4

/*! @brief Internal helper function to switch a window between modes.

    It will first release all the previous stored mode related objects, and if the window wasn't
    empty to start with, unmap the vspace range (so new window data isn't munged with old mapped
    pages). Then it sets the mode variable of the window. This means that using this helper function
    to set the mode to W_MODE_EMPTY will effectively delete all the mode-related objects and unmap
    old stuff in the window.

    @param window The window to switch mode for. (No ownership)
    @param mode The new mode to switch to.
*/
static void
window_switch_mode(struct w_window* window, enum w_window_mode mode)
{
    /* Clean up all windoe mode states. */
    assert(window && window->magic == W_MAGIC);

    /* Free the pager capability. */
    if (window->pager.capPtr) {
        vka_cnode_revoke(&window->pager);
        vka_cnode_delete(&window->pager);
        vka_cspace_free(&procServ.vka, window->pager.capPtr);   
        window->pager.capPtr = 0;
        window->pagerPID = PID_NULL;
    }

    /* Unreference the associated dataspace. */
    if (window->ramDataspace) {
        ram_dspace_unref(window->ramDataspace->parentList, window->ramDataspace->ID);
        window->ramDataspace = NULL;
        window->ramDataspaceOffset = (vaddr_t) 0;
    }

    if (window->mode != W_MODE_EMPTY) {
        /* There was something previous here. So we must first unmap this window. */
        if (window->parentList == &procServ.windowList && window->clientOwnerPID != PID_NULL) {
            /* Get owner client PCB to access its vspace. */
            struct proc_pcb* clientPCB = pid_get_pcb(&procServ.PIDList,
                                                     window->clientOwnerPID);
            if (clientPCB) {
                vs_unmap_window(&clientPCB->vspace, window->wID);
            }
        }
    }
    window->mode = mode;
}

/*! @brief Window OAT creation callback function.
    
    This callback function is called by the OAT allocation helper library in <data_struct/coat.h>,
    in order to create window objects.
*/    
static cvector_item_t
window_oat_create(coat_t *oat, int id, uint32_t arg[COAT_ARGS])
{
    struct w_window *nw = kmalloc(sizeof(struct w_window));
    if (!nw) {
        ROS_ERROR("window_oat_create out of memory!");
        return NULL;
    }
    memset(nw, 0, sizeof(struct w_window));
    nw->magic = W_MAGIC;
    nw->wID = id;
    nw->parentList = (struct w_list*) oat;
    nw->pagerPID = PID_NULL;
    assert(nw->parentList->magic == W_LIST_MAGIC);

    /* Mint the badged capability representing this window. */
    nw->capability = procserv_mint_badge(W_BADGE_BASE + id);
    if (!nw->capability.capPtr) {
        ROS_ERROR("window_oat_create could not mint cap!");
        free(nw);
        return NULL;
    }
    return (cvector_item_t) nw;
}

/*! @brief Window OAT deletion callback function.
    
    This callback function is called by the OAT allocation helper library in <data_struct/coat.h>,
    in order to delete window objects previously created by window_oat_create().
*/  
static void
window_oat_delete(coat_t *oat, cvector_item_t *obj)
{
    struct w_window *window = (struct w_window *) obj;
    assert(window);
    assert(window->magic == W_MAGIC);

    /* Clean up window mode state. */
    window_switch_mode(window, W_MODE_EMPTY);

    /* Free the reservation. */
    if (window->reservation.res != NULL) {
        assert(window->vspace);
        vspace_free_reservation(window->vspace, window->reservation);
        window->reservation.res = NULL;
    }

    /* Free the capability. */
    if (window->capability.capPtr) {
        vka_cnode_revoke(&window->capability);
        vka_cnode_delete(&window->capability);
        vka_cspace_free(&procServ.vka, window->capability.capPtr);
    }

    /* Free the actual window structure. */
    memset(window, 0, sizeof(struct w_window));
    kfree(window);
}

/* --------------------------------------- Window functions ------------------------------------- */

void
w_init(struct w_list *wlist)
{
    assert(wlist);
    dprintf("Initialising window allocation table (max %d windows).\n", W_MAX_WINDOWS);

    /* Configure the object allocation table creation / deletion callback func pointers. */
    wlist->windows.oat_expand = NULL;
    wlist->windows.oat_create = window_oat_create;
    wlist->windows.oat_delete = window_oat_delete;
    wlist->magic = W_LIST_MAGIC;

    /* Initialise the allocation table. */
    coat_init(&wlist->windows, 1, W_MAX_WINDOWS);
}

void
w_deinit(struct w_list *wlist)
{
    assert(wlist);
    coat_release(&wlist->windows);
}

struct w_window*
w_create_window(struct w_list *wlist, vaddr_t size, int ownerPID, seL4_Word permissions,
    vspace_t *vspace, reservation_t reservation, bool cacheable)
{
    assert(wlist);
    uint32_t arg[COAT_ARGS];
    struct w_window* w = NULL;

    /* Allocate the window ID. */
    int ID = coat_alloc(&wlist->windows, arg, (cvector_item_t *) &w);
    if (ID == W_INVALID_WINID) {
        ROS_ERROR("Could not allocate window.");
        return NULL;
    }

    /* Fill in the structure. */
    assert(w != NULL && w->magic == W_MAGIC);
    w->clientOwnerPID = ownerPID;
    w->size = size;
    w->mode = W_MODE_EMPTY;
    w->permissions = permissions;
    w->vspace = vspace;
    w->reservation = reservation;
    w->cacheable = cacheable;
    return w;
}

int
w_delete_window(struct w_list *wlist, int windowID)
{
    coat_free(&wlist->windows, windowID);
    return ESUCCESS;
}

struct w_window*
w_get_window(struct w_list *wlist, int windowID)
{
    if (windowID <= W_INVALID_WINID || windowID >= W_MAX_WINDOWS) {
        /* Invalid ID. */
        return NULL;
    }
    struct w_window* window = (struct w_window*) coat_get(&wlist->windows, windowID);
    if (!window) {
        /* No such window ID exists. */
        return NULL;
    }
    assert(window->magic == W_MAGIC);
    return window;
}

void
w_set_pager_endpoint(struct w_window *window, cspacepath_t endpoint, uint32_t pid)
{
    assert(window && window->magic == W_MAGIC);
    if (!endpoint.capPtr) {
        window_switch_mode(window, W_MODE_EMPTY);
        return;
    }
    window_switch_mode(window, W_MODE_PAGER);
    window->pager = endpoint;
    window->pagerPID = pid;
}

void
w_set_anon_dspace(struct w_window *window, struct ram_dspace *dspace, vaddr_t offset)
{
    assert(window && window->magic == W_MAGIC);
    if (!dspace) {
        window_switch_mode(window, W_MODE_EMPTY);
        return;
    }
    assert(dspace->magic = RAM_DATASPACE_MAGIC);
    window_switch_mode(window, W_MODE_ANONYMOUS);
    window->ramDataspace = dspace;
    window->ramDataspaceOffset = offset;
    ram_dspace_ref(dspace->parentList, dspace->ID);
}

void
w_purge_dspace(struct w_list *wlist, struct ram_dspace *dspace)
{
    assert(wlist);
    for (int i = 1; i < W_MAX_WINDOWS; i++) {
        struct w_window *window = w_get_window(wlist, i);

        if (window && window->ramDataspace) {
            if (window->ramDataspace == dspace || (window->ramDataspace->parentList == 
                    dspace->parentList && window->ramDataspace->ID == dspace->ID)) {
                /* Set it back to empty. Not that this will unmap the window. */
                window_switch_mode(window, W_MODE_EMPTY);
            }
        }
    }
}

int
w_resize_window(struct w_window *window, vaddr_t vaddr, vaddr_t size)
{
    assert(window && window->magic == W_MAGIC);
    if (size == window->size) {
        /* Nothing to do here. */
        return ESUCCESS;
    }

    int error = sel4utils_move_resize_reservation(window->vspace, window->reservation,
            (void*) vaddr, size);
    if (error) {
        return EINVALIDWINDOW;
    }

    dvprintf("window ID %d resized from size 0x%x to 0x%x\n", window->wID, window->size, size);
    window->size = size;
    return ESUCCESS;
}

/* -------------------------------- Window Association functions -------------------------------- */

static int
w_associate_compare(const void * a, const void * b)
{
    return ( ((struct w_associated_window*) a)->offset <
             ((struct w_associated_window*) b)->offset ?
            -1 : 1
    );
}

/*! @brief Updates the window association list by sorting the base address of the window.
    @param aw The window association list of a process.
 */
static void
w_associate_update(struct w_associated_windowlist *aw)
{
    assert(aw);
    qsort(aw->associated, aw->numIndex, sizeof(struct w_associated_window), w_associate_compare);
    aw->updated = true;
}

static void
w_associate_reserve(struct w_associated_windowlist *aw, int num) {
    if (num < aw->associatedVectorSize) {
        /* Nothing to do. */
        return;
    }
    if (aw->associatedVectorSize == 0 || aw->associated == NULL) {
        aw->associatedVectorSize = num;
        if (aw->associatedVectorSize < W_INITIAL_SIZE) {
            aw->associatedVectorSize = W_INITIAL_SIZE;
        }
        aw->associated = kmalloc(sizeof(struct w_associated_window) * aw->associatedVectorSize);
        assert(aw->associated);
        return;
    }
    aw->associatedVectorSize = (aw->associatedVectorSize * 2) + 1;
    aw->associated = krealloc(aw->associated, 
            sizeof(struct w_associated_window) * aw->associatedVectorSize);
    assert(aw->associated);
}

void
w_associate_init(struct w_associated_windowlist *aw)
{
    aw->associated = NULL;
    w_associate_clear(aw);
}

int
w_associate(struct w_associated_windowlist *aw, int winID, vaddr_t offset, vaddr_t size)
{
    assert(aw);
    assert(winID != W_INVALID_WINID);
    assert(winID > 0 && winID < W_MAX_WINDOWS);
    if (aw->numIndex >= W_MAX_ASSOCIATED_WINDOWS) {
        return ENOMEM;
    }
    w_associate_reserve(aw, aw->numIndex + 1    );
    aw->associated[aw->numIndex].winID = winID;
    aw->associated[aw->numIndex].offset = offset;
    aw->associated[aw->numIndex].size = size;
    aw->numIndex++;
    aw->updated = 0;
    return ESUCCESS;
}

void
w_associate_print(struct w_associated_windowlist *aw)
{
    assert(aw);
    for (int i = 0; i < aw->numIndex; i++) {
        dprintf("    • Associated window %d: winID %d addr 0x%x →→→ 0x%x, size 0x%x\n", i,
                aw->associated[i].winID, aw->associated[i].offset,
                aw->associated[i].offset + aw->associated[i].size,
                aw->associated[i].size);
    }
}

void
w_unassociate(struct w_associated_windowlist *aw, int winID)
{
    assert(aw);
    for (int i = 0; i < aw->numIndex; i++) {
        if (aw->associated[i].winID == winID) {
            aw->associated[i] = aw->associated[--aw->numIndex];
        }
    }
    /* Removing something from a sorted list doesn't make it unsorted. */
}

void
w_associate_clear(struct w_associated_windowlist *aw)
{
    assert(aw);
    aw->numIndex = 0;
    if (aw->associated != NULL) {
        kfree(aw->associated);
        aw->associated = NULL;
    }
    aw->associatedVectorSize = 0;
    /* An empty list is sorted. */
    aw->updated = true;
    /* Reserve an initial few window spots. */
    w_associate_reserve(aw, W_INITIAL_SIZE);
}

void
w_associate_release_associated_all_windows(struct w_list *wlist, struct w_associated_windowlist *aw)
{
    assert(aw && wlist);
    for (int i = 0; i < aw->numIndex; i++) {
        /* Delete this window from the window list. */
        w_delete_window(wlist, aw->associated[i].winID);
    }
    w_associate_clear(aw);
}

static inline char
w_associate_window_contains(struct w_associated_window *w, vaddr_t addr)
{
    return (w->offset <= addr &&
            w->offset + w->size > addr);
}

static int
w_associate_find_index(struct w_associated_windowlist *aw, vaddr_t addr)
{
    assert(aw);

    /* Re-sort the list if it's out of date. */
    if (!aw->updated) {
        w_associate_update(aw);
        assert(aw->updated);
    }

    /* Binary search for the associated window. */

    int startIndex = 0;
    int endIndex = aw->numIndex;
    int currentIndex = -1;
    char found = false;

    while (1) {
        currentIndex = (startIndex + endIndex) / 2;

        if (startIndex >= endIndex) {
            break;
        }

        assert(currentIndex >= 0);
        assert(currentIndex < aw->numIndex);

        if (w_associate_window_contains(&aw->associated[currentIndex], addr)) {
            /* We have found the correct window. */
            found = true;
            break;
        } else if (aw->associated[currentIndex].offset <= addr) {
            /* Guess is too small, search down the bigger half. */
            startIndex = currentIndex + 1;
        } else {
            /* Guess is too large, search down the smaller half. */
            endIndex = currentIndex;
        }
    }

    return found ? currentIndex : (-1 - currentIndex);
}

struct w_associated_window *
w_associate_find(struct w_associated_windowlist *aw, vaddr_t addr)
{
    int findResult = w_associate_find_index(aw, addr);
    return findResult < 0 ? NULL : &aw->associated[findResult];
}

struct w_associated_window *
w_associate_find_winID(struct w_associated_windowlist *aw, int winID)
{
    /* Re-sort the list if it's out of date. */
    if (!aw->updated) {
        w_associate_update(aw);
        assert(aw->updated);
    }

    for (int i = 0; i < aw->numIndex; i++) {
        if (aw->associated[i].winID == winID) {
            return &aw->associated[i];
        }
    }
    return NULL;
}

bool
w_associate_check(struct w_associated_windowlist *aw, vaddr_t offset, vaddr_t size)
{
    /* Search for both the start vaddress and end vaddress. If either of them are valid,
       then this window is invalid. Otherwise if the searched index are the same, then this
       means there was not a window in between offset and size. 
    */
    int findResult1 = w_associate_find_index(aw, offset);
    if (findResult1 >= 0) {
        return false;
    }
    int findResult2 = w_associate_find_index(aw, (offset + size - 1));
    if (findResult2 >= 0) {
        return false;
    }
    return (findResult1 == findResult2) ? true : false;
}

struct w_associated_window *
w_associate_find_range(struct w_associated_windowlist *aw, vaddr_t offset, vaddr_t size)
{
    /* Search for both the start vaddress and end vaddress. If they both point to the same window,
       then that window must contain the entire range. */
    int findResult1 = w_associate_find_index(aw, offset);
    if (findResult1 < 0) {
        return NULL;
    }
    int findResult2 = w_associate_find_index(aw, (offset + size - 1));
    if (findResult2 < 0) {
        return NULL;
    }
    if (findResult1 != findResult2) {
        /* A window ended and another window started in between our given range. This means
           the given range is NOT contained by a single window. */
        return NULL;
    }
    return &aw->associated[findResult1];
}
