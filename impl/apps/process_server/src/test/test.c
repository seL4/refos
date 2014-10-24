/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "test.h"
#include <stdlib.h>
#include <string.h>
#include <autoconf.h>
#include <data_struct/cvector.h>
#include <data_struct/cqueue.h>
#include <data_struct/chash.h>
#include <data_struct/cbpool.h>
#include <refos/test.h>
#include <refos-util/nameserv.h>
#include "test_addrspace.h"
#include "test_memserv.h"
#include "test_process.h"
#include "../state.h"

/*! @file
    @brief Process server root task unit test module. */

#ifdef CONFIG_REFOS_RUN_TESTS

#include <refos/test.h>

/* --------------------------------------- KAlloc test ------------------------------------------ */

static int
test_kalloc(void)
{
    test_start("kalloc");

    /* Test that malloc works and the process server can allocate from static heap properly. */
    for (int repeats = 0; repeats < 100; repeats++) {
        int *a = kmalloc(sizeof(int) * 10240);
        assert(a);
        for (int i = 0; i < 10240; i++) a[i] = i;
        for (int i = 0; i < 10240; i++) test_assert(a[i] == i);
        kfree(a);
    }

    /* Test that kernel obj allocation works and that the VKA allocator has been
       bootstrapped properly. */
    vka_object_t obj[100];
    int error = -1;
    for (int repeats = 0; repeats < 100; repeats++) {
        for (int i = 0; i < 100; i++) {
            error = vka_alloc_endpoint(&procServ.vka, &obj[i]);
            test_assert(!error);
            test_assert(obj[i].cptr != 0);
        }
        for (int i = 0; i < 100; i++) {
            vka_free_object(&procServ.vka, &obj[i]);
        }
        for (int i = 0; i < 100; i++) {
            error = vka_alloc_frame(&procServ.vka, seL4_PageBits, &obj[i]);
            test_assert(!error);
            test_assert(obj[i].cptr != 0);
        }
        for (int i = 0; i < 100; i++) {
            vka_free_object(&procServ.vka, &obj[i]);
        }
    }

    return test_success();
}

/* --------------------------------- Data structure lib tests ----------------------------------- */

static int
test_cvector(void)
{
    test_start("cvector");
    // Src: https://gist.github.com/EmilHernvall/953968
    cvector_t v;
    cvector_init(&v);
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
test_cqueue(void)
{
    test_start("cqueue");
    cqueue_t q;
    cqueue_init(&q, 62);
    for (int k = 0; k < 100; k++) {
        for (int i = 0; i < 60; i++) {
            cqueue_push(&q, (cqueue_item_t) i);
        }
        for (int i = 0; i < 60; i++) {
            int item = (int) cqueue_pop(&q);
            test_assert(item == i);
        }
    }
    cqueue_free(&q);
    return test_success();
}

static int
test_chash(void)
{
    test_start("chash");
    chash_t h;
    chash_init(&h, 12);
    for (int i = 0; i < 1024; i++) {
        chash_set(&h, i, (chash_item_t) 0x3F1);
    }
    for (int i = 0; i < 1024; i++) {
        int t = (int) chash_get(&h, i);
        test_assert(t == 0x3F1);
    }
    chash_remove(&h, 123);
    test_assert(chash_get(&h, 123) == NULL);
    int f = chash_find_free(&h, 100, 200);
    test_assert(f == 123);
    for (int i = 0; i < 1024; i++) {
        chash_remove(&h, i);
    }
    chash_release(&h);
    return test_success();
}

static int
test_cpool(void)
{
    test_start("cpool");
    cpool_t p;
    cpool_init(&p, 1, 10240);
    for (int i = 1; i < 10240; i+=2) {
        int v = cpool_alloc(&p);
        test_assert(v >= 1 && v <= 10240);
        test_assert(cpool_check(&p, v) == false);
    }
    cpool_free(&p, 1);
    int v = cpool_alloc(&p);
    test_assert(v == 1);
    cpool_release(&p);
    return test_success();
}

static int
test_cbpool(void)
{
    test_start("cbpool");
    cbpool_t p;
    cbpool_init(&p, 1024);
    int obj[16];
    for (int i = 0; i < 16; i++) {
        obj[i] = cbpool_alloc(&p, 32);
        test_assert(obj[i] >= 0 && obj[i] <= 1024);
        for (int j = 0; j < 32; j++) {
            test_assert(cbpool_check_single(&p, i  + j) == true);
        }
    }
    for (int i = 0; i < 16; i++) {
        cbpool_free(&p, obj[i], 32);
    }
    uint32_t v = cbpool_alloc(&p, 1024);
    test_assert(v == 0);
    for (int i = 0; i < 1024; i+=2) {
        test_assert(cbpool_check_single(&p, i) == true);
        cbpool_set_single(&p, i, false);
        test_assert(cbpool_check_single(&p, i) == false);
        cbpool_set_single(&p, i, true);
        test_assert(cbpool_check_single(&p, i) == true);
    }
    cbpool_release(&p);
    return test_success();
}

/* ----------------------------------- NameServ Library test ------------------------------------ */

static void
test_nameserv_callback_freecap(seL4_CPtr cap) {
   assert(cap == 0x12345);
}

int
test_nameserv_lib(void)
{
    test_start("nameserv lib");
    nameserv_state_t ns;
    nameserv_init(&ns, test_nameserv_callback_freecap);
    int error;

    error = nameserv_add(&ns, "hello_test", 0x12345);
    test_assert(error == ESUCCESS);
    error = nameserv_add(&ns, "kitty_test", 0x12345);
    test_assert(error == ESUCCESS);
    error = nameserv_add(&ns, "foo_test", 0x12345);
    test_assert(error == ESUCCESS);

    int n;
    seL4_CPtr anonCap = 0;

    /* Test path end case. */
    n = nameserv_resolve(&ns, "hello.txt", NULL);
    test_assert(n == REFOS_NAMESERV_RESOLVED);
    anonCap = 0;

    /* Test path resolve cases. */

    n = nameserv_resolve(&ns, "hello_test/hello.txt", &anonCap);
    tvprintf("nameserv_resolve n0 %d\n", n);
    test_assert(n == 10);
    test_assert(anonCap == 0x12345);

    anonCap = 0;
    n = nameserv_resolve(&ns, "hello_test/hi/bye/hello_foo.txt", &anonCap);
    tvprintf("nameserv_resolve n1 %d\n", n);
    test_assert(n == 10);
    test_assert(anonCap == 0x12345);

    anonCap = 0;
    n = nameserv_resolve(&ns, "/kitty_test/hi/bye/hello_foo.txt", &anonCap);
    tvprintf("nameserv_resolve n2 %d\n", n);
    test_assert(n == 11);
    test_assert(anonCap == 0x12345);

    /* Test no resolvation cases. */
    n = nameserv_resolve(&ns, "food_test/bye/foo.txt", NULL);
    test_assert(n == 0);
    n = nameserv_resolve(&ns, "", NULL);
    test_assert(n == 0);
    n = nameserv_resolve(&ns, NULL, NULL);
    test_assert(n == 0);

    /* Test deletion. */
    nameserv_delete(&ns, "asd_bogus");
    nameserv_delete(&ns, "foo_test");
    nameserv_delete(&ns, "hello_test");

    /* Test that we cannot find the deleted names any more. */
    n = nameserv_resolve(&ns, "hello_test/hello.txt", NULL);
    test_assert(n == 0);
    n = nameserv_resolve(&ns, "foo_test/", NULL);
    test_assert(n == 0);
    anonCap = 0;

    /* Test that we can still get the undeleted name. */
    n = nameserv_resolve(&ns, "kitty_test/hi/bye/hello_foo.txt", &anonCap);
    test_assert(n == 10);
    test_assert(anonCap == 0x12345);

    nameserv_release(&ns);
    return test_success();
}

/* ------------------------------------ ProcServ Unit tests ------------------------------------- */

void
test_run_all(void)
{
    tprintf("ROOT_TASK_TESTS | Running unit tests.\n");
    test_title = "ROOT_TASK_TESTS";

    test_kalloc();
    test_cvector();
    test_cqueue();
    test_chash();
    test_cpool();
    test_cbpool();
    test_pid();
    test_pd();
    test_vspace(0);
    test_vspace_mapping();
    test_vspace(1);
    test_thread();
    test_window_list();
    test_window_associations();
    test_ram_dspace_list();
    test_ram_dspace_read_write();
    test_proc_client_watch();
    test_ram_dspace_content_init();
    test_nameserv_lib();

    test_print_log();
}

#endif /* CONFIG_REFOS_RUN_TESTS */
