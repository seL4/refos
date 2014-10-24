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
#include "test_addrspace.h"
#include "../state.h"
#include "../system/addrspace/vspace.h"
#include <refos/test.h>

/* ------------------------------- Page Dir module test ------------------------------- */

int
test_pd(void)
{
    test_start("page directory");
    seL4_CPtr p[PD_MAX];
    for (int i = 0; i < PD_MAX; i++) {
        p[i] = pd_assign(&procServ.PDList).kpdObject;
        test_assert(p[i] != 0);
    }
    for (int i = 0; i < PD_MAX; i++) {
        pd_free(&procServ.PDList, p[i]);
    }
    for (int i = 0; i < 2; i++) {
        p[i] = pd_assign(&procServ.PDList).kpdObject;
        test_assert(p[i] != 0);
    }
    for (int i = 0; i < 2; i++) {
        pd_free(&procServ.PDList, p[i]);
    }
    return test_success();
}

/* ------------------------------- VSpace module test ------------------------------- */

int
test_vspace(int run)
{
    test_start(run == 0 ? "vspace (run 1)" : "vspace (run 2)");
    const int numTestVS = MIN(8, MIN((PID_MAX - 1), PD_MAX));
    int error = -1;

    struct vs_vspace vs[numTestVS];

    /* Allocate ALL the vspaces. */
    for (int i = 0; i < numTestVS; i++) {
        uint32_t bogusPID = (i * 31337 % 1234);
        error = vs_initialise(&vs[i], bogusPID);

        test_assert(error == ESUCCESS);
        test_assert(vs[i].magic == REFOS_VSPACE_MAGIC);
        test_assert(vs[i].ref == 1);
        test_assert(vs[i].pid == bogusPID);
        test_assert(vs[i].kpd != 0);
        test_assert(vs[i].cspace.capPtr != 0);
        test_assert(vs[i].cspaceSize == REFOS_CSPACE_RADIX);
    }

    /* Ref every second one thrice. */
    for (int i = 0; i < numTestVS; i+=2) {
        vs_ref(&vs[i]);
        vs_ref(&vs[i]);
        vs_ref(&vs[i]);
    }

    /* Deref every VS. */
    for (int i = 0; i < numTestVS; i++) {
        vs_unref(&vs[i]);
    }

    /* Check that every second one is still alive. */
    for (int i = 0; i < numTestVS; i++) {
        if (i % 2 == 0) {
            test_assert(vs[i].ref == 3);
            test_assert(vs[i].magic == REFOS_VSPACE_MAGIC);
            vs_unref(&vs[i]);
            test_assert(vs[i].ref == 2);
            test_assert(vs[i].magic == REFOS_VSPACE_MAGIC);
            vs_unref(&vs[i]);
            test_assert(vs[i].ref == 1);
            test_assert(vs[i].magic == REFOS_VSPACE_MAGIC);
            vs_unref(&vs[i]);
            test_assert(vs[i].ref == 0);
            test_assert(vs[i].magic != REFOS_VSPACE_MAGIC);
        } else {
            test_assert(vs[i].ref == 0);
            test_assert(vs[i].magic != REFOS_VSPACE_MAGIC);
        }
    }

    return test_success();
}

int
test_vspace_mapping(void)
{
    test_start("vspace mapping");

    /* Create a vspace for testing mapping. */
    struct vs_vspace vs;
    int error = vs_initialise(&vs, 31337);
    test_assert(error == ESUCCESS);
    test_assert(vs.magic == REFOS_VSPACE_MAGIC);

    /* Create a memory segment window. */
    const vaddr_t window = 0x10000;
    const vaddr_t windowSize = 0x8000;
    int windowID;
    error = vs_create_window(&vs, window, windowSize, W_PERMISSION_WRITE | W_PERMISSION_READ,
            true, &windowID);
    test_assert(error == ESUCCESS);
    test_assert(windowID != W_INVALID_WINID);

    /* Allocate a frame to map. */
    vka_object_t frame;
    error = vka_alloc_frame(&procServ.vka, seL4_PageBits, &frame);
    test_assert(error == ESUCCESS);
    test_assert(frame.cptr != 0);

    /* Try to map in some invalid spots. */
    tvprintf("trying mapping into invalid spots...\n");
    error = vs_map(&vs, 0x9A0, &frame.cptr, 1);
    test_assert(error == EINVALIDWINDOW);
    error = vs_map(&vs, window - 0x9A0, &frame.cptr, 1);
    test_assert(error == EINVALIDWINDOW);
    error = vs_map(&vs, window + windowSize + 0x1, &frame.cptr, 1);
    test_assert(error == EINVALIDWINDOW);
    error = vs_map(&vs, window + windowSize + 0x5123, &frame.cptr, 1);
    test_assert(error == EINVALIDWINDOW);

    /* Try to unmap from some invalid spots. */
    tvprintf("trying unmapping from invalid spots...\n");
    error = vs_unmap(&vs, window - 0x9A0, 1);
    test_assert(error == EINVALIDWINDOW);
    error = vs_unmap(&vs, window + windowSize + 0x423, 5);
    test_assert(error == EINVALIDWINDOW);
    error = vs_unmap(&vs, window, windowSize + 1);
    test_assert(error == EINVALIDWINDOW);

    /* Map the frame many times in all the valid spots. */
    for (vaddr_t waddr = window; waddr < window + windowSize; waddr += (1 << seL4_PageBits)) {
        tvprintf("trying mapping into valid spot 0x%x...\n", (uint32_t) waddr);
        /* Map the frame. */
        error = vs_map(&vs, waddr, &frame.cptr, 1);
        test_assert(error == ESUCCESS);
        /* Try to map frame here again. Should complain. */
        error = vs_map(&vs, waddr, &frame.cptr, 1);
        test_assert(error == EUNMAPFIRST);
    }

    /* Unmap and remap the frame many times in all the valid spots. */
    for (vaddr_t waddr = window; waddr < window + windowSize; waddr += (1 << seL4_PageBits)) {
        tvprintf("trying remapping into valid spot 0x%x...\n", (uint32_t) waddr);
        /* Unmap the frame. */
        error = vs_unmap(&vs, waddr, 1);
        test_assert(error == ESUCCESS);
        /* Remap the frame. */
        error = vs_map(&vs, waddr, &frame.cptr, 1);
        test_assert(error == ESUCCESS);
    }

    /* Clean up. Note that deleting the vspace should delete the created window. */
    tvprintf("cleaning up everything in vspace...\n");
    vs_unref(&vs);
    test_assert(vs.magic != REFOS_VSPACE_MAGIC);
    vka_free_object(&procServ.vka, &frame);

    return test_success();
}

#endif /* CONFIG_REFOS_RUN_TESTS */