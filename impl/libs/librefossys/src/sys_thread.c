/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */

#include <stdarg.h>

/* Future Work 2:
   Find out why calls to assert_fail() call printf() and result
   in another call to assert_fail() on x86 architecture. assert_fail()
   correctly calls printf(), but this call to printf() erroneously
   calls assert_fail() which results in infinite recursion. Calls to
   printf() work fine in other parts of RefOS.
*/
long sys_set_thread_area(va_list ap) {
	return 0;
}
long sys_set_tid_address(va_list ap) {
	return 0;
}
