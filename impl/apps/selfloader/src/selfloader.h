/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*! @file
    @brief Process selfloader system application for RefOS. */

#ifndef _BOOTSTRAP_H_
#define _BOOTSTRAP_H_

#include <stdio.h>
#include <refos/refos.h>
#include <refos/vmlayout.h>
#include <refos-rpc/name_client.h>
#include <refos-rpc/name_client_helper.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <refos-rpc/serv_client.h>
#include <refos-rpc/serv_client_helper.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>
#include <refos-util/init.h>
#include <elf/elf.h>

// Debug printing.
#include <refos-util/dprintf.h>

#ifndef CONFIG_REFOS_DEBUG
    #define printf(x,...)
#endif /* CONFIG_REFOS_DEBUG */

/*! @brief Self-loader application elf file segment information

    It stores segment and file information of each segment to initiate dataspace and window
    of corresponding size.
 */
struct sl_elf_segment_info {
    seL4_Word source;                  /*!< Offset into ELF file containing segment data. */
    unsigned long segmentSize;         /*!< Size of the entire segment. */
    unsigned long fileSize;            /*!< Size of segment that has ELF content. Rest is zeroed. */
    unsigned long flags;               /*!< Read / write flags. */
    seL4_Word vaddr;                   /*!< VAddr into client vspace at which segment resides. */
};

/*! @brief Self-loader application global state. */
struct sl_state {
    serv_connection_t fileservConnection;

    data_mapping_t elfFileHeader;
    sl_dataspace_t elfSegment;

    unsigned int endOfProgram;
    unsigned int heapRegionStart;
    sl_dataspace_t stackRegion;
    sl_dataspace_t heapRegion;
    sl_dataspace_t mmapRegion;
};

#define SELFLOADER_MINI_MORECORE_REGION_SIZE 0x32000

#endif /* _BOOTSTRAP_H_ */


