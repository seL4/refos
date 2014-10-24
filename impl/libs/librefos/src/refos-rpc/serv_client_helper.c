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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sel4/sel4.h>

#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <refos-rpc/serv_client.h>
#include <refos-rpc/serv_client_helper.h>
#include <refos-util/cspace.h>
#include <refos-util/dprintf.h>

/*! @brief Special helper function to handle the fact that the process server is connectionless. */
static bool
serv_special_connectless_server(seL4_CPtr anon) {
    return (anon == REFOS_NAMESERV_EP || anon == REFOS_PROCSERV_EP);
}

static serv_connection_t
serv_connect_internal(char *serverPath, bool paramBuffer)
{
    _svprintf("Connecting to server [%s]...\n", serverPath);
    serv_connection_t sc;
    memset(&sc, 0, sizeof(serv_connection_t));
    sc.error = EINVALID;

    /* Resolve server path to find the server's anon cap. */
    _svprintf("    Querying nameserv to find anon cap for [%s]....\n", serverPath);
    sc.serverMountPoint = nsv_resolve(serverPath);
    if (!sc.serverMountPoint.success || ROS_ERRNO() != ESUCCESS) {
        _svprintf("    WARNING: Server not found.\n");
        sc.error = ESERVERNOTFOUND;
        goto exit1;
    }

    _svprintf("    Result path prefix [%s] anon 0x%x dspace [%s]....\n",
        sc.serverMountPoint.nameservPathPrefix, sc.serverMountPoint.serverAnon,
        sc.serverMountPoint.dspaceName);
    if (serv_special_connectless_server(sc.serverMountPoint.serverAnon)) {
        /* Connectionless server. */
        _svprintf("    Known connectionless server detected.\n");
        sc.connectionLess = true;
        sc.serverSession = sc.serverMountPoint.serverAnon;
        sc.error = ESUCCESS;
        return sc;
    } else {
        /* Make connection request to server using the anon cap. */
        _svprintf("    Make connection request to server [%s] using the anon cap 0x%x...\n",
                serverPath, sc.serverMountPoint.serverAnon);
        sc.serverSession = serv_connect_direct(sc.serverMountPoint.serverAnon, REFOS_LIVENESS,
                                               &sc.error);
    }
    if (!sc.serverSession || sc.error != ESUCCESS) {
        _svprintf("    WARNING: Failed to anonymously connect to server.\n");
        goto exit2;
    }

    /* Try pinging the server. */
    int error = serv_ping(sc.serverSession);
    if (error) {
        _svprintf("    WARNING: Failed to ping file server.\n");
        sc.error = error;
        goto exit3;
    }

    /* Set up the parameter buffer between client (ie. us) and server. */
    if (paramBuffer) {
        sc.paramBuffer = data_open_map(REFOS_PROCSERV_EP, "anon", 0, 0,
                                       PROCESS_PARAM_DEFAULTSIZE, -1);
        if (sc.paramBuffer.err != ESUCCESS) {
            _svprintf("    WARNING: Failed to create param buffer dspace.\n");
            sc.error = sc.paramBuffer.err;
            goto exit3;
        }
        assert(sc.paramBuffer.window && sc.paramBuffer.dataspace);
        assert(sc.paramBuffer.vaddr != NULL);

        /* Set this parameter buffer on server. */
        error = serv_set_param_buffer(sc.serverSession, sc.paramBuffer.dataspace,
                                      PROCESS_PARAM_DEFAULTSIZE);
        if (error) {
            _svprintf("    Failed to set remote server parameter buffer.");
            sc.error = error;
            goto exit4;
        }
    } else {
        sc.paramBuffer.err = -1;
    }

    _svprintf("Successfully connected to server [%s]!\n", serverPath);
    sc.error = ESUCCESS;
    return sc;

    /* Exit stack. */
exit4:
    assert(sc.paramBuffer.err == ESUCCESS);
    data_mapping_release(sc.paramBuffer);
exit3:
    assert(sc.serverSession);
    if (!sc.connectionLess) {
        serv_disconnect_direct(sc.serverSession);
        seL4_CNode_Delete(REFOS_CSPACE, sc.serverSession, REFOS_CDEPTH);
        csfree(sc.serverSession);
    }
exit2:
    nsv_mountpoint_release(&sc.serverMountPoint);
exit1:
    assert(sc.error != ESUCCESS);
    return sc;
}

serv_connection_t
serv_connect(char *serverPath)
{
    return serv_connect_internal(serverPath, true);
}

serv_connection_t
serv_connect_no_pbuffer(char *serverPath)
{
    return serv_connect_internal(serverPath, false);
}

void 
serv_disconnect(serv_connection_t *sc)
{
    if (sc == NULL || sc->error != ESUCCESS) {
        return;
    }

    /* Clean up the parameter buffer. */
    if (sc->paramBuffer.err == ESUCCESS && sc->paramBuffer.vaddr != NULL) {
        data_mapping_release(sc->paramBuffer);
    }

    /* Disconnect session. */
    if (sc->serverSession && !sc->connectionLess) {
        serv_disconnect_direct(sc->serverSession);
        csfree_delete(sc->serverSession);
    }

    /* Release the mountpoint. */
    nsv_mountpoint_release(&sc->serverMountPoint);
    memset(sc, 0, sizeof(serv_connection_t));
}
