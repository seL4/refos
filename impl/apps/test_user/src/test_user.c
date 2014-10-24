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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include <refos/test.h>
#include <refos-io/stdio.h>
#include <refos-util/init.h>
#include <refos/sync.h>
#include <syscall_stubs_sel4.h>

/* Debug printing. */
#include <refos-util/dprintf.h>

#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <refos/vmlayout.h>
#include <data_struct/cvector.h>

#define BSS_MAGIC 0xBA13DD37
#define BSS_ARRAY_SIZE 0x20000
#define TEST_USER_TEST_APPNAME "/fileserv/test_user"
#define TEST_NUMTHREADS 8

char bssArray[BSS_ARRAY_SIZE];
int bssVar = BSS_MAGIC;
int bssVar2;

const char* dprintfServerName = "USER_TEST";
int dprintfServerColour = 38;

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

static int
test_malloc_huge(void)
{
    test_start("huge malloc");
    const size_t heapAllocSize = 0x4000000; // 64MB allocation.
    char *heapArray = malloc(heapAllocSize);
    test_assert(heapArray);
    free(heapArray);
    return test_success();
}

static void
test_memory(void)
{
    test_bss();
    test_stack();
    test_heap();
    test_malloc_huge();
}

static int
test_param(void)
{
    test_start("param");
    int paramStrMatch = !strcmp(TEST_USER_TEST_APPNAME, (char*) PROCESS_STATICPARAM_ADDR);
    test_assert(paramStrMatch);
    return test_success();
}

static int
test_libc_maths(void)
{
    test_start("libc maths");
    static double eps = 0.000001f;
    test_assert(abs(sin(3.4) - 0.25554110202) < eps);
    test_assert(abs(cosh(0.4) - 1.08107237184) < eps);
    test_assert(abs(tan(0.75) - 0.93159645994) < eps);
    test_assert(abs(floor(313.421326) - 313.0) < eps);
    return test_success();
}

static int
test_libc_string(void)
{
    test_start("libc string");
    char g[] = "the quick brown fox jumped over the lazy dog.";
    memset(g, '-', 3);
    strcat(g, " 12345.");
    size_t sz = strlen(g);
    test_assert(sz == 52);
    int c = strcmp(g,"blob.");
    test_assert(c != 0);
    c = strcmp(g, "--- quick brown fox jumped over the lazy dog. 12345.");
    test_assert(c == 0);
    return test_success();
}

static void
test_libc(void)
{
    test_libc_maths();
    test_libc_string();
}

static seL4_CPtr testThreadEP;
static sync_mutex_t testThreadMutex;
static uint32_t testThreadCount = 0;

static int
test_threads_func(void *arg)
{
    /* Thread entry point which loops and then signals parent and hangs. */
    for (int i = 0; i < 10000; i++) {
        sync_acquire(testThreadMutex);
        testThreadCount++;
        sync_release(testThreadMutex);
    }
    for(int i = 0; i < 20; i++) {
        for(int delay = 0; delay < 1000000; delay++);
    }
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    seL4_Call(testThreadEP, tag);
    while(1);
    return 0;
}

static int
test_threads(void)
{
    test_start("threads");
    int threadID;
    testThreadEP = 0;
    testThreadCount = 0;

    static char test_clone_stack[TEST_NUMTHREADS][4096];

    /* Create the endpoint on which threads will signal. */
    tvprintf("test_threads creating new ep...\n");
    testThreadEP = proc_new_endpoint();
    test_assert(testThreadEP != 0);
    test_assert(REFOS_GET_ERRNO() == ESUCCESS);

    tvprintf("test_threads creating new mutex...\n");
    testThreadMutex = sync_create_mutex();
    test_assert(testThreadMutex);

    /* Start the threads. */
    for(int i = 0; i < TEST_NUMTHREADS; i++) {
        tvprintf("test_threads cloning thread child %d...\n", i);
        threadID = proc_clone(test_threads_func, &test_clone_stack[i][4096], 0, 0);
        test_assert(REFOS_GET_ERRNO() == ESUCCESS);
        test_assert(threadID == i + 1);
    }

    /* Increment shared vraiable. */
    for (int i = 0; i < 10000; i++) {
        sync_acquire(testThreadMutex);
        testThreadCount++;
        sync_release(testThreadMutex);
    }
    
    /* Block and wait for thread exit signal. */
    for(int i = 0; i < TEST_NUMTHREADS; i++) {
        tvprintf("test_threads waiting thread child %d...\n", i);
        seL4_Word badge;
        seL4_Wait(testThreadEP, &badge);
        test_assert(badge == 0);
    }

    /* Test that were no race conditions on mutexed variable. */
    test_assert(testThreadCount == (10000 * TEST_NUMTHREADS) + 10000);

    tvprintf("test_threads deleting ep...\n");
    sync_destroy_mutex(testThreadMutex);
    proc_del_endpoint(testThreadEP);
    return test_success();
}

static int
test_cvector(void)
{
    test_start("cvector");
    // Src: https://gist.github.com/EmilHernvall/953968
    cvector_t v; cvector_init(&v);
    cvector_add(&v, (cvector_item_t)1); cvector_add(&v, (cvector_item_t)2);
    cvector_add(&v, (cvector_item_t)3); cvector_add(&v, (cvector_item_t)4);
    cvector_add(&v, (cvector_item_t)5);
    test_assert(cvector_count(&v) == (int)5);
    test_assert(cvector_get(&v, 0) == (cvector_item_t)1);
    test_assert(cvector_get(&v, 1) == (cvector_item_t)2);
    test_assert(cvector_get(&v, 2) == (cvector_item_t)3);
    test_assert(cvector_get(&v, 3) == (cvector_item_t)4);
    test_assert(cvector_get(&v, 4) == (cvector_item_t)5);
    cvector_delete(&v, 1);
    cvector_delete(&v, 3);
    test_assert(cvector_count(&v) == (int)3);
    test_assert(cvector_get(&v, 0) == (cvector_item_t)1);
    test_assert(cvector_get(&v, 1) == (cvector_item_t)3);
    test_assert(cvector_get(&v, 2) == (cvector_item_t)4);
    cvector_free(&v);
    int vcStress = 10000;
    for (int i = 0; i < vcStress; i++) {
        int data = ((i << 2) * 0xcafebabe) ^ 0xdeadbeef;
        cvector_add(&v, (cvector_item_t)data);
        test_assert(cvector_count(&v) == (int)(i + 1));
        test_assert(cvector_get(&v, i) == (cvector_item_t)data);
        data = (data << 7) ^ 0xbaabaabb;
        cvector_set(&v, i, (cvector_item_t)data);
        test_assert(cvector_count(&v) == (int)(i + 1));
        test_assert(cvector_get(&v, i) == (cvector_item_t)data);
    }
    cvector_free(&v);
    return test_success();
}

static int
test_filetable_read(void)
{
    test_start("filetable read");
    
    FILE * testFile = fopen("fileserv/hello.txt", "r");
    test_assert(testFile);

    char str[128], str2[128];
    fscanf(testFile, "%s %s", str, str2);
    tvprintf("str [%s] str2 [%s]\n", str, str2);
    test_assert(strcmp(str, "hello") == 0);
    test_assert(strcmp(str2, "world!") == 0);
    test_assert(feof(testFile));
    fclose(testFile);

    testFile = fopen("fileserv/hello_this_file_doesnt_exist.txt", "r");
    test_assert(!testFile);

    return test_success();
}

static int
test_filetable_write(void)
{
    test_start("filetable write");
    
    FILE * testFile = fopen("fileserv/test_file_abc", "w");
    test_assert(testFile);
    for (int i = 0; i < 32; i++) {
        fprintf(testFile, "hello!\n");
    }
    for (int i = 0; i < 12; i++) {
        int nw = fwrite("abc", 1, 3, testFile);
        test_assert(nw == 3);
    }

    fprintf(testFile, "hello!\n");

    /* Write a big block to file. */
    static char temp[20480];
    int nw = write(fileno(testFile), temp, 20480);
    test_assert(nw == 20480);
    fclose(testFile);

    testFile = fopen("fileserv/test_file_abc", "r");
    test_assert(testFile);
    char str[128];
    for (int i = 0; i < 32; i++) {
        fscanf(testFile, "%s\n", str);
        test_assert(!strcmp(str, "hello!"));
    }
    for (int i = 0; i < 12; i++) {
        int nr = fread(str, 1, 3, testFile);
        test_assert(nr == 3);
        test_assert(str[0] == 'a');
        test_assert(str[1] == 'b');
        test_assert(str[2] == 'c');
    }
    fclose(testFile);
    return test_success();
}

static int
test_gettime(void)
{
    test_start("gettime");
    time_t last = 0;
    for (int i = 0; i < 150; i++) {
        time_t t = time(NULL);
        tvprintf("%d (last %d).\n", t, last);
        test_assert(t >= last);
        last = t;
    }
    return test_success();
}

#endif /* CONFIG_REFOS_RUN_TESTS */

int
main()
{
#ifdef CONFIG_REFOS_RUN_TESTS
    refos_initialise();

    printf("USER_TEST | Hello world!\n");
    printf("USER_TEST | Running RefOS User-level tests.\n");
    test_title = "USER_TEST";

    test_memory();
    test_param();
    test_libc();
    test_threads();
    test_cvector();
    test_filetable_read();
    test_filetable_write();
    test_gettime();

    test_print_log();
#endif

    return 0x1234;
}