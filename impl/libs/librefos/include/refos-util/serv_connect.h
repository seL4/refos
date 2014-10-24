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
    @brief Server client connection module implementation.

    This module implements client related management on the server. It provides a generic
    ID free list for recycling any ID as well as methods for cleaning up client related information.
*/

#ifndef _REFOS_NAMESERV_SERV_CLIENT_CONNECTION_IMPL_LIBRARY_H_
#define _REFOS_NAMESERV_SERV_CLIENT_CONNECTION_IMPL_LIBRARY_H_

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <data_struct/cvector.h>
#include <data_struct/coat.h>
#include <refos/refos.h>
#include <refos-rpc/rpc.h>

#define SRC_CLIENT_LIST_MAGIC 0x26B7B92A
#define SRC_CLIENT_INVALID_ID COAT_INVALID_ID

/*! @brief Server client session structure,

    Client session structure. The client ID is with respect to the server itself.
    Liveness cap is a badged endpoint cap of the process server while session cap is a badged
    endpoint cap of the fileserver.
 */
struct srv_client {
    rpc_client_state_t rpcClient; /* Inherited, must be first. */
    uint32_t magic;
    uint32_t cID;

    seL4_CPtr liveness;
    seL4_CPtr session;
    int32_t deathID;

    uint32_t paramBufferStart;
    seL4_CPtr paramBuffer;
    seL4_CPtr paramBufferSize;
};

struct srv_client_table {
    coat_t allocTable; /* Inherited struct, must be first. */
    cvector_t pendingFreeList;
    uint32_t magic;

    uint32_t clientMagic;
    int maxClients;
    int badgeBase;
    seL4_CPtr sessionSrcEP;
};

/*! @brief Initialise client allocation table. */
void client_table_init(struct srv_client_table *ct, int maxClients, int badgeBase, uint32_t magic,
                       seL4_CPtr sessionSrcEP);

/*! @brief Release the client allocation table. */
void client_table_release(struct srv_client_table *ct);

/*! @brief Perform client table post IPC actions.

    Actually frees the client IDs that need to be freed. Should be called
    at the end of every IPC. The reason for having this is so that a client deletion syscall may
    be implemented; there is no way to reply to the client if we delete the client in the middle of
    the client's syscall to delete itself.
*/
void client_table_postaction(struct srv_client_table *ct);

/*! @brief Assigns an client ID and creates a client structure. */
struct srv_client* client_alloc(struct srv_client_table *ct, seL4_CPtr liveness);

/*! @brief Gets the associated client structure given an ID. */
struct srv_client* client_get(struct srv_client_table *ct, int id);

/*! @brief Gets the associated client structure given an unwrapped badge. */
struct srv_client* client_get_badge(struct srv_client_table *ct, int badge);

/*! @brief Queue this client up for deletion at the end of syscall. */
void client_queue_delete(struct srv_client_table *ct, int id);

/*! @brief Queue client up for deletion based on deathID. */
int client_queue_delete_deathID(struct srv_client_table *ct, int deathID);

#endif /* _REFOS_NAMESERV_SERV_CLIENT_CONNECTION_IMPL_LIBRARY_H_ */