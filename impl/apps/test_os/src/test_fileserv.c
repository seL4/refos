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
#ifdef CONFIG_REFOS_RUN_TESTS

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "test_fileserv.h"
#include <refos-util/walloc.h>
#include <refos-rpc/serv_client.h>
#include <refos-rpc/serv_client_helper.h>

/* -------------------------------- File server tests -------------------------------------- */

static int
test_file_server_connect()
{
    test_start("fs connection");

    /* Find the file server. */
    nsv_mountpoint_t mp = nsv_resolve("fileserv/*");
    test_assert(mp.success == true);
    test_assert(mp.serverAnon != 0);

    seL4_CPtr fileservAnon = mp.serverAnon;
    const int fs_test_repeat = 5;

    /* Attempt to ping the file server. */
    for (int i = 0; i < fs_test_repeat; i++) {
        int error = serv_ping(fileservAnon);
        test_assert(error == ESUCCESS);
    }

    /* Repeatedly connect to and disconnect from the file server. */
    for (int i = 0; i < fs_test_repeat; i++) {
        int error;
        seL4_CPtr fileservSession = serv_connect_direct(fileservAnon, REFOS_LIVENESS, &error);
        test_assert(fileservSession && error == ESUCCESS);

        if (fileservSession) {
            serv_disconnect_direct(fileservSession);
            seL4_CNode_Delete(REFOS_CSPACE, fileservSession, REFOS_CDEPTH);
            csfree(fileservSession);
        }
    }

    /* Release the resources stored in the valid mountpoint. */
    nsv_mountpoint_release(&mp);
    test_assert(mp.success == false);
    test_assert(mp.serverAnon == 0);

    return test_success();
}

static int
test_file_server_dataspace()
{
    /* ---------- datamap test ------------ */
    test_start("fs cpio dspace datamap");
    int error;

    /* Find the file server. */
    nsv_mountpoint_t mp = nsv_resolve("fileserv/*");
    test_assert(mp.success == true);
    test_assert(mp.serverAnon != 0);
    seL4_CPtr fileservAnon = mp.serverAnon;

    seL4_CPtr fileservSession = serv_connect_direct(fileservAnon, REFOS_LIVENESS, &error);
    test_assert(fileservSession && error == ESUCCESS);

    /* Allocate a temporary window. */
    seL4_CPtr tempWindow = 0;
    seL4_Word tempWindowVaddr = walloc(2, &tempWindow);
    test_assert(tempWindowVaddr && tempWindow);

    /* Open a new dataspace on the fileserver to test with. */
    seL4_CPtr dspace = data_open(fileservSession, "hello.txt", 0, O_RDWR, 0, &error);
    test_assert(dspace && error == ESUCCESS);

    /* Test datamap. */
    error = data_datamap(fileservSession, dspace, tempWindow, 3);
    test_assert(error == ESUCCESS);
    int scmp = strncmp((char*) tempWindowVaddr, "lo world!", 9);
    test_assert(scmp == 0);

    test_success();

    /* ---------- init_data test ------------ */
    test_start("fs cpio dspace init_data");

    /* Open a new anon dataspace to init data on */
    seL4_CPtr anonDS = data_open(REFOS_PROCSERV_EP, "anon", 0, O_RDWR, 0x1000, &error);
    test_assert(anonDS && error == ESUCCESS);

    /* Inititialise content of this anon dataspace with our fileserv CPIO dataspace. */
    error = data_init_data(fileservSession, anonDS, dspace , 3);
    test_assert(error == ESUCCESS);

    /* Allocate another temporary window. */
    seL4_CPtr tempWindowAnon = 0;
    seL4_Word tempWindowVaddrAnon = walloc(2, &tempWindowAnon);
    test_assert(tempWindowVaddrAnon && tempWindowAnon);

    /* Datamap initialised anon dataspace. */
    error = data_datamap(REFOS_PROCSERV_EP, anonDS, tempWindowAnon, 0);
    test_assert(error == ESUCCESS);
    scmp = strncmp((char*) tempWindowVaddrAnon, "lo world!", 9);
    test_assert(scmp == 0);

    /* Clean up. */
    data_dataunmap(fileservSession, tempWindow);
    data_dataunmap(REFOS_PROCSERV_EP, tempWindowAnon);
    if (tempWindow) {
        walloc_free(tempWindowVaddr, 2);
    }
    if (tempWindowAnon) {
        walloc_free(tempWindowVaddrAnon, 2);
    }
    if (fileservSession) {
        serv_disconnect_direct(fileservSession);
        seL4_CNode_Delete(REFOS_CSPACE, fileservSession, REFOS_CDEPTH);
        csfree(fileservSession);
    }

    /* Release the resources stored in the valid mountpoint. */
    nsv_mountpoint_release(&mp);
    test_assert(mp.success == false);
    test_assert(mp.serverAnon == 0);

    return test_success();
}

static int
test_file_server_serv_connect()
{
    test_start("fs serv_connect");
    for (int i = 0; i < 5; i++) {
        serv_connection_t c = serv_connect("/fileserv/*");
        test_assert(c.error == ESUCCESS);
        test_assert(c.paramBuffer.err == ESUCCESS);
        strcpy(c.paramBuffer.vaddr, "test");
        serv_disconnect(&c);
    }
    return test_success();
}

void
test_file_server(void)
{
    test_file_server_connect();
    test_file_server_dataspace();
    test_file_server_serv_connect();
}

#endif /* CONFIG_REFOS_RUN_TESTS */
