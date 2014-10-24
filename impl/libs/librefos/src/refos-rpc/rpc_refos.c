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
#include <stdbool.h>
#include <stdlib.h>
#include <sel4/sel4.h>

#include <refos-rpc/rpc.h>
#include <refos/refos.h>
#include <refos/vmlayout.h>
#include <refos-util/dprintf.h>
#include <refos-util/cspace.h>

seL4_CPtr rpc_copyout_cptr(seL4_CPtr v) __attribute__((weak)); 
seL4_CPtr
rpc_copyout_cptr(seL4_CPtr v)
{
    /* Note that this is used to implement malloc itself , so we can NOT malloc here anywhere
       in this function. */

    if (!v) {
        /* Don't copyout a NULL cap. */
        return (seL4_CPtr) 0;
    }

    /* Allocate a new cslot and copy the cap out from the shared recieve slot.
       Remember that this function passes ownership of the cap, so the caller
       needs to delete the cap and free the slot, akin to void *mem = malloc(x);
    */
    seL4_CPtr cslot = csalloc();
    if (cslot == 0) {
        REFOS_SET_ERRNO(ENOMEM);
        return (seL4_CPtr) 0;
    }
    int error = seL4_CNode_Move(
        REFOS_CSPACE, cslot, REFOS_CSPACE_DEPTH,
        REFOS_CSPACE, v, REFOS_CSPACE_DEPTH
    );
    assert(error == seL4_NoError);
    (void) error;

    return cslot;
}

ENDPT rpc_sv_get_reply_endpoint(void *cl) __attribute__((weak)); 
ENDPT
rpc_sv_get_reply_endpoint(void *cl)
{
    rpc_client_state_t *c = (rpc_client_state_t *)cl;
    if (!c) {
        return 0;
    }
    return c->reply;
}

bool rpc_sv_skip_reply(void *cl) __attribute__((weak)); 
bool
rpc_sv_skip_reply(void *cl)
{
    rpc_client_state_t *c = (rpc_client_state_t *)cl;
    if (!c) return false;
    return c->skip_reply;
}

void
rpc_helper_client_release(void *cl)
{
    rpc_client_state_t *c = (rpc_client_state_t*) cl;
    if (!c) return;
    assert(c->num_obj == 0);
    
    // Delete the client's reply cap slot.
    if (c->reply) {
        seL4_CNode_Revoke(REFOS_CSPACE, c->reply, REFOS_CSPACE_DEPTH);
        csfree_delete(c->reply);
        c->reply = 0;
    }
}



