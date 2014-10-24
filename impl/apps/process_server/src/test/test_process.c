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
#include <autoconf.h>
#include <refos/test.h>
#include "test.h"
#include "test_process.h"
#include "../system/process/pid.h"
#include "../system/process/thread.h"
#include "../system/process/proc_client_watch.h"
#include "../state.h"
#include "../common.h"

/* ------------------------------- PID / ASID module test ------------------------------- */

static void
test_pid_iteration_callback(struct proc_pcb *pcb, void *cookie)
{
    assert(cookie);
    int *result = (int*) cookie;
    (*result)++;
}

int
test_pid(void)
{
    test_start("PID");

    uint32_t testPID;
    uint32_t emptyIndex = 0;
    uint32_t pidTable[PID_MAX];
    struct proc_pcb *pcb;
    int count = 0, expectedCount = 0;

    /* Assign _ALL_ the ID. */
    for (int i = 0; i < (PID_MAX - 1); i++) {
        testPID = pid_alloc(&procServ.PIDList);
        test_assert(testPID != PID_NULL);

        /* Check no duplication. */
        for (int j = 0; j < i; j++) {
            test_assert(pidTable[j] != testPID);
        }

        pidTable[i] = testPID;
        expectedCount++;
    }

    /* Free all the even indexed ID. */
    for (int i = 0; i < (PID_MAX - 1); i += 2) {
        pid_free(&procServ.PIDList, pidTable[i]);
        pidTable[i] = PID_NULL;
        expectedCount--;
    }

    /* Test PID iteration. */
    pid_iterate(&procServ.PIDList, test_pid_iteration_callback, (void*)(&count));
    test_assert(count == expectedCount);

    /* Test find. */
    for (int i = 0; i < (PID_MAX - 1); i++) {
        pcb = pid_get_pcb(&procServ.PIDList, pidTable[i]);
        if (pidTable[i] == PID_NULL) {
            test_assert(pcb == NULL);
        } else {
            test_assert(pcb != NULL);
        }
    }

    /* Assign all the even ID again. */
    for (int i = 0; i < (PID_MAX - 1); i += 2) {
        testPID = pid_alloc(&procServ.PIDList);
        test_assert(testPID != PID_NULL);

        /* Check no duplication. */
        for (int j = 0; j < (PID_MAX - 1); j++) {
            if (pidTable[j] == PID_NULL) {
                emptyIndex = j;
            }
            test_assert(pidTable[j] != testPID);
        }

        pidTable[emptyIndex] = testPID;
    }

    /* Test find again. */
    for (int i = 0; i < (PID_MAX - 1); i++) {
        pcb = pid_get_pcb(&procServ.PIDList, pidTable[i]);
        test_assert(pcb != NULL);
    }

    /* Free all the IDs. */
    for (int i = 0; i < (PID_MAX - 1); i++) {
        pid_free(&procServ.PIDList, pidTable[i]);
    }

    return test_success();
}

/* ------------------------------------- Thread module test ------------------------------------- */

int
test_thread(void)
{
    test_start("thread");
    const int numTestThreads = 32;
    struct proc_tcb thr[numTestThreads];

    /* Create a vspace for threads to go into. */
    struct vs_vspace vs;
    int error = vs_initialise(&vs, 31337);
    test_assert(error == ESUCCESS);
    test_assert(vs.magic == REFOS_VSPACE_MAGIC);

    /* Create the threads. */
    for (int i = 0; i < numTestThreads; i++) {
        int error = thread_config(&thr[i], 12, 1337, &vs);
        test_assert(error == ESUCCESS);
        test_assert(thr[i].magic == REFOS_PROCESS_THREAD_MAGIC);
        test_assert(thr[i].priority == 12);
        test_assert(thr[i].vspaceRef == &vs);
        test_assert(thr[i].entryPoint == 1337);

        /* Test that the vspace has been referenced. */
        test_assert(vs.magic == REFOS_VSPACE_MAGIC);
        test_assert(vs.ref == 1 + 1 + i);
    }

    /* We can't really test starting them without causing a horrible mess of things. */

    /* Free all the threads. */
    for (int i = 0; i < numTestThreads; i++) {
        thread_release(&thr[i]);
        test_assert(thr[i].magic != REFOS_PROCESS_THREAD_MAGIC);
    }

    /* Free the vspace. */
    test_assert(vs.ref == 1);
    vs_unref(&vs);
    test_assert(vs.ref == 0);
    test_assert(vs.magic != REFOS_VSPACE_MAGIC);

    return test_success();
}

/* ------------------------------- Proc Watch Client module test -------------------------------- */

int
test_proc_client_watch(void)
{
    test_start("client watch");
    struct proc_watch_list wl;
    client_watch_init(&wl);

    const uint32_t dummyPIDs[] = {0x312, 0x32, 0xF2, 0x6F};
    vka_object_t dummyEP[4];
    cspacepath_t dummyEPMinted[4];

    /* Create dummy async EPs to test with. */
    for (int i = 0; i < 4; i++) {
        /* Create endpoint. */
        int error = vka_alloc_async_endpoint(&procServ.vka, &dummyEP[i]);
        test_assert(!error);
        test_assert(dummyEP[i].cptr != 0);

        /* Copy a copy of the endpoint. */
        cspacepath_t srcPath;
        vka_cspace_make_path(&procServ.vka, dummyEP[i].cptr, &srcPath);
        error = vka_cspace_alloc_path(&procServ.vka, &dummyEPMinted[i]);
        test_assert(!error);
        error = vka_cnode_mint(&dummyEPMinted[i], &srcPath, seL4_AllRights,
                               seL4_CapData_Badge_new(123));
        test_assert(!error);
    }

    /* Watch a bunch of test clients and dummy notify EPs. */
    for (int i = 0; i < 4; i++) {
        int error = client_watch(&wl, dummyPIDs[i], dummyEPMinted[i].capPtr);
        test_assert(error == ESUCCESS);
    }

    /* Test that getting an invalid PID results in a NULL CPtr. */
    seL4_CPtr cpInvalid = client_watch_get(&wl, 0x2FF);
    test_assert(cpInvalid == 0);

    /* Get our PIDs and see if we get out EPs back. */
    for (int i = 0; i < 4; i++) {
        seL4_CPtr cp = client_watch_get(&wl, dummyPIDs[i]);
        test_assert(cp == dummyEPMinted[i].capPtr);
        /* Unwatch the client and see if we still get it. */
        client_unwatch(&wl, dummyPIDs[i]);
        cp = client_watch_get(&wl, dummyPIDs[i]);
        test_assert(cp == 0);
    }

    /* Release all allocated endpoints. Note that the client_watch call takes ownership of the
       minted caps along with their cslots, so we do NOT need to free those here. */
    for (int i = 0; i < 4; i++) {
        cspacepath_t path;
        vka_cspace_make_path(&procServ.vka, dummyEP[i].cptr, &path);
        vka_cnode_revoke(&path);
        vka_cnode_delete(&path);
        vka_free_object(&procServ.vka, &dummyEP[i]);
    }

    client_watch_release(&wl);
    return test_success();
}

#endif /* CONFIG_REFOS_RUN_TESTS */