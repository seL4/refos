/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sel4/sel4.h>

#include <refos/refos.h>
#include <refos/sync.h>
#include <refos-util/cspace.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>

/*! @file
    @brief Basic kernel synchronisation library. */

#define SYNC_ASYNC_BADGE_MAGIC 0x4188A

struct sync_mutex_ {
    seL4_CPtr mapping;
};

sync_mutex_t
sync_create_mutex()
{
    sync_mutex_t mutex;

    /* Create the mutex struct. */
    mutex = (sync_mutex_t) malloc(sizeof(struct sync_mutex_));
    if (!mutex)
        return NULL;

    /* Create the endpoint. */
    mutex->mapping = proc_new_async_endpoint_badged(SYNC_ASYNC_BADGE_MAGIC);
    if (REFOS_GET_ERRNO() != ESUCCESS || mutex->mapping == 0) {
        free(mutex);
        return NULL;
    }

    /* Prime the endpoint. */
    seL4_Signal(mutex->mapping);
    return mutex;
}

void 
sync_destroy_mutex(sync_mutex_t mutex)
{
    proc_del_async_endpoint(mutex->mapping);
    free(mutex);
}

void
sync_acquire(sync_mutex_t mutex)
{
    seL4_Word badge;
    assert(mutex);
    seL4_Wait(mutex->mapping, &badge);
    assert(badge == SYNC_ASYNC_BADGE_MAGIC);
}

void
sync_release(sync_mutex_t mutex)
{
    /* Release the lock and wake the next thread up. */
    seL4_Signal(mutex->mapping);
}

int
sync_try_acquire(sync_mutex_t mutex)
{
    seL4_Word badge = 0;
    seL4_MessageInfo_t info = seL4_Poll(mutex->mapping, &badge);
    (void) info;
    return badge == SYNC_ASYNC_BADGE_MAGIC;
}
