# Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause

________case RPC_{{fname.upper()}}:\n
____________assert({{fname}}_handler);\n
____________server_{{fname}}(rpc_userptr);\n
____________break;
