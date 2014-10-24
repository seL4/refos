/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
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
