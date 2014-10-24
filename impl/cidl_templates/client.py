#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

{{py: first = True}}
{{return_type}} {{fname}}(
        {{for type, itype, name, mode, dr, apfx, aref, apsfx in calist}}
            {{if first}}
                {{py: first = False}}
            {{else}}
                , 
            {{endif}}
            {{type}} {{name}}
        {{endfor}}
    ) {\n

____int rpc__error_;\n

{{if return_type != 'void'}}
    ____{{return_type}} __ret__;\n
    ____memset(&__ret__, 0, sizeof({{return_type}}));\n
{{endif}}
\n\n

____rpc_init("{{fname}}", RPC_{{fname.upper()}});\n

{{if connect_ep != ''}}
    ____rpc_set_dest({{connect_ep}});\n
{{elif default_connect_ep != ''}}
    ____rpc_set_dest({{default_connect_ep}});\n
{{endif}}

{{for type, itype, name, mode, dr, apfx, aref, apsfx in alist}}
    ____rpc_push_{{itype}}{{apfx}}({{aref}}{{name}}
        {{if itype in ['buf', 'bufref']}}
            , sizeof({{type.replace('*', '')}})
        {{endif}}
    {{apsfx}});\n
{{endfor}}

\n\n
____rpc__error_ = rpc_call_server();\n
____if (rpc__error_) {\n
________rpc_release();\n
        {{if return_type != 'void':}}
            ________return __ret__;\n
        {{endif}}
____}\n\n


{{for type, itype, name, mode, dr, apfx, aref, apsfx in oalist}}
    ____

    {{if itype in ['uint', 'cptr']}}
        {{name}} = ({{type}}) rpc_pop_{{itype}}();\n
    {{else}}
        rpc_pop_{{itype}}{{apfx}}(
            {{aref}}{{name}}
            {{if itype in ['buf', 'bufref']}}
                , sizeof({{type.replace('*', '')}})
            {{endif}}
            {{apsfx}}
        );\n
    {{endif}}
    
{{endfor}}

{{for type, itype, name, mode, dr, apfx, aref, apsfx in oalist}}
    {{if itype == 'cptr'}}
        ____{{name}} = rpc_copyout_cptr({{name}});\n
    {{endif}}
{{endfor}}

____rpc_release();\n

    {{if return_type != 'void'}}
        ____return __ret__;\n
    {{endif}}
}
\n
