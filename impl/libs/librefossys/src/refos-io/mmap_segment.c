/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <refos/vmlayout.h>
#include <refos/error.h>
#include <refos-io/mmap_segment.h>
#include <refos-io/internal_state.h>
#include <refos-util/dprintf.h>
#include <refos-util/init.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>

#define REFOS_IO_INTERNAL_MMAP_PAGE_STATUS_BUFFER_SIZE 0x11000
static char _refosioMMapPageStatusBuffer[REFOS_IO_INTERNAL_MMAP_PAGE_STATUS_BUFFER_SIZE];

#define REFOS_IO_INTERNAL_MMAP_SEGMENT_BUFFER_SIZE 0x1000
static char _refosioMMapSegmentStatusBuffer[REFOS_IO_INTERNAL_MMAP_SEGMENT_BUFFER_SIZE];

void
refosio_mmap_init(refos_io_mmap_segment_state_t *s)
{
    cbpool_init_static(&s->mmapRegionPageStatus, PROCESS_MMAP_LIMIT_SIZE_NPAGES,
            _refosioMMapPageStatusBuffer, REFOS_IO_INTERNAL_MMAP_PAGE_STATUS_BUFFER_SIZE);
    cbpool_init_static(&s->mmapRegionSegmentStatus, PROCESS_MMAP_SEGMENTS,
            _refosioMMapSegmentStatusBuffer, REFOS_IO_INTERNAL_MMAP_SEGMENT_BUFFER_SIZE);
}

int
refosio_mmap_segment_fill(refos_io_mmap_segment_state_t *s, uint32_t vaddrOffsetPage)
{
    int error = EINVALID;
    assert(vaddrOffsetPage >= PROCESS_MMAP_BOT && vaddrOffsetPage < PROCESS_MMAP_TOP);
    uint32_t segmentID = (PROCESS_MMAP_TOP - vaddrOffsetPage) /
        (PROCESS_MMAP_SEGMENT_SIZE_NPAGES * REFOS_PAGE_SIZE);
    assert(segmentID < PROCESS_MMAP_SEGMENTS);

    if (cbpool_check_single(&s->mmapRegionSegmentStatus, segmentID)) {
        /* This segment is already filled. Nothing to do here. */
        return ESUCCESS;
    }

    /* Create the window. */
    uint32_t vaddr = PROCESS_MMAP_TOP -
            ((segmentID + 1) * (PROCESS_MMAP_SEGMENT_SIZE_NPAGES * REFOS_PAGE_SIZE));
    assert(vaddr >= PROCESS_MMAP_BOT && vaddr < PROCESS_MMAP_TOP);

    seL4_CPtr window = proc_create_mem_window(vaddr,
            PROCESS_MMAP_SEGMENT_SIZE_NPAGES * REFOS_PAGE_SIZE);
    if (!window || REFOS_GET_ERRNO() != ESUCCESS) {
        seL4_DebugPrintf("mmap_segment_fill: Could not create window.\n");
        return EINVALIDWINDOW;
    }

    /* Create the dataspace. */
    seL4_CPtr dataspace = data_open(REFOS_PROCSERV_EP, "anon", 0, 0,
            PROCESS_MMAP_SEGMENT_SIZE_NPAGES * REFOS_PAGE_SIZE, &error);
    if (error) {
        seL4_DebugPrintf("mmap_segment_fill: Could not create anon dspace.\n");
        error = ENOMEM;
        goto exit1;
    }

    /* Map the segment dataspace into the window. */
    error = data_datamap(REFOS_PROCSERV_EP, dataspace, window, 0);
    if (error != ESUCCESS) {
        seL4_DebugPrintf("mmap_segment_fill: Could not map anon dspace.\n");
        goto exit2;
    }

    /* Set the segment allocated status to TRUE. */
    cbpool_set_single(&s->mmapRegionSegmentStatus, segmentID, true);

    csfree_delete(dataspace);
    csfree_delete(window);
    return ESUCCESS;

    /* Exit stack. */
exit2:
    data_close(REFOS_PROCSERV_EP, dataspace);
    csfree_delete(dataspace);
exit1:
    proc_delete_mem_window(window);
    csfree_delete(window);
    return error;
}

int
refosio_mmap_segment_release(refos_io_mmap_segment_state_t *s, uint32_t vaddrOffsetPage)
{
    assert(vaddrOffsetPage >= PROCESS_MMAP_BOT && vaddrOffsetPage < PROCESS_MMAP_TOP);
    uint32_t segmentID = (PROCESS_MMAP_TOP - vaddrOffsetPage) /
        (PROCESS_MMAP_SEGMENT_SIZE_NPAGES * REFOS_PAGE_SIZE);
    assert(segmentID < PROCESS_MMAP_SEGMENTS);

    if (!cbpool_check_single(&s->mmapRegionSegmentStatus, segmentID)) {
        /* This segment is already released. Nothing to do here. */
        return ESUCCESS;
    }

    /* Check that every page associated has neem release. Otherwise we don't unmap yet. */
    for (int i = 0; i < PROCESS_MMAP_SEGMENT_SIZE_NPAGES; i++) {
        uint32_t page = ( (PROCESS_MMAP_TOP - vaddrOffsetPage) / REFOS_PAGE_SIZE ) + i;
        if (cbpool_check_single(&s->mmapRegionPageStatus, page)) {
            /* A page is still mapped here. */
            return EUNMAPFIRST;
        }
    }

    /* Get the window. */
    seL4_CPtr window = proc_get_mem_window(vaddrOffsetPage);
    if (!window) {
        /* Nothing mapped here. Nothing to do. */
        seL4_DebugPrintf("mmap_segment_release: No window to release. Doing nothing.\n");
        return ESUCCESS;
    }

    /* Get the associated dataspace and release it. */
    refos_err_t refos_err = -EINVALID;
    seL4_CPtr dspace = proc_get_mem_window_dspace(window, &refos_err);
    if (refos_err != ESUCCESS) {
        seL4_DebugPrintf("mmap_segment_fill: Failed to get window dataspace, error occued.\n");
        return (int) refos_err;
    }
    if (dspace) {
        int error = data_close(REFOS_PROCSERV_EP, dspace);
        if (error) {
            seL4_DebugPrintf("mmap_segment_fill: Failed delete dataspace.\n");
        }
        csfree_delete(dspace);
    }

    /* Finally, delete the window. */
    int error = proc_delete_mem_window(window);
    if (error) {
        seL4_DebugPrintf("mmap_segment_fill: Failed to delete window, error occued.\n");
    }
    csfree_delete(window);

    /* Set the segment allocated status back to FALSE. */
    cbpool_set_single(&s->mmapRegionSegmentStatus, segmentID, false);
    return ESUCCESS;
}

int
refosio_mmap_anon(refos_io_mmap_segment_state_t *s, int npages, uint32_t *vaddrDest)
{
    if (!npages) {
        /* Nothing to do here. */
        return ESUCCESS;
    }
    assert(s);

    /* Allocate the mmap pages from the page bitmap allocator. */
    uint32_t vaddrOffsetPage = cbpool_alloc(&s->mmapRegionPageStatus, npages);
    if (vaddrOffsetPage == CBPOOL_INVALID) {
        seL4_DebugPrintf("mmap_anon: Could not allocate page region. Out of virtual memory.\n");
        return ENOMEM;
    }

    /* Loop through every segment and fill it in. */
    for (int i = 0; i < npages; i++) {
        uint32_t vaddr = PROCESS_MMAP_TOP - ((vaddrOffsetPage + i + 1) * REFOS_PAGE_SIZE);

        /* Fill in the segment. If the segment already is filled then it does nothing. */
        int error = refosio_mmap_segment_fill(s, vaddr);
        if (error != ESUCCESS) {
            seL4_DebugPrintf("mmap_segment_fill failed. Cannot fully recover from this.\n");

            /* All the segments up to this point have been allocated. All the segments here and
               after have not been, so unset them. This prevents inconsistent state between the page
               bitmap and actually mapped pages. */

            cbpool_free(&s->mmapRegionPageStatus, vaddrOffsetPage + i, npages - i);
            return error;
        }
    }

    if (vaddrDest) {
        (*vaddrDest) = PROCESS_MMAP_TOP - ((vaddrOffsetPage + npages) * REFOS_PAGE_SIZE);
    }
    return ESUCCESS;
}

int
refosio_munmap_anon(refos_io_mmap_segment_state_t *s, uint32_t vaddr, int npages)
{
    if (npages == 0) {
        /* Nothing to do here. */
        return ESUCCESS;
    }
    if (vaddr >= PROCESS_MMAP_TOP) {
        seL4_DebugPrintf("unmap_anon: invalid vaddr, too high.");
        return EINVALIDPARAM;
    }
    if (vaddr < PROCESS_MMAP_BOT) {
        seL4_DebugPrintf("unmap_anon: invalid vaddr, too low.");
        return EINVALIDPARAM;
    }

    /* Free every page in the range. */
    uint32_t vaddrOffsetPage = (PROCESS_MMAP_TOP - vaddr) / REFOS_PAGE_SIZE;
    assert(vaddrOffsetPage < PROCESS_MMAP_LIMIT_SIZE_NPAGES);
    cbpool_free(&s->mmapRegionPageStatus, vaddrOffsetPage, npages);

    /* Loop through every segment and check for affected. */
    for (int i = 0; i < npages; i++) {
        uint32_t vaddr = PROCESS_MMAP_TOP - ((vaddrOffsetPage + i + 1) * REFOS_PAGE_SIZE);
        int error = refosio_mmap_segment_release(s, vaddr);
        if (error != ESUCCESS && error != EUNMAPFIRST) {
            /* Best and easiest thing we can do here is just leak memory. */
            seL4_DebugPrintf("refosio_mmap_segment_release failed. Leaked memory.\n");
        }
    }

    return ESUCCESS;
}