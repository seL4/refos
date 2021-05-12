/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _CMACROUTILS_H_
#define _CMACROUTILS_H_

#ifdef _MIN
    #define _MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef _MAX
#define _MAX(a,b) (((a)>(b))?(a):(b))
#endif
#ifndef _MiB
#define _MiB(x) (1024*1024*(x))
#endif
#ifndef _ALIGN
#define _ALIGN(x, y) ((y) % (x) == 0 ? (y) : (y) - ((y) % (x)) + (x))
#endif

#ifdef PAGE_SIZE
    #define _PAGE_ALIGN(x) ((x) & ~(PAGE_SIZE - 1))
#endif

#endif 
