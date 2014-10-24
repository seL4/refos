/* A conio.h like implementation for VTANSI displays.
 *
 * Copyright (c) 2009 Joachim Nilsson <troglobit@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _TETRIS_IO_H_
#define _TETRIS_IO_H_

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <refos/refos.h>
#include <refos-io/stdio.h>
#include <refos-util/init.h>

/* Board dimensions. */
#define B_COLS 12
#define B_ROWS 23
#define B_SIZE (B_COLS * B_ROWS)

/* Attributes */
#define RESETATTR    0
#define BRIGHT       1
#define DIM          2
#define UNDERSCORE   4
#define BLINK        5           /* May not work on all displays. */
#define REVERSE      7
#define HIDDEN       8

/* Colours for text and background. */
#define A_FG_K       30
#define A_FG_R       31
#define A_FG_G       32
#define A_FG_Y       33
#define A_FG_B       34
#define A_FG_M       35
#define A_FG_C       36
#define A_FG_W       37
#define A_FG_RESET   39

#define A_BG_K       40
#define A_BG_R       41
#define A_BG_G       42
#define A_BG_Y       43
#define A_BG_B       44
#define A_BG_M       45
#define A_BG_C       46
#define A_BG_W       47
#define A_BG_RESET   49

/*! @brief Esc[2JEsc[1;1H - Clear screen and move cursor to 1,1 (upper left) pos. */
#define clrscr()              puts ("\e[2J\e[1;1H")
/*! @brief Esc[K - Erases from the current cursor position to the end of the current line. */
#define clreol()              puts ("\e[K")
/*! @brief Esc[2K - Erases the entire current line. */
#define delline()             puts ("\e[2K")
/*! @brief Esc[Line;ColumnH - Moves the cursor to the specified position (coordinates) */
#define gotoxy(x,y)           printf("\e[%d;%dH", y, x); fflush(stdout)
/*! @brief Esc[?25l (lower case L) - Hide Cursor */
#define hidecursor()          puts ("\e[?25l")
/*! @brief Esc[?25h (lower case H) - Show Cursor */
#define showcursor()          puts ("\e[?25h")
/*! @brief Print a solid block. */
#define printblock(x)         printf ("\e[%dm  ", x); fflush(stdout)

/*! @brief Esc[Value;...;Valuem - Set Graphics Mode */
#define __set_gm(attr,color,val)                                                \
        if (!color) {                                                           \
                printf("\e[%dm", attr);                                         \
        } else {                                                                \
                printf("\e[%d;%dm", color & 0x10 ? 1 : 0, (color & 0xF) + val); \
        }                                                                       \
        fflush(stdout)

#define textattr(attr)        __set_gm(attr, 0, 0)
#define textcolor(color)      __set_gm(RESETATTR, color, 30)
#define textbackground(color) __set_gm(RESETATTR, color, 40) 

static inline int
io_nonblock_getkey(void)
{
    return refos_async_getc();
}

#endif /* _TETRIS_IO_H_ */
