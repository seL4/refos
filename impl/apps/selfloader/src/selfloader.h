/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
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

#if CONFIG_WORD_SIZE == 64
#define Elf_auxv_t Elf64_auxv_t
#elif CONFIG_WORD_SIZE == 32
#define Elf_auxv_t Elf32_auxv_t
#else
#error "Word size unsupported"
#endif /* CONFIG_WORD_SIZE */

#define ENV_STR_SIZE 128

#define AT_SYSINFO 32

/*! @brief Structure to store 32-bit aux */
typedef struct {
  uint32_t a_type;
  union {
      uint32_t a_val;
  } a_un;
} Elf32_auxv_t;

/*! @brief Structure to store 64-bit aux */
typedef struct {
    uint64_t a_type;
    union {
        uint64_t a_val;
    } a_un;
} Elf64_auxv_t;

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


