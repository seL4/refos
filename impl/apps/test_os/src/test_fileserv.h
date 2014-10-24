/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _OS_TESTS_FILE_SERVER_H_
#define _OS_TESTS_FILE_SERVER_H_

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

#include <refos-rpc/name_client.h>
#include <refos-rpc/name_client_helper.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>
#include <refos-rpc/serv_client.h>

void test_file_server(void);

#endif /* _OS_TESTS_FILE_SERVER_H_ */

#endif /* _FILESERV_TESTS_H_ */