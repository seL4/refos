/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _SERVER_COMMON_HELPER_LIBRARY_H_
#define _SERVER_COMMON_HELPER_LIBRARY_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sel4/sel4.h>
#include <refos/refos.h>
#include <refos-util/dprintf.h>
#include <refos-util/serv_connect.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>
#include <refos-rpc/proc_common.h>

/*! @file
    @brief Server common shared helper functions.

    This module implements helper functions to help reduce the work required to get a RefOS server
    up and running. It handles the most common setup cases for servers, especially dataspace
    servers, including endpoint creation, registration, and so forth.
*/

#ifndef CONFIG_REFOS_DEBUG
    #define printf(x,...)
#endif /* CONFIG_REFOS_DEBUG */

#define SRV_UNBADGED 0
#define SRV_DEFAULT_MAX_CLIENTS PROCSERV_MAX_PROCESSES
#define SRV_DEFAULT_NOTIFICATION_BUFFER_SIZE 0x8000
#define SRV_DEFAULT_PARAM_BUFFER_SIZE 0x2000
#define SRV_MAGIC 0x261A2055

#ifndef SET_ERRNO_PTR
    #define SET_ERRNO_PTR(e, ev) if (e) {(*e) = (ev);}
#endif

/*! @brief Common server dispatcher message structure.

    Simple structure containing the seL4 message and seL4 badge recieved from the IPC.
*/
typedef struct srv_msg {
    seL4_MessageInfo_t message;
    seL4_Word badge;
} srv_msg_t;


/*! @brief Enum to keep track of the common server dispatching return values. */
enum srv_dispatch_return_code_enum {
    DISPATCH_ERROR = -2,     /*!< A dispatch error has occured. Should not happen. */
    DISPATCH_PASS,           /*!< Wrong dispatcher. Pass onto another one. */
    DISPATCH_SUCCESS         /*!< This is the correct dispatcher, or correctly dispatched. */
};

/*! @brief Common server state configuration.

    This structure stores the server's configuration, including things like name to register under,
    badge layouts, number of max clients, and so forth. Servers wishing to use this library should
    fill this config structure with their configuration parameters then pass this into 
    srv_common_init().
*/
typedef struct srv_common_config {
    /*! @brief Maximum number of clients in the client table. Set to 0 to disable. */
    int maxClients; 
    uint32_t clientBadgeBase; /*!< @brief Client badge number base. */
    uint32_t clientMagic; /*!< @brief Client structure magic number identifier. */

    /*! @brief Size of death / fault notification buffer. Set to 0 to disable. */
    int notificationBufferSize;
    /*! @brief Size of procserv parameter buffer. Set to 0 to disable. */
    int paramBufferSize;

    /*! @brief Server name, used for debug printing. */
    char* serverName;
    /*! @brief Server registration mountpoint. NULL to disable registration. */
    char* mountPointPath;
    /*! @brief Server registration parent nameserver, to register under. */
    seL4_CPtr nameServEP;

    /*! @brief Fault / death async notification badge number. */
    uint32_t faultDeathNotifyBadge;
} srv_common_config_t;

struct srv_common;
typedef struct srv_common srv_common_t;

/*! @brief Common server state structure. */
struct srv_common {
    uint32_t magic;
    srv_common_config_t config;

    /* Endpoints and badges. */
    seL4_CPtr anonEP;
    seL4_CPtr notifyAsyncEP;
    seL4_CPtr notifyClientFaultDeathAsyncEP;

    /* Mapped shared buffers. */
    data_mapping_t notifyBuffer;
    size_t notifyBufferStart;
    data_mapping_t procServParamBuffer;

    /* Client table structure. */
    struct srv_client_table clientTable;

    /* Default client table handlers. These should provide a simple default implementation for
       the serv interface defined in the generated <refos-rpc/serv_server.h>. */

    struct srv_client* (*ctable_connect_direct_handler) (srv_common_t *srv, srv_msg_t *m,
            seL4_CPtr liveness, int* _errno);

    refos_err_t (*ctable_set_param_buffer_handler) (srv_common_t *srv, struct srv_client *c,
            srv_msg_t *m, seL4_CPtr parambufferDataspace, uint32_t parambufferSize);

    void (*ctable_disconnect_direct_handler) (srv_common_t *srv, struct srv_client *c);
};

/*! @brief Notification handler callback type. */
typedef int (*srv_notify_handler_callback_fn_t)(struct proc_notification *notification);

/*! @brief Notification handler callbacks structure. */
typedef struct srv_common_notify_handler_callbacks {
    srv_notify_handler_callback_fn_t handle_server_fault;
    srv_notify_handler_callback_fn_t handle_server_content_init;
    srv_notify_handler_callback_fn_t handle_server_death_notification;
} srv_common_notify_handler_callbacks_t;

/*! @brief Initialise server common state.
    @param s The common server state structure to initialise.
    @param config The  structure containing info on server configuration.
    @return ESUCCESS on success, refos_err_t error otherwise.
*/
int srv_common_init(srv_common_t *s, srv_common_config_t config);

/*! @brief Helper function to mint a badged server endpoint cap.
    @param badge The badge ID to mint.
    @param ep The endpoint to mint from.
    @return The minted capability (Ownership transferred).
*/
seL4_CPtr srv_mint(int badge, seL4_CPtr ep);

/*! @brief Helper function to check recieved caps and unwrapped mask.
    @param m The recieved message.
    @param unwrappedMask The expected capability unwrap mask.
    @param numExtraCaps The expected nubmer of recieved capabilities.
    @return true if the recieved message capabilities match the unwrapped mask and number, false
            if there is a mismatck and the recieved caps are invalid.
*/
bool srv_check_dispatch_caps(srv_msg_t *m, seL4_Word unwrappedMask, int numExtraCaps);

/*! @brief Server notification dispatcher helper.

    The goal of this helper function is to remove the common shared ringbuffer notification reading
    and handling routines used by servers wishing to recieve async notifications. When a message
    is read from the given server state's notification buffer, then the corresponding callback
    handler function pointer is called.

    @param srv The server common state structure. (No ownership)
    @param callbacks Struct containing callbacks, one for each possible type of notification.
                     If the given server does not handle a certain type of notification, simply
                     set the corresponding callback as NULL.
    @return ESUCCESS on success, refos_err_t error otherwise.       
*/
int srv_dispatch_notification(srv_common_t *srv, srv_common_notify_handler_callbacks_t callbacks);

#endif /* _SERVER_COMMON_HELPER_LIBRARY_H_ */