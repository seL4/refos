#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

________case RPC_{{fname.upper()}}:\n
____________assert({{fname}}_handler);\n
____________server_{{fname}}(rpc_userptr);\n
____________break;
