/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "dispatch.h"
#include <refos-util/serv_connect.h>

 /*! @file
     @brief Common timer server dispatcher helper functions. */

/*! @brief Special anonymous client structure.

    We use this to temporarily book-keep an anonymous client who has not fully connected yet. This
    solves the chicken-and-egg problem of needing a rpc_client_t to communicate so the client can
    communicate to set up real communication session.
*/
static struct srv_client _anonClient;

int
check_dispatch_interface(srv_msg_t *m, void **userptr, int labelMin, int labelMax)
{
    assert(userptr);
    if (seL4_MessageInfo_get_label(m->message) != seL4_Fault_NullFault) {
        /* Not a Syscall, pass onto next dispatcher. */
        return DISPATCH_PASS;
    }

    struct srv_client *c = NULL;
    if (m->badge) {
        /* Try to look up client. */
        c = client_get_badge(&timeServCommon->clientTable, m->badge);
    } else {
        /* Anonymous client, unbadged. */
        c = &_anonClient;
        memset(c, 0, sizeof(struct srv_client));
        c->magic = TIMESERV_DISPATCH_ANON_CLIENT_MAGIC;
    }

    if (!c) {
        /* No client registered here, not our syscall to handle. */
        return DISPATCH_PASS;
    }

    seL4_Word syscallFunc = seL4_GetMR(0);
    if (syscallFunc <= labelMin || syscallFunc >= labelMax) {
        /* Not our type of syscall to handle. */
        return DISPATCH_PASS;
    }

    c->rpcClient.userptr = (void*) m;
    c->rpcClient.minfo = m->message;
    (*userptr) = (void*) c;
    return DISPATCH_SUCCESS;
}
