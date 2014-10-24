#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

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
