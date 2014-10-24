/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <refos-util/init.h>
#include <refos-util/cspace.h>
#include <refos-util/walloc.h>
#include <refos-util/dprintf.h>
#include <refos/refos.h>
#include <refos/vmlayout.h>
#include <syscall_stubs_sel4.h>
#include <autoconf.h>
#include "stdio_copy.h"

/*! @file
    @brief RefOS client environment initialisation helper functions. */

/* Forward declarations to avoid spectacular circular library dependency header soup. */
extern void refosio_init_morecore(struct sl_procinfo_s *procInfo);
extern void refos_init_timer(char *dspacePath);
extern void filetable_init_default(void);

/* Static buffer for the cspace allocator, to avoid malloc() circular dependency disaster. */
#define REFOS_UTIL_CSPACE_STATIC_SIZE 0x8000
static char _refosUtilCSpaceStatic[REFOS_UTIL_CSPACE_STATIC_SIZE];
static char *_refosEnv[1];
extern char **__environ; /*! < POSIX standard, from MUSLC lib. */

/* Provide default weak links to dprintf. */
const char* dprintfServerName __attribute__((weak)) = "????????";
int dprintfServerColour __attribute__((weak)) = 33;

void
refos_initialise_os_minimal(void)
{
    /* Initialise userspace allocator helper libraries. */
    csalloc_init(PROCCSPACE_ALLOC_REGION_START, PROCCSPACE_ALLOC_REGION_END);
    walloc_init(PROCESS_WALLOC_START, PROCESS_WALLOC_END);
}

void refos_initialise_selfloader(void)
{
    /* Initialise userspace allocator helper libraries. */
    csalloc_init(PROCCSPACE_SELFLOADER_CSPACE_START, PROCCSPACE_SELFLOADER_CSPACE_END);
    walloc_init(PROCESS_SELFLOADER_RESERVED_READELF,
        PROCESS_SELFLOADER_RESERVED_READELF + PROCESS_SELFLOADER_RESERVED_READELF_SIZE);
}

void
refos_initialise(void)
{
    SET_MUSLC_SYSCALL_TABLE;

    /* Temporarily use seL4_DebugPutChar before printf is set up. On release kernel this will
       do nothing. */
    refos_seL4_debug_override_stdout();

    /* We first initialise the cspace allocator statically, as MMap and heap (which malloc 
       depend on) needs this. */
    csalloc_init_static(PROCCSPACE_ALLOC_REGION_START, PROCCSPACE_ALLOC_REGION_END,
            _refosUtilCSpaceStatic, REFOS_UTIL_CSPACE_STATIC_SIZE);

    /* Initialise dynamic MMap and heap. */
    refosio_init_morecore(refos_static_param_procinfo());

    /* Initialise userspace allocator helper libraries. */
    walloc_init(PROCESS_WALLOC_START, PROCESS_WALLOC_END);

    /* Write to the STDIO output device. */
    refos_override_stdio(NULL, NULL);
    refos_setup_dataspace_stdio(REFOS_DEFAULT_STDIO_DSPACE);

    /* Initialise file descriptor table. */
    filetable_init_default();

    /* Initialise timer so we can sleep. */
    refos_init_timer(REFOS_DEFAULT_TIMER_DSPACE);

    /* Initialise default environment variables. */
    _refosEnv[0] = NULL;
    __environ = _refosEnv;
    setenv("SHELL", "/fileserv/terminal", true);
    setenv("PWD", "/", true);
    #ifdef CONFIG_REFOS_TIMEZONE
        setenv("TZ", CONFIG_REFOS_TIMEZONE, true);
    #endif
}

char *
refos_static_param(void)
{
    return (char*)(PROCESS_STATICPARAM_ADDR);
}

struct sl_procinfo_s *
refos_static_param_procinfo(void)
{
    return (struct sl_procinfo_s *)(PROCESS_STATICPARAM_PROCINFO_ADDR);
}