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
#include <refos-io/morecore.h>
#include <refos-io/internal_state.h>
#include <refos-io/ipc_state.h>
#include <refos-io/mmap_segment.h>
#include <refos-util/init.h>
#include <refos-util/dprintf.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <autoconf.h>

void
refosio_setup_morecore_override(char *mcRegion, unsigned int mcRegionSize)
{
    if (!mcRegion) {
        refosIOState.staticMoreCoreOverride = 0;
        refosIOState.staticMoreCoreOverrideBase = (uintptr_t) 0;
        refosIOState.staticMoreCoreOverrideTop = (uintptr_t) 0;
        return;
    }
    refosIOState.staticMoreCoreOverride = mcRegion;
    refosIOState.staticMoreCoreOverrideBase = (uintptr_t) refosIOState.staticMoreCoreOverride;
    refosIOState.staticMoreCoreOverrideTop =
            (uintptr_t) &refosIOState.staticMoreCoreOverride[mcRegionSize];
}

void
refosio_init_morecore(struct sl_procinfo_s *procInfo)
{
    assert(procInfo);

    if (procInfo->magic != SELFLOADER_PROCINFO_MAGIC) {
        /* Use seL4_DebugPrintf to print an error message here, as printf is not set up yet. */
        seL4_DebugPrintf("refosio_init_morecore called with invalid procinfo structure.\n");
        seL4_DebugPrintf("You have probably called refos_initialise() from a non-full userland\n");
        seL4_DebugPrintf("environment.\n");
        #if defined(SEL4_DEBUG_KERNEL)
        seL4_DebugHalt();
        #endif
        return;
    }

    if (refosIOState.staticMoreCoreOverride) {
        /* We skip initialising dynamic morecore if there already is a morecore override. */
        return;
    }

#ifdef CONFIG_REFOS_DEBUG_VERBOSE
    seL4_DebugPrintf("bootinfo stack 0x%x --> 0x%x\n", procInfo->stackRegion.vaddr,
            procInfo->stackRegion.vaddr + procInfo->stackRegion.size);
    seL4_DebugPrintf("bootinfo heap 0x%x --> 0x%x\n", procInfo->heapRegion.vaddr,
            procInfo->heapRegion.vaddr + procInfo->heapRegion.size);
#endif

    /* Enable the dynamic heap. */
    refosIOState.procInfo = procInfo;
    refosIOState.dynamicHeap = true;

    /* Set the mmap allocator region. */
    refosio_mmap_init(&refosIOState.mmapState);
    refosIOState.dynamicMMap = true;
}

int
refosio_morecore_expand(sl_dataspace_t *region, size_t sizeAdd)
{
    assert(region && region->dataspace && region->vaddr);
    if (sizeAdd <= 0) {
        /* Nothing to do here. */
        return ESUCCESS;
    }
    refosio_internal_save_IPC_buffer();

    /* Expand the dataspace. */
    int error = data_expand(REFOS_PROCSERV_EP, region->dataspace, region->size + sizeAdd);
    if (error != ESUCCESS) {
        seL4_DebugPrintf("WARNING: refosio_morecore_expand failed to expand dspace.\n");
        seL4_DebugPrintf("Client's malloc will be broken.\n");
        return error;
    }

    /* Resize the brk window to fit. */
    assert(region->window);
    error = proc_resize_mem_window(region->window, region->size + sizeAdd);
    if (error != ESUCCESS) {
        seL4_DebugPrintf("WARNING: refosio_morecore_expand failed to expand window.\n");
        seL4_DebugPrintf("Client's malloc will be broken.\n");
        return error;
    }
    refosio_internal_restore_IPC_buffer();

#ifdef CONFIG_REFOS_DEBUG_VERBOSE
    seL4_DebugPrintf("heap region expand 0x%x --> from 0x%x to 0x%x\n", region->vaddr,
            region->vaddr + region->size, region->vaddr + region->size + sizeAdd);
#endif

    region->size += sizeAdd;
    return ESUCCESS;
}