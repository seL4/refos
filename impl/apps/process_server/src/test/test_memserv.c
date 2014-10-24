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

#include <stdlib.h>
#include <string.h>
#include "test_memserv.h"
#include "../state.h"
#include "../system/memserv/window.h"
#include "../system/memserv/dataspace.h"
#include "../system/memserv/ringbuffer.h"
#include <refos/test.h>

/* -------------------------------------- Window module test ------------------------------------ */

int
test_window_list(void)
{
    test_start("windows");
    struct w_list wlist;
    w_init(&wlist);
    int nWindowTest = W_MAX_WINDOWS / 2;

    /* Spam allocate all the windows. */
    for (int i = 1; i < nWindowTest; i++) {
        reservation_t tempr;
        struct w_window* w = w_create_window(&wlist, 1024, -1, 0, NULL, tempr, true);
        test_assert(w);
        test_assert(w->wID == i);
        test_assert(w->magic == W_MAGIC);
        test_assert(w->size == 1024);
        test_assert(w->clientOwnerPID == -1);
        test_assert(w->mode == W_MODE_EMPTY);
        struct w_window* w_ = w_get_window(&wlist, w->wID);
        test_assert(w_ && w_->wID == i);
        test_assert(w_ == w);
    }

    /* Free every second window. */
    for (int i = 1; i < nWindowTest; i+=2) {
        int err = w_delete_window(&wlist, i);
        test_assert(err == ESUCCESS);
    }

    /* Make sure every second window is no longer get-able. */
    for (int i = 1; i < nWindowTest; i++) {
        struct w_window* w_ = w_get_window(&wlist, i);
        if (i % 2) {
            test_assert(w_ == NULL);
        } else {
            test_assert(w_ != NULL);
            test_assert(w_->magic == W_MAGIC);
            test_assert(w_->wID == i);
        }
    }

    w_deinit(&wlist);
    return test_success();
}


int
test_window_associations(void)
{
    test_start("window associations");

    struct w_associated_windowlist aw;
    w_associate_init(&aw);
    
    w_associate(&aw, 4, 400, 10);
    w_associate(&aw, 2, 200, 10);
    w_associate(&aw, 3, 300, 10);
    w_associate(&aw, 1, 100, 10);
    w_associate(&aw, 5, 500, 10);
    
#if REFOS_TEST_VERBOSE_PRINT
    tvprintf("------- Unsorted window list \n");
    w_associate_print(&aw);
    
    tvprintf("------- Sorted window list \n");
    w_associate_find(&aw, (vaddr_t) 0);
    w_associate_print(&aw);
#endif
    
    /* Associate some windows, and test window conflict checking with icky window
       boundary vaddr cases. */
    int testVaddr[] = {400, 401, 300, 301, 90, 110, 105, 500, 510, 505, 499, 200, 201};
    int expectedWinID[] = {4, 4, 3, 3, W_INVALID_WINID, W_INVALID_WINID, 1, 5, W_INVALID_WINID,
            5, W_INVALID_WINID, 2, 2};
    int numTestVaddr = 13;
    int foundWinID;
    struct w_associated_window *foundWin = NULL;
    for (int i = 0; i < numTestVaddr; i++) {
        foundWin = w_associate_find(&aw, testVaddr[i]);
        foundWinID = foundWin == NULL ? W_INVALID_WINID : foundWin->winID;
        tvprintf("------- winID for vaddr %d: %d\n", testVaddr[i], foundWinID);
        test_assert(foundWinID == expectedWinID[i]);
    }
    
    /* Test w_associate_check with icky window boundary vaddr cases. */
    int testWin[] = {100, 1, 201, 20, 210, 10, 490, 50, 0, 499};
    bool expectedCheckResult[] = {false, false, true, false, false};
    int numTestWin = 5;
    for (int i = 0; i < numTestWin; i++) {
        bool check = w_associate_check(&aw, testWin[i*2], testWin[i*2+1]);
        tvprintf("------- check for window [%d -> %d], %d\n", testWin[i*2],
                 testWin[i*2] + testWin[i*2+1], check);
        test_assert(check == expectedCheckResult[i]);
    }

    /* Test w_associate_find_range with icky window boundary cases. */
    int testRange[] = {100, 1, 201, 20, 210, 10, 490, 50, 0, 499, 500, 10, 305, 5};
    int expectedFindRangeResult[] = {1, W_INVALID_WINID, W_INVALID_WINID, W_INVALID_WINID, W_INVALID_WINID,
            5, 3};
    int numTestRange = 7;
    for (int i = 0; i < numTestRange; i++) {
        foundWin = w_associate_find_range(&aw, testRange[i*2], testRange[i*2+1]);
        foundWinID = foundWin == NULL ? W_INVALID_WINID : foundWin->winID;
        tvprintf("------- check range for window [%d -> %d], %d (expected to be %d)\n",
                testRange[i*2], testRange[i*2] + testRange[i*2+1],
                foundWinID, expectedFindRangeResult[i]);
        test_assert(foundWinID == expectedFindRangeResult[i]);
    }

    /* Test that clearing window association list actually clears. */
    w_associate_clear(&aw);
    for (int i = 0; i < numTestWin; i++) {
        bool check = w_associate_check(&aw, testWin[i*2], testWin[i*2+1]);
        test_assert(check == true);
    }

    return test_success();
}

/* --------------------------------- RAM dataspace module test ---------------------------------- */

int
test_ram_dspace_list(void)
{
    test_start("ram dataspace list");

    struct ram_dspace_list rlist;
    ram_dspace_init(&rlist);

    /* Create a bunch of RAM dataspaces to test with. */
    const int rsz[] = {3, 1, REFOS_PAGE_SIZE, REFOS_PAGE_SIZE + 1,
            REFOS_PAGE_SIZE + 2, REFOS_PAGE_SIZE * 12 + 1};
    const int nrsz = 6;
    struct ram_dspace *testDSpace[nrsz];

    tvprintf("Creating dataspaces...\n");
    for (int i = 0; i < nrsz; i++) {
        tvprintf("    Creating dataspace %d (size %d)...\n", i, rsz[i]);
        testDSpace[i] = ram_dspace_create(&rlist, rsz[i]);
        test_assert(testDSpace[i] != NULL);
        test_assert(testDSpace[i]->magic == RAM_DATASPACE_MAGIC);
        test_assert(testDSpace[i]->ID == 1 + i);
        test_assert(testDSpace[i]->ref == 1);
    }

    /* Test that getting ID works. */
    tvprintf("Testing dataspace get...\n");
    for (int i = 0; i < nrsz; i++) {
        struct ram_dspace * d = ram_dspace_get(&rlist, testDSpace[i]->ID);
        test_assert(d);
        test_assert(d->magic == RAM_DATASPACE_MAGIC);
        test_assert(d->ID == testDSpace[i]->ID);
    }

    /* Delete some dataspaces and make sure get doesn't work on them any more. */
    tvprintf("Deleting dataspaces...\n");
    ram_dspace_ref(&rlist, 6);
    test_assert(testDSpace[5]->ref == 2);
    ram_dspace_unref(&rlist, 6);
    test_assert(testDSpace[5]->ref == 1);
    ram_dspace_unref(&rlist, 6);
    ram_dspace_unref(&rlist, 5);
    for (int i = 0; i < 4; i++) {
        tvprintf("   Testing that dspace ID %d get still works...\n", testDSpace[i]->ID);
        struct ram_dspace * d = ram_dspace_get(&rlist, testDSpace[i]->ID);
        test_assert(d);
        test_assert(d->magic == RAM_DATASPACE_MAGIC);
        test_assert(d->ID == testDSpace[i]->ID);
        test_assert(d->ID == i + 1);
    }
    for (int i = 4; i < 6; i++) {
        tvprintf("   Testing that dspace ID %d get returns NULL...\n", i + 1);
        struct ram_dspace * d = ram_dspace_get(&rlist, i + 1);
        test_assert(!d);
    }

    /* Create new dataspaces again, which should reuse the just freed IDs. */
    tvprintf("Reallocating dataspaces...\n");
    for (int i = 4; i < 6; i++) {
        testDSpace[i] = ram_dspace_create(&rlist, rsz[i]);
        test_assert(testDSpace[i] != NULL);
        test_assert(testDSpace[i]->magic == RAM_DATASPACE_MAGIC);
        test_assert(testDSpace[i]->ID == 1 + i);
    }

    /* Create new dataspace should assign new ID. */
    tvprintf("Testing new dataspace creation to assign new ID...\n");
    struct ram_dspace *testDS_ = ram_dspace_create(&rlist, REFOS_PAGE_SIZE);
    test_assert(testDS_ && testDS_->magic == RAM_DATASPACE_MAGIC);
    test_assert(testDS_->ID == 7);

    /* Test get-size and resize. */
    test_assert(ram_dspace_get_size(testDS_) == REFOS_PAGE_SIZE);
    int error = ram_dspace_expand(testDS_, REFOS_PAGE_SIZE * 34);
    test_assert(error == ESUCCESS);
    test_assert(ram_dspace_get_size(testDS_) == REFOS_PAGE_SIZE * 34);

    ram_dspace_deinit(&rlist);
    return test_success();
}

/* Internal declarations. */
int ram_dspace_read_page(char *buf, size_t len, struct ram_dspace *dataspace, uint32_t offset);
int ram_dspace_write_page(char *buf, size_t len, struct ram_dspace *dataspace, uint32_t offset);

/*! \brief Runs a single instance of the test_ram_dspace_read_write test.
           Internal helper function.
*/
static int
test_ram_dspace_read_write_instance_helper(int testBufSize, int writeSize, int writeOffset,
        int readSize, int readOffset, char usePage, int srcOffset, int ansOffset, int ansSize)
{
    struct ram_dspace_list rlist;
    ram_dspace_init(&rlist);

    /* Function pointer prototypes for read/write function, so we can easily switch between using
       ram_dspace_write_page / ram_dspace_write and ram_dspace_read_page / ram_dspace_read tests.
    */
    int (*dsRead)(char *buf, size_t len, struct ram_dspace *dataspace, uint32_t offset) = NULL;
    int (*dsWrite)(char *buf, size_t len, struct ram_dspace *dataspace, uint32_t offset) = NULL;

    int i, error;

    /* Assign the function pointers based on the type of read/write call we want to test. */
    if (usePage) {
        dsRead = &ram_dspace_read_page;
        dsWrite = &ram_dspace_write_page;
    } else {
        dsRead = &ram_dspace_read;
        dsWrite = &ram_dspace_write;
    }

    /* Allocate some test buffers. */
    char *srcBuf = kmalloc(testBufSize);
    test_assert(srcBuf != NULL);

    char *testBufA = kmalloc(testBufSize);
    test_assert(testBufA != NULL);

    char *testBufB = kmalloc(testBufSize);
    test_assert(testBufB != NULL);

    /* Initialise test buffers. */
    for (i = 0; i < testBufSize; i++) {
        srcBuf[i] = (rand() & 0xFF);
        testBufA[i] = 0;
        testBufB[i] = 0;
    }

    /* Create a test RAM dataspace to play with. */
    struct ram_dspace *testDSpace = ram_dspace_create(&rlist, testBufSize);
    test_assert(testDSpace != NULL);
    test_assert(testDSpace->magic == RAM_DATASPACE_MAGIC);

    error = dsWrite(srcBuf + srcOffset, writeSize, testDSpace, writeOffset);
    test_assert(!error);
    error = dsRead(testBufB, readSize, testDSpace, readOffset);
    test_assert(!error);

    memcpy(testBufA, srcBuf + ansOffset, ansSize);
    for (i = 0; i < ansSize; i++) {
        test_assert(testBufA[i] == testBufB[i]);
    }

    /* Clean up. */
    kfree(testBufA);
    kfree(testBufB);
    kfree(srcBuf);

    ram_dspace_deinit(&rlist);
    return ESUCCESS;
}

int
test_ram_dspace_read_write(void)
{
    test_start("ram dataspace read/write");
    int error = 0;

    /* Test read/write with aligned page. */
    error = test_ram_dspace_read_write_instance_helper (
            REFOS_PAGE_SIZE, REFOS_PAGE_SIZE, 0, REFOS_PAGE_SIZE, 0,
            true, 0, 0, REFOS_PAGE_SIZE
    );
    test_assert(error == ESUCCESS);

    error = test_ram_dspace_read_write_instance_helper (
            REFOS_PAGE_SIZE, REFOS_PAGE_SIZE, 0, REFOS_PAGE_SIZE, 0,
            false, 0, 0, REFOS_PAGE_SIZE
    );
    test_assert(error == ESUCCESS);

    /* Test read/write with some offsets. */
    error = test_ram_dspace_read_write_instance_helper (
            REFOS_PAGE_SIZE, REFOS_PAGE_SIZE / 2, REFOS_PAGE_SIZE / 4,
            REFOS_PAGE_SIZE / 8, REFOS_PAGE_SIZE / 2, false, 0, REFOS_PAGE_SIZE / 4,
            REFOS_PAGE_SIZE / 8
    );
    test_assert(error == ESUCCESS);

    error = test_ram_dspace_read_write_instance_helper (
            REFOS_PAGE_SIZE, REFOS_PAGE_SIZE / 2, REFOS_PAGE_SIZE / 4,
            REFOS_PAGE_SIZE / 8, REFOS_PAGE_SIZE / 2, true, 0, REFOS_PAGE_SIZE / 4,
            REFOS_PAGE_SIZE / 8
    );
    test_assert(error == ESUCCESS);

    /* Test read/write with offsets on big dataspace. */
    error = test_ram_dspace_read_write_instance_helper (
            REFOS_PAGE_SIZE * 3, REFOS_PAGE_SIZE * 2, REFOS_PAGE_SIZE / 2, 578,
            REFOS_PAGE_SIZE + 200, false, 0, (REFOS_PAGE_SIZE / 2) + 200, 578
    );
    test_assert(error == ESUCCESS);

    error = test_ram_dspace_read_write_instance_helper (
            REFOS_PAGE_SIZE * 3, REFOS_PAGE_SIZE * 2, REFOS_PAGE_SIZE / 2,
            REFOS_PAGE_SIZE, REFOS_PAGE_SIZE * 2, false, 0, REFOS_PAGE_SIZE + (REFOS_PAGE_SIZE / 2),
            REFOS_PAGE_SIZE / 2
    );
    test_assert(error == ESUCCESS);

    return test_success();
}

int
test_ram_dspace_content_init(void)
{
    test_start("ram dataspace content init");
    struct ram_dspace_list rlist;
    ram_dspace_init(&rlist);

    /* Create a dummy EP. */
    const int npages = 9;
    cspacepath_t dummyEP = procserv_mint_badge(20500);
    cspacepath_t dummyWaiter = procserv_mint_badge(20501);
    test_assert(dummyEP.capPtr);
    test_assert(dummyWaiter.capPtr);

    /* Create a test RAM dataspace to play with. */
    struct ram_dspace *dspace = ram_dspace_create(&rlist, npages * REFOS_PAGE_SIZE);
    test_assert(dspace != NULL);
    test_assert(dspace->magic == RAM_DATASPACE_MAGIC);

    int error = ram_dspace_need_content_init(dspace, 0x1000);
    test_assert(error == -EINVALID);

    /* Enable content init mode, with dummy EP. */
    error = ram_dspace_content_init(dspace, dummyEP, 0x54);
    test_assert(error == ESUCCESS);

    /* Test content-init bit. */
    error = ram_dspace_need_content_init(dspace, npages * REFOS_PAGE_SIZE + 0x35);
    test_assert(error == -EINVALIDPARAM);
    for (int i = 0; i < npages; i++) {
        int val = ram_dspace_need_content_init(dspace, i * REFOS_PAGE_SIZE);
        test_assert(val == true);
        ram_dspace_set_content_init_provided(dspace,i * REFOS_PAGE_SIZE);
        val = ram_dspace_need_content_init(dspace, i * REFOS_PAGE_SIZE);
        test_assert(val == false);
    }

    /* Test waiter. */
    error = ram_dspace_add_content_init_waiter(dspace, 0x2000, dummyWaiter);
    test_assert(error == ESUCCESS);

    ram_dspace_unref(&rlist, dspace->ID);
    ram_dspace_deinit(&rlist);
    return test_success();
}


/* ------------------------------- Ring buffer module test ------------------------------- */

int
test_ringbuffer(void)
{
    test_start("ring buffer");
    
    struct ram_dspace_list rlist;
    ram_dspace_init(&rlist);

    /* Create a RAM dataspace to attach to a ringbuffer. */
    struct ram_dspace *ds = ram_dspace_create(&rlist,
        (REFOS_PAGE_SIZE * 3) + RINGBUFFER_METADATA_SIZE + 1);
    char *buf = kmalloc(REFOS_PAGE_SIZE * 3);
    char *outbuf = kmalloc(REFOS_PAGE_SIZE * 3);
    test_assert(ds && buf && outbuf);
    test_assert(ds->magic == RAM_DATASPACE_MAGIC);

    /* Initialise ring buffer content. */
    for (int i = 0; i < REFOS_PAGE_SIZE * 3; i++) {
        outbuf[i] = 0;
    }
    for (int i = 0; i < REFOS_PAGE_SIZE * 3; i++) {
        buf[i] = 'a' + (i / REFOS_PAGE_SIZE);
    }

    /* Create a write ringbuffer and make sure it has reffed the dataspace. */
    struct rb_buffer *rb =  rb_create(ds, RB_WRITEONLY);
    test_assert(rb && rb->magic == RINGBUFFER_MAGIC);
    test_assert(rb->dataspace->ID = ds->ID);
    test_assert(rb->dataspace->ref == 2);

    /* Test rb_write icky wrapping case works. */
    rb->localEnd = 4096;
    rb->localStart = 8193;
    ram_dspace_write(((char*) &rb->localEnd), sizeof(rb->localEnd), rb->dataspace,
            sizeof(uint32_t));
    ram_dspace_write(((char*) &rb->localStart), sizeof(rb->localStart), rb->dataspace, 0);
    rb_write(rb, buf + REFOS_PAGE_SIZE, REFOS_PAGE_SIZE);

    ram_dspace_read_page(outbuf, REFOS_PAGE_SIZE - (sizeof(seL4_Word) * 2),
            rb->dataspace, (sizeof(seL4_Word) * 2));
    ram_dspace_read_page(outbuf + REFOS_PAGE_SIZE - (sizeof(seL4_Word) * 2),
            REFOS_PAGE_SIZE, rb->dataspace, REFOS_PAGE_SIZE);
    ram_dspace_read_page(outbuf + (REFOS_PAGE_SIZE * 2) - (sizeof(seL4_Word) * 2),
            REFOS_PAGE_SIZE, rb->dataspace, REFOS_PAGE_SIZE * 2);
    ram_dspace_read_page(outbuf + (REFOS_PAGE_SIZE * 3) - (sizeof(seL4_Word) * 2),
            (sizeof(seL4_Word) * 2), rb->dataspace,
            REFOS_PAGE_SIZE * 3 - (sizeof(seL4_Word) * 2));

    for (int i = 0; i < REFOS_PAGE_SIZE; i++) {
        test_assert(buf[i] != outbuf[i]);
    }
    for (int i = REFOS_PAGE_SIZE; i < REFOS_PAGE_SIZE * 2; i++) {
        test_assert(buf[i] == outbuf[i]);
    }
    for (int i = REFOS_PAGE_SIZE * 2; i < REFOS_PAGE_SIZE * 3; i++) {
        test_assert(buf[i] != outbuf[i]);
    }

    /* Create another write ringbuffer and make sure it has reffed the dataspace. */
    struct rb_buffer *rbw =  rb_create(ds, RB_WRITEONLY);
    test_assert(rbw && rbw->magic == RINGBUFFER_MAGIC);
    test_assert(rbw->dataspace->ID = ds->ID);
    test_assert(rbw->dataspace->ref == 3);
    for (int i = 0; i < REFOS_PAGE_SIZE * 3; i++) {
        buf[i] = '0' + (i / REFOS_PAGE_SIZE);
        outbuf[i] = 133;
    }
    size_t bytesRead = 42;

    /* Create read ringbuffer and make sure it has reffed the dataspace. */
    struct rb_buffer *rbr =  rb_create(ds, RB_READONLY);
    test_assert(rbr && rbr->magic == RINGBUFFER_MAGIC);
    test_assert(rbr->dataspace->ID = ds->ID);
    test_assert(rbr->dataspace->ref == 4);

    /* Test rb_read icky wrapping case works, after rb_write. */
    rbw->localStart = rbw->localEnd = 1337;
    rb_write(rbw, buf, REFOS_PAGE_SIZE * 3);
    
    rbr->localStart = rbr->localEnd = 1337;
    rb_read(rbr, outbuf, REFOS_PAGE_SIZE * 3, &bytesRead);
    
    test_assert(bytesRead == REFOS_PAGE_SIZE * 3);
    
    for (int i = 0; i < REFOS_PAGE_SIZE * 3; i++) {
        test_assert(buf[i] == outbuf[i]);
    }
    
    /* Test ring buffer deletion. */
    rb_delete(rb);
    test_assert(ds->ref == 3);
    test_assert(ds->magic == RAM_DATASPACE_MAGIC);

    rb_delete(rbw);
    test_assert(ds->ref == 2);
    test_assert(ds->magic == RAM_DATASPACE_MAGIC);

    rb_delete(rbr);
    test_assert(ds->ref == 1);
    test_assert(ds->magic == RAM_DATASPACE_MAGIC);

    kfree(buf);
    kfree(outbuf);

    ram_dspace_deinit(&rlist);
    return test_success();
}


#endif /* CONFIG_REFOS_RUN_TESTS */
