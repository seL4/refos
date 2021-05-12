/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

ENTRY(_start)

SECTIONS
{
    PROVIDE (__executable_start = 0x8000);
    . = 0x8000;

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
