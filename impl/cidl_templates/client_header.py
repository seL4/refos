# Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause

{{if len(comment_text) > 0}}
    /*{{comment_text}} */\n
{{endif}}

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
);
\n
