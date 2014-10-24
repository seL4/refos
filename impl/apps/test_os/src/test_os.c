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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <refos/refos.h>
#include <refos/test.h>
#include <refos/error.h>
#include <refos/share.h>
#include <refos/vmlayout.h>
#include <refos-io/morecore.h>
#include <refos-io/stdio.h>
#include <refos-rpc/name_client.h>
#include <refos-rpc/name_client_helper.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>
#include <refos-rpc/serv_client.h>
#include <refos-util/cspace.h>
#include <refos-util/init.h>
#include <refos-util/walloc.h>
#include <syscall_stubs_sel4.h>

#include "test_fileserv.h"
#include "test_anon_ram.h"

/* Debug printing. */
#include <refos-util/dprintf.h>

#define MMAP_SIZE 0x100000 // 16MB.
static char mmapRegion[MMAP_SIZE];

#define BSS_MAGIC 0xBA13DD37
#define BSS_ARRAY_SIZE 0x20000
#define TEST_KERNEL_VM_RESERVED_START 0xE0000000
#define TEST_USERLAND_TEST_APP "/fileserv/test_user"

char bssArray[BSS_ARRAY_SIZE];
int bssVar = BSS_MAGIC;
int bssVar2;

const char* dprintfServerName = "OS_TEST";
int dprintfServerColour = 37;

/* ---------------------------------- Memory tests ---------------------------------------- */

static int
test_bss(void)
{
    test_start("bss zero");
    test_assert(bssVar == BSS_MAGIC);
    test_assert(bssVar2 == 0);
    for (int i = 0; i < BSS_ARRAY_SIZE; i++) {
        test_assert(bssArray[i] == 0);
        bssArray[i] = (char)((i*7)%250);
    }
    test_assert(bssVar == BSS_MAGIC);
    test_assert(bssVar2 == 0);
    for (int i = 0; i < BSS_ARRAY_SIZE; i++) {
        test_assert(bssArray[i] == (char)((i*7)%250));
    }
    return test_success();
}

static int
test_stack(void)
{
    test_start("stack");
    const size_t stackAllocSize = 0x2000;
    char stackArray[stackAllocSize];
    for (int i = 0; i < stackAllocSize; i++) {
        stackArray[i] = (char)(i * 1234);
    }
    for (int i = 0; i < stackAllocSize; i++) {
        test_assert( stackArray[i] == (char)(i * 1234));
    }
    return test_success();
}

static int
test_heap(void)
{
    test_start("heap");
    /* Test heap malloc actually works. */
    const size_t heapAllocSize = 0x16000;
    char *heapArray = malloc(heapAllocSize);
    test_assert(heapArray);
    for (int i = 0; i < heapAllocSize; i++) {
        heapArray[i] = (i%2)?'z':'a';
    }
    for (int i = 0; i < heapAllocSize; i++) {
        test_assert(heapArray[i] == (i%2)?'z':'a');
    }
    free(heapArray);
    /* Test that free works by continually allocating and free-ing a large block. */
    for (int i = 0; i < 10000; i++) {
        char *heapArray2 = malloc(heapAllocSize);
        test_assert(heapArray2);
        heapArray2[4] = 123;
        test_assert(heapArray2[4] == 123);
        free(heapArray);
    }
    return test_success();
}

/* -------------------------------- Process server tests -------------------------------------- */

static int
test_process_server_ping(void)
{
    test_start("process server ping");
    for (int i = 0; i < 8; i++) {
        int error = proc_ping();
        test_assert(error == ESUCCESS);
    }
    return test_success();
}

static int
test_process_server_endpoints(void)
{
    test_start("process server endpoints");
    
    seL4_CPtr ep = proc_new_endpoint();
    test_assert(ep && ROS_ERRNO() == ESUCCESS);
    
    seL4_CPtr aep = proc_new_async_endpoint();
    test_assert(aep && ROS_ERRNO() == ESUCCESS);
    
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, 0x31336);
    seL4_NBSend(aep, tag);

    seL4_Word badge;
    seL4_Wait(aep, &badge);

    test_assert(badge == 0);
    test_assert(seL4_GetMR(0) == 0x31336);

    proc_del_endpoint(ep);
    proc_del_async_endpoint(aep);

    return test_success();
}

static int
test_process_server_window(void)
{
    test_start("process server memwindows");
    seL4_Word testBase = 0x20000000;
    
    /* Create invalid window in kernel memory. */
    seL4_CPtr invalidKernelMemWindow = proc_create_mem_window(
            TEST_KERNEL_VM_RESERVED_START + 100, 0x1000);
    test_assert(invalidKernelMemWindow == 0 && ROS_ERRNO() == EINVALIDPARAM);

    /* Create valid test windows. */
    int windowBases[] = {0x2000, 0x4000, 0x6000, 0x8000, 0x10000};
    seL4_CPtr validWindows[5];
    int nWindows = 5;
    for (int i = 0; i < nWindows; i++) {
        validWindows[i] = proc_create_mem_window(testBase + windowBases[i], 10);
        test_assert(validWindows[i] != 0 && ROS_ERRNO() == ESUCCESS);
    }

    int testWin[] = {0x2000, 1, 0x4001, 20, 0x7000, 10, 0x6002, 50, 0, 0x9FFF};
    int expectedResult[] = {EINVALIDWINDOW, EINVALIDWINDOW, ESUCCESS, EINVALIDWINDOW, EINVALIDWINDOW};
    int numTestWin = 5;
    for (int i = 0; i < numTestWin; i++) {
        seL4_CPtr testWindow = proc_create_mem_window(testBase + testWin[i*2], testWin[i*2+1]);
        test_assert(ROS_ERRNO() == expectedResult[i]);
        if (expectedResult[i] == ESUCCESS) {
            test_assert(testWindow);
        }
        if (testWindow) {
            int error = proc_delete_mem_window(testWindow);
            test_assert(error == ESUCCESS);
        }
    }

    for (int i = 0; i < nWindows; i++) {
        if (validWindows[i]) {
            int error = proc_delete_mem_window(validWindows[i]);
            test_assert(error == ESUCCESS);
        }
    }

    return test_success();
}

static int
test_process_server_window_resize(void)
{
    test_start("process server memwindow resize");
    seL4_Word testBase = 0x20000000;

    /* Test window resize. */
    seL4_CPtr testRWindow = proc_create_mem_window(testBase + 0x1000, 0x2000);
    test_assert(testRWindow && ROS_ERRNO() == ESUCCESS);

    seL4_CPtr testCheckWindow = proc_create_mem_window(testBase + 0x3000, 0x1000);
    test_assert(testCheckWindow && ROS_ERRNO() == ESUCCESS);
 
    /* Resizing the latter window should succeed. */
    int error = proc_resize_mem_window(testCheckWindow, 0x2000);
    test_assert(error == ESUCCESS);
    /* Increasing the previous window should cause overlap error. */
    error = proc_resize_mem_window(testRWindow, 0x2100);
    test_assert(error == EINVALIDPARAM);
    /* Zero size not allowed. */
    error = proc_resize_mem_window(testRWindow, 0);
    test_assert(error == EINVALIDPARAM);
    /* Shrinking window is OK. */
    error = proc_resize_mem_window(testRWindow, 0x1900);
    test_assert(error == ESUCCESS);
    /* Invalid window. */
    error = proc_resize_mem_window(0x0, 0x2100);
    test_assert(error == EINVALIDWINDOW);


    /* Delete the latter window. */
    error = proc_delete_mem_window(testCheckWindow);
    test_assert(error == ESUCCESS);

    /* Increasing the previous window should now succeed, as the latter window causing the overlap
       is now gone. */
    error = proc_resize_mem_window(testRWindow, 0x2100);
    test_assert(error == ESUCCESS);
    error = proc_delete_mem_window(testRWindow);
    test_assert(error == ESUCCESS);

    return test_success();
}

static int
test_process_server_param_buffer(void)
{
    test_start("process server param sharing");
    int error;
    seL4_CPtr ds = data_open(REFOS_PROCSERV_EP, "anon",
            0x0, 0x0, PROCESS_PARAM_DEFAULTSIZE, &error);
    test_assert(error == ESUCCESS);
    test_assert(ds);
    error = proc_set_parambuffer(ds, PROCESS_PARAM_DEFAULTSIZE);
    test_assert(error == ESUCCESS);
    return test_success();
}

static int
test_process_server_nameserv(void)
{
    test_start("process server registration");
    int error;
    nsv_mountpoint_t mp;
    char *testServerName = "os_test_dummy_server";
    char *testServerPath = "/os_test_dummy_server/foo.txt";
    
    /* Should not find this server. */
    mp = nsv_resolve(testServerPath);
    test_assert(ROS_ERRNO() == ESERVERNOTFOUND);
    test_assert(mp.success == false);
    test_assert(mp.serverAnon == 0);
    
    /* Make a quick anon cap. */
    seL4_CPtr aep = proc_new_async_endpoint();
    test_assert(aep && ROS_ERRNO() == ESUCCESS);
    
    /* We register ourselves now under this server name. */
    error = nsv_register(REFOS_NAMESERV_EP, testServerName, aep);
    test_assert(error == ESUCCESS);
    
    /* We should find ourself now. */
    mp = nsv_resolve(testServerPath);
    test_assert(ROS_ERRNO() == ESUCCESS);
    test_assert(mp.success == true);
    test_assert(mp.serverAnon != 0);
    test_assert(strcmp(mp.dspaceName, "foo.txt") == 0);
    test_assert(strcmp(mp.nameservPathPrefix, "/os_test_dummy_server/") == 0);

    /* Test this anon cap we recieved and make sure it is actually the one we passed in. */
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, 0xabbadadd);
    seL4_NBSend(mp.serverAnon, tag);

    seL4_Word badge;
    seL4_Wait(aep, &badge);
    test_assert(badge == 0);
    test_assert(seL4_GetMR(0) == 0xabbadadd);

    /* Release the resources stored in the valid mountpoint. */
    nsv_mountpoint_release(&mp);
    test_assert(mp.success == false);
    test_assert(mp.serverAnon == 0);

    /* We now unregister ourselves. */
    error = nsv_unregister(REFOS_NAMESERV_EP, testServerName);
    test_assert(error == ESUCCESS);
    
    /* We should not be able to find this server again. */
    mp = nsv_resolve(testServerPath);
    test_assert(ROS_ERRNO() == ESERVERNOTFOUND);
    test_assert(mp.success == false);
    test_assert(mp.serverAnon == 0);

    proc_del_async_endpoint(aep);
    return test_success();
}

static int
test_start_userland_test(void)
{
    tprintf("TEST_OS | Starting RefOS userland environment unit tests...\n");
    int status = EINVALID;
    int error;
    (void) error;

    /* Start the application */
    error = proc_new_proc(TEST_USERLAND_TEST_APP, "", true, 70, &status);
    assert(error == ESUCCESS);
    assert(status == 0x1234);
    return 0;
}


static void
test_process_server(void)
{
    test_process_server_ping();
    test_process_server_endpoints();
    test_process_server_window();
    test_process_server_window_resize();
    test_process_server_param_buffer();
    test_process_server_nameserv();
}

/* -------------------------------- RefOSUtil tests -------------------------------------- */

static int
test_rosutil_calloc(void)
{
    test_start("rosutil calloc");
    for (int i = 0; i <= 64; i++) {
        seL4_CPtr c = csalloc();
        test_assert(c);
        csfree(c);
    }
    return test_success();
}

static int
test_rosutil_walloc(void)
{
    test_start("rosutil walloc");
    for (int i = 0; i < 8; i++) {
        int npages = i % 2 ? 2 : 3;
        seL4_CPtr windowCap;
        seL4_Word vaddr = walloc(npages, &windowCap);
        test_assert(vaddr && windowCap);
        walloc_free(vaddr, npages);
    }
    return test_success();
}

static void
test_rosutil(void)
{
    test_rosutil_calloc();
    test_rosutil_walloc();
}

static int
test_share(void)
{
    test_start("share");

    char *testBuf = malloc(4096 * 3);
    char *srcBuf = malloc(4096 * 3);
    char *sharedBuf = malloc(4096 * 3 + 9);
    int i;
    unsigned int bytesRead = 0;
    unsigned int start = 0;
    unsigned int end = 0;

    for (i = 0; i < 4096 * 3; i++) {
        srcBuf[i] = 'a' + (i / 4096);
        testBuf[i] = 0;
    }

    for (i = 0; i < 4096 * 3 + 9; i++) {
        sharedBuf[i] = 0;
    }

    int ret = refos_share_write(srcBuf, 4096 * 3, sharedBuf, 4096 * 3 + 9, &end);
    tvprintf("return is %d\n", ret);
    tvprintf("end is %d\n", end);
    test_assert(ret == 0 && end == 4096 * 3);

    ret = refos_share_read(testBuf, 4096 * 3, sharedBuf, 4096 * 3 + 9, &start, &bytesRead);
    tvprintf("return is %d\n", ret);
    tvprintf("bytesRead is %d\n", bytesRead);
    test_assert(ret == 0 && bytesRead == 4096 * 3);

    for (i = 0; i < 4096 * 3; i++) {
        test_assert(testBuf[i] == srcBuf[i]);
    }

    for (i = 0; i < 4096 * 3; i++) {
        srcBuf[i] = 0;
        testBuf[i] = 0;
    }

    for (i = 0; i < 4096 * 3 + 9; i++) {
        sharedBuf[i] = 0;
    }

    for (i = 0; i < 4096; i++) {
        srcBuf[i] = 'a';
    }
    for (i = 8192; i < 4096 * 3; i++) {
        srcBuf[i] = 'a';
    }

    end = 8192;
    start = 8192;
    bytesRead = 0;
    unsigned int *ptr = (unsigned int*)sharedBuf;
    ptr[0] = start;
    ptr[1] = end;

    char *srcBuf2 = malloc(4096 * 2);
    for (i = 0; i < 4096 * 2; i++) {
        srcBuf2[i] = 'a';
    }

    ret = refos_share_write(srcBuf2, 4096 * 2, sharedBuf, 4096 * 3 + 9, &end);
    ret = refos_share_read(testBuf, 4096 * 2, sharedBuf, 4096 * 3 + 9, &start, &bytesRead);
    for (i = 0; i < 4096 * 2; i++) {
        test_assert(testBuf[i] == srcBuf2[i]);
    }

    return test_success();

}

/* ---------------------------------- OS Level tests ---------------------------------------- */

static void
test_OS_level(void)
{
    test_bss();
    test_stack();
    test_heap();
    test_process_server();
    test_anon_dataspace();
    test_file_server();
    test_rosutil();
    test_share();
}

#endif /* CONFIG_REFOS_RUN_TESTS */

int
main()
{
#ifdef CONFIG_REFOS_RUN_TESTS
    SET_MUSLC_SYSCALL_TABLE;
    refosio_setup_morecore_override(mmapRegion, MMAP_SIZE);
    refos_initialise_os_minimal();
    refos_setup_dataspace_stdio(REFOS_DEFAULT_STDIO_DSPACE);

    tprintf("OS_TESTS | Running RefOS OS-level tests.\n");
    test_title = "OS_TESTS";
    test_OS_level();
    test_print_log();
    
    test_start_userland_test();
    tprintf("OS_TESTS | Back to Refos OS-level. Running userland second time.\n");
    test_start_userland_test();
    tprintf("OS_TESTS | Back to Refos OS-level. Quitting.\n");
#endif /* CONFIG_REFOS_RUN_TESTS */
    
    return 0;
}



