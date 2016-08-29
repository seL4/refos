/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */

#ifndef _CSTATICSTRING_H_
#define _CSTATICSTRING_H_

#ifndef C_STATIC_STRING_MAXLEN
    #define C_STATIC_STRING_MAXLEN 64
#endif

typedef struct csstring_s {
    char str[C_STATIC_STRING_MAXLEN];
} csstring_t;

#endif /* _CSTATICSTRING_H_ */

