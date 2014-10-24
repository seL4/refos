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
    @brief Process Server anon RAM dataspace implementation.

    A dataspace implementation which is backed by physical RAM. Provides methods for creating &
    deleting ram dataspaces, as well as manages reading and writing to them directly. The actual
    frames objects are lazily allocated. Dataspace objects support shared strong references through
    refcounting.
*/

#ifndef _REFOS_PROCESS_SERVER_SYSTEM_MEMSERV_RAM_DATASPACE_H_
#define _REFOS_PROCESS_SERVER_SYSTEM_MEMSERV_RAM_DATASPACE_H_

#include <stdint.h>
#include <stdbool.h>
#include <sel4/sel4.h>
#include <data_struct/cvector.h>
#include <data_struct/coat.h>
#include <vspace/vspace.h>
#include "../../common.h"

#define RAM_DATASPACE_MAGIC 0xF89D8531 
#define RAM_DATASPACE_LIST_MAGIC 0xC923BE76
#define RAM_DATASPACE_WAITER_MAGIC 0x351095BC
#define RAM_DATASPACE_INVALID_ID 0

struct ram_dspace_list;

/*! @brief Ram dataspace structure

    A single ram dataspace backed by physical kernel pages. This structure assumes ownership of its
    associated pages at all times.
 */
struct ram_dspace {
    int ID;
    uint32_t magic;
    cspacepath_t capability;
    uint32_t ref;

    /* Anonymous RAM frames. */
    vka_object_t *pages; /*< Has ownership. */
    uint32_t npages;

    /* Content init state. */
    bool contentInitEnabled;
    cspacepath_t contentInitEP;
    uint32_t contentInitPID; /* No ownership. */
    uint32_t *contentInitBitmask;
    cvector_t contentInitWaitingList; /* ram_dspace_waiter */

    /* Physical device state. */
    bool physicalAddrEnabled;
    uint32_t physicalAddr;

    /*! Weak reference to this dataspace's parent. */
    struct ram_dspace_list *parentList; /* No ownership. */
};

/*! @brief Ram dataspace list. */
struct ram_dspace_list {
    coat_t allocTable; /* struct ram_dspace */
    uint32_t magic;
};

/*! @brief Ram dataspace content init waiter structure */
struct ram_dspace_waiter {
    int pageidx;
    cspacepath_t reply;
    uint32_t magic;
};

/* ------------------------------- RAM dataspace table functions -------------------------------- */

/*! @brief Initialises an empty ram dataspace list. */
void ram_dspace_init(struct ram_dspace_list *rdslist);

/*! @brief De-initialises an empty ram dataspace list. */
void ram_dspace_deinit(struct ram_dspace_list *rdslist);

/*! @brief Creates a new ram dataspace and inserts into a ram dataspace list.
    @param rdslist The ram dataspace list to allocate from.
    @param size The size of the new dataspace in bytes.
    @return The newly created ram dataspace if success (No ownership), NULL otherwise.
 */
struct ram_dspace *ram_dspace_create(struct ram_dspace_list *rdslist, size_t size);

/*! @brief Adds a shared reference to ram dataspace from a ram dataspace list.
    @param rdslist The ram dataspace list to reference the dataspace from.
    @param ID The ID of target ram dataspace to be refed.
 */
void ram_dspace_ref(struct ram_dspace_list *rdslist, int ID);

/*! @brief Unreference a ram dataspace from a ram dataspace list. If this is the last reference,
           then the ram dataspace will be deleted from the list.
    @param rdslist The ram dataspace list to unref the dataspace from.
    @param ID The ID of target ram dataspace to be unrefed.
 */
void ram_dspace_unref(struct ram_dspace_list *rdslist, int ID);

/*! @brief Checks whether a page in the ram dataspace exists, and finds & returns it if it does.
    @param dataspace The ram dataspace to find and get the page object from.
    @param offset Offset into the ram dataspace.
    @return CPtr to frame if there's a page at the given offset in the given dataspace,
            0 otherwise. No ownership transfer.
 */
seL4_CPtr ram_dspace_check_page(struct ram_dspace *dataspace, uint32_t offset);

/*! @brief Retrieves a page at a given offset. If the page hasn't been created, it will be
           allocated. Note that this does NOT perform content init.
    @param dataspace The ram dataspace to get the page object from.
    @param offset Offset into the ram dataspace.
    @return CPtr to frame if success, 0 if offset invalid or out of memory. No ownership transfer.
 */
seL4_CPtr ram_dspace_get_page(struct ram_dspace *dataspace, uint32_t offset);

/*! @brief Finds a ram dataspace in a ram dataspace list by a dataspace ID.
    @param rdslist The source list of ram dataspaces. (No ownership)
    @param ID The dataspace ID to locate the ram dataspace in the list.
    @return The (weak) reference to target ram dataspace if found, NULL otherwise.
 */
struct ram_dspace *ram_dspace_get(struct ram_dspace_list *rdslist, int ID);

/*! @brief Finds a ram dataspace in a ram dataspace list by a dataspace badge.
    @param rdslist The source list of ram dataspaces. (No ownership)
    @param badge The dataspace badge to locate the ram dataspace in the list.
    @return The (weak) reference to target ram dataspace if found, NULL otherwise.
 */
struct ram_dspace *ram_dspace_get_badge(struct ram_dspace_list *rdslist, seL4_Word badge);

/*! @brief Returns the size in bytes of the given dataspace. 
    @param dataspace The dataspace to retrieve size for.
    @return Size of the given dataspace in bytes on success, 0 otherwise.
*/
uint32_t ram_dspace_get_size(struct ram_dspace *dataspace);

/*! @brief Expands the given dataspace.
    @param dataspace The dataspace to expand for.
    @param size The new dataspace size.
    @return ESUCCESS on success, refos_error otherwise.
*/
int ram_dspace_expand(struct ram_dspace *dataspace, uint32_t size);

/*! @brief Sets the dataspace to start at the given physical address.

    Sets the dataspace to start at the given physical address. The following pages of the
    dataspace will be mapped to the following contiguous physical memory regions. Used to
    implement device MMIO mapping. Requires the dataspace to be empty and not initialised by
    any other content already.
    This should be called immediately after dataspace creation.

    @param dataspace The dataspace to set physical address for.
    @param paddr The physical address to start the dataspace at.
    @return ESUCCESS on success, refos_error otherwise.
*/
int ram_dspace_set_to_paddr(struct ram_dspace *dataspace, uint32_t paddr);

/* --------------------------- RAM dataspace read / write functions ----------------------------- */

/*! @brief Reads data from a ram dataspace.
    @param buf The destination buffer to copy data to. (No ownership)
    @param len The length of the data to be copied.
    @param dataspace The source ram dataspace. (No ownership)
    @param offset The offset into the dataspace to read from.
    @return ESUCCESS if success, refos_error otherwise.
 */
int ram_dspace_read(char *buf, size_t len, struct ram_dspace *dataspace, uint32_t offset);

/*! @brief Writes data to a ram dataspace.
    @param buf The source buffer containing the data. (No ownership)
    @param len The length of the data to be written.
    @param dataspace The target dataspace to be written to. (No ownership)
    @param offset The offset into the dataspace to write to.
    @return ESUCCESS if success, refos_error otherwise.
 */
int ram_dspace_write(char *buf, size_t len, struct ram_dspace *dataspace, uint32_t offset);

/* --------------------------- RAM dataspace content init functions ----------------------------- */

/*! @brief Sets the RAM dataspace to be initialised by another RAM dataspace.
    @param dataspace The dataspace set content init for.
    @param initEP The external dspace server that will initialise this dataspace. (Takes ownership)
    @param initPID The external dspace server PID.
    @return ESUCCESS if success, refos_error otherwise.
*/
int ram_dspace_content_init(struct ram_dspace *dataspace, cspacepath_t initEP, uint32_t initPID);

/*! @brief Returns whether content initialisation is needed for the given offset.
    @param dataspace The target dataspace.
    @param offset The offset into the dataspace to get content init state for.
    @return TRUE if need content init at given offset, FALSE if already initialised, or -refos_error
            if content init is not enabled for the given dataspace, or if offset is invalid.  
*/
int ram_dspace_need_content_init(struct ram_dspace *dataspace, uint32_t offset);

/*! @brief Add a new content-init blocked waiter.

    Adds a new content-init waiter at the given offset to this dataspace. When the content
    has been provided, the given endpoint will be replied to.

    @param dataspace The dataspace that the waiter wants to wait on.
    @param offset The offset at which the client is waiting for, into the dataspace.
    @param reply The reply endpoint, which will unblock the waiting client.
    @return ESUCCESS if success, refos_error otherwise.
*/
int ram_dspace_add_content_init_waiter(struct ram_dspace *dataspace, uint32_t offset,
                                       cspacepath_t reply);

/*! @brief Add a new content-init blocked waiter using current caller reply cap.
    @param dataspace The dataspace that the waiter wants to wait on.
    @param offset The offset at which the client is waiting for, into the dataspace.
    @return ESUCCESS if success, refos_error otherwise.
*/
int ram_dspace_add_content_init_waiter_save_current_caller(struct ram_dspace *dataspace,
                                                           uint32_t offset);

/*! @brief Wakes up any waiters waiting at this offset.

    Wakes up any waiter clients waiting at this offset for content-init. This does NOT set the
    content provided flag, simply loops through the waiting list the replies to clients.

    Note that we do not need to perform and VSpace mapping operations here on the waiting clients,
    but simply waking them back up is enough, as they will cause another VM fault upon wake-up and
    then since the content has been initialised now, it will me mapped straight in on the second VM
    fault. This may be optimised further by performing VSpace mappings right now before waking up
    the clients.

    @param dataspace The dataspace containing the waiters.
    @param offset The offset at which the clients are waiting for, into the dataspace.
*/
void ram_dspace_content_init_reply_waiters(struct ram_dspace *dataspace, uint32_t offset);

/*! @brief Set the content-init page of dataspace at offset to be provided.

    Set the provided bitmask flag of the dataspace at the offset to be TRUE, meaning that that page
    has been content-init provided. Any waiters waiting for the content to be provided should now be
    woken up, and any further access to these pages should not cause another content-init delegation
    notification. Does NOT automatically reply to the waiters, that must be separately done via a
    separate call to ram_dspace_content_init_reply_waiters().

    @param dataspace The target dataspace.
    @param offset The offset into the dataspace to get content init state for.
*/
void ram_dspace_set_content_init_provided(struct ram_dspace *dataspace, uint32_t offset);

#endif /* _REFOS_PROCESS_SERVER_SYSTEM_MEMSERV_RAM_DATASPACE_H_ */