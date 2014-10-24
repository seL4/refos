/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "../../common.h"
#include "../../state.h"
#include "vspace.h"
#include "../memserv/window.h"
#include "../process/pid.h"
#include "../process/process.h"
#include <autoconf.h>
#include <refos/refos.h>
#include <sel4utils/vspace.h>

/*! @file
    @brief Client address space objects. */

#define VSPACE_WINDOW_VERBOSE_DEBUG true

/* -------------------- VSpace Helper Library Callback Functions ---------------------------------*/

static void
vs_vspace_allocated_object_bookkeeping_callback(void *allocated_object_cookie, vka_object_t object)
{
    struct vs_vspace *vs = (struct vs_vspace *) allocated_object_cookie;
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);

    /* Create out own copy of this allocated object struct. */
    vka_object_t *kobj = kmalloc(sizeof(vka_object_t));
    if (!kobj) {
        ROS_WARNING("Could not allocate new vka_object_t to book keep vspace allocated object.");
        ROS_WARNING("Object will be LEAKED.");
        return;
    }
    memcpy(kobj, &object, sizeof(vka_object_t));

    /* Add to end of free list. */
    cvector_add(&vs->kobjVSpaceAllocatedFreelist, (cvector_item_t) kobj);
}


/* ---------------------------------- VSpace struct ----------------------------------------------*/

int
vs_initialise(struct vs_vspace *vs, uint32_t pid)
{
    assert(vs);
    dvprintf("    Initialising a vspace...\n");
    memset(vs, 0, sizeof(*vs));
    int error = ESUCCESS;

    vs->magic = REFOS_VSPACE_MAGIC;
    vs->ref = 1;
    vs->pid = pid;

    /* Initialise the free list vector. */
    cvector_init(&vs->kobjVSpaceAllocatedFreelist);

    /* Initialise window association list. */
    w_associate_init(&vs->windows);

    /* Assign a kernel page directory. */
    dvprintf("        Assigning new kernel page directory objects...\n");
    struct pd_info pdi = pd_assign(&procServ.PDList);
    if (!pdi.kpdObject | !pdi.kcnodeObject) {
        ROS_ERROR("Failed to allocate page directory for new process.");
        error = ENOMEM;
        goto exit1;   
    }
    vs->kpd = pdi.kpdObject;

    /* Create the CSpace path associated with this address space's root CNode. */
    cspacepath_t pathTemp;
    vs->cspaceUnguarded = pdi.kcnodeObject;
    vs->cspaceSize = REFOS_CSPACE_RADIX;
    vka_cspace_make_path(&procServ.vka, vs->cspaceUnguarded, &pathTemp);

    /* Mint a guarded cspace from the created cspace. */
    dvprintf("        Allocating cslot for guarded cspace...\n");
    error = vka_cspace_alloc_path(&procServ.vka, &vs->cspace);
    if (error) {
        ROS_ERROR("Failed to allocate guarded cspace cslot: error %d\n", error);
        error = ENOMEM;
        goto exit2;
    }

    dvprintf("        Minting guarded cspace...\n");
    vs->cspaceGuardData = seL4_CapData_Guard_new(0, REFOS_CSPACE_GUARD);
    error = vka_cnode_mint(&vs->cspace, &pathTemp, seL4_AllRights, vs->cspaceGuardData);
    assert(error == seL4_NoError);
    (void) error;

    /* Self-reference so the thread knows about its own cspace. */
    dvprintf("        Copying self-reference cap into REFOS_CSPACE cslot...\n");
    error = seL4_CNode_Copy(
            vs->cspace.capPtr, REFOS_CSPACE, REFOS_CDEPTH,
            vs->cspace.root, vs->cspace.capPtr, vs->cspace.capDepth,
            seL4_AllRights
    );
    if (error) {
        ROS_ERROR("Could not copy self reference cspace cap: error %d\n", error);
        error = EINVALID;
        goto exit3;
    }

    /* Create a vspace object to keep reservations book-keeping. */
    dvprintf("        Initialising child sel4utils vspace struct...\n");
    error = sel4utils_get_vspace (
            &procServ.vspace, &vs->vspace, &vs->vspaceData,
            &procServ.vka, vs->kpd,
            vs_vspace_allocated_object_bookkeeping_callback, (void*) vs
    );
    if (error) {
        ROS_ERROR("Failed to initialise sel4utils vspace struct: %d\n", error);
        error = ENOMEM;
        goto exit3;
    }

    dvprintf("        VSpace setup OK, new vspace is ready to go.\n");
    return ESUCCESS;

    /* Exit stack. */
exit3:
    vka_cnode_delete(&vs->cspace);
    vka_cspace_free(&procServ.vka, vs->cspace.capPtr);
exit2:
    vka_cnode_revoke(&pathTemp);
    pd_free(&procServ.PDList, vs->kpd);
exit1:
    return error;
}

static void
vs_release(struct vs_vspace *vs)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);
    assert(vs->magic == REFOS_VSPACE_MAGIC);
    assert(vs->ref == 0);
    dvprintf("     Releasing VSpace PID %d...\n", vs->pid);
    cspacepath_t pathTemp;

    /* Clear the associated windows list. */
    dvprintf("         Clearing VSpace associated window list...\n");
    for (int i = 0; i < vs->windows.numIndex; i++) {
        vs_unmap_window(vs, vs->windows.associated[i].winID);
    }
    w_associate_release_associated_all_windows(&procServ.windowList, &vs->windows);

    /* Free the allocated vspace book-keeping objects. */
    dvprintf("         Releasing VSpace list of vspace bookkeeping kobjs...\n");
    int c = cvector_count(&vs->kobjVSpaceAllocatedFreelist);
    for (int i = 0; i < c; i++) {
        vka_object_t *kobj = (vka_object_t *) cvector_get(&vs->kobjVSpaceAllocatedFreelist, i);
        assert(kobj);
        vka_cspace_make_path(&procServ.vka, kobj->cptr, &pathTemp);
        vka_cnode_revoke(&pathTemp);
        vka_cnode_delete(&pathTemp);
        vka_free_object(&procServ.vka, kobj);
        kfree(kobj);
        cvector_set(&vs->kobjVSpaceAllocatedFreelist, i, (cvector_item_t) NULL);
    }
    cvector_reset(&vs->kobjVSpaceAllocatedFreelist);

    /* Teardown the vspace. */
    vspace_tear_down(&vs->vspace, VSPACE_FREE);

    /* Free the allocated kernel objects. */
    dvprintf("         Releasing VSpace kobjs...\n");
    vka_cnode_revoke(&vs->cspace);
    vka_cnode_delete(&vs->cspace);
    vka_cspace_free(&procServ.vka, vs->cspace.capPtr);

    /* Note the unguarded original CNode capability belongs to the PD, we don't have ownership of
       that. pd_free here will release it back into the pool to be reused. */
    dvprintf("         Releasing PD kobjs...\n");
    pd_free(&procServ.PDList, vs->kpd);
    memset(vs, 0, sizeof(struct vs_vspace));
}

void
vs_ref(struct vs_vspace *vs)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);
    vs->ref++;
}

void
vs_unref(struct vs_vspace *vs)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);
    vs->ref--;
    if (vs->ref <= 0) {
        /* Release this vspace. */
        vs_release(vs);
    }
}

void
vs_track_obj(struct vs_vspace *vs, vka_object_t object)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);
    vs_vspace_allocated_object_bookkeeping_callback((void *)vs, object);;
}
/* ---------------------------------- VSpace windows ---------------------------------------------*/

int
vs_create_window(struct vs_vspace *vs, vaddr_t vaddr, vaddr_t size, seL4_Word permissions,
                 bool cacheable, int *winID)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);
    assert(winID != NULL);
    *winID = W_INVALID_WINID;
    int error = EINVALID;

    /* Check the window segment with window association list for any conflicts. */
    if (w_associate_check(&vs->windows, vaddr, size) == false) {
        dvprintf("memory window overlaps with another one.\n");

        #if VSPACE_WINDOW_VERBOSE_DEBUG
        dvprintf("―――――――――――――――――――――――――― Detailed Report ――――――――――――――――――――――――\n");
        dvprintf("attempted to create window 0x%x →→→ 0x%x.\n", vaddr, vaddr + size);
        w_associate_print(&vs->windows);
        dvprintf("―――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――\n");
        #endif

        return EINVALIDWINDOW;
    }

    /* Create the seL4 vspace reservation. */
    reservation_t r = vspace_reserve_range_at(&vs->vspace, (void*) vaddr, (size_t) size,
            w_convert_permission_to_caprights(permissions), cacheable);
    if (r.res == NULL) {
        dvprintf("vspace reservation failed.\n");
        return EINVALIDWINDOW;
    }

    /* Add to global window list. */
    struct w_window *window = w_create_window(&procServ.windowList, size, vs->pid,
                                              permissions, &vs->vspace, r, cacheable);
    if (!window) {
        ROS_ERROR("window creation failed.\n");
        error = ENOMEM;
        goto exit0;
    }
    assert(window->wID != W_INVALID_WINID);

    /* Now associate the window. */
    error = w_associate(&vs->windows, window->wID, vaddr, size);
    if (error) {
        ROS_ERROR("Window associate failed.");
        assert(!"Window associate failed even after check. This is likely a bug.");
        error = EINVALID;
        goto exit1;
    }

    *winID = window->wID;
    return ESUCCESS;

    /* Exit stack. */
exit1: 
    /* Note that ownership of reservation is transferred to window. */
    w_delete_window(&procServ.windowList, window->wID);
    return error;
exit0:
    vspace_free_reservation(&vs->vspace, r);
    return error;
}

void
vs_delete_window(struct vs_vspace *vs, int wID)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);

    /* Find the associated window. */
    struct w_associated_window *awindow = w_associate_find_winID(&vs->windows, wID);
    if (!awindow || awindow->winID != wID) {
        ROS_WARNING("vs_delete_window: no such window exists.");
        return;
    }

    /* Unmap everything in the associated window. */
    vs_unmap_window(vs, wID);

    /* Unassociate this window. */
    w_unassociate(&vs->windows, wID);

    /* Delete the window from the global window list. */
    int error = w_delete_window(&procServ.windowList, wID);
    assert(error == ESUCCESS);
    (void) error;
}

int
vs_resize_window(struct vs_vspace *vs, int wID, vaddr_t size)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);
    if (!size) {
        return EINVALIDPARAM;
    }

    /* Find the associated window. */
    struct w_associated_window *awindow = w_associate_find_winID(&vs->windows, wID);
    if (!awindow || awindow->winID != wID) {
        ROS_WARNING("vs_resize_window: no such window assoc exists.");
        return EINVALIDWINDOW;
    }

    /* Find the global window structure. */
    struct w_window* window = w_get_window(&procServ.windowList, wID);
    if (!window) {
        ROS_WARNING("vs_resize_window: no such window exists.");
        return EINVALIDWINDOW;
    }
    assert(awindow->offset == (vaddr_t) reservation_to_res(window->reservation)->start);

    if (size > awindow->size) {
        /* If the new size is larger, we need to check the new portion of the window. */
        vaddr_t sizeIncrease = size - awindow->size;
        if (!w_associate_check(&vs->windows, awindow->offset + awindow->size, sizeIncrease)) {
            dvprintf("memory window resize fail: overlaps another window.\n");

            #if VSPACE_WINDOW_VERBOSE_DEBUG
            dvprintf("―――――――――――――――――――――――――― Detailed Report ――――――――――――――――――――――――\n");
            dvprintf("attempted to resize window 0x%x →→→ 0x%x to 0x%x.\n", awindow->offset,
                    awindow->offset + awindow->size, awindow->offset + size);
            w_associate_print(&vs->windows);
            dvprintf("―――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――\n");
            #endif

            return EINVALIDPARAM;
        }
    } else if (size < awindow->size) {
        /* If new size is smaller, we need to unmap. */
        vaddr_t sizeDecrease = awindow->size - size;
        int npages = (sizeDecrease / REFOS_PAGE_SIZE) + ((sizeDecrease % REFOS_PAGE_SIZE) ? 1 : 0);
        int error = vs_unmap(vs, REFOS_PAGE_ALIGN(awindow->offset + size), npages);
        if (error) {
            ROS_WARNING("vs_resize_window: failed to map window.");
            return error;
        }
    }

    /* Actually perform the window resize. This will also update the vspace reservation. */
    int error = w_resize_window(window, awindow->offset, size);
    if (error) {
        ROS_WARNING("vs_resize_window: failed to resize window. Shouldn't happen.");
        return error;
    }

    awindow->size = size;
    return ESUCCESS;
}

/* ---------------------------------- VSpace mapping ---------------------------------------------*/

int
vs_map(struct vs_vspace *vs, vaddr_t vaddr, seL4_CPtr frames[], int nFrames)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);
    int error = EINVALID;
    vaddr = REFOS_PAGE_ALIGN(vaddr);

    /* Check the window association to make sure there exists a window there. */
    struct w_associated_window *awindow = w_associate_find_range(&vs->windows, vaddr,
            nFrames * REFOS_PAGE_SIZE);
    if (!awindow) {
        dvprintf("could not find window association.\n");

        #if VSPACE_WINDOW_VERBOSE_DEBUG
        dvprintf("―――――――――――――――――――――――――― Detailed Report ――――――――――――――――――――――――\n");
        dvprintf("attempted to map vaddr range 0x%x →→→ 0x%x.\n", vaddr,
                vaddr + nFrames * REFOS_PAGE_SIZE);
        w_associate_print(&vs->windows);
        dvprintf("―――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――\n");
        #endif

        return EINVALIDWINDOW;
    }

    /* Retrieve the window structure. */
    struct w_window* window = w_get_window(&procServ.windowList, awindow->winID);
    if (!window) {
        dvprintf("could not find window.\n");
        assert(!"window book keeping bug. Should not happen.");
        return EINVALIDWINDOW;
    }
    assert(window->vspace == &vs->vspace);

    /* Check that every frame in the region is unmapped. */
    vaddr = REFOS_PAGE_ALIGN(vaddr);
    for (vaddr_t va = 0; va < nFrames; va++) {
        seL4_CPtr existingFrame = vspace_get_cap(&vs->vspace,
                (void*) (vaddr + va * REFOS_PAGE_SIZE));
        if (existingFrame) {
            /* There's already mapped frame here. */
            return EUNMAPFIRST;
        }
    }

    /* Make a copy of every cap given. */
    seL4_CPtr* frameCopy = malloc(sizeof(seL4_CPtr) * nFrames);
    if (!frameCopy) {
        ROS_ERROR("Could not allocate frame copy array, procserv out of memory.\n");
        return ENOMEM;
    }
    memset(frameCopy, 0, sizeof(seL4_CPtr) * nFrames);
    for (int i = 0; i < nFrames; i++) {
        vka_cspace_alloc(&procServ.vka, &frameCopy[i]);
        if (!frameCopy[i]) {
            ROS_ERROR("Could not allocate cspace to copy array.\n");
            error = ENOMEM;
            goto exit1;
        }
        cspacepath_t pathDest, pathSrc;
        vka_cspace_make_path(&procServ.vka, frameCopy[i], &pathDest);
        vka_cspace_make_path(&procServ.vka, frames[i], &pathSrc);
        vka_cnode_copy(&pathDest, &pathSrc, seL4_AllRights);
    }

    /* Map pages at the vspace reservation. */
    error = vspace_map_pages_at_vaddr(&vs->vspace, frameCopy, NULL, (void*) vaddr, nFrames,
                                          seL4_PageBits, window->reservation);
    if (error) {
        dvprintf("could not map pages into vaddr 0x%x. error: %d\n", (uint32_t) vaddr, error);
        error = EUNMAPFIRST;
        goto exit1;
    }

    /* Flush the page caches. */
    procserv_flush(frameCopy, nFrames);

    dvprintf("mapping vaddr 0x%x OK.\n", (uint32_t) vaddr);
    free(frameCopy);
    return ESUCCESS;

    /* Exit stack. */
exit1:
    for (int i = 0; i < nFrames; i++) {
        if (frameCopy[i]) {
            cspacepath_t path;
            vka_cspace_make_path(&procServ.vka, frameCopy[i], &path);
            vka_cnode_revoke(&path);
            vka_cnode_delete(&path);
            vka_cspace_free(&procServ.vka, frameCopy[i]);
        }
    }
    free(frameCopy);
    return error;
}

int
vs_map_across_vspace(struct vs_vspace *vsSrc, vaddr_t vaddrSrc, struct w_window *windowDest,
                     uint32_t windowDestOffset, struct proc_pcb **outClientPCB)
{
    assert(vsSrc && vsSrc->magic == REFOS_VSPACE_MAGIC);
    assert(windowDest && windowDest->magic == W_MAGIC);

    /* Find the cap in the source vspace's pagetable. */
    seL4_CPtr frameCap = vspace_get_cap(&vsSrc->vspace, (void*) vaddrSrc);
    if (!frameCap) {
        dvprintf("vs_map_across_vspace could not find source frame.\n");
        return EINVALIDPARAM;
    }

    /* Verify that the offset is within the window limits. */
    if (windowDestOffset >= windowDest->size) {
        ROS_ERROR("invalid window offset address!\n");
        return EINVALIDPARAM;
    }

    /* Find the client which this window lives in. */
    struct proc_pcb *clientPCB = pid_get_pcb(&procServ.PIDList, windowDest->clientOwnerPID);
    if (!clientPCB) {
        ROS_ERROR("could not find window's corresponding client.\n");
        return EINVALIDWINDOW;
    }
    if (outClientPCB) {
        (*outClientPCB) = clientPCB;
    }

    /* Check that this client actually has its own window mapped. */
    struct w_associated_window *wa = w_associate_find_winID(&clientPCB->vspace.windows,
                                                            windowDest->wID);
    if (!wa) {
        ROS_ERROR("client did not map its window, so invalid map call.\n");
        return EINVALIDWINDOW;
    }

    return vs_map(&clientPCB->vspace, wa->offset + windowDestOffset, &frameCap, 1);
}

int
vs_map_device(struct vs_vspace *vs, struct w_window *window, uint32_t windowOffset,
              uint32_t paddr , uint32_t size, bool cached)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);
    assert(window && window->magic == W_MAGIC);

    if (size != REFOS_PAGE_SIZE) {
        ROS_WARNING("Large device frames not supported yet.");
        assert(!"Large device frames not implemented.");
        return EUNIMPLEMENTED;
    }

    /* Verify that the offset is within the window limits. */
    if (windowOffset + size > window->size) {
        ROS_ERROR("invalid window offset address!\n");
        return EINVALIDPARAM;
    }

    /* Check that this client actually has its own window mapped. */
    struct proc_pcb* clientPCB = pid_get_pcb(&procServ.PIDList, window->clientOwnerPID);
    if (!clientPCB) {
        ROS_ERROR("invalid window owner!\n");
        return EINVALID;
    }
    assert(clientPCB->magic == REFOS_PCB_MAGIC);
    if (&clientPCB->vspace != vs) {
        return EACCESSDENIED;
    }
    struct w_associated_window *wa = w_associate_find_winID(&clientPCB->vspace.windows,
                                                            window->wID);
    if (!wa) {
        ROS_ERROR("client did not map its window, so invalid map call.\n");
        return EINVALIDWINDOW;
    }

    /* Check window cacheable state matches with requested cached state. */
    if (cached != window->cacheable) {
        ROS_WARNING("Window cachable and frame cache request mismatch. Access denied.");
        return EACCESSDENIED;
    }

    /* Find the device cap. */
    cspacepath_t deviceFrame = procserv_find_device((void*) paddr, size);
    if (deviceFrame.capPtr == 0) {
        ROS_WARNING("No such device.");
        return EFILENOTFOUND;
    }
    dvprintf("Device 0x%x found at cslot 0x%x...\n", paddr, deviceFrame.capPtr);

    /* Map the device frame. */
    vaddr_t vaddr = wa->offset + windowOffset;
    int error = vs_map(vs, vaddr, &deviceFrame.capPtr, 1);
    if (error != ESUCCESS) {
        ROS_WARNING("Failed to map device.");
        return error;
    }

    vka_cnode_delete(&deviceFrame);
    vka_cspace_free(&procServ.vka, deviceFrame.capPtr);
    return ESUCCESS;
}

static void
vs_unmap_frame(struct vs_vspace *vs, vaddr_t vaddr)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);

    /* Find the cap in the vspace's pagetable. */
    seL4_CPtr frameCap = vspace_get_cap(&vs->vspace, (void*) vaddr);
    if (!frameCap) {
        return;
    }

    /* Unmap the page & clear the pagetable entries. */
    vspace_unmap_pages(&vs->vspace, (void*) vaddr, 1, seL4_PageBits, VSPACE_PRESERVE);

    /* Revoke and delete the cap. */
    cspacepath_t path;
    vka_cspace_make_path(&procServ.vka, frameCap, &path);
    vka_cnode_revoke(&path);
    vka_cnode_delete(&path);
    vka_cspace_free(&procServ.vka, frameCap);

    dvprintf("vs_unmap_frame 0x%x OK.\n", (uint32_t) vaddr);
}

int
vs_unmap(struct vs_vspace *vs, vaddr_t vaddr, int nFrames)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);

    /* Check the window association to make sure there exists a window there. */
    struct w_associated_window *awindow = w_associate_find_range(&vs->windows, vaddr,
            nFrames * REFOS_PAGE_SIZE);
    if (!awindow) {
        dvprintf("could not find window association for vaddr 0x%x.\n", (uint32_t) vaddr);
        return EINVALIDWINDOW;
    }

    /* Retrieve the window structure. */
    struct w_window* window = w_get_window(&procServ.windowList, awindow->winID);
    if (!window) {
        dvprintf("could not find window.\n");
        assert(!"window book keeping bug. Should not happen.");
        return EINVALIDWINDOW;
    }
    assert(window->vspace == &vs->vspace);

    /* Unmap pages in vspace. */
    for (vaddr_t va = 0; va < nFrames; va++) {
        vs_unmap_frame(vs, vaddr + va * REFOS_PAGE_SIZE);
    }
    return ESUCCESS;
}

void
vs_unmap_window(struct vs_vspace *vs, int wID)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);

    /* Find the associated window. */
    struct w_associated_window *awindow = w_associate_find_winID(&vs->windows, wID);
    if (!awindow || awindow->winID != wID) {
        return;
    }

    /* Retrieve the window structure. */
    struct w_window* window = w_get_window(&procServ.windowList, awindow->winID);
    if (!window) {
        dvprintf("could not find window.\n");
        assert(!"window book keeping bug. Should not happen.");
        return;
    }
    assert(window->vspace == &vs->vspace);

    /* Unmap everything in the associated window. */
    int nFrames = (awindow->size / REFOS_PAGE_SIZE) + ((awindow->size % REFOS_PAGE_SIZE) ? 1 : 0);
    for (vaddr_t va = 0; va < nFrames; va++) {
        vs_unmap_frame(vs, awindow->offset + va * REFOS_PAGE_SIZE);
    }
}

cspacepath_t
vs_get_frame(struct vs_vspace *vs, vaddr_t vaddr)
{
    assert(vs && vs->magic == REFOS_VSPACE_MAGIC);
    cspacepath_t path;
    memset(&path, 0, sizeof(cspacepath_t));
    seL4_CPtr frameCap = vspace_get_cap(&vs->vspace, (void*) vaddr);
    vka_cspace_make_path(&procServ.vka, frameCap, &path);
    return path;
}
