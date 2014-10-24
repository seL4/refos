/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/**
 * @file   rpc.h
 * @date   Fri 09 Aug 2013 13:00:56 EST 
 *
 * @brief  seL4 CIDL RPC Backend Interface
 *
 * This library provides the backend which the code generated from CIDL - Simple C IDL compiler
 * uses in order to work. This library must deal with basic serialisation / deserialisation,
 * manage basic client state on server side, and manage the communication channel in between.
 * The CIDL generated code expects this library to be there.
 *
 * NOTE: All of the implementations here may _NOT_ themselves send RPCs, with few exception.
 * Remember that if your printf involves an IPC to a server, this means no printf unless you
 * use another IPC buffer. Likewise with malloc.
 */

#ifndef _REFOS_RPC_H_
#define _REFOS_RPC_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/**
 * Maximum number of tracked objects for a single RPC call. Shouldn't need to be too long unless
 * you have functions which take a lot of arguments.
 */
#define RPC_MAX_TRACKED_OBJS 32

// -------------------------------------------------------------------------------------------------
// ------------------------------------------ IDL Declarations -------------------------------------
// -------------------------------------------------------------------------------------------------

#define _SEL4_
#ifdef _SEL4_
    #include <sel4/sel4.h>
    #define ENDPT seL4_CPtr
    typedef char byte;
    typedef seL4_CPtr cptr;
    typedef seL4_CPtr cslot;
    typedef seL4_CPtr seL4_CSlot;
    typedef seL4_MessageInfo_t msginfo_t;
#else
    #error "Unimplemented backend."
#endif

// -------------------------------------------------------------------------------------------------
// ------------------------------------------- RPC Helper ------------------------------------------
// -------------------------------------------------------------------------------------------------

/**
 * A helper function to allocate & manage the allocated memory for an RPC. Works like cstdlib
 * malloc().
 * @param[in] sz       Size of memory to allocate in bytes.
 * @return             Pointer to allocated memory.
 */
void* rpc_malloc(size_t sz);

/**
 * Free the given object at memory address. Works like cstdlib free().
 * @param[in] addr     The address ti free.
 */
void rpc_free(void *addr);

/**
 * Use the given cslot as the destination slot for cap transfer. If this isn't called explicitly,
 * the provided client/server interface below should automatically call this with a default cslot.
 * @param[in] recv_cslot CSpace slot to use to recieve caps.
 */
void rpc_setup_recv(cslot recv_cslot);

/**
 * Use the given cslot as the destination slot for cap transfer. This version does not assume
 * default cspace.
 * @param[in] recv_cslot CSpace slot to use to recieve caps.
 */
void rpc_setup_recv_cspace(seL4_CPtr cspace, seL4_CPtr recv_cslot, seL4_Word depth);

/**
 * Resets the data pointers (e.g. current IPC MR number and cap number) in the client's send buffer.
 * @param[in] cl       Generic reference to caller client state structure (optional).
 */
void rpc_reset_contents(void *cl);

// -------------------------------------------------------------------------------------------------
// ---------------------------------------------- Client RPC ---------------------------------------
// -------------------------------------------------------------------------------------------------

/**
 * Initialise client state, ready for a new RPC call.
 * @param[in] name_str Name of call being initiated (for easier debugging, may ignore this).
 * @param[in] label    The call enum label which server will use to identify which call.
 */
void rpc_init(const char* name_str, int32_t label);

/**
 * Set a custom destination endpoint. By default the destination endpoint used is the one specified
 * in the interface XML file. To support interfaces which make sense to be served by multiple
 * servers, the IDL param mode='connect_ep' is used. CIDL will see this flag and call this function
 * with the marked parameter.
 * @param[in] dest     Destination server endpoint that the next rpc_call_server will invoke.
 */
void rpc_set_dest(ENDPT dest);

/**
 * Add a new integer type variable to the end of the current global RPC packet.
 * @param[in] v        Value of integer to add.
 */
void rpc_push_uint(uint32_t v);

/**
 * Add a new C string type variable to the end of the current global RPC packet.
 * @param[in] v        String to add.
 */
void rpc_push_str(const char* v);

/**
 * Add a new generic buffer object to the end of the current global RPC packet.
 * @param[in] v        Pointer to buffer obj to add.
 * @param[in] sz       Size of the buffer obj in bytes.
 */
void rpc_push_buf(void* v, size_t sz);

/**
 * Add a new array of generic buffer objects to the end of the current global RPC packet.
 * @param[in] v        Pointer to array of buffer objects to add.
 * @param[in] sz       Size of a single buffer obj in bytes.
 * @param[in] count    Number of objects in v.
 */
void rpc_push_buf_array(void* v, size_t sz, uint32_t count);

/**
 * Add a new capablity to the end of the current global RPC packet.
 * @param[in] v        Capability to add.
 */
void rpc_push_cptr(ENDPT v);

/**
 * Read the next integer type variable from current global RPC packet.
 * @return             Value of next integer.
 */
uint32_t rpc_pop_uint();

/**
 * Read the next C string from current global RPC packet.
 * NOTE: Using a string as output is not safe in standard C function calls, and likewise it is also
 * not a good idea at all for RPC calls, and is prone to buffer overflow.
 * @param[out] v       Pointer to allocated string to read into.
 */
void rpc_pop_str(char* v);

/**
 * Read the next generic buffer object from current global RPC packet.
 * @param[out] v       Pointer to allocated buffer object to read into.
 * @param[in] sz       Size of generic object to read in bytes.
 */
void rpc_pop_buf(void* v, size_t sz);

/**
 * Read the next capability from current global RPC packet. Note that this function may choose to
 * copy out the recieved cap from the recieve slot in this function and leave an empty stub in
 * rpc_copyout_cptr, or alternatively it may just simply return the recieve slot here and then
 * implement copyout in rpc_copyout_cptr. See @ref rpc_copyout_cptr.
 * @return             CSpace pointer to recieved capability.
 */
ENDPT rpc_pop_cptr();

/**
 * Read the next buffer object array from current global RPC packet.
 * @param[out] v       Pointer to allocated buffer object to read into.
 * @param[in] sz       Size of generic object to read in bytes.
 * @param[in] count    Max. number of objects in array (only used for checking).
 */
void rpc_pop_buf_array(void* v, size_t sz, uint32_t count);

/**
 * Invoke the server. The destination endpoint may be automatically determined from the label for
 * simplicity, unless overridden by a call to @ref rpc_custom_dest.
 * @return             0 on success, non-zero otherwise.
 */
int rpc_call_server();

/**
 * Finish up the current client RPC call and release all allocated objects.
 */
void rpc_release();

/**
 * Allocate a new cslot, and then copy out the given recieved capabiliy from the recieve cslot to a
 * new cslot. This function exists to optionally separate @ref rpc_pop_cptr from allocating a new
 * cslot. This function useful in the case where allocating a cslot itself requires an RPC call,
 * as RPC calling from @ref rpc_pop_cptr may result in munging the IPC buffer. This function may need
 * to temporarily copy the given cap to an empty slot, in order to IPC to allocate a new cslot.
 * @param[in] v        CSpace pointer to capability to copy out.
 * @return             CSpace pointer to copied out capability.
 */
ENDPT rpc_copyout_cptr(ENDPT v);

// -------------------------------------------------------------------------------------------------
// ------------------------------------------- Server RPC ------------------------------------------
// -------------------------------------------------------------------------------------------------

/**
 * Helper structure which stores the basic state needed for an RPC server to manage a single RPC
 * client. Note that the rpc_sv server interface here passes generic void* as client struct and
 * does not assume you use this struct, so this struct is provided for convenience and using this
 * struct is optional. The default implementation assumes the void *cl parameters to be
 * a valid rpc_client_state_t* object;
 *
 * A good way to use this structure is to 'inherit' it by putting it as the first element of your
 * own client structure. Then you may pointer case freely between your own client structure and a
 * rpc_client_state_t.
 */
typedef struct rpc_client_state_s {
    msginfo_t minfo;
    void* obj[RPC_MAX_TRACKED_OBJS];
    uint32_t num_obj;

    bool skip_reply;
    ENDPT reply;

    void *userptr;
} rpc_client_state_t;

/**
 * A simple buffer structure. Buffer array (pointer / length combinations in C) are converted to
 * this on server side.
 */
typedef struct rpc_buffer_s {
    void *data;
    uint32_t count;
} rpc_buffer_t;

/**
 * Initialise server's client state, ready for a new RPC call.
 * @param[in] cl       Generic reference to caller client state structure.
 */
void rpc_sv_init(void *cl);

/**
 * Read next integer from recieved RPC packet.
 * @param[in] cl       Generic reference to caller client state structure.
 * @return             The next integer in packet.
 */
uint32_t rpc_sv_pop_uint(void *cl);

/**
 * Read next string from recieved RPC packet.
 * @param[in] cl       Generic reference to caller client state structure.
 * @return             The allocated and read string.
 */
char* rpc_sv_pop_str(void *cl);

/**
 * Read next buffer object from recieved RPC packet.
 * @param[in] cl       Generic reference to caller client state structure.
 * @param[out] v        Allocated buffer obj to read into.
 * @param[in] sz       Size of buffer object in bytes.
 */
void rpc_sv_pop_buf(void *cl, void *v, size_t sz);

/**
 * Read next array of buffer objects from recieved RPC packet.
 * @param[in] cl       Generic reference to caller client state structure.
 * @param[in] sz       Size of a single buffer object in bytes.
 * @return             The allocated and read buffer obj array.
 */
rpc_buffer_t rpc_sv_pop_buf_array(void *cl, size_t sz);

/**
 * Read next capability from recieved RPC packet. For an unwrapped capability, the unwrapped badge
 * value is returned from this function. In the case of a transferred cap, it is _NOT_ copied out
 * here (i.e. the value of recv_cslot in given @ref rpc_setup_recv is simply returned. It is up to
 * the server handler to copy out given transferred caps. It is also up to the server handler to
 * know (and check) which caps are unwrapped and which are transferred.
 * @param[in] cl       Generic reference to caller client state structure.
 * @return             The next capability.
 */
ENDPT rpc_sv_pop_cptr(void *cl);

/**
 * Add a new integer variable to the end of the current global RPC packet.
 * @param[in] cl       Generic reference to caller client state structure.
 * @param[in] v        String to add.
 */
void rpc_sv_push_uint(void *cl, uint32_t v);

/**
 * Add a new buffer object to the end of the current global RPC packet.
 * @param[in] cl       Generic reference to caller client state structure.
 * @param[in] v        Pointer to buffer object to add.
 * @param[in] sz       size of the given buffer object in bytes.
 */
void rpc_sv_push_buf(void *cl, void* v, size_t sz);

/**
 * Add a capability to the end of the current global RPC packet.
 * @param[in] cl       Generic reference to caller client state structure.
 * @param[in] v        CPtr to capability to add and transfer.
 */
void rpc_sv_push_cptr(void *cl, ENDPT v);

/**
 * Add an array of objects to the end of the current global RPC packet.
 * @param[in] cl       Generic reference to caller client state structure.
 * @param[in] v        A value rpc_buffer_t containing pointer to buffer and count info.
 * @param[in] sz       size of the a single object in bytes.
 */
void rpc_sv_push_buf_array(void *cl, rpc_buffer_t v, size_t sz);

/**
 * Reply to the client RPC. Depending on whether the reply is immediate or saved-first then replied
 * later, this function should send to the correct corresponding reply endpoint in either case. 
 * @param[in] cl       Generic reference to caller client state structure.
 */
void rpc_sv_reply(void* cl);

/**
 * End the current RPC for the given client caller and release all tis allocated objects.
 * @param[in] cl       Generic reference to caller client state structure.
 */
void rpc_sv_release(void *cl);

/**
 * Optionally save the current client RPC reply cap to reply later. A default weak definition is
 * provided, and servers may want to override this. If this function returns True, the generated
 * reply_*() functions are not called and must be invoked manually by the server. Useful
 * for implementing server-side asynchronous syscalls which providing a synchronous syscall
 * interface to clients. You may RPC from this function.
 * @param[in] cl       Generic reference to caller client state structure.
 * @return             False to reply immediately, True if the reply endpoint was saved.
 */
bool rpc_sv_save_reply(void *cl);

/**
 * Retrieve the saved reply endpoint if it has been saved, or 0 otherwise.
 * @param[in] cl       Generic reference to caller client state structure.
 * @return             saved reply endpoint if it was saved, 0 otherwise to reply immediately.
 */
ENDPT rpc_sv_get_reply_endpoint(void *cl);

void rpc_sv_track_obj(void* cl, void* addr);

void rpc_sv_free_tracked_objs(void* cl);

bool rpc_sv_skip_reply(void *cl);

#endif /* _REFOS_RPC_H_ */

