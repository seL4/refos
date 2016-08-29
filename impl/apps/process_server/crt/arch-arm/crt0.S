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

.text

/*
 * Image Entry point.
 */
.global _start
_start:
    /* Setup a stack for ourselves. */
    ldr sp, =_stack_top

    /* Setup bootinfo. The pointer to the bootinfo struct starts in 'r0'. */
    bl seL4_InitBootInfo

    /* Call main. */
    bl main
    b exit

/* .text Literal Pool */
.pool

/* Stack for the image. */
.bss
.align 4
_stack_bottom:
.space 65536
_stack_top:
