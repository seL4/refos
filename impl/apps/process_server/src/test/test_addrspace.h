/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _REFOS_PROCESS_SERVER_TEST_ADDRSPACE_H_
#define _REFOS_PROCESS_SERVER_TEST_ADDRSPACE_H_

#include <autoconf.h>
#include "test.h"

#ifdef CONFIG_REFOS_RUN_TESTS

int test_pd(void);

int test_vspace(int run);

int test_vspace_mapping(void);

#endif /* CONFIG_REFOS_RUN_TESTS */

#endif /* _REFOS_PROCESS_SERVER_TEST_ADDRSPACE_H_ */