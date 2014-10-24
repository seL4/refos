/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <refos-rpc/rpc.h>
#include <refos/refos.h>
#include <refos/vmlayout.h>
#include <refos-util/dprintf.h>

#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))
#define ROUND_DOWN(N, S) (((N) / (S)) * (S))

// Static memory pool to allocate from for IPC. This is needed as normal malloc() might itself
// require an RPC, resulting in unexpected behaviour.
#define RPC_STATIC_MEMPOOL_OBJ_SIZE 4096
static char _rpc_static_mempool[RPC_MAX_TRACKED_OBJS][RPC_STATIC_MEMPOOL_OBJ_SIZE];
static bool _rpc_static_mempool_table[RPC_MAX_TRACKED_OBJS];

// Current global MR and cap index, used for setmr and getmr.
uint32_t _rpc_mr;
uint32_t _rpc_cp;

// Other global rpc state.
static seL4_CPtr _rpc_recv_cslot;
ENDPT _rpc_dest_ep;
seL4_MessageInfo_t _rpc_minfo;
uint32_t _rpc_label;
const char* _rpc_name;

// ------------------------------------------- RPC Helper ------------------------------------------

void*
rpc_malloc(size_t sz)
{
    // Minimal static buffer pool allocation.
    // Note that we cannot malloc here, as malloc could call mmap which could call us back,
    // resulting in a cyclic dependency.
    assert(sz <= RPC_STATIC_MEMPOOL_OBJ_SIZE);
    int i;
    for (i = 0; i < RPC_MAX_TRACKED_OBJS; i++) {
        if (!_rpc_static_mempool_table[i]) {
            break;
        }
    }
    assert(i < RPC_MAX_TRACKED_OBJS);
    _rpc_static_mempool_table[i] = true;
    return _rpc_static_mempool[i];
}

void
rpc_free(void *addr)
{
    int i = (((char*)addr) - (&_rpc_static_mempool[0][0])) / RPC_STATIC_MEMPOOL_OBJ_SIZE;
    assert(i >= 0 && i < RPC_MAX_TRACKED_OBJS);
    assert(_rpc_static_mempool_table[i]);
    _rpc_static_mempool_table[i] = false;
}

uint32_t
rpc_marshall(uint32_t cur_mr, const char *str, uint32_t slen)
{
    assert(str);
    if (slen == 0) {
        return cur_mr;
    }

    int i;
    for (i = 0; i < ROUND_DOWN(slen, 4); i+=4, str+=4) {
        seL4_SetMR(cur_mr++, *(seL4_Word*) str);
    }
    if (i != slen) {
        seL4_Word w = 0;
        memcpy(&w, str, slen - i);
        seL4_SetMR(cur_mr++, w);
    }

    return cur_mr;
}

uint32_t
rpc_unmarshall(uint32_t cur_mr, char *str, uint32_t slen)
{
    assert(str);
    if (slen == 0) return cur_mr;

    int i;
    for (i = 0; i < ROUND_DOWN(slen, 4); i+=4, str+=4) {
        *(seL4_Word*) str = seL4_GetMR(cur_mr++);
    }
    if (i != slen) {
        seL4_Word w = seL4_GetMR(cur_mr++);
        memcpy(str, &w, slen - i);
    }

    return cur_mr;
}

void
rpc_setup_recv(seL4_CPtr recv_cslot)
{
	assert(recv_cslot);
	seL4_SetCapReceivePath(REFOS_CSPACE, recv_cslot, REFOS_CSPACE_DEPTH);
	_rpc_recv_cslot = recv_cslot;
}

void
rpc_setup_recv_cspace(seL4_CPtr cspace, seL4_CPtr recv_cslot, seL4_Word depth)
{
    assert(recv_cslot);
    seL4_SetCapReceivePath(cspace, recv_cslot, depth);
    _rpc_recv_cslot = recv_cslot;
}

void
rpc_reset_contents(void *cl)
{
    (void) cl;
    _rpc_mr = 1;
    _rpc_cp = 0;
}

// ------------------------------------------- Client RPC ------------------------------------------

static seL4_CPtr
rpc_get_endpoint(int32_t label)
{
    if (_rpc_dest_ep) return _rpc_dest_ep;
    assert(!"rpc_get_endpoint: unknown label.");
    return (seL4_CPtr)0;
}

void
rpc_init(const char* name_str, int32_t label)
{
    _rpc_label = label;
    _rpc_name = name_str;

	rpc_reset_contents(NULL);

    seL4_SetMR(0, label);

    if (!_rpc_recv_cslot) {
        rpc_setup_recv(REFOS_THREAD_CAP_RECV);
    } else if (seL4_MessageInfo_get_extraCaps(_rpc_minfo) > 0) {
        // Flush recieving path of previous recieved caps.
        seL4_CNode_Delete(REFOS_CSPACE, _rpc_recv_cslot, REFOS_CDEPTH);
    }
}

void
rpc_push_uint(uint32_t v)
{
    seL4_SetMR(_rpc_mr++, v);
}

void
rpc_push_str(const char* v)
{
    uint32_t slen = strlen(v);
    rpc_push_uint(slen);
    _rpc_mr = rpc_marshall(_rpc_mr, v, slen);
}

void
rpc_push_buf(void* v, size_t sz)
{
    if (!sz) return;
    if (!v) sz = 0;
    _rpc_mr = rpc_marshall(_rpc_mr, v, sz);
}

void
rpc_push_buf_array(void* v, size_t sz, uint32_t count)
{
    char *rv = (char*)v;
    rpc_push_uint(count);
    for (uint32_t i = 0; i < count; i++) {
        rpc_push_buf(rv + i * sz, sz);
    }
}

void
rpc_push_cptr(ENDPT v)
{
	seL4_SetCap(_rpc_cp++, v);
}

void
rpc_set_dest(ENDPT dest)
{
    _rpc_dest_ep = dest;
}

uint32_t
rpc_pop_uint()
{
    return seL4_GetMR(_rpc_mr++);
}  

void
rpc_pop_str(char* v)
{
    // WARNING: Outputting to a C char string is never a safe thing to do.
    uint32_t slen = rpc_pop_uint();
    _rpc_mr = rpc_unmarshall(_rpc_mr, v, slen);
    v[slen] = '\0';
}

void
rpc_pop_buf(void* v, size_t sz)
{
    if (!sz) return;
    assert(v);
    _rpc_mr = rpc_unmarshall(_rpc_mr, v, sz);
}

ENDPT
rpc_pop_cptr()
{
   assert(_rpc_recv_cslot);
   if (seL4_MessageInfo_get_extraCaps(_rpc_minfo) < 1) {
       //assert(!"RPC Failed to recieve the cap");
       return 0;
   }
   return _rpc_recv_cslot;
}

void
rpc_pop_buf_array(void* v, size_t sz, uint32_t count)
{
    uint32_t cn = rpc_pop_uint();
    assert(cn <= count);
    for (int i = 0; i < cn; i++) {
        rpc_pop_buf(((char*)v) + (i * sz), sz);
    }
}

int
rpc_call_server()
{
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, _rpc_cp, _rpc_mr);
    int ept = rpc_get_endpoint(_rpc_label);
    _rpc_minfo = seL4_Call(ept, tag);
    rpc_reset_contents(NULL);
    return 0;
}

void
rpc_release()
{
    _rpc_dest_ep = 0;
}


// ---------------------------------------------- Server RPC ---------------------------------------

void
rpc_sv_init(void *cl)
{
    rpc_reset_contents(cl);
    if (!_rpc_recv_cslot) rpc_setup_recv(REFOS_THREAD_CAP_RECV);
	if (!cl) {
        return;
    }
    rpc_client_state_t* c = (rpc_client_state_t*)cl;
    c->num_obj = 0;
    c->skip_reply = false;
}

uint32_t
rpc_sv_pop_uint(void *cl)
{
    (void)cl;
    return seL4_GetMR(_rpc_mr++);
}

char*
rpc_sv_pop_str(void *cl)
{
    uint32_t slen = rpc_sv_pop_uint(cl);
    char *str = rpc_malloc((slen + 1) * sizeof(char));
    assert(str);
    _rpc_mr = rpc_unmarshall(_rpc_mr, str, slen);
    str[slen] = '\0';
    return str;
}

void
rpc_sv_pop_buf(void *cl, void *v, size_t sz)
{
    if (!sz) return;
    _rpc_mr = rpc_unmarshall(_rpc_mr, v, sz);
}

rpc_buffer_t
rpc_sv_pop_buf_array(void *cl, size_t sz)
{
    uint32_t count = rpc_sv_pop_uint(cl);
    char *v = rpc_malloc(count * sz);
    for (uint32_t i = 0; i < count; i++) {
        _rpc_mr = rpc_unmarshall(_rpc_mr, v + i * sz, sz);
    }
    rpc_buffer_t buffer;
    buffer.data = v;
    buffer.count = count;
    return buffer;
}

ENDPT
rpc_sv_pop_cptr(void *cl)
{
    rpc_client_state_t* c = (rpc_client_state_t*)cl;
    if (_rpc_cp >= seL4_MessageInfo_get_extraCaps(c->minfo)) { 
        return 0;
    }
    seL4_Word unw = seL4_MessageInfo_get_capsUnwrapped(c->minfo);
    if (unw & (1 << _rpc_cp)) {
        return seL4_CapData_Badge_get_Badge(seL4_GetBadge(_rpc_cp++));
    }
    _rpc_cp++;
    assert(_rpc_recv_cslot);
    return _rpc_recv_cslot;
}

void
rpc_sv_push_uint(void *cl, uint32_t v)
{
    (void)cl;
    seL4_SetMR(_rpc_mr++, v);
}

void
rpc_sv_push_buf(void *cl, void* v, size_t sz)
{
    if (!sz) return;
    _rpc_mr = rpc_marshall(_rpc_mr, v, sz);
}

void
rpc_sv_push_cptr(void *cl, ENDPT v)
{
    if (!v) return;
    seL4_SetCap(_rpc_cp++, v);
}

void
rpc_sv_push_buf_array(void *cl, rpc_buffer_t v, size_t sz)
{
    rpc_sv_push_uint(cl, v.count);
    for (uint32_t i = 0; i < v.count; i++) {
        rpc_sv_push_buf(cl, ((char*)(v.data)) + (i * sz), sz);
    }
}

void
rpc_sv_reply(void* cl)
{
    if (rpc_sv_skip_reply(cl)) return;
    seL4_CPtr reply_endpoint = rpc_sv_get_reply_endpoint(cl);
    seL4_MessageInfo_t reply = seL4_MessageInfo_new(0, 0, _rpc_cp, _rpc_mr);
    if (reply_endpoint) {
        seL4_Send(reply_endpoint, reply);
    } else {
        seL4_Reply(reply);
    }
}

void
rpc_sv_release(void *cl)
{
    rpc_client_state_t* c = (rpc_client_state_t*)cl;
    (void)c;

    _rpc_dest_ep = 0;

    if (seL4_MessageInfo_get_extraCaps(c->minfo) > 0) {
        // Flush recieving path of previous recieved caps.
        seL4_CNode_Delete(REFOS_CSPACE, _rpc_recv_cslot, REFOS_CSPACE_DEPTH);
    }
}

void
rpc_sv_track_obj(void* cl, void* addr)
{
    rpc_client_state_t* c = (rpc_client_state_t*)cl;
    assert(c->num_obj < RPC_MAX_TRACKED_OBJS - 1);
    c->obj[c->num_obj++] = addr;
}

void
rpc_sv_free_tracked_objs(void* cl)
{
    rpc_client_state_t* c = (rpc_client_state_t*)cl;
    for (int i = 0; i < c->num_obj; i++) {
        rpc_free(c->obj[i]);
    }
    c->num_obj = 0;
}


