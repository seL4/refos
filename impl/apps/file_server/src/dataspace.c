/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <refos/refos.h>
#include <refos/vmlayout.h>
#include "badge.h"
#include "state.h"
#include "dataspace.h"

 /*! @file
     @brief File server CPIO dataspace object allocation and management.

     This module contains the concrete implementations for file server CPIO dataspaces. The
     dispatcher module simple does basic error checking before calling one of the helper functions
     here.
*/

/* --------------------- CPIO Dataspace OAT Callback Functions ---------------------------------- */

/*! @brief Dataspace object OAT creation function.

    This is called by coat helper library to create a new structure. The first argument is the
    client's DeathID, second argument is the fileData pointer, 3rd argument is the fileData size,
    and 4th argument the permissions mask.

    Allocates and fills in a new dataspace structure with the given arguments, and mints a new cap
    representing this new dataspace.

    @param oat The parent struct fs_dataspace_table structure of dspace object being created.
    @param id The dataspace object ID that has been allocated for this object to go into.
    @param arg The arguments (see above).
    @return A new dataspace (struct fs_dataspace) if success, NULL otherwise. (Passes ownership, it
            is the caller's responsibility to call dspace_oat_delete on the returned object.)
*/
static cvector_item_t
dspace_oat_create(coat_t *oat, int id, uint32_t arg[COAT_ARGS])
{
    /* Allocate and fill in structure. */
    struct fs_dataspace *ndspace = malloc(sizeof(struct fs_dataspace));
    if (!ndspace) {
        assert(!"oom");
        ROS_ERROR("dspace_oat_create out of memory!");
        return NULL;
    }

    ndspace->magic = FS_DATASPACE_MAGIC;
    ndspace->dID = id;
    ndspace->deathID = arg[0];
    ndspace->dataspaceCap = csalloc();
    ndspace->fileData = (char*) arg[1];
    ndspace->fileDataSize = arg[2];
    ndspace->permissions = arg[3];
    ndspace->fileCreated = false;

    /* Check that the dataspace cap cslot has been successfully allocated, and that
       the given pointer is valid. */
    if (!ndspace->dataspaceCap || !ndspace->fileData) {
        free(ndspace);
        return NULL;
    }

    /* Create the badged cap represending this dataspace. */
    int error = seL4_CNode_Mint(
            REFOS_CSPACE, ndspace->dataspaceCap, REFOS_CDEPTH,
            REFOS_CSPACE, fileServCommon->anonEP, REFOS_CDEPTH,
            seL4_AllRights, seL4_CapData_Badge_new(id + FS_DSPACE_BADGE_BASE)
    );
    assert(!error);
    (void) error;

    return (cvector_item_t) ndspace;
}

/*! @brief Dataspace object OAT deletion function.
    @param oat The parent struct fs_dataspace_table structure of dspace object being deleted.
    @param obj The dataspace object (struct fs_dataspace) to release resources for.
               (Takes ownership since it frees the given structure.)
*/
static void
dspace_oat_delete(coat_t *oat, cvector_item_t *obj)
{
    struct fs_dataspace *dspace = (struct fs_dataspace *) obj;
    assert(dspace && dspace->magic == FS_DATASPACE_MAGIC);
    dprintf("Deleting dataspace ID %d\n", dspace->dID);

    assert(dspace->dataspaceCap);
    seL4_CNode_Revoke(REFOS_CSPACE, dspace->dataspaceCap, REFOS_CDEPTH);
    csfree_delete(dspace->dataspaceCap);

    /* Finally, free the entire structure. */
    free(dspace);
}

/* ----------------------- CPIO Dataspace Table Functions --------------------------------------- */

void
dspace_table_init(struct fs_dataspace_table *dt)
{
    /* Configure the object allocation table creation / deletion callback func pointers. */
    dt->allocTable.oat_expand = NULL;
    dt->allocTable.oat_create = dspace_oat_create;
    dt->allocTable.oat_delete = dspace_oat_delete;

    /* Initialise our data structures. */
    coat_init(&dt->allocTable, 1, FILESERVER_MAX_DATASPACES);
    chash_init(&dt->windowAssocTable, FILESERVER_WINDOW_ASSOC_HASHSIZE);
    chash_init(&dt->dspaceAssocTable, FILESERVER_DSPACE_ASSOC_HASHSIZE);
}

void
dspace_table_release(struct fs_dataspace_table *dt)
{
    for (int i = 0; i < PROCESS_MAX_WINDOWS; i++) {
        dspace_window_unassociate(dt, i);
    }
    coat_release(&dt->allocTable);
    chash_release(&dt->windowAssocTable);
}

/* --------------------- CPIO Dataspace Allocation Functions ------------------------------------ */

struct fs_dataspace*
dspace_alloc(struct fs_dataspace_table *dt, uint32_t deathID, char *fileData, size_t fileDataSize,
             seL4_Word permissions)
{
    struct fs_dataspace* ndspace = NULL;

    uint32_t arg[COAT_ARGS];
    arg[0] = deathID;
    arg[1] = (uint32_t) fileData;
    arg[2] = (uint32_t) fileDataSize;
    arg[3] = (uint32_t) permissions;

    /* Allocate an ID, and the dspace structure associated with it. */
    int ID = coat_alloc(&dt->allocTable, arg, (cvector_item_t *) &ndspace);
    if (!ndspace) {
        ROS_ERROR("dspace_alloc couldn't allocate a dataspace.");
        return NULL;
    }

    assert(ID != COAT_INVALID_ID);
    assert(ndspace->magic == FS_DATASPACE_MAGIC);
    (void) ID;
    return ndspace;
}

struct fs_dataspace*
dspace_get(struct fs_dataspace_table *dt, int id)
{
    if (id < 0 || id >= FILESERVER_MAX_DATASPACES) {
        /* Invalid ID. */
        return NULL;
    }
    struct fs_dataspace* ndspace = (struct fs_dataspace*) coat_get(&dt->allocTable, id);
    if (!ndspace) {
        /* No such dataspace ID exists. */
        return NULL;
    }
    assert(ndspace->magic == FS_DATASPACE_MAGIC);
    return ndspace;
}

struct fs_dataspace*
dspace_get_badge(struct fs_dataspace_table *dt, int badge)
{
    if (badge < FS_DSPACE_BADGE_BASE || badge >= FS_DSPACE_BADGE_BASE + FILESERVER_MAX_DATASPACES) {
        /* Invalid badge. */
        return NULL;
    }
    return dspace_get(dt, badge - FS_DSPACE_BADGE_BASE);
}

void
dspace_delete(struct fs_dataspace_table *dt, int id)
{
    coat_free(&dt->allocTable, id);
}

/* ----------------------------- CPIO Dataspace Functions --------------------------------------- */

/*! @brief Internal unassociation helper function. */
static void
dspace_externalID_unassociate(chash_t *ht, int objID)
{
    struct dataspace_association_info *di = (struct dataspace_association_info *)
            chash_get(ht, objID);
    if (!di) {
        /* No association structure here to free. */
        return;
    }
    chash_set(ht, objID, (chash_item_t) NULL);
    if (di->objectCap) {
        seL4_CNode_Revoke(REFOS_CSPACE, di->objectCap, REFOS_CDEPTH);
        seL4_CNode_Delete(REFOS_CSPACE, di->objectCap, REFOS_CDEPTH);
        csfree(di->objectCap);
    }
    free(di);
}

/*! @brief Internal association helper function. */
static int
dspace_externalID_associate(chash_t *ht, int objID, int dsID, int dsOffset,
                            seL4_CPtr cap)
{
    dspace_externalID_unassociate(ht, objID);
    struct dataspace_association_info *di = malloc(sizeof(struct dataspace_association_info));
    if (!di) {
        return ENOMEM;
    }
    di->dataspaceID = dsID;
    di->dataspaceOffset = dsOffset;
    di->objectCap = cap;
    chash_set(ht, objID, (chash_item_t) di);
    return ESUCCESS;
}

int
dspace_window_associate(struct fs_dataspace_table *dt, int winID, int dsID, int dsOffset,
                              seL4_CPtr windowCap)
{
    return dspace_externalID_associate(&dt->windowAssocTable, winID, dsID, dsOffset, windowCap);
}

struct dataspace_association_info *
dspace_window_find(struct fs_dataspace_table *dt, int winID)
{
    return (struct dataspace_association_info *) chash_get(&dt->windowAssocTable, winID);
}

void
dspace_window_unassociate(struct fs_dataspace_table *dt, int winID)
{
    dspace_externalID_unassociate(&dt->windowAssocTable, winID);
}

int
dspace_external_associate(struct fs_dataspace_table *dt, int xdsID, int dsID, int dsOffset,
                                seL4_CPtr xdspaceCap)
{
    return dspace_externalID_associate(&dt->dspaceAssocTable, xdsID, dsID, dsOffset, xdspaceCap);
}

void
dspace_external_unassociate(struct fs_dataspace_table *dt, int xdsID)
{
    dspace_externalID_unassociate(&dt->dspaceAssocTable, xdsID);
}

struct dataspace_association_info *
dspace_external_find(struct fs_dataspace_table *dt, int xdsID)
{
    return (struct dataspace_association_info *) chash_get(&dt->dspaceAssocTable, xdsID);
}
