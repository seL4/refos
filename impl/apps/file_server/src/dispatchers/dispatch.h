/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _FILESERV_SERV_DISPATCH_H_
#define _FILESERV_SERV_DISPATCH_H_

#include "../state.h"

 /*! @file
     @brief Common file server dispatcher helper functions. */

#define FS_DISPATCH_ANON_CLIENT_MAGIC 0x2F883C2C
#define SET_ERRNO_PTR(e, ev) if (e) {(*e) = (ev);}

/*! @brief Helper function to check for an interface.

    Most of the other check_dispatcher_*_interface functions use call this helper function, that
    does most of the real work. It generates a usable userptr containing the client_t structure of
    the calling process. If the calling syscall label enum is outside of given range,  DISPATCH_PASS
    is returned.

    @param m The recieved message structure.
    @param userptr Output userptr containing corresponding client, to be passed into generated
                   interface dispatcher function.
    @param labelMin The minimum syscall label to accept. 
    @param labelMax The maximum syscall label to accept. 
*/
int check_dispatch_interface(srv_msg_t *m, void **userptr, int labelMin, int labelMax);

#endif /* _FILESERV_SERV_DISPATCH_H_ */