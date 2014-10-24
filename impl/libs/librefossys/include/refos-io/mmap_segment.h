/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_IO_MMAP_SEGMENT_H_
#define _REFOS_IO_MMAP_SEGMENT_H_

#include <autoconf.h>
#include <sel4/sel4.h>
#include <stdlib.h>
#include <data_struct/cbpool.h>
#include <refos/refos.h>
#include <refos/vmlayout.h>

#define PROCESS_MMAP_LIMIT_SIZE_NPAGES (PROCESS_MMAP_LIMIT_SIZE / REFOS_PAGE_SIZE)
#define PROCESS_MMAP_SEGMENT_SIZE_NPAGES (128UL)
#define PROCESS_MMAP_SEGMENTS (PROCESS_MMAP_LIMIT_SIZE_NPAGES / PROCESS_MMAP_SEGMENT_SIZE_NPAGES)

/*! @file
    @brief MMap implementation for RefOS userland.

    Unfortunately, the POSIX MMap interface doesn't play very happily with RefOS dataspace and
    window segment abstraction. Some tradeoffs here must be made between speed, overhead and
    efficiency.

    RefOS userland dynamic MMap works as follows:

              bitmap (1 bit per 4096 page) 
                        ▼
         1 0 0 1 0 1 1 0 1 ... 1 1 1 1 1 1 1 1 0 0 0 0 0 0 . . .
         |_________________________________|_____________ . . .
         ◀―――――――――――― segment ――――――――――――▶ 
                (128 pages per segment)

    Pages are tracked using a bitmap, and segments are allocated via a bitmap allocator. This forms
    a crude 2 level page table. When a segment is allocated, all the page bits contained at flipped
    to 1. When unmap occurs, all the page bits are flipped to zero, and any segments with their
    entire contents unmapped would be deleted.

    Note that we do not book-keep the dataspace and window caps here. We reply on the get functions
    from process server to book keep them, to avoid the inefficient double book-keeping.

    ref: http://gcc.gnu.org/onlinedocs/libstdc++/manual/bitmap_allocator.html
         http://en.wikipedia.org/wiki/Free_space_bitmap

*/

typedef struct refos_io_mmap_segment_state {

    /*! 524288 page bitmap. Not much memory, only 16384 bytes. */
    cbpool_t mmapRegionPageStatus;

    /*! 4096 segment bitmap. Negligible memory, only 128 bytes. */
    cbpool_t mmapRegionSegmentStatus;

} refos_io_mmap_segment_state_t;

void refosio_mmap_init(refos_io_mmap_segment_state_t *s);

int refosio_mmap_segment_fill(refos_io_mmap_segment_state_t *s, uint32_t vaddrOffsetPage);

int refosio_mmap_segment_release(refos_io_mmap_segment_state_t *s, uint32_t vaddrOffsetPage);

int refosio_mmap_anon(refos_io_mmap_segment_state_t *s, int npages, uint32_t *vaddrDest);

int refosio_munmap_anon(refos_io_mmap_segment_state_t *s, uint32_t vaddr, int npages);

#endif /* _REFOS_IO_MMAP_SEGMENT_H_ */