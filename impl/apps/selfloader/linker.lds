/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

ENTRY(_start)

SECTIONS
{
    PROVIDE (__executable_start = 0xDF000000);
    . = 0xDF000000;

    /* Code. */
    .text : ALIGN(4096) {
        _text = .;
        *(.text*)
    }

    /* Read Only Data. */
    .rodata : ALIGN(4096) {
        . = ALIGN(32);
        *(.rodata*)
    }

    /* Data / BSS */
    .data : ALIGN(4096) {
        *(.data)
    }
    .bss : ALIGN(4096) {
        *(.bss)
        *(COMMON)
    }
}
