/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _OS_TESTS_ANON_RAM_DSPACE_H_
#define _OS_TESTS_ANON_RAM_DSPACE_H_

#include <autoconf.h>

#ifdef CONFIG_REFOS_RUN_TESTS

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <refos/refos.h>
#include <refos/test.h>
#include <refos/error.h>
#include <refos/vmlayout.h>

#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>

void test_anon_dataspace(void);

#endif /* CONFIG_REFOS_RUN_TESTS */

#endif /* _OS_TESTS_ANON_RAM_DSPACE_H_ */