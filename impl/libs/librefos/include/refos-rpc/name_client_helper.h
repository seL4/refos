/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _RPC_INTERFACE_NAME_CLIENT_HELPER_H_
#define _RPC_INTERFACE_NAME_CLIENT_HELPER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <refos-rpc/name_client.h>
#include <refos-rpc/rpc.h>
#include <refos/error.h>
#include <refos/refos.h>

/*! @file
    @brief Helper functions for the name server interface.

    This file contains a simple layer of helper functions that make using the nameserv interface
    much easier, but are too complex to have been generated by the stub generator. RefOS uses a
    simple hierachical distributed naming scheme where each nameserver may resolves a certain
    prefix of a path.
*/

#define NAMESERV_RESOLVED -1
#define NAMESERV_PATH_MAXLEN 512

/*! @brief Struct containing a mountpoint, which is a completely resolved namespace path. */
typedef struct nsv_mountpoint {
    bool success;
    seL4_CPtr serverAnon; /* Has ownership. */
    char dspaceName[NAMESERV_PATH_MAXLEN];

    seL4_CPtr nameservRoot; /* No ownership. */
    char nameservPathPrefix[NAMESERV_PATH_MAXLEN];
} nsv_mountpoint_t;

/*! @brief Helper function for nsv_resolve_segment_internal() which resolves a single segment.
           You probably don't want to call this directly; use the nsv_resolve() helper function.
    @param nameserv The name server to resolve with.
    @param path The path to resolve.
    @param resolvedBytes Output containing number of bytes resolved.
    @return 0 on error, anon capability to next name server on success.
*/
static inline seL4_CPtr
nsv_resolve_segment(seL4_CPtr nameserv, char* path, int* resolvedBytes)
{
    refos_err_t errnoRetVal = EINVALID;
    int tempResolvedBytes = 0;

    seL4_CPtr tcap = nsv_resolve_segment_internal(nameserv, path, &tempResolvedBytes, &errnoRetVal);
    if (errnoRetVal != ESUCCESS) {
        REFOS_SET_ERRNO(errnoRetVal);
        return 0;
    }

    REFOS_SET_ERRNO(ESUCCESS);
    if (resolvedBytes) {
        (*resolvedBytes) = tempResolvedBytes;
    }
    return tcap;
}

/*! @brief Resolve a path completely.
    
    This function will completely resolve the given path down to the server that actually
    contains the dataspace. It will search through the namespace hierachy until the leaf node.

    @param path String containing the path to resolve.
    @return A mountpoint info structure containing the results of the resolve; look in the
            success flag of the struct to check for any errors.
*/
nsv_mountpoint_t nsv_resolve(char* path);

/*! @brief Release a mount point structure.
    @param m The mountpoint info structure to release. Does NOT actually free the given structure,
             only releases the associated resources. (Takes ownership)
*/
void nsv_mountpoint_release(nsv_mountpoint_t *m);

#endif /* _RPC_INTERFACE_NAME_CLIENT_HELPER_H_ */
