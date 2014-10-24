/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_H_
#define _REFOS_H_

#include <sel4/sel4.h>
#include <refos/error.h>

/* Include Kconfig variables. */
#include <autoconf.h>

/*! @file
    @brief Common definitions, macros and inline functions used by RefOS userland library.

    Common definitions, macros and inline functions used by RefOS userland library. Contains
    constant definitions (such as page size), helper functions, helper macros...etc.
*/

/* ----------------------------------- CSpace defines ------------------------------------------- */

#define REFOS_CSPACE_RADIX 16
#define REFOS_CSPACE_GUARD (32 - REFOS_CSPACE_RADIX)
#define REFOS_CSPACE_DEPTH 32
#define REFOS_CDEPTH REFOS_CSPACE_DEPTH

/* ------------------------------ Reserved CSpace caps ------------------------------------------ */

#define REFOS_CSPACE               0x2
#define REFOS_PROCSERV_EP          0x3
#define REFOS_LIVENESS             0x4
#define REFOS_CSPACE_RECV_TEMPSWAP 0x5
#define REFOS_THREAD_CAP_RECV      0x6
#define REFOS_THREAD_TCB           0x8
#define REFOS_DEVICE_IO_PORTS      0x9

/* Process server acts as the root nameserver. */
#define REFOS_NAMESERV_EP          REFOS_PROCSERV_EP

/* --------------------------------- RPC Method bases ------------------------------------------- */

#define PROCSERV_METHODS_BASE       0x1000
#define DATASERV_METHODS_BASE       0x1100
#define NAMESERV_METHODS_BASE   0x1200
#define MEMSERV_METHODS_BASE    0x1300
#define SERV_METHODS_BASE       0x1400
#define DEVICE_METHODS_BASE     0x1500

#define PROCSERV_NOTIFY_TAG 0xA82D2
#define PROCSERV_MAX_PROCESSES 2048

/* ----------------------------------- Helper functions ----------------------------------------- */

/*! @brief The RefOS system small-page size. Should be 4k on most platforms. */
#define REFOS_PAGE_SIZE (1 << seL4_PageBits)

/*! @brief Align the given virtual address to the page size, rounding down. */
#define REFOS_PAGE_ALIGN(x) ((x) & ~(REFOS_PAGE_SIZE - 1))

/*! @brief Helper function to calculate number of pages that a given size in bytes covers, rounding
           up to the next page.
    @param size The size of region in bytes.
    @return The number of pages needed to cover the given size.
*/
static inline uint32_t
refos_round_up_npages(uint32_t size)
{
    return (size / REFOS_PAGE_SIZE) + ((size % REFOS_PAGE_SIZE) ? 1 : 0);
}

#endif /* _REFOS_H_ */
