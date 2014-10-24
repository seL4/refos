/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_VIRTUAL_MEMORY_LAYOUT_H_
#define _REFOS_VIRTUAL_MEMORY_LAYOUT_H_

/* Include Kconfig variables. */
#include <autoconf.h>

/*! @file
    @brief RefOS client virtual memory layout.

    Virtual memory layout definitions, also including space layout. This design is very loosely
    based off the linux VM space, in which there is an expanding heap growing upwards, and MMap
    region growing downwards.
    
    @image html vmlayout.png
 */

#define PROCESS_MAX_THREADS 1024
#define PROCESS_MAX_SESSIONS 1024
#define PROCESS_MAX_WINDOWS 8192
 
/*
    Reference: http://duartes.org/gustavo/blog/post/anatomy-of-a-program-in-memory
*/

// ---------------------------------- RefOS OS VSpace ---------------------------

#define PROCESS_KERNEL_RESERVED 0xE0000000 /*!< seL4 kernel restriction. */

#define PROCESS_STATICPARAM_ADDR 0xDFF30000
#define PROCESS_STATICPARAM_SIZE 0x1000
#define PROCESS_STATICPARAM_PROCINFO_ADDR (PROCESS_STATICPARAM_ADDR + 0x800)

#define PROCESS_PARAM_DEFAULTSIZE 0x8000
#define PROCESS_PARAM_DEFAULTSIZE_NPAGES 8

#define PROCESS_SELFLOADER_RESERVED_READELF 0xDFE00000
#define PROCESS_SELFLOADER_RESERVED_READELF_SIZE 0x20000
#define PROCESS_SELFLOADER_RESERVED 0xDF000000 /* For the selfloader process to live in. */

#define PROCESS_WALLOC_END 0xD0000000
#define PROCESS_WALLOC_START 0xC0001000

// ---------------------------------- User Executable region ---------------------------

#define PROCESS_TASK_SIZE 0xC0000000 /* Max. user executable region. */

#define PROCESS_STACK_TOP 0xBFFF0000 /* Stack. */
#define PROCESS_RLIMIT_STACK 0x20000 /* 32 x 4096 = 128kb stack. */
#define PROCESS_STACK_BOT (PROCESS_STACK_TOP - PROCESS_RLIMIT_STACK)

/* MMap region. */
#define PROCESS_MMAP_TOP 0xBF000000  /* MMAP region. */
#define PROCESS_MMAP_LIMIT_SIZE (1UL << 31UL)  /* 2 GB of available virtual memory. */
#define PROCESS_MMAP_BOT (PROCESS_MMAP_TOP - PROCESS_MMAP_LIMIT_SIZE)

/* Heap region. */
#define PROCESS_HEAP_INITIAL_SIZE 0x1000 /* 4kb of initial heap region. */

// ---------------------------------- RefOS OS CSpace ---------------------------

/* Selfloader special CSpace reserves. */
#define PROCCSPACE_SELFLOADER_RESERVED 35
#define PROCCSPACE_SELFLOADER_CSPACE_START 16
#define PROCCSPACE_SELFLOADER_CSPACE_END 34

/* Device Server special capabilities. Note that these overlap with selfloader...because no one
   likely needs a selfloader to act as a device server. */
#define PROCCSPACE_DEVICE_SERV_RESERVED 35

#define PROCCSPACE_ALLOC_REGION_START 61000
#define PROCCSPACE_ALLOC_REGION_END 65000
#define PROCCSPACE_ALLOC_REGION_SIZE (PROCCSPACE_ALLOC_REGION_END - PROCCSPACE_ALLOC_REGION_START)


#endif /* _REFOS_VIRTUAL_MEMORY_LAYOUT_H_ */
