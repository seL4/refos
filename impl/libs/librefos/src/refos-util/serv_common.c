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
#include <string.h>
#include <assert.h>

#include <refos/vmlayout.h>
#include <refos/refos.h>
#include <refos/error.h>
#include <refos/share.h>
#include <refos-util/cspace.h>
#include <refos-util/serv_common.h>
#include <refos-util/serv_connect.h>

#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <refos-rpc/name_client.h>
#include <refos-rpc/name_client_helper.h>

/* -------------------- Server Default Client Table Handler Helpers ----------------------------- */

struct srv_client*
srv_ctable_connect_direct_handler(srv_common_t *srv, srv_msg_t *m,
        seL4_CPtr liveness, int* _errno)
{
    assert(srv && srv->magic == SRV_MAGIC && m);

    /* Check that the liveness cap passed in correctly. */
    if(!srv_check_dispatch_caps(m, 0x00000000, 1)) {
        SET_ERRNO_PTR(_errno, EINVALIDPARAM);
        return NULL;
    }
    int error = ENOMEM;

    /* Copyout the liveness cap, create session cap cslot. Do not printf before the copyout. */
    seL4_CPtr livenessCP = rpc_copyout_cptr(liveness);
    if (!liveness || !livenessCP) {
        goto error0;
    }

    /* Allocate the client structure. */
    struct srv_client *c = client_alloc(&srv->clientTable, livenessCP);
    if (!c) {
        goto error1;
    }

    dprintf("Adding new %s client cID = %d. Hi! (:D)\n", srv->config.serverName, c->cID);
    assert(c->session);

    /* Authenticate the client to the process server, using its liveness cap. */
    error = proc_watch_client(c->liveness, srv->notifyClientFaultDeathAsyncEP, &c->deathID);
    if (error != ESUCCESS) {
        goto error2;
    }

    SET_ERRNO_PTR(_errno, ESUCCESS);
    return c;

    /* Exit stack. */
error2:
    client_queue_delete(&srv->clientTable, c->cID);
    client_table_postaction(&srv->clientTable);
error1:
    seL4_CNode_Delete(REFOS_CSPACE, livenessCP, REFOS_CDEPTH);
    csfree(livenessCP);
error0:
    SET_ERRNO_PTR(_errno, error);
    return NULL;

}

refos_err_t
srv_ctable_set_param_buffer_handler(srv_common_t *srv, struct srv_client *c,
        srv_msg_t *m, seL4_CPtr parambufferDataspace, uint32_t parambufferSize)
{
    assert(srv && srv->magic == SRV_MAGIC);
    assert(c && m);

    /* Special case: unset the parameter buffer. */
    if (!parambufferDataspace && parambufferSize == 0) {
        seL4_CNode_Revoke(REFOS_CSPACE, c->paramBuffer, REFOS_CDEPTH);
        seL4_CNode_Delete(REFOS_CSPACE, c->paramBuffer, REFOS_CDEPTH);
        csfree(c->paramBuffer);
        c->paramBuffer = 0;
        c->paramBufferSize = 0;
        return ESUCCESS;
    }
 
    /* Sanity check parameters. */
    if (!srv_check_dispatch_caps(m, 0x00000000, 1)) {
        return EINVALIDPARAM;
    }
    if (parambufferSize == 0) {
        return EINVALIDPARAM;
    }

    /* Set the parameter buffer by copying out the given dspace cap.
       Do not printf before copyout. */
    c->paramBuffer = rpc_copyout_cptr(parambufferDataspace);
    if (!c->paramBuffer) {
        ROS_ERROR("Failed to copyout the cap.");
        return ENOMEM;
    }
    c->paramBufferSize = parambufferSize;
    dprintf("Set param buffer for client cID = %d...\n", c->cID);

    return ESUCCESS;

}

void
srv_ctable_disconnect_direct_handler(srv_common_t *srv, struct srv_client *c)
{
    assert(srv && srv->magic == SRV_MAGIC && c);
    dprintf("Disconnecting %s client cID = %d. Bye! (D:)\n", srv->config.serverName, c->cID);
    client_queue_delete(&srv->clientTable, c->cID);
    proc_unwatch_client(c->liveness);
}

/* ---------------------------------------------------------------------------------------------- */

int
srv_common_init(srv_common_t *s, srv_common_config_t config)
{
    assert(s);

    dprintf("Initialising %s common server state...\n", config.serverName);
    memset(s, 0, sizeof(srv_common_t));
    s->config = config;

    /* Create sync anon EP. */
    dprintf("    creating %s anon endpoint...\n", config.serverName);
    s->anonEP = proc_new_endpoint();
    if (!s->anonEP) {
        ROS_ERROR("srv_common_init could not create endpoint for %s.", config.serverName);
        return ENOMEM;
    }

    /* Create async EP. */
    dprintf("    creating %s async endpoint...\n", config.serverName);
    s->notifyAsyncEP = proc_new_async_endpoint();
    if (!s->notifyAsyncEP) {
        ROS_ERROR("srv_common_init could not create async endpoint.");
        return ENOMEM;
    }

    /* Mint badged async EP. */
    dprintf("    creating async death and/or fault notification badged EP...\n");
    s->notifyClientFaultDeathAsyncEP = srv_mint(config.faultDeathNotifyBadge, s->notifyAsyncEP);
    if (!s->notifyClientFaultDeathAsyncEP) {
        ROS_ERROR("srv_common_init could not create minted async endpoint.");
        return EINVALID;
    }

    /* Bind the notification AEP. */
    dprintf("    binding notification AEP...\n");
    int error = seL4_TCB_BindNotification(REFOS_THREAD_TCB, s->notifyAsyncEP);
    if (error != seL4_NoError) {
        ROS_ERROR("srv_common_init could not bind async endpoint.");
        return EINVALID;
    }

    /* Register ourselves under our mountpoint name. */
    if (config.mountPointPath != NULL) {
        dprintf("    registering under the mountpoint [%s]...\n", config.mountPointPath);
        error = nsv_register(config.nameServEP, config.mountPointPath, s->anonEP);
        if (error != ESUCCESS) {
            ROS_ERROR("srv_common_init could not register anon ep.");
            return error;
        }
    }

    /* Initialise client table. */
    if (config.maxClients > 0) {
        dprintf("    initialising client table for %s...\n", config.serverName);
        client_table_init(&s->clientTable, config.maxClients, config.clientBadgeBase,
                          config.clientMagic, s->anonEP);

        dprintf("    initialising client table default handlers for %s...\n", config.serverName);
        s->ctable_connect_direct_handler = srv_ctable_connect_direct_handler;
        s->ctable_set_param_buffer_handler = srv_ctable_set_param_buffer_handler;
        s->ctable_disconnect_direct_handler = srv_ctable_disconnect_direct_handler;
    }

    /* Set up our server --> process server notification buffer. */
    if (config.notificationBufferSize > 0) {

        /* Open and map the notification buffer on our side. */
        dprintf("    initialising notification buffer for %s...\n", config.serverName);
        s->notifyBuffer = data_open_map(REFOS_PROCSERV_EP, "anon", 0, 0,
                                        config.notificationBufferSize, -1);
        if (s->notifyBuffer.err != ESUCCESS) {
            ROS_ERROR("srv_common_init failed to open & map anon dspace for notifications.");
            return s->notifyBuffer.err;
        }

        /* Share and set the notification buffer on the procserv side. */
        assert(s->notifyBuffer.dataspace);
        error = proc_notification_buffer(s->notifyBuffer.dataspace);
        if (error != ESUCCESS) {
            ROS_ERROR("srv_common_init failed to set notification buffer.");
            return error;
        }

        s->notifyBufferStart = 0;
    }

    /* Set up our server --> process server parameter buffer. */
    if (config.paramBufferSize > 0) {

        dprintf("    initialising procserv param buffer for %s...\n", config.serverName);
        s->procServParamBuffer = data_open_map(REFOS_PROCSERV_EP, "anon", 0, 0,
                                               config.paramBufferSize, -1);
        if (s->procServParamBuffer.err != ESUCCESS) {
            ROS_ERROR("srv_common_init failed to open & map anon dspace for param buffer.");
            return s->procServParamBuffer.err;
        }
        error = proc_set_parambuffer(s->procServParamBuffer.dataspace, config.paramBufferSize);
        if (error != ESUCCESS) {
            ROS_ERROR("srv_common_init failed to set procserv param buffer.");
            return error;
        }
    }

    s->magic = SRV_MAGIC;
    return ESUCCESS;
}

seL4_CPtr
srv_mint(int badge, seL4_CPtr ep)
{
    assert(ep);
    seL4_CPtr mintEP = csalloc();
    if (!mintEP) {
        ROS_ERROR("Could not allocate cslot to mint badge.");
        return 0;
    }
    int error = seL4_CNode_Mint (
        REFOS_CSPACE, mintEP, REFOS_CDEPTH,
        REFOS_CSPACE, ep, REFOS_CDEPTH,
        seL4_CanWrite | seL4_CanGrant,
        seL4_CapData_Badge_new(badge)
    );
    if (error != seL4_NoError) {
        ROS_ERROR("Could not mint badge.");
        csfree(mintEP);
        return 0;
    }
    return mintEP;
}

bool
srv_check_dispatch_caps(srv_msg_t *m, seL4_Word unwrappedMask, int numExtraCaps)
{
    assert(m);
    if (seL4_MessageInfo_get_capsUnwrapped(m->message) != unwrappedMask ||
            seL4_MessageInfo_get_extraCaps(m->message) != numExtraCaps) {
        return false;
    }
    return true;
}

int
srv_dispatch_notification(srv_common_t *srv, srv_common_notify_handler_callbacks_t callbacks)
{
    assert(srv && srv->magic == SRV_MAGIC);

    /* Allocate a notification structure. */
    struct proc_notification *notification = malloc(sizeof(struct proc_notification)); 
    if (!notification) {
        dprintf("Out of memory while creating notification struct.\n");
        return DISPATCH_ERROR;
    }

    dvprintf("%s recieved fault notification!\n", srv->config.serverName);
    data_mapping_t *nb = &srv->notifyBuffer;
    uint32_t bytesRead = 0;
    int error = DISPATCH_PASS;

    while (1) {
        bytesRead = 0;

        error = refos_share_read (
                (char*) notification, sizeof(struct proc_notification),
                nb->vaddr, nb->size,
                &srv->notifyBufferStart, &bytesRead
        );
        if (!error && bytesRead == 0) {
            /* No more notifications to read. */
            break;
        }

        if (error || bytesRead != sizeof(struct proc_notification)) {
            /* Error during reading notification. */
            dprintf("Could not read information from notification buffer!\n");
            dprintf("    error = %d\n, read = %d", error, bytesRead);
            free(notification);
            return DISPATCH_ERROR;
        }

        /* Paranoid sanity check here. */
        assert(notification->magic == PROCSERV_NOTIFICATION_MAGIC);

        error = DISPATCH_ERROR;
        switch (notification->label) {
        case PROCSERV_NOTIFY_FAULT_DELEGATION:
            if (callbacks.handle_server_fault) {
                error = callbacks.handle_server_fault(notification);
                break;
            }
        case PROCSERV_NOTIFY_CONTENT_INIT:
            if (callbacks.handle_server_content_init) {
                error = callbacks.handle_server_content_init(notification);
                break;
            }
        case PROCSERV_NOTIFY_DEATH:
            if (callbacks.handle_server_death_notification) {
                error = callbacks.handle_server_death_notification(notification);
                break;
            }
        default:
            /* Should never get here. */
            ROS_WARNING("Unknown notification.\n");
            assert(!"Unknown notification.");
            break;
        }
    }

    if (error) {
        ROS_WARNING("Notification handling error on %s: %d\n", srv->config.serverName, error);
    }

    free(notification);
    return error;
}
