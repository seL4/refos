/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_INITIALISE_HELPER_H_
#define _REFOS_INITIALISE_HELPER_H_

#include <stdint.h>
#include <stdbool.h>
#include <sel4/sel4.h>
#include <autoconf.h>

/*! @file
    @brief RefOS client environment initialisation helper functions.

    Helper functions which initialise a RefOS user or system process environment. Initialises the
    allocators and file tables and so forth.
*/

#define SELFLOADER_PROCINFO_MAGIC 0xD174A029
#define REFOS_DEFAULT_TIMER_DSPACE "/dev_timer/time"
#define REFOS_DEFAULT_DSPACE_IPC_MAXLEN 64

#if defined(CONFIG_REFOS_STDIO_DSPACE_SERIAL)
    #define REFOS_DEFAULT_STDIO_DSPACE "/dev_console/serial"
#elif defined(CONFIG_REFOS_STDIO_DSPACE_SCREEN)
    #define REFOS_DEFAULT_STDIO_DSPACE "/dev_console/screen"
#else
    #define REFOS_DEFAULT_STDIO_DSPACE "/dev_console/stdio"
#endif

/*! @brief Self-loader dataspace mapping struct. */
typedef struct sl_dataspace_s {
    seL4_CPtr dataspace;
    seL4_CPtr window;
    uint32_t vaddr;
    uint32_t size;
} sl_dataspace_t;

/*! @brief Self-loader process boot info struct. Contains ELF related information used for dynamic
           heap and mmap. */
typedef struct sl_procinfo_s {
    uint32_t magic;
    uint32_t elfSegmentEnd;

    sl_dataspace_t heapRegion;
    sl_dataspace_t stackRegion;
} sl_procinfo_t;

/*! @brief Initialise minimal OS environment, for RefOS low-level OS servers. */
void refos_initialise_os_minimal(void);

/*! @brief Initialise a tiny environment, just for the selfloader. */
void refos_initialise_selfloader(void);

/*! @brief Initialise a full RefOS userland environment. Requires all the system infrastructure to
           be set up. */
void refos_initialise(void);

/*! @brief Returns pointer to the contents of the static parameter buffer.
    
    The static parameter buffer is used to pass information from the process management server
    downwards to its child process; This is needed so that the process server can pass information
    down before proper parameter sharing using a shared dataspace has been set up.

    Assumes that the parent process has written the content to a constant predefined address.

    @return Pointer to static parameter buffer.
 */
char *refos_static_param(void);

/*! @brief Returns pointer to the contents of the static procinfo structure.
    @return Pointer to static procinfo structure. (See sl_procinfo_s).
*/
struct sl_procinfo_s *refos_static_param_procinfo(void);

#endif /* _REFOS_INITIALISE_HELPER_H_ */
