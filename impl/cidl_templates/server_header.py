# Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause

void server_{{fname}}(void *rpc_userptr);\n

void reply_{{fname}}(void *rpc_userptr
    {{for type, itype, name, mode, dr, apfx, aref, apsfx in oalist}}
        {{if mode == 'array'}}{{py: type = 'rpc_buffer_t'}}{{endif}}
        , {{type}} rpc_{{name}}
    {{endfor}});\n

extern {{return_type}} {{fname}}_handler(void *rpc_userptr
    {{for type, itype, name, mode, dr, apfx, aref, apsfx in calist}}
        {{if mode == 'length' or mode == 'connect_ep'}}{{continue}}{{endif}} 
        {{if mode == 'array'}}{{py: type = 'rpc_buffer_t'}}{{endif}}
        , {{type}} rpc_{{name}}
    {{endfor}}
) {{weak_attrib}};\n
