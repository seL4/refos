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
    @brief Name server implementation library.

    This module is a helper library that provide functionality which implements a name server. Since
    naming in RefOS is distributed, multiple servers act as name servers, forming a hierachical tree
    directory structure. Each name server resolves a segment of the path, and the destination server
    is the last segment of the path.

    This library provides a standardised implementation of name server to maximise code reuse. The
    dispatcher code for each server which also acts as a name server only needs to implement the
    plugging dispatchers.
*/

#ifndef _REFOS_NAMESERV_IMPL_LIBRARY_H_
#define _REFOS_NAMESERV_IMPL_LIBRARY_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <data_struct/cvector.h>
#include <refos/error.h>

#define REFOS_NAMESERV_MAGIC 0x5FA09B37
#define REFOS_NAMESERV_ENTRY_MAGIC 0x5FA09B37
#define REFOS_NAMESERV_RESOLVED -1

/*! @brief A single entry in the name server registration list. Internal structure, don't touch. */
typedef struct nameserv_entry {
    char* name; /* Has ownership. */
    seL4_CPtr anonEP; /* Has ownership. */
    uint32_t magic;
} nameserv_entry_t;

/*! @brief Name server registration list. Encapsulates the state of a name server implementation. */
typedef struct nameserv_state {
    uint32_t magic;
    void (*free_capability) (seL4_CPtr cap);
    cvector_t entries; /* nameserv_entry_t */
} nameserv_state_t;

/*! @brief Initialise nameserver list.
    @param n The nameserver list to initialise.
    @param free_cap Function pointer to a function to free given capability, in the current
                    environment.
*/
void nameserv_init(nameserv_state_t *n, void (*free_cap) (seL4_CPtr cap));

/*! @brief De-Initialise nameserver list, and release all owned resources.
    @param n The nameserver list to de-initialise.
*/
void nameserv_release(nameserv_state_t *n);

/*! @brief Add a new nameserver registration.
    @param n The nameserver list to add to.
    @param name NULL-terminated string of server name.
    @param anonEP The anonymous endpoint of given server.
    @return ESUCCESS if success, refos_error otherwise.
*/
int nameserv_add(nameserv_state_t *n, const char* name, seL4_CPtr anonEP);

/*! @brief Remove previously added name server registration.
    @param n The nameserver list to delete from.
    @param name NULL-terminated string of server name to delete.
*/
void nameserv_delete(nameserv_state_t *n, const char* name);

/*! @brief Resolve next segment of given path at the given registration list.
    @param n The nameserver list to resolve using.
    @param path NULL-terminated string containing the path to resolve. (No ownership transfer)
    @param outAnonCap Output anonymous cap, if resolve results in external deferral.
    @return 0 if resolve failed, int > 0 (containing number of chars resolved if resolvation success
            with external deferral, or REFOS_NAMESERV_RESOLVED if resolvation success with leaf at
            the current server.
*/
int nameserv_resolve(nameserv_state_t *n, const char* path, seL4_CPtr *outAnonCap);

#endif /* _REFOS_NAMESERV_IMPL_LIBRARY_H_ */