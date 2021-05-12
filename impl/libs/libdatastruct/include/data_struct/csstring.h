/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
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

