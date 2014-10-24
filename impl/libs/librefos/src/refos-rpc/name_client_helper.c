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

#include <refos-rpc/name_client.h>
#include <refos-rpc/name_client_helper.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <refos-util/dprintf.h>

static bool
nsv_check_path_resolved(char* path)
{
    if (!path) {
        return false;
    }
    if (path[0] == '/') {
        path++;
    }

    int len = strlen(path);
    for (int i = 0; i < len; i++) {
        if (path[i] == '/') {
            return false;
        }
    }

    return true;
}

void
nsv_mountpoint_release(nsv_mountpoint_t *m)
{
    if (!m || !m->success || !m->serverAnon) {
        return;
    }
    m->success = false;
    if (m->serverAnon != REFOS_PROCSERV_EP) {
        proc_del_endpoint(m->serverAnon);
    }
    m->serverAnon = 0;
    memset(m, 0, sizeof(nsv_mountpoint_t));
}

nsv_mountpoint_t
nsv_resolve(char* path)
{
    nsv_mountpoint_t ret;
    memset(&ret, 0, sizeof(nsv_mountpoint_t));

    if (!path) {
        REFOS_SET_ERRNO(EINVALIDPARAM);
        return ret;
    }

    char *cpath = path;
    seL4_CPtr nameServer = REFOS_NAMESERV_EP;

    while (1) {
        int resolvedBytes = 0;
        seL4_CPtr nextNameServer = 0;

        /* Check if path has already been resolved. */
        bool resolved = nsv_check_path_resolved(cpath);
        if (!resolved) {
            nextNameServer = nsv_resolve_segment(nameServer, cpath, &resolvedBytes);
        }

        /* Have we reached the leaf of the path? */
        if (resolved || resolvedBytes == NAMESERV_RESOLVED) {
            ret.success = true;
            ret.serverAnon = nameServer;
            ret.nameservRoot = REFOS_NAMESERV_EP;
            strcpy(ret.dspaceName, cpath);
            strncpy(ret.nameservPathPrefix, path, cpath - path);
            REFOS_SET_ERRNO(ESUCCESS);
            return ret;
        }

        /* Delete the name server. */
        if (nameServer != REFOS_NAMESERV_EP) {
            assert(nameServer);
            proc_del_endpoint(nameServer);
            nameServer = 0;
        }

        /* Was resolve invalid? */
        if (resolvedBytes == 0) {
            ret.success = false;
            REFOS_SET_ERRNO(ESERVERNOTFOUND);
            return ret;
        }

        /* External resolve. */
        assert(nextNameServer);
        nameServer = nextNameServer;
        cpath += resolvedBytes;
        if (cpath[0] == '/') {
            cpath++;
        }
    }
}