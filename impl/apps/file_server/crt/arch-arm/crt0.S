/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

.extern seL4_InitBootInfo
.extern exit

.text

/*
 * Image Entry point.
 */
.global _start
_start:
    /* Setup a stack for ourselves. */
    ldr     sp, =_stack_top

    /* Call main. */
    bl      main
    b       exit

/*
 * Symbols required for libgcc.
 */
.global raise
.global __aeabi_unwind_cpp_pr0
.global __aeabi_unwind_cpp_pr1
.global __aeabi_unwind_cpp_pr2
raise:
__aeabi_unwind_cpp_pr0:
__aeabi_unwind_cpp_pr1:
__aeabi_unwind_cpp_pr2:
    b       raise

/* .text Literal Pool */
.pool

/* Stack for the image. */
.bss
.align  4
_stack_bottom:
.space  0x4000
_stack_top:
