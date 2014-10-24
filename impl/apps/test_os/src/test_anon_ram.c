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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "test_anon_ram.h"


/* -------------------------------- RAM Dataspace tests -------------------------------------- */

static int
test_anon_dspace()
{
    test_start("anon dataspace");

    /* Open an anonymous dataspace and map it entirely in our VSpace. */
    data_mapping_t anon = data_open_map(REFOS_PROCSERV_EP, "anon", 0x0, 0, 0x2000, -1);
    test_assert(anon.err == ESUCCESS);
    test_assert(anon.size == 0x2000);
    test_assert(anon.sizeNPages == (0x2000 / REFOS_PAGE_SIZE));
    test_assert(anon.session == REFOS_PROCSERV_EP);
    test_assert(anon.dataspace != 0);
    test_assert(anon.window != 0);
    test_assert(anon.vaddr != NULL);

    /* Write to and read from anon dataspace. */
    strcpy(anon.vaddr, "hello world!");
    int cmp = strcmp(anon.vaddr, "hello world!");
    tvprintf("anon dataspace contents: [%s]\n", anon.vaddr);
    test_assert(cmp == 0);

    /* Test resize. */
    int error = data_expand(REFOS_PROCSERV_EP, anon.dataspace, 0x3000);
    test_assert(error == ESUCCESS);
    test_assert(data_get_size(REFOS_PROCSERV_EP, anon.dataspace) == 0x3000);

    /* Clean up. */
    error = data_mapping_release(anon);
    test_assert(error == ESUCCESS);
    return test_success();
}

void
test_anon_dataspace(void)
{
    test_anon_dspace();
}

#endif /* CONFIG_REFOS_RUN_TESTS */
