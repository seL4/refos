/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/mman.h>
#include <utils/arith.h>

#include <refos/vmlayout.h>
#include <refos-io/internal_state.h>
#include <refos-io/ipc_state.h>
#include <refos-util/dprintf.h>
#include <refos-util/init.h>

#define _ENOMEM 12

/*! How many pages of memory to expand the heap every increment.
    Too small and this leads to many many expensive resizing operations, too large and we allocate
    all this uneccessary memory and never use it.
*/
#define REFOSIO_HEAP_EXPAND_INCREMENT_NPAGES 2

int refosio_morecore_expand(sl_dataspace_t *region, size_t sizeAdd);

long
sys_brk(va_list ap)
{
    uintptr_t newbrk = va_arg(ap, uintptr_t);

    /* Static more-core override mode. */
    if (refosIOState.staticMoreCoreOverride != NULL) {
        uintptr_t ret = (uintptr_t) 0;
        if (!newbrk) {
            /* If the newbrk is 0, we return the bottom of the static heap */
    		ret = refosIOState.staticMoreCoreOverrideBase;
        } else if (newbrk < refosIOState.staticMoreCoreOverrideTop &&
                newbrk > (uintptr_t)&refosIOState.staticMoreCoreOverride[0]) {
    		ret = refosIOState.staticMoreCoreOverrideBase = newbrk;
        } else {
            seL4_DebugPrintf("ERROR: refos static sbrk out of memory.\n");
            assert(!"ERROR: refos static sbrk out of memory.");
    		ret = -_ENOMEM;
        }

        return ret;
    }

    /* Dynamic more-core sbrk mode. */
    if (!refosIOState.dynamicHeap) {
        seL4_DebugPrintf("Morecore not available. Please call refosio_setup_morecore_override.\n");
        assert(!"Morecore not available. Please call refosio_setup_morecore_override.");
        return -_ENOMEM;
    }
    assert(refosIOState.procInfo);
    assert(refosIOState.procInfo->magic == SELFLOADER_PROCINFO_MAGIC);

    if (!newbrk) {
        /* Assign to bottom of dynamic heap address. */
        assert(refosIOState.procInfo->heapRegion.vaddr);
        return refosIOState.procInfo->heapRegion.vaddr;
    } else if (newbrk < refosIOState.procInfo->heapRegion.vaddr) {
        /* Invalid brk location. */
        return -ENOMEM;
    } else if (newbrk < refosIOState.procInfo->heapRegion.vaddr +
            refosIOState.procInfo->heapRegion.size) {
        /* Our current heap region is large enough. */
        return newbrk;
    }

    /* Our current heap region is too small expand the window and dataspace. */
    /* First work out the increase in size. */
    uint32_t increaseSize = (newbrk + 1) -
            (refosIOState.procInfo->heapRegion.vaddr + refosIOState.procInfo->heapRegion.size);
    assert(increaseSize > 0);
    uint32_t increaseSizePages = refos_round_up_npages(increaseSize);
    increaseSizePages = MAX(increaseSizePages, REFOSIO_HEAP_EXPAND_INCREMENT_NPAGES);

    /* Then expand the region and dataspace. */
    refosio_internal_save_IPC_buffer();
    int error = refosio_morecore_expand(&refosIOState.procInfo->heapRegion,
            increaseSizePages * REFOS_PAGE_SIZE);
    if (error != ESUCCESS) {
        seL4_DebugPrintf("ERROR: refos dynamic sbrk out of memory.\n");
        assert(!"ERROR: refos dynamic sbrk out of memory.");
        return -_ENOMEM;
    }
    refosio_internal_restore_IPC_buffer();

    /* New size should now be lower. */
    if (newbrk < refosIOState.procInfo->heapRegion.vaddr + refosIOState.procInfo->heapRegion.size) {
        return newbrk;
    }

    seL4_DebugPrintf("ERROR: corruption detected, this should never happen.\n");
    assert(!"ERROR: corruption detected, this should never happen.");
    return -_ENOMEM;
}

long
sys_mmap2(va_list ap)
{
    char *addr =  va_arg(ap, char*);
    unsigned int length = va_arg(ap, unsigned int);
    int prot = va_arg(ap, int);
    int flags = va_arg(ap, int);
    int fd = va_arg(ap, int);
    off_t offset = va_arg(ap, int);
    
    (void) prot;
    (void) offset;
    (void) fd;
    (void) addr;

    /* Static more-core override mode. */
    if (refosIOState.staticMoreCoreOverride != NULL) {
        if (flags & MAP_ANONYMOUS) {
            /* Steal from the top of the static region. */
            uintptr_t base = refosIOState.staticMoreCoreOverrideTop - length;
            if (base < refosIOState.staticMoreCoreOverrideBase) {
                assert(!"ERROR: refos static mmap out of memory.");
                return -_ENOMEM;
            }
            refosIOState.staticMoreCoreOverrideTop = base;
            return base;
        }
        assert(!"File mapping not implemented.");
        return -_ENOMEM;
    }

    if (!refosIOState.dynamicMMap) {
        seL4_DebugPrintf("MMap not available. Please call refosio_setup_morecore_override.\n");
        assert(!"MMap not available. Please call refosio_setup_morecore_override.");
        return -_ENOMEM;
    }

    /* Dynamic more-core mmap mode. */
    if (flags & MAP_ANONYMOUS) {
        uint32_t vaddr = 0;
        uint32_t sizeNPages = refos_round_up_npages(length);

        /* Allocate pages and map window. */
        refosio_internal_save_IPC_buffer();
        int error = refosio_mmap_anon(&refosIOState.mmapState, sizeNPages, &vaddr);
        if (error != ESUCCESS || !vaddr) {
            seL4_DebugPrintf("refosio_mmap_anon mapping failed.\n");
            return -_ENOMEM;
        }
        refosio_internal_restore_IPC_buffer();

        return vaddr;
    }

    seL4_DebugPrintf("File mapping not implemented.\n");
    assert(!"File mapping not implemented.");
    return -_ENOMEM;
}

long
sys_munmap(va_list ap)
{
    char *addr =  va_arg(ap, char*);
    unsigned int length = va_arg(ap, unsigned int);

    if (!length) {
        return 0;
    }

    /* Static more-core override mode. */
    if (refosIOState.staticMoreCoreOverride != NULL) {
        /* Do nothing here. Don't unmap static morecore. */
        return 0;
    }

    if (!refosIOState.dynamicMMap) {
        /* No mmap. How can we possibly have munmap? This is madness. */
        return -1;
    }

    if ((uint32_t)addr >= PROCESS_MMAP_BOT && (uint32_t)addr < PROCESS_MMAP_TOP) {
        uint32_t sizeNPages = refos_round_up_npages(length);
        int error = refosio_munmap_anon(&refosIOState.mmapState, (uint32_t) addr, sizeNPages);
        if (error  != ESUCCESS) {
            seL4_DebugPrintf("refosio_munmap_anon mapping failed. Ignoring unmap.\n");
            return -1;
        }
        return 0;
    }

    return 0;
}
