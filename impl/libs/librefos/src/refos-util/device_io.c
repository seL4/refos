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
#include <autoconf.h>

#include <refos-util/device_io.h>
#include <refos-util/dprintf.h>

/*! @file
    @brief Device IO manager helper library.

    The platform support library libplatsupport expects a device I/O interface.
    This helper library is responsible for implementing that interface which libplatsupport relies
    on, so we can use the device drivers in libplatsupport. This includes device MMIO mapping /
    unmapping, DMA allocation, and x86 IO port operations.
*/

/* ---------------------------------------- Device mapping -------------------------------------- */

/*! @brief Map device MMIO frame(s)
    
    The actual mapping is done via an IPC to the process server. This function is simply a wrapper
    on that, book-keeping the allocated vspace window so we can unmap it.

    @param cookie The deviceIO state (dev_io_ops_t).
    @param paddr The physical address of the device to be mapped.
    @param size The size in butes of the MMIO frames.
    @param cached Whether to map this in cached or uncached mode.
    @param flags Hints about the nature of the mapping (unused).
    @return VAddr pointer to mapped device frame(s) if success, NULL otherwise.
*/
static void *
dev_io_map(void* cookie, uintptr_t paddr, size_t size, int cached, ps_mem_flags_t flags)
{
    dvprintf("iomap paddr 0x%x sz 0x%x %s flags 0x%x.\n", (uint32_t) paddr, size,
             cached ? "cached" : "uncached", (uint32_t) flags);

    dev_io_ops_t *io = (dev_io_ops_t *) cookie;
    assert(io && io->magic == DEVICE_IO_TABLE_MAGIC);
    (void) flags;

    /* Allocate mapping structure. */
    data_mapping_t *deviceMapping = malloc(sizeof(data_mapping_t));
    if (!deviceMapping) {
        ROS_ERROR("Could not allocate device IO structure. Out of memory.");
        return NULL;
    }

    /* Open and map the device MMIO dataspace. */
    (*deviceMapping) = data_open_map(REFOS_PROCSERV_EP, "anon",
            DSPACE_FLAG_DEVICE_PADDR | (cached ? 0 : DSPACE_FLAG_UNCACHED),
            (int) paddr, size, size);
    if (deviceMapping->err) {
        ROS_ERROR("Could not open and map device dataspace.");
        goto exit1;
    }

    /* Add to vaddr --> mapping table to book-keep deletion on unmap(). */
    chash_set(&io->MMIOMappings, (uint32_t) deviceMapping->vaddr, (chash_item_t) deviceMapping);

    dvprintf("dev_io_map paddr 0x%x OK --> vaddr 0x%x.\n",
             (uint32_t) paddr, (uint32_t) deviceMapping->vaddr);

    return deviceMapping->vaddr;

    /* Exit stack. */
exit1:
    assert(deviceMapping);
    free(deviceMapping);

    return NULL;
}

/*! @brief Unmap a previous MMIO mapped device frame(s).
    @param cookie The deviceIO state (dev_io_ops_t).
    @param vaddr The previous mapped vaddr of device frame(s).
    @param size The size of previous mapped device frame(s) 
*/
static void
dev_io_unmap(void *cookie, void *vaddr, size_t size)
{
    dvprintf("iounmap vaddr 0x%x sz 0x%x.\n", (uint32_t) vaddr, size);
    dev_io_ops_t *io = (dev_io_ops_t *) cookie;
    assert(io && io->magic == DEVICE_IO_TABLE_MAGIC);

    /* Retrieve the previously mapped MMIO entry. */
    data_mapping_t *deviceMapping = (data_mapping_t *)
        chash_get(&io->MMIOMappings, (uint32_t) vaddr);
    if (!deviceMapping) {
        ROS_ERROR("dev_io_unmap failed, no such mapping exists.");
        return;
    }
    assert(deviceMapping && deviceMapping->vaddr == vaddr);

    /* Delete this mapping. */
    data_mapping_release(*deviceMapping);
    memset(deviceMapping, 0, sizeof(data_mapping_t));
    free(deviceMapping);

    /* Delete from vaddr hash table. */
    chash_remove(&io->MMIOMappings, (uint32_t) vaddr);
}

/* --------------------------------------- Device IO Ports -------------------------------------- */

static int
dev_io_port_in(void* cookie, uint32_t port, int io_size, uint32_t *result)
{
    dev_io_ops_t *io = (dev_io_ops_t *) cookie;
    assert(io && io->magic == DEVICE_IO_TABLE_MAGIC);
    (void) io;

#if defined(PLAT_PC99)
    if (io->IOPorts) {
        seL4_IA32_IOPort_In8_t res8;
        seL4_IA32_IOPort_In16_t res16;
        seL4_IA32_IOPort_In32_t res32;

        res8.error = -1;
        res16.error = -1;
        res32.error = -1;

        switch (io_size) {
            case 1: /* 8 bits. */
                res8 = seL4_IA32_IOPort_In8(io->IOPorts, port);
                if (!res8.error && result != NULL) {
                    (*result) = (uint32_t) res8.result;
                }
                return res8.error;
            case 2: /* 16 bits. */
                res16 = seL4_IA32_IOPort_In16(io->IOPorts, port);
                if (!res16.error && result != NULL) {
                    (*result) = (uint32_t) res16.result;
                }
                return res16.error;
            case 4: /* 32 bits. */
                res32 = seL4_IA32_IOPort_In32(io->IOPorts, port);
                if (!res32.error && result != NULL) {
                    (*result) = (uint32_t) res32.result;
                }
                return res32.error;
        }
    }
#endif

    return -1;
}

static int
dev_io_port_out(void* cookie, uint32_t port, int io_size, uint32_t val)
{
    dev_io_ops_t *io = (dev_io_ops_t *) cookie;
    assert(io && io->magic == DEVICE_IO_TABLE_MAGIC);
    (void) io;

#if defined(PLAT_PC99)
    if (io->IOPorts) {
        switch (io_size) {
            case 1: /* 8 bits. */
                seL4_IA32_IOPort_Out8(io->IOPorts, port, (uint8_t) val);
                return 0;
            case 2: /* 16 bits. */
                seL4_IA32_IOPort_Out16(io->IOPorts, port, (uint16_t) val);
                return 0;
            case 4: /* 32 bits. */
                seL4_IA32_IOPort_Out32(io->IOPorts, port, (uint32_t) val);
                return 0;
        }
    }
#endif

    return -1;
}

/* ----------------------------------------- Device DMA ----------------------------------------- */

static void*
dev_dma_alloc(void *cookie, size_t size, int align, int cached, ps_mem_flags_t flags)
{
    assert(!"dev_dma_alloc unimplemented.");
    return NULL;
}

static void
dev_dma_free(void *cookie, void *addr, size_t size)
{
    assert(!"dev_dma_free unimplemented.");
}

static uintptr_t
dev_dma_pin(void *cookie, void *addr, size_t size)
{
    assert(!"dev_dma_pin unimplemented.");
    return (uintptr_t) 0;
}

static void
dev_dma_unpin(void *cookie, void *addr, size_t size)
{
    assert(!"dev_dma_unpin unimplemented.");
}

static void
dev_dma_cache_op(void *cookie, void *addr, size_t size, dma_cache_op_t op)
{
    assert(!"dev_dma_cache_op unimplemented.");
}

/* ----------------------------------- Device IO init function ---------------------------------- */

void
devio_init(dev_io_ops_t *io)
{
    assert(io);
    memset(io, 0, sizeof(dev_io_ops_t));
    io->magic = DEVICE_IO_TABLE_MAGIC;

#if defined(PLAT_PC99)
    io->IOPorts = REFOS_DEVICE_IO_PORTS;
#endif

    /* Set the function pointers for the IO mapper. */
    ps_io_mapper_t *ioMapper = &io->opsIO.io_mapper;
    ioMapper->cookie = (void*) io;
    ioMapper->io_map_fn = dev_io_map;
    ioMapper->io_unmap_fn = dev_io_unmap;

    /* Set the function pointers for the IO port operations. */
    ps_io_port_ops_t *ioPorts = &io->opsIO.io_port_ops;
    ioPorts->cookie = (void*) io;
    ioPorts->io_port_in_fn = dev_io_port_in;
    ioPorts->io_port_out_fn = dev_io_port_out;

    /* Set the function pointers for the DMA operations. */
    ps_dma_man_t *dmaManager = &io->opsIO.dma_manager;
    dmaManager->cookie = (void*) io;
    dmaManager->dma_alloc_fn = dev_dma_alloc;
    dmaManager->dma_free_fn = dev_dma_free;
    dmaManager->dma_pin_fn = dev_dma_pin;
    dmaManager->dma_unpin_fn = dev_dma_unpin;
    dmaManager->dma_cache_op_fn = dev_dma_cache_op;

    /* Initialise MMIO mapping table. */
    chash_init(&io->MMIOMappings, DEVICE_MMIO_MAPPING_HASHTABLE_SIZE);
}
