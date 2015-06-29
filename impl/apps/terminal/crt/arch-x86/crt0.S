/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#define __ASM__
#include <sel4/arch/constants.h>

    .global _start

    .text

#ifdef X86_64

.align 0x1000
_start:
    leaq    _stack_top, %rsp

    call    main
# the return value is in rax
    movq    %rax, %rdi
    call    exit

1:  jmp     1b

#else

_start:
    leal    _stack_top, %esp

    /* Setup the global "bootinfo" structure. */
    pushl   %ebx
# call    seL4_InitBootInfo
    addl    $4, %esp
    movw    $IPCBUF_GDT_SELECTOR, %ax
    movw    %ax, %gs

    /* Start main. */
    call    main

    /* Call exit with the return value from main. */
    pushl   %eax
    call    exit
1:  jmp     1b

#endif

    .bss
    .align  4

_stack_bottom:
    .space  16384
_stack_top:
