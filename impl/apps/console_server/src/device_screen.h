/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CONSOLE_SERVER_DEVICE_SCREEN_H_
#define _CONSOLE_SERVER_DEVICE_SCREEN_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sel4/sel4.h>
#include <refos-util/device_io.h>
#include "ega_vterm.h"

/*! @file
    @brief Console Server EGA screen device manager. */

#define CONSERV_DEVICE_SCREEN_MAGIC 0xC2325410

/*! @brief Screen device waiter structure. */
struct device_screen_state {
    uint32_t magic;
    bool initialised;

    volatile uint16_t *frameBuffer;
    int width;
    int height;

    vterm_state_t* vterm;
};

/*! @brief Initialise screen device driver and terminal emulator.
    @param s The screen device state structure to initialise. (No ownership)
    @param io The initialised device IO manager. (No ownership).
*/
void device_screen_init(struct device_screen_state* s, dev_io_ops_t *io);

/*! @brief Clear the screen. Does nothing when no screen device available.
    @param s The screen device state to clear screen for.
*/
void device_screen_reset(struct device_screen_state* s);

/*! @brief Directly put a string on the screen. Does NOT get parsed by virtual terminal.
           Does nothing when no screen device available.
    @param s The screen device state to put string directly into.
    @param r The row position to start string at.
    @param c The column position to start string at.
    @param str The NULL_terminated string to write.
*/
void device_screen_direct_puts(struct device_screen_state* s, int r, int c, const char* str);

/*! @brief Write a VT buffer to the screen. Gets interpreted by the virtual terminal.
           Does nothing when no screen device available.
    @param s The screen device state containing the virtual terminal to write buffer into.
    @param buf The buffer to write and interpret as virtual terminal input stream.
    @param len Length of give buffer to write.
*/
void device_screen_write(struct device_screen_state* s, char* buf, int len);

#endif /* _CONSOLE_SERVER_DEVICE_SCREEN_H_ */
