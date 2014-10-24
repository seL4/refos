/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "dataspace.h"
#include "../../badge.h"
#include "../../state.h"
#include "../process/pid.h"
#include <refos/refos.h>

/*! @file
    @brief Process Server anon RAM dataspace implementation. */

extern seL4_MessageInfo_t _dispatcherEmptyReply;

/* --------------------------- RAM dataspace OAT callback functions ----------------------------- */

/*! @brief Dataspace OAT creation callback function.
    
    This callback function is called by the OAT allocation helper library in <data_struct/coat.h>,
    in order to create dataspace objects. Here we malloc some memory for the structure, initialise
    its data structures, initialise its page array, and mint the dataspace badge capability.

    @param oat The parent dataspace list (struct ram_dspace_list*).
    @param id The dataspace ID allocated by the OAT table.
    @param arg Arg[0] is the dataspace size, the rest unused.
    @return A new dataspace (struct ram_dspace *) on success, NULL on error. (Transfers ownership)
*/
static cvector_item_t
ram_dspace_oat_create(coat_t *oat, int id, uint32_t arg[COAT_ARGS])
{
    struct ram_dspace *ndspace = malloc(sizeof(struct ram_dspace));
    if (!ndspace) {
        ROS_ERROR("ram_dspace_oat_create out of memory!");
        return NULL;
    }
    memset(ndspace, 0, sizeof(struct ram_dspace));
    ndspace->magic = RAM_DATASPACE_MAGIC;
    ndspace->ID = id;
    ndspace->npages = (arg[0] / REFOS_PAGE_SIZE) + ((arg[0] % REFOS_PAGE_SIZE) ? 1 : 0);
    ndspace->ref = 1;
    ndspace->contentInitBitmask = NULL;
    ndspace->contentInitEP.capPtr = 0;
    ndspace->contentInitPID = PID_NULL;
    ndspace->parentList = (struct ram_dspace_list *) oat;
    assert(ndspace->parentList->magic == RAM_DATASPACE_LIST_MAGIC);

    /* Initialise content init list. */
    cvector_init(&ndspace->contentInitWaitingList);

    /* Create the page array. */
    ndspace->pages = kmalloc(sizeof(vka_object_t) * ndspace->npages);
    if (!ndspace->pages) {
        ROS_ERROR("ram_dspace_oat_create could not allocate page array, procserv out of mem!");
        goto exit1;
    }
    memset(ndspace->pages, 0, sizeof(vka_object_t) * ndspace->npages);

    /* Mint the badged capability representing this ram dataspace. */
    ndspace->capability = procserv_mint_badge(RAM_DATASPACE_BADGE_BASE + id);
    if (!ndspace->capability.capPtr) {
        ROS_ERROR("ram_dspace_oat_create could not mint cap!");
        goto exit2;
    }

    return (cvector_item_t) ndspace;

    /* Exit stack. */
exit2:
    assert(ndspace->pages);
    free(ndspace->pages);
exit1:
    free(ndspace);
    return NULL;
}

/*! @brief Dataspace OAT deletion callback function.
    
    This callback function is called by the OAT library defined in <data_struct/coat.h>, in order
    to delete dataspace objects created by ram_dspace_oat_create(). It unmaps the dataspace from
    all mapped windows, frees the caps, cslots, frames & frame arrays, and then the structure 
    itself.

    @param oat The parent dataspace list (struct ram_dspace_list*).
    @param obj The dataspace to delete (struct ram_dspace *) (Takes ownership).
*/
static void
ram_dspace_oat_delete(coat_t *oat, cvector_item_t *obj)
{
    struct ram_dspace *rds = (struct ram_dspace *) obj;
    assert(rds);
    assert(rds->magic == RAM_DATASPACE_MAGIC);
    
    /* Unmap everywhere where this dataspace has been mapped, by notifying the global procServ
       window list that this dataspace has been deleted. This will loop through the windowList,
       find any windows associated with this dspace, and unmap it and set back to EMPTY.
       This potentially does VSpace mapping operations.
    */
    w_purge_dspace(&procServ.windowList, rds);

    /* We now should be at ref 0. */
    if (rds->ref != 0) {
        ROS_WARNING("WARNING WARNING WARNING: RAM dataspace is being force released with hanging");
        ROS_WARNING("strong reference. This is most likely a procserv bug. The most likely cause");
        ROS_WARNING("is things such as a ring buffer object that has been attached to a");
        ROS_WARNING("dataspace, that belongs to a RDS list which has been freed.");
        assert(!"RAM dspace hanging reference. Process server bug.");
    }

    /* Free the content initialised bitmask. */
    if (rds->contentInitBitmask) {
        kfree(rds->contentInitBitmask);
        rds->contentInitBitmask = NULL;
    }

    /* Free the content init endpoint & cslot. */
    if (rds->contentInitEnabled) {
        assert(rds->contentInitEP.capPtr);
        vka_cnode_revoke(&rds->contentInitEP);
        vka_cnode_delete(&rds->contentInitEP);
        vka_cspace_free(&procServ.vka, rds->contentInitEP.capPtr);
        rds->contentInitPID = PID_NULL;
    }

    /* Clear the content init waiting list. */
    int waitingListCount = cvector_count(&rds->contentInitWaitingList);
    for (int i = 0; i < waitingListCount; i++) {
        struct ram_dspace_waiter *waiter = (struct ram_dspace_waiter *)
                cvector_get(&rds->contentInitWaitingList, i);
        assert(waiter && waiter->magic == RAM_DATASPACE_WAITER_MAGIC);
        assert(waiter->reply.capPtr);

        vka_cnode_revoke(&waiter->reply);
        vka_cnode_delete(&waiter->reply);
        vka_cspace_free(&procServ.vka, waiter->reply.capPtr);
        kfree(waiter);
    }
    cvector_free(&rds->contentInitWaitingList);

    /* Free the pages. */
    assert(rds->pages);
    for (int i = 0; i < rds->npages; i++) {
        if (rds->pages[i].cptr) {
            cspacepath_t path;
            vka_cspace_make_path(&procServ.vka, rds->pages[i].cptr, &path);
            vka_cnode_revoke(&path);
            if (rds->physicalAddrEnabled) {
                /* Frames belong to a device, we do not own this frame. Just delete the cslot. */
                vka_cnode_delete(&path);
                vka_cspace_free(&procServ.vka, path.capPtr);
            } else {
                /* We do own this anonymous dataspace frame. */
                vka_free_object(&procServ.vka, &rds->pages[i]);
            }
        }
    }
    kfree(rds->pages);

    /* Free the capability. */
    assert(rds->capability.capPtr);
    vka_cnode_revoke(&rds->capability);
    vka_cnode_delete(&rds->capability);
    vka_cspace_free(&procServ.vka, rds->capability.capPtr);

    /* Free the actual window structure. */
    free(rds);
}

/* ------------------------------- RAM dataspace table functions -------------------------------- */

/*! @brief Calculates the page index into the dataspace based on the nbytes offset.
    @param nbytes The offset into the ram dataspace.
    @return The page index into the dataspace.
 */
static inline uint32_t
ram_dspace_get_index(size_t nbytes)
{
    return (uint32_t)(nbytes / REFOS_PAGE_SIZE);
}

void
ram_dspace_init(struct ram_dspace_list *rdslist)
{
    assert(rdslist);
    dprintf("Initialising RAM dataspace allocation table (max %d dspaces).\n",
            RAM_DATASPACE_MAX_NUM_DATASPACE);

    /* Configure the object allocation table creation / deletion callback func pointers. */
    rdslist->allocTable.oat_expand = NULL;
    rdslist->allocTable.oat_create = ram_dspace_oat_create;
    rdslist->allocTable.oat_delete = ram_dspace_oat_delete;
    rdslist->magic = RAM_DATASPACE_LIST_MAGIC;

    /* Initialise the allocation table. */
    coat_init(&rdslist->allocTable, 1, RAM_DATASPACE_MAX_NUM_DATASPACE);
}

void
ram_dspace_deinit(struct ram_dspace_list *rdslist)
{
    assert(rdslist);
    for (int i = 1; i < RAM_DATASPACE_MAX_NUM_DATASPACE; i++) {
        struct ram_dspace *dspace = ram_dspace_get(rdslist, i);
        if (dspace) {
            assert(dspace->magic == RAM_DATASPACE_MAGIC);
            dspace->ref--;
        }
    }
    coat_release(&rdslist->allocTable);
}

struct ram_dspace *
ram_dspace_create(struct ram_dspace_list *rdslist, size_t size)
{
    assert(rdslist);
    uint32_t arg[COAT_ARGS];
    struct ram_dspace *dspace= NULL;

    /* Allocate the dataspace ID and structure. */
    arg[0] = (uint32_t) size;
    int ID = coat_alloc(&rdslist->allocTable, arg, (cvector_item_t *) &dspace);
    if (ID == RAM_DATASPACE_INVALID_ID) {
        ROS_ERROR("Could not allocate window.");
        return NULL;
    }
    assert(dspace && dspace->magic == RAM_DATASPACE_MAGIC);
    return dspace;
}

void
ram_dspace_ref(struct ram_dspace_list *rdslist, int ID)
{
    struct ram_dspace *dspace = ram_dspace_get(rdslist, ID);
    if (!dspace) {
        ROS_ERROR("ram_dspace_ref no such dataspace!");
        assert(!"ram_dspace_ref no such dataspace! Procserv book keeping error.");
        return;
    }
    assert(dspace->ref > 0);
    dspace->ref++;
}

void
ram_dspace_unref(struct ram_dspace_list *rdslist, int ID)
{
    struct ram_dspace *dspace = ram_dspace_get(rdslist, ID);
    if (!dspace) {
        ROS_ERROR("ram_dspace_unref no such dataspace!");
        assert(!"ram_dspace_unref no such dataspace! Procserv book keeping error.");
        return;
    }
    assert(dspace->ref > 0);
    dspace->ref--;
    if (dspace->ref == 0) {
        /* Last reference, delete the object. */
        coat_free(&rdslist->allocTable, ID);
    }
}
      
seL4_CPtr
ram_dspace_check_page(struct ram_dspace *dataspace, uint32_t offset)
{
    assert(dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);
    uint32_t idx = ram_dspace_get_index(offset);
    if (idx >= dataspace->npages) {
        /* Offset of of range. */
        return (seL4_CPtr) 0;
    }
    return dataspace->pages[idx].cptr;
}

seL4_CPtr
ram_dspace_get_page(struct ram_dspace *dataspace, uint32_t offset)
{
    assert(dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);
    uint32_t idx = ram_dspace_get_index(offset);
    if (idx >= dataspace->npages) {
        /* Offset of of range. */
        return (seL4_CPtr) 0;
    }
    if (!dataspace->pages[idx].cptr) {
        if (dataspace->physicalAddrEnabled) {
            /* Allocate a physical address device memory region frame to fill this page. */
            cspacepath_t deviceFrame = procserv_find_device(
                    (void*) dataspace->physicalAddr + idx * REFOS_PAGE_SIZE, REFOS_PAGE_SIZE);
            if (!deviceFrame.capPtr) {
                ROS_WARNING("Could not allocate frame object. No such device.");
                return (seL4_CPtr) 0;
            }
            memset(&dataspace->pages[idx], 0, sizeof(vka_object_t));
            dataspace->pages[idx].cptr = deviceFrame.capPtr;
        } else {
            /* Allocate a normal frame to fill this page. */
            int error = vka_alloc_frame(&procServ.vka, seL4_PageBits, &dataspace->pages[idx]);
            if (error || !dataspace->pages[idx].cptr) {
                ROS_ERROR("Could not allocate frame object. Procserv out of memory.");
                return (seL4_CPtr) 0;
            }
        }
    }
    return dataspace->pages[idx].cptr;
}

struct ram_dspace *
ram_dspace_get(struct ram_dspace_list *rdslist, int ID)
{
    if (ID <= RAM_DATASPACE_INVALID_ID || ID >= RAM_DATASPACE_MAX_NUM_DATASPACE) {
        /* Invalid ID. */
        return NULL;
    }
    struct ram_dspace* dspace = (struct ram_dspace*) coat_get(&rdslist->allocTable, ID);
    if (!dspace) {
        return NULL;
    }
    assert(dspace->magic == RAM_DATASPACE_MAGIC);
    return dspace;
}

struct ram_dspace *
ram_dspace_get_badge(struct ram_dspace_list *rdslist, seL4_Word badge)
{
    return ram_dspace_get(rdslist, badge - RAM_DATASPACE_BADGE_BASE);
}

uint32_t
ram_dspace_get_size(struct ram_dspace *dataspace)
{
    assert(dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);
    return dataspace->npages * REFOS_PAGE_SIZE;
}

int
ram_dspace_expand(struct ram_dspace *dataspace, uint32_t size)
{
    assert(dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);
    uint32_t npages = (size / REFOS_PAGE_SIZE) + ((size % REFOS_PAGE_SIZE) ? 1 : 0);

    if (npages < dataspace->npages) {
        /* Contraction not supported. */
        return EINVALIDPARAM;
    } else if (npages == dataspace->npages) {
        /* Nothing to do here. */
        return ESUCCESS;
    }
    uint32_t nbitmaskPrev = (dataspace->npages / 32) + 1;

    /* Expand the dataspace. */
    dataspace->pages = krealloc(dataspace->pages, sizeof(vka_object_t) * npages);
    if (!dataspace->pages) {
        ROS_ERROR("ram_dspace_expand could not reallocate page array, procserv out of mem!");
        return ENOMEM; /* Easier to not clean up, leave extra bit of mem. */
    }
    uint32_t pageDiff = npages - dataspace->npages;
    assert(pageDiff);
    memset(&dataspace->pages[dataspace->npages], 0, sizeof(vka_object_t) * pageDiff);

    /* Expand the dataspace content init mask. */
    uint32_t nbitmask = (npages / 32) + 1;
    if (dataspace->contentInitBitmask && nbitmaskPrev < nbitmask) {
        dataspace->contentInitBitmask = krealloc (
                dataspace->contentInitBitmask, nbitmask * sizeof(uint32_t)
        );
        if (!dataspace->contentInitBitmask) {
            ROS_ERROR("ram_dspace_expand failed to realloc content init bitmask. Procserv OOM.");
            return ENOMEM; /* Easier to not clean up, leave extra bit of mem. */
        }
        uint32_t bitmaskDiff = nbitmask - nbitmaskPrev;
        assert(bitmaskDiff > 0);
        memset(&dataspace->contentInitBitmask[nbitmaskPrev], 0, bitmaskDiff * sizeof(uint32_t));
    }

    dataspace->npages = npages;
    return ESUCCESS;
}

int
ram_dspace_set_to_paddr(struct ram_dspace *dataspace, uint32_t paddr)
{
    /* Check that the dataspace isn't already occupied with something. */
    if (dataspace->contentInitEnabled) {
        ROS_WARNING("Dataspace is already content init enabled, cannot set to physaddr mode.");
        return EINVALID;
    }
    if (dataspace->physicalAddrEnabled) {
        ROS_WARNING("Dataspace is already set to physaddr mode.");
        return EINVALID;
    }

    /* Check that the dataspace is empty. */
    dprintf("Checking pages...\n");
    for (int i = 0; i < dataspace->npages; i++) {
        if (dataspace->pages[i].cptr) {
            ROS_WARNING("Dataspace already has mapped anonymous content.");
            return EINVALID;
        }
    }

    /* Quick sanity check that something actually exists at the given paddr. */
    dprintf("procserv_find_device...\n");
    cspacepath_t deviceFrame = procserv_find_device((void*) paddr, REFOS_PAGE_SIZE);
    if (!deviceFrame.capPtr) {
        ROS_WARNING("No such device 0x%x.", paddr);
        return EFILENOTFOUND;
    }
    vka_cnode_delete(&deviceFrame);
    vka_cspace_free(&procServ.vka, deviceFrame.capPtr);

    dprintf("procserv_find_device OK...\n");
    dataspace->physicalAddrEnabled = true;
    dataspace->physicalAddr = paddr;
    return ESUCCESS;
}

/* --------------------------- RAM dataspace read / write functions ----------------------------- */

/*! @brief Reads data from a single page within a ram dataspace.

    Reads data from a single page within a ram dataspace. Can _NOT_ read across page boundaries.
    This is an internal helper function to implement the more general dataspace_read which can read
    across page boundaries.
    If the offset is not paged aligned, will only read data up to the next boundary. This should be
    an internal function and not exposed, but is not declared static here to give special access to
    unit tests.

    @param buf The destination buffer to copy data to. (No ownership)
    @param len The length of the data to be copied.
    @param dataspace The source ram dataspace. (No ownership)
    @param offset The offset into the dataspace to read from.
    @return ESUCCESS if success, refos_err_t otherwise.
 */
int
ram_dspace_read_page(char *buf, size_t len, struct ram_dspace *dataspace, uint32_t offset)
{
    vaddr_t skipBytes = offset - REFOS_PAGE_ALIGN(offset);
    if (len > (REFOS_PAGE_SIZE - skipBytes)) {
        dvprintf("WARNING: capping at len > PAGE_SIZE - skipBytes.\n");
        len = (REFOS_PAGE_SIZE - skipBytes);
    }
    seL4_CPtr frame = ram_dspace_get_page(dataspace, offset);
    if (!frame) {
        ROS_ERROR("ram_dspace_read_page failed to allocate page. Procserv out of memory.");
        return ENOMEM;
    }
    return procserv_frame_read(frame, buf, len, skipBytes);
}

/*! @brief Writes data to a page within a ram dataspace.

    Writes data to a page within a ram dataspace. Can _NOT_ write across page boundaries.
    This is an internal helper function to implement the more general dataspace_write which can write
    across page boundaries.
    If the offset is not paged aligned, will only write data up to the next boundary. This should be
    an internal function and not exposed, but is not declared static here to give special access to
    unit tests.

    @param buf The source buffer containing the data. (No ownership)
    @param len The length of the data to be written.
    @param dataspace The destination dataspace to be written to. (No ownership)
    @param offset The offset into the dataspace to write to.
    @return ESUCCESS if success, refos_err_t otherwise.
 */
int
ram_dspace_write_page(char *buf, size_t len, struct ram_dspace *dataspace, uint32_t offset)
{
    vaddr_t skipBytes = offset - REFOS_PAGE_ALIGN(offset);
    if (len > (REFOS_PAGE_SIZE - skipBytes)) {
        dvprintf("WARNING: capping at len > PAGE_SIZE - skipBytes.\n");
        len = (REFOS_PAGE_SIZE - skipBytes);
    }
    seL4_CPtr frame = ram_dspace_get_page(dataspace, offset);
    if (!frame) {
        ROS_ERROR("ram_dataspace_write_page failed to allocate page. Procserv out of memory.");
        return ENOMEM;
    }
    return procserv_frame_write(frame, buf, len, skipBytes);
}

int
ram_dspace_read(char *buf, size_t len, struct ram_dspace *dataspace, uint32_t offset)
{
    assert(buf && dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);
    int start = offset;
    int bufOffset = 0;

    /* Check if the read length runs off the end of the dataspace. */
    if (len > ((dataspace->npages * REFOS_PAGE_SIZE) - offset)) {
        return EINVALIDPARAM;
    }

    while (start < offset + len) {
        int end = MIN(REFOS_PAGE_ALIGN(start) + REFOS_PAGE_SIZE, offset + len);

        /* Read data out of the next page. */
        int error = ram_dspace_read_page (
                (char*)(buf + bufOffset), end - start,
                dataspace, start
        );
        if (error) {
            ROS_ERROR("ram_dspace_read_page failed.");
            return error;
        }

        bufOffset += end - start;
        start = end;
    }
    return ESUCCESS;
}

int
ram_dspace_write(char *buf, size_t len, struct ram_dspace *dataspace, uint32_t offset)
{
    assert(buf && dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);
    int start = offset;
    int bufOffset = 0;

    /* Check if the write length runs off the end of the dataspace. */
    if (len > ((dataspace->npages * REFOS_PAGE_SIZE) - offset)) {
        return EINVALIDPARAM;
    }

    while (start < offset + len) {
        int end = MIN(REFOS_PAGE_ALIGN(start) + REFOS_PAGE_SIZE, offset + len);

        /* Write data into the next page. */
        int error = ram_dspace_write_page (
                (char*)(buf + bufOffset), end - start,
                dataspace, start
        );
        if (error) {
            ROS_ERROR("ram_dspace_write_page failed.");
            return error;
        }

        bufOffset += end - start;
        start = end;
    }

    return ESUCCESS;
}

/* --------------------------- RAM dataspace content init functions ----------------------------- */

int
ram_dspace_content_init(struct ram_dspace *dataspace, cspacepath_t initEP, uint32_t initPID)
{
    assert(dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);

    /* We can't content init a dataspace that is physical address bound. */
    if (dataspace->physicalAddrEnabled) {
        ROS_WARNING("Can't content init a dataspace that is physical address bound.");
        return EINVALID;
    }

    /* Free any previous content initialised bitmasks. */
    if (dataspace->contentInitBitmask) {
        kfree(dataspace->contentInitBitmask);
        dataspace->contentInitBitmask = NULL;
    }

    /* Free any previous content initialisation endpoints. */
    if (dataspace->contentInitEnabled) {
        assert(dataspace->contentInitEP.capPtr);
        vka_cnode_revoke(&dataspace->contentInitEP);
        vka_cnode_delete(&dataspace->contentInitEP);
        vka_cspace_free(&procServ.vka, dataspace->contentInitEP.capPtr);
        dataspace->contentInitEP.capPtr = 0;
    }

    /* Special case, no endpoint, means unset content initialisation. */
    if (initEP.capPtr == 0) {
        dataspace->contentInitEnabled = false;
        dataspace->contentInitPID = PID_NULL;
        return ESUCCESS;
    }

    /* Allocate bitmask. */
    uint32_t nbitmask = (dataspace->npages / 32) + 1;
    dataspace->contentInitBitmask = kmalloc(nbitmask * sizeof(uint32_t));
    if (!dataspace->contentInitBitmask) {
        ROS_ERROR("ram_dspace_content_init failed to malloc content init bitmask. Procserv OOM.");
        return ENOMEM;
    }
    memset(dataspace->contentInitBitmask, 0, nbitmask * sizeof(uint32_t));

    /* Clear the waiting list. */
    int waitingListCount = cvector_count(&dataspace->contentInitWaitingList);
    for (int i = 0; i < waitingListCount; i++) {
        struct ram_dspace_waiter *waiter = (struct ram_dspace_waiter *)
                cvector_get(&dataspace->contentInitWaitingList, i);
        assert(waiter && waiter->magic == RAM_DATASPACE_WAITER_MAGIC);
        assert(waiter->reply.capPtr);

        vka_cnode_revoke(&waiter->reply);
        vka_cnode_delete(&waiter->reply);
        vka_cspace_free(&procServ.vka, waiter->reply.capPtr);
        kfree(waiter);
    }
    cvector_free(&dataspace->contentInitWaitingList);
    cvector_init(&dataspace->contentInitWaitingList);

    /* Set the content EP, taking ownership of given endpoint. */
    dataspace->contentInitEP = initEP;
    dataspace->contentInitEnabled = true;
    dataspace->contentInitPID = initPID;
    return ESUCCESS;
}

int
ram_dspace_need_content_init(struct ram_dspace *dataspace, uint32_t offset)
{
    assert(dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);

    if (!dataspace->contentInitEnabled || !dataspace->contentInitBitmask) {
        return -EINVALID;
    }
    if (offset > ram_dspace_get_size(dataspace)) {
        return -EINVALIDPARAM;
    }

    uint32_t npage = (offset / REFOS_PAGE_SIZE);
    uint32_t idxbitmask = npage / 32;
    uint32_t idxshift = npage % 32;
    assert(npage <= dataspace->npages);

    return !((dataspace->contentInitBitmask[idxbitmask] >> idxshift) & 0x1);
}

int
ram_dspace_add_content_init_waiter(struct ram_dspace *dataspace, uint32_t offset,
                                   cspacepath_t reply)
{
    assert(dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);
    if (!reply.capPtr) {
        ROS_WARNING("add_content_init_waiter: null cap given. Nothing to do.");
        return EINVALIDPARAM;
    }
    if (offset > ram_dspace_get_size(dataspace)) {
        return EINVALIDPARAM;
    }
    if (!dataspace->contentInitEnabled) {
        dvprintf("add_content_init_waiter: content init not enabled.");
        return EINVALID;
    }
    uint32_t npage = (offset / REFOS_PAGE_SIZE);
    assert(npage < dataspace->npages);

    /* Allocate the waiter structure. */
    struct ram_dspace_waiter* waiter = kmalloc(sizeof(struct ram_dspace_waiter));
    if (!waiter) {
        ROS_ERROR("add_content_init_waiter could not malloc waiter struct. Procserv OOM.");
        return ENOMEM;
    }

    /* Fill out the waiter structure and add to waiting list. */
    waiter->magic = RAM_DATASPACE_WAITER_MAGIC;
    waiter->pageidx = npage;
    waiter->reply = reply;
    cvector_add(&dataspace->contentInitWaitingList, (cvector_item_t) waiter);
    return ESUCCESS;
}

int
ram_dspace_add_content_init_waiter_save_current_caller(struct ram_dspace *dataspace,
                                                       uint32_t offset)
{
    /* Allocate anew cslot to save the reply cap to. */
    cspacepath_t callerReply;
    int error = vka_cspace_alloc_path(&procServ.vka, &callerReply);
    if (error != seL4_NoError) {
        ROS_ERROR("Could not allocate path to save caller. PRocserv outof cslots.");
        return ENOMEM;
    }
    assert(callerReply.capPtr);

    /* Save the caller cap. */
    error = vka_cnode_saveCaller(&callerReply);
    if (error != seL4_NoError) {
        ROS_ERROR("Could not copy out reply cap.");
        return EINVALID;
    }

    /* Add reply cap as waiter (takes ownership of the cap) */
    return ram_dspace_add_content_init_waiter(dataspace, offset, callerReply);
}


void
ram_dspace_content_init_reply_waiters(struct ram_dspace *dataspace, uint32_t offset)
{
    assert(dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);

    /* Calculate (and check) the dspace page number. */
    uint32_t npage = (offset / REFOS_PAGE_SIZE);
    if(npage >= dataspace->npages) {
        ROS_ERROR("ram_dspace_content_init_reply_waiters: offset out of bounds.")
        return;
    }

    /* Loop through the waiting list, find any clients that are currently blocked on the page
       we have data for, and reply to them. */
    int waitingListCount = cvector_count(&dataspace->contentInitWaitingList);
    for (int i = 0; i < waitingListCount; i++) {
        struct ram_dspace_waiter *waiter = (struct ram_dspace_waiter *)
                cvector_get(&dataspace->contentInitWaitingList, i);
        assert(waiter && waiter->magic == RAM_DATASPACE_WAITER_MAGIC);
        assert(waiter->reply.capPtr);

        if (waiter->pageidx == npage) {
            /* Unblock this client. */
            seL4_Send(waiter->reply.capPtr, _dispatcherEmptyReply);

            /* Remove this waiter from the waiting list. */
            cvector_delete(&dataspace->contentInitWaitingList, i);
            waitingListCount--;
            assert(waitingListCount == cvector_count(&dataspace->contentInitWaitingList));
            i--;

            /* Delete this waiter. */
            vka_cnode_revoke(&waiter->reply);
            vka_cnode_delete(&waiter->reply);
            vka_cspace_free(&procServ.vka, waiter->reply.capPtr);
            kfree(waiter);
        }
    }
}

void
ram_dspace_set_content_init_provided(struct ram_dspace *dataspace, uint32_t offset)
{
    assert(dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);

    if (!dataspace->contentInitEnabled || !dataspace->contentInitBitmask) {
        ROS_WARNING("set_content_init_provided called with content init disabled.");
        return;
    }
    if (offset > ram_dspace_get_size(dataspace)) {
        ROS_WARNING("set_content_init_provided offset out-of-bounds.");
        return;
    }

    uint32_t npage = (offset / REFOS_PAGE_SIZE);
    uint32_t idxbitmask = npage / 32;
    uint32_t idxshift = npage % 32;
    assert(npage <= dataspace->npages);

    /* Set the bitmask bit. */
    dataspace->contentInitBitmask[idxbitmask] |= (1 << idxshift);
}


