#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

void server_{{fname}}(void *rpc_userptr) {\n

____rpc_sv_init(rpc_userptr);\n\n

{{for type, itype, name, mode, dr, apfx, aref, apsfx in alist}}
    ____
    
    {{if itype in ['buf', 'buf_array'] and mode != 'array'}}
        {{py:type = type.replace('*', '') + '*'}}
    {{endif}}
    
    {{if mode == 'array'}}
        rpc_buffer_t rpc_{{name}} = (rpc_buffer_t)
    {{elif (itype != 'buf' and type != 'str') or mode == 'array'}}
        {{type}} rpc_{{name}} = ({{type}})
    {{else}}
        {{type}} rpc_{{name}} = rpc_malloc(sizeof({{type.replace('*', '')}}));\n
        ____
    {{endif}}

    rpc_sv_pop_{{itype}}{{apfx}}(
        rpc_userptr
        {{if itype == 'buf' and mode != 'array'}}
            , rpc_{{name}}
        {{endif}}
        {{if itype in ['buf', 'buf_array']}}
            , sizeof({{type.replace('*', '')}})
        {{endif}}
    );\n
{{endfor}}

{{for type, itype, name, mode, dr, apfx, aref, apsfx in oalist}}
    ____
    {{if itype == 'buf'}}
        {{if mode == 'array'}}
            {{py: lenv = 'rpc_' + apsfx.replace(', ', '')}}
            rpc_buffer_t rpc_{{name}};\n
            ____rpc_{{name}}.data = rpc_malloc({{lenv}} * sizeof({{type.replace('*', '')}}));\n
            ____rpc_{{name}}.count = {{lenv}};\n
        {{else}}
            {{type.replace('*', '')}}* rpc_{{name}} = rpc_malloc(sizeof({{type.replace('*', '')}}));\n
        {{endif}}
    {{elif itype == 'str'}}
        {{type}} rpc_{{name}} = rpc_malloc(RPC_DEFAULT_CHARBUF_SIZE); 
        // WARNING: This is not safe!\n
    {{else}}
        {{type}} rpc_{{name}} = 0;
        {{if name != '__ret__'}}
             // WARNING: Cannot have output type int.\n
        {{endif}}
    {{endif}}
{{endfor}}
\n

{{for type, itype, name, mode, dr, apfx, aref, apsfx in calist}}
    {{if mode == 'array'}}
    ____rpc_sv_track_obj(rpc_userptr, rpc_{{name}}.data);\n
    {{elif not itype in ['uint', 'cptr']}}
    ____rpc_sv_track_obj(rpc_userptr, rpc_{{name}});\n
    {{endif}}
{{endfor}}
\n

{{for type, itype, name, mode, dr, apfx, aref, apsfx in ralist}}
    {{if mode == 'array'}}
    ____rpc_sv_track_obj(rpc_userptr, rpc_{{name}}.data);\n
    {{elif not itype in ['uint', 'cptr']}}
    ____rpc_sv_track_obj(rpc_userptr, rpc_{{name}});\n
    {{endif}}
{{endfor}}
\n

____
{{if return_type != 'void'}}
    {{for type, itype, name, mode, dr, apfx, aref, apsfx in ralist}}
        {{if aref == '&'}}{{py: aref = '*'}}{{endif}}
        {{aref}}rpc_{{name}} = 
        {{break}}
    {{endfor}}
{{endif}}
    {{fname}}_handler(rpc_userptr
    {{for type, itype, name, mode, dr, apfx, aref, apsfx in calist}}
        {{if mode == 'length' or mode == 'connect_ep'}}{{continue}}{{endif}}
        {{if aref == '&'}}
            {{# Since bufrefs have been sent over as ordinary buffers, we need to deref them
              # before passing into the impl function.}}
            {{py: aref = '*'}}
        {{endif}}
        {{if mode == 'array'}}{{py: aref = ''}}{{endif}}
        , {{aref}}rpc_{{name}}
    {{endfor}}
);\n\n

____reply_{{fname}}(rpc_userptr
    {{for type, itype, name, mode, dr, apfx, aref, apsfx in oalist}}
        {{if aref == '&'}}
            {{# Since bufrefs have been sent over as ordinary buffers, we need to deref them
              # before passing into the reply function.}}
            {{py: aref = '*'}}
        {{endif}}
        , {{aref}}rpc_{{name}}
    {{endfor}}
);\n

}\n\n

void reply_{{fname}}(void *rpc_userptr
    {{for type, itype, name, mode, dr, apfx, aref, apsfx in oalist}}
        {{if mode == 'array'}}{{py: type = 'rpc_buffer_t'}}{{endif}}
        , {{type}} rpc_{{name}}
    {{endfor}}
    ) {\n

____rpc_reset_contents(rpc_userptr);\n

{{for type, itype, name, mode, dr, apfx, aref, apsfx in oalist}}
    ____rpc_sv_push_{{itype}}{{apfx}}(
        rpc_userptr, {{aref}}rpc_{{name}}
        {{if itype == 'buf'}}
            , sizeof({{type.replace('*', '')}})
        {{endif}}
    );\n
{{endfor}}
\n
____rpc_sv_reply(rpc_userptr);\n
____rpc_sv_free_tracked_objs(rpc_userptr);\n
____rpc_sv_release(rpc_userptr);\n
}
\n
