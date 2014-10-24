/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_PROCESS_SERVER_TEST_PROCESS_MODULE_H_
#define _REFOS_PROCESS_SERVER_TEST_PROCESS_MODULE_H_

#include <autoconf.h>
#include "test.h"

#ifdef CONFIG_REFOS_RUN_TESTS

int test_pid(void);

int test_thread(void);

int test_proc_client_watch(void);

#endif /* CONFIG_REFOS_RUN_TESTS */

#endif /* _REFOS_PROCESS_SERVER_TEST_PROCESS_MODULE_H_ */