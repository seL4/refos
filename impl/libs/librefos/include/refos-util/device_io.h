/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _REFOS_UTIL_DEVICE_IO_MANAGER_H_
#define _REFOS_UTIL_DEVICE_IO_MANAGER_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sel4/sel4.h>
#include <data_struct/chash.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>
#include <platsupport/io.h>

/*! @file
    @brief Device IO manager helper library. */

#define DEVICE_IO_TABLE_MAGIC 0x1C62D6B2
#define DEVICE_IO_DEV_MAPPING_MAGIC 0x57545A05

/* We don't need a huge table here; there aren't that many total devices on a system. */
#define DEVICE_MMIO_MAPPING_HASHTABLE_SIZE 128

/*! @brief Global Device IO state structure. */
typedef struct dev_io_ops {
    struct ps_io_ops opsIO;
    chash_t MMIOMappings; /*!< vaddr --> data_mapping_t */
    seL4_CPtr IOPorts;
    uint32_t magic;
} dev_io_ops_t;

/*! @brief Initialise device IO manager state.
    
    This function sets the function pointers for the device IO interfaces that any libplatsupport
    driver depends on. This means that this initialisation must be done before any libplatsupport
    drivers can be used.

    @param io The device state structure to initialise (No ownership transfer).
*/
void devio_init(dev_io_ops_t *io);

#endif /* _REFOS_UTIL_DEVICE_IO_MANAGER_H_ */