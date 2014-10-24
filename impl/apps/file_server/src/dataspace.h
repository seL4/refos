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
    @brief File server CPIO dataspace object allocation and management. */

#ifndef _FILE_SERVER_CPIO_DATASPACE_H_
#define _FILE_SERVER_CPIO_DATASPACE_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <data_struct/cvector.h>
#include <data_struct/coat.h>
#include <data_struct/chash.h>
#include <refos/refos.h>
#include <refos-rpc/rpc.h>

/*! @file
    @brief File server CPIO dataspace object allocation and management. */  

#define FILESERVER_MAX_DATASPACES 8192
#define FILESERVER_DSPACE_ASSOC_HASHSIZE 4096
#define FILESERVER_WINDOW_ASSOC_HASHSIZE 4096
#define FS_DATASPACE_MAGIC 0x4B1C8007

/*! @brief File server dataspace

    File server dataspace structure. Dataspace cap is a badged endpoint cap of the file server.
    The structure has no ownership of the actual file data.
 */
struct fs_dataspace {
    uint32_t magic;
    uint32_t dID;
    uint32_t deathID;

    seL4_CPtr dataspaceCap;
    seL4_Word permissions;

    char *fileData; /* Not owned. */
    size_t fileDataSize;
    bool fileCreated;
};

/*! @brief File server CPIO dataspace association

    File server dataspace association structure. It provides look up of a fileserver dataspace
    using an index. This is used for both window ――▶ dataspace associations, and external
    dataspace ――▶ dataspace associations. Window ――▶ dataspace associations are used in pager mode,
    to quickly find the mapped dataspace for a given windowID. External dataspace ――▶ dataspace
    associations are used in content-init mode, to quickly find the internal dataspace given an
    external dataspaceID.
 */
struct dataspace_association_info {
    int dataspaceID;         /*!< The internal dataspace ID being associated to. */
    int dataspaceOffset;     /*!< Offset into the internal dataspace ID. */
    seL4_CPtr objectCap;     /*!< The associated object's capability; window cap or dspace cap. */
};

struct fs_dataspace_table {
    coat_t allocTable;
    chash_t windowAssocTable; /* struct dataspace_association_info */
    chash_t dspaceAssocTable; /* struct dataspace_association_info */
};

/* ----------------------- CPIO Dataspace Table Functions --------------------------------------- */

/*! @brief Initialise CPIO dataspace allocation table.
    @param dt The dspace table to initialise. (No ownership passed)
*/
void dspace_table_init(struct fs_dataspace_table *dt);

/*! @brief De-initialise a CPIO dataspace allocation table.
    @param dt The dspace table to release. (No ownership passed, does NOT release the structure)
*/
void dspace_table_release(struct fs_dataspace_table *dt);


/* --------------------- CPIO Dataspace Allocation Functions ------------------------------------ */

/*! @brief Assigns an dataspace ID and creates a fs_dataspace structure.
    @param dt The dspace table to allocate from.
    @param deathID The dspace table to allocate from.
    @param fileData The CPIO file data pointer. (No ownership passed)
    @param fileDataSize The CPIO file data size.
    @param permissions The dataspace permissions mask.
    @return Weak pointer to created dataspace. (ie. No ownership)
*/
struct fs_dataspace* dspace_alloc(struct fs_dataspace_table *dt, uint32_t deathID, char *fileData,
        size_t fileDataSize, seL4_Word permissions);

/*! @brief Gets the associated dataspace structure given an ID.
    @param dt The dspace table to get from.
    @param id The dataspace ID of the dataspace to get.
    @return Weak pointer to dataspace associated with given dataspace ID. (No ownership)
*/
struct fs_dataspace* dspace_get(struct fs_dataspace_table *dt, int id);

/*! @brief Gets the associated dataspace structure given an unwrapped badge.
    @param dt The dspace table to get from.
    @param badge The badge corresponding to dataspace ID of the dataspace to get.
    @return Weak pointer to dataspace associated with given dataspace badge. (No ownership)
*/
struct fs_dataspace* dspace_get_badge(struct fs_dataspace_table *dt, int badge);

/*! @brief Delete the given dataspace and release all its memory.
    @param dt The dspace table to get from.
    @param id The dataspace ID of the dataspace to delete.
*/
void dspace_delete(struct fs_dataspace_table *dt, int id);


/* ----------------------------- CPIO Dataspace Functions --------------------------------------- */

/*! @brief Associate given window with the dataspace.

    When a client maps one of out CPIO dataspaces to its window, we notify the memory manager (ie.
    the process server) that we are acting as the pager for this window. This function does the
    book-keeping of window ――▶ dataspace, so when the procserv notifies us to act as the pager,
    we know which dataspace are supposed to be mapped into which faulting window.

    @param dt The dspace table.
    @param winID The window ID to associate this dataspace with.
    @param dsID The dataspace to associate with given window.
    @param dsOffset The offset into the dataspace, at which it is mapped into the window.
    @param windowCap Capability to the window, corresponding to given windowID.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int dspace_window_associate(struct fs_dataspace_table *dt, int winID, int dsID, int dsOffset,
                            seL4_CPtr windowCap);

/*! @brief Un-associate given window with previously associated dataspace.
    @param dt The dspace table.
    @param winID The window ID to unassociate.
*/
void dspace_window_unassociate(struct fs_dataspace_table *dt, int winID);

/*! @brief Find the dataspace associated with given windowID.
    @param dt The dspace table.
    @param winID The window ID of window to find associated dataspace for.
*/
struct dataspace_association_info *dspace_window_find(struct fs_dataspace_table *dt, int winID);

/*! @brief Associate given external dataspace with the dataspace.
    
    When a client data-inits an external CPIO dataspace with one of ours, we need to book-keep
    the externalDS ――▶ internalDS relationship. This function does this book-keeping, so when
    the external dataserv notifies us to provide content for one of its dataspaces, we know
    which internal dspace to initialise with.

    @param dt The dspace table.
    @param xdsID The external dataspace ID. Need to call data_have_data on the external-dataspace
                 before calling this function, which will give us an external dataspace ID.
    @param dsID The internal dataspace ID, which contains the content to be initialised.
    @param dsOffset The offset into the dataspace, at which it is content initialised.
    @param xdspaceCap The external dataspace cap.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
int dspace_external_associate(struct fs_dataspace_table *dt, int xdsID, int dsID, int dsOffset,
                              seL4_CPtr xdspaceCap);

/*! @brief Un-associate given external dataspace with the dataspace.
    @param dt The dspace table.
    @param xdsID The external dataspace ID to unassociate.
*/
void dspace_external_unassociate(struct fs_dataspace_table *dt, int xdsID);

/*! @brief Find the dataspace associated with given external dataspaceID.
    @param dt The dspace table.
    @param xdsID The external dataspace ID to find associated internal dataspace for.
*/
struct dataspace_association_info *dspace_external_find(struct fs_dataspace_table *dt, int xdsID);

#endif /* _FILE_SERVER_CPIO_DATASPACE_H_ */
