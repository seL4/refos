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
#include <refos-util/serv_connect.h>
#include <refos-util/cspace.h>

/*! @file
    @brief Server client connection module implementation. */

static cvector_item_t
client_oat_create(coat_t *oat, int id, uint32_t arg[COAT_ARGS])
{
    struct srv_client_table *ct = ((struct srv_client_table*) oat);
    assert(ct && ct->magic == SRC_CLIENT_LIST_MAGIC);

    /* Allocate and set up new client structure. */
    struct srv_client *nclient = malloc(sizeof(struct srv_client));
    if (!nclient) {
        printf("ERROR: client_oat_create out of memory!\n");
        return NULL;
    }
    memset(&nclient->rpcClient, 0, sizeof(nclient->rpcClient));
    nclient->magic = ct->clientMagic;
    nclient->cID = id;
    nclient->liveness = arg[0];
    nclient->deathID = -1;
    nclient->paramBufferStart = 0;
    nclient->paramBuffer = 0;

    /* Mint a session cap. */
    nclient->session = csalloc();
    if (!nclient->session) {
        printf("ERROR: could not not allocate cslot.\n");
        goto exit1;
    }
    int error = seL4_CNode_Mint (
            REFOS_CSPACE, nclient->session, REFOS_CDEPTH,
            REFOS_CSPACE, ct->sessionSrcEP, REFOS_CDEPTH,
            seL4_CanWrite | seL4_CanGrant,
            seL4_CapData_Badge_new(nclient->cID + ct->badgeBase)
    );
    if (error != seL4_NoError) {
        printf("ERROR: failed to mint client session cap.\n");
        goto exit2;
    }

    return (cvector_item_t) nclient;

    /* Exit stack. */
exit2:
    csfree(nclient->session);
exit1:
    assert(nclient);
    free(nclient);
    return NULL;
}

static void
client_oat_delete(coat_t *oat, cvector_item_t *obj)
{
    struct srv_client_table *ct = ((struct srv_client_table*) oat);
    assert(ct && ct->magic == SRC_CLIENT_LIST_MAGIC);
    (void) ct;

    struct srv_client *client = (struct srv_client *) obj;
    assert(client && client->magic == ct->clientMagic);

    /* Clean up client info from cspace. */
    if (client->liveness) {
        //seL4_CNode_Revoke(REFOS_CSPACE, client->liveness, REFOS_CDEPTH); // FIXME REVOKE BUG
        seL4_CNode_Delete(REFOS_CSPACE, client->liveness, REFOS_CDEPTH);
        csfree(client->liveness);
    }
    if (client->session) {
        //seL4_CNode_Revoke(REFOS_CSPACE, client->session, REFOS_CDEPTH); // FIXME REVOKE BUG
        seL4_CNode_Delete(REFOS_CSPACE, client->session, REFOS_CDEPTH);
        csfree(client->session);
    }

    if (client->paramBuffer) {
        //seL4_CNode_Revoke(REFOS_CSPACE, client->paramBuffer, REFOS_CDEPTH); // FIXME REVOKE BUG
        seL4_CNode_Delete(REFOS_CSPACE, client->paramBuffer, REFOS_CDEPTH);
        csfree(client->paramBuffer);
    }

    /* Finally, free the entire structure. */
    free(client);
}

void
client_table_init(struct srv_client_table *ct, int maxClients, int badgeBase, uint32_t magic,
                  seL4_CPtr sessionSrcEP)
{
    ct->magic = SRC_CLIENT_LIST_MAGIC;
    ct->maxClients = maxClients;
    ct->clientMagic = magic;
    ct->badgeBase = badgeBase;
    ct->sessionSrcEP = sessionSrcEP;

    /* Configure the object allocation table creation / deletion callback func pointers. */
    ct->allocTable.oat_expand = NULL;
    ct->allocTable.oat_create = client_oat_create;
    ct->allocTable.oat_delete = client_oat_delete;

    /* Initialise our data structures. */
    coat_init(&ct->allocTable, 1, ct->maxClients);
    cvector_init(&ct->pendingFreeList);
}

void
client_table_release(struct srv_client_table *ct)
{
    coat_release(&ct->allocTable);
    cvector_free(&ct->pendingFreeList);
}

void
client_table_postaction(struct srv_client_table *ct)
{
    /* Actually delete all the clients on the pending free list. */
    int len = cvector_count(&ct->pendingFreeList);
    for (int i = 0; i < len; i++) {
        /* Actually delete this client. */
        int id = (int) cvector_get(&ct->pendingFreeList, i);
        if (!client_get(ct, id)) {
            assert(!"Client in pending free list doesn't exist. Book keeping error.");
            continue;
        }
        coat_free(&ct->allocTable, id);
    }
    /* Clear the pending free list. */
    cvector_reset(&ct->pendingFreeList);
}

struct srv_client*
client_alloc(struct srv_client_table *ct, seL4_CPtr liveness)
{
    struct srv_client* nclient = NULL;
    uint32_t arg[COAT_ARGS];
    arg[0] = liveness;

    /* Allocate an ID, and the client structure associated with it. */
    int ID = coat_alloc(&ct->allocTable, arg, (cvector_item_t *) &nclient);
    if (!nclient) {
        printf("ERROR: client_alloc couldn't allocate a client ID.\n");
        return NULL;
    }

    assert(ID != COAT_INVALID_ID);
    assert(nclient->magic == ct->clientMagic);
    (void) ID;
    return nclient;
}

struct srv_client*
client_get(struct srv_client_table *ct, int id)
{
    if (id < 0 || id >= ct->maxClients) {
        /* Invalid ID. */
        return NULL;
    }
    struct srv_client* nclient = (struct srv_client*) coat_get(&ct->allocTable, id);
    if (!nclient) {
        /* No such client ID exists. */
        return NULL;
    }
    assert(nclient->magic == ct->clientMagic);
    return nclient;
}

struct srv_client*
client_get_badge(struct srv_client_table *ct, int badge)
{
    if (badge < ct->badgeBase || badge >= ct->badgeBase + ct->maxClients) {
        return NULL;
    }
    return client_get(ct, badge - ct->badgeBase );
}

void
client_queue_delete(struct srv_client_table *ct, int id)
{
    /* Sanity check on the given ID. */
    if (!client_get(ct, id)) {
        return;
    }
    int len = cvector_count(&ct->pendingFreeList);
    for (int i = 0; i < len; i++) {
        int cid = (int) cvector_get(&ct->pendingFreeList, i);
        if (id == cid) {
            /* Client already queued for deletion. Ignore. */
            return;
        }
    }
    /* Queue this client ID up to be deleted. */
    cvector_add(&ct->pendingFreeList, (cvector_item_t) id);
}

int
client_queue_delete_deathID(struct srv_client_table *ct, int deathID)
{
    for (int i = 0; i < ct->maxClients; i++) {
        struct srv_client *c = client_get(ct, i);
        if (c && c->deathID == deathID) {
            client_queue_delete(ct, c->cID);
            return 0;
        }
    }
    return -1;
}
