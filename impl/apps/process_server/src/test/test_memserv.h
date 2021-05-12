/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _REFOS_PROCESS_SERVER_TEST_MEMSERV_H_
#define _REFOS_PROCESS_SERVER_TEST_MEMSERV_H_

#include <autoconf.h>
#include "test.h"

#ifdef CONFIG_REFOS_RUN_TESTS

int test_window_list(void);

int test_window_associations(void);

int test_ram_dspace_list(void);

int test_ram_dspace_read_write(void);

int test_ram_dspace_content_init(void);

int test_ringbuffer(void);

#endif /* CONFIG_REFOS_RUN_TESTS */

#endif /* _REFOS_PROCESS_SERVER_TEST_MEMSERV_H_ */