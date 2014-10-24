/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_IO_INTERNAL_STATE_H_
#define _REFOS_IO_INTERNAL_STATE_H_

#include <stdint.h>
#include <stdbool.h>
#include <sel4/sel4.h>
#include "stdio.h"
#include "morecore.h"
#include "mmap_segment.h"
#include "filetable.h"

#include <refos-util/walloc.h>
#include <refos-rpc/serv_client.h>
#include <refos-rpc/serv_client_helper.h>

struct sl_procinfo_s;

typedef struct refos_io_internal_state {
    /* STDIO read / write override functions. One example where this comps in handy is the OS
       server, which obviously cannot IPC itself to print. */
    stdio_read_fn_t stdioReadOverride;
    stdio_read_fn_t stdioWriteOverride;

    /*! \brief Statically allocated morecore area.

    This is a rather terrible workaround for servers which would like to use malloc before
    The RefOS userland infrastructure is properly set up.

    */
    char* staticMoreCoreOverride;
    uintptr_t staticMoreCoreOverrideBase;
    uintptr_t staticMoreCoreOverrideTop;

    /*! The STDIO dataspace, owned by Console server. */
    serv_connection_t stdioSession;
    seL4_CPtr stdioDataspace;

    /*! File descriptor table. */
    fd_table_t fdTable;

    /*! Dynamic morecore heap state. */
    bool dynamicHeap;
    struct sl_procinfo_s *procInfo;

    /*! Dynamic morecore mmap state. */
    bool dynamicMMap;
    refos_io_mmap_segment_state_t mmapState;

    /*! Timer state. */
    FILE * timerFD;
} refos_io_internal_state_t;

extern refos_io_internal_state_t refosIOState;

#endif /* _REFOS_IO_INTERNAL_STATE_H_ */