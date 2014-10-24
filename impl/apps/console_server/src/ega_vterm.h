/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CONSOLE_SERVER_EGA_VIRTUAL_TERMINAL_H_
#define _CONSOLE_SERVER_EGA_VIRTUAL_TERMINAL_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <vterm/vterm.h>

/*! @file
    @brief Virtual EGA terminal emulator.

    This module uses libvterm and acts as the thin connection layer between it and a PC99 EGA 80x25
    text mode buffer. Any ANSI sequences and control characters written to the virtual terminal will
    be interpreted accordingly, and utf-8 characters will be replaced by a placeholder.
*/

#define VTERM_MAGIC 0x3FA60990

/*! @brief VTerm global state. */
typedef struct vterm_state {
    uint32_t magic;
    VTerm *vt; /* Has ownership. */
    VTermScreen *vts; /* No ownership */
    VTermState *vtstate;  /* No ownership */
    bool autoRenderUpdate;

    int height;
    int width;
    volatile uint16_t *buffer; /* No ownership */
    
    int fgColour;
    int bgColour;
} vterm_state_t;

/*! @brief VTerm colour definitions.

    These correspond to the 8-bit VGA / CGA colour palette.
    Note that this is slightly different to the standard ANSI colour palette, but
    is easily convertible.
*/
enum vterm_colors {
    VTERM_BLACK,
    VTERM_LOW_BLUE,
    VTERM_LOW_GREEN,
    VTERM_LOW_CYAN,
    VTERM_LOW_RED,
    VTERM_LOW_MAGENTA,
    VTERM_BROWN,
    VTERM_LIGHT_GRAY,
    VTERM_DARK_GRAY,
    VTERM_HIGH_BLUE,
    VTERM_HIGH_GREEN,
    VTERM_HIGH_CYAN,
    VTERM_HIGH_RED,
    VTERM_HIGH_MAGENTA,
    VTERM_HIGH_YELLOW,
    VTERM_HIGH_WHITE
};

/* ----------------------------- Virtual Terminal Functions ------------------------------------- */

/*! @brief Initialise the virtual terminal emulator library and EGA connecting layer.
    @param s The emulator state. (No ownership)
    @param width Width of the screen buffer.
    @param height Height of the screen buffer.
    @param buffer Address of the screen buffer. May be an in-memory buffer, or may be the actual
                  mapped device frame buffer. (No ownership)
    @return ESUCCESS if success, refos_err_t otherwise.
*/
int vterm_init(vterm_state_t *s, int width, int height, volatile uint16_t *buffer);

/*! @brief De-initialise the virtual terminal emulator library state. Does not actually release
           given structure.
    @param s The emulator state. (No ownership)
*/
void vterm_deinit(vterm_state_t *s);

/*! @brief Write buffer out to virtual terminal. Any ANSI sequences and control characters in the
           given buffer will be interpreted by the virtual terminal.
    @param s The emulator state. (No ownership)
    @param buffer The buffer to write.
    @param len Length of given buffer in bytes.
*/
void vterm_write(vterm_state_t *s, char *buffer, int len);

/*! @brief Similar to vterm_write(), but allows printf formatting.
    @param s The emulator state. (No ownership)
    @param fmt The printf format string.
*/
void vterm_printf(vterm_state_t *s, const char* fmt, ...);

/*! @brief Render out terminal screen information to the given screen buffer in vterm_init().
           This needs to be done every time the screen has changed. If the autoRenderUpdate flag
           has been set, this will be called automatically on every vterm_write() or vterm_printf().
           The default value for autoRenderUpdate is true.
    @param s The emulator state. (No ownership)
*/
void vterm_render_buffer(vterm_state_t *s);

#endif /* _CONSOLE_SERVER_EGA_VIRTUAL_TERMINAL_H_ */

