/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "device_screen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <autoconf.h>
#include "state.h"
#include "ega_vterm.h"

/*! @file
    @brief Console Server EGA screen device manager. */

#if defined(PLAT_PC99) && defined(CONFIG_REFOS_ENABLE_EGA)
    /* Assumptions on the graphics mode and frame buffer location. */
    #define DEVICE_SCREEN_EGA_TEXT_FBUFFER_BASE_PADDR 0xB8000
    #define DEVICE_SCREEN_EGA_MODE_WIDTH 80
    #define DEVICE_SCREEN_EGA_MODE_HEIGHT 25
#endif

void
device_screen_init(struct device_screen_state* s, dev_io_ops_t *io)
{
    assert(io);
    memset(s, 0, sizeof(struct device_screen_state));
    s->magic = CONSERV_DEVICE_SCREEN_MAGIC;

    #if defined(PLAT_PC99) && defined(CONFIG_REFOS_ENABLE_EGA)

    /* Allocate virtual terminal struct. */
    s->vterm = malloc(sizeof(vterm_state_t));
    if (!s->vterm) {
        ROS_ERROR("Failed to allocate vterm structure. Console server out of memory.");
        return;
    }

    /* Map the frame buffer. */
    s->frameBuffer = ps_io_map(&io->opsIO.io_mapper, DEVICE_SCREEN_EGA_TEXT_FBUFFER_BASE_PADDR,
                               0x1000, false, PS_MEM_NORMAL);
    if (!s->frameBuffer) {
        ROS_ERROR("Failed to map EGA Text Frame Buffer. Disabling /dev/screen.");
        free(s->vterm);
        return;
    }
    s->width = DEVICE_SCREEN_EGA_MODE_WIDTH;
    s->height = DEVICE_SCREEN_EGA_MODE_HEIGHT;

    /* Initialise virtual terminal state. */
    int error = vterm_init(s->vterm, s->width, s->height, s->frameBuffer);
    if (error) {
        ROS_ERROR("Failed to initialise virtual terminal. Shouldn't happen.");
        assert(!"Failed to initialise virtual terminal. Shouldn't happen.");
        return;
    }

    s->initialised = true;
    device_screen_reset(s);
    vterm_printf(s->vterm,
        COLOUR_Y "RefOS Console server initialised virtual terminal...\n" COLOUR_RESET);
    #endif

}

void
device_screen_reset(struct device_screen_state* s)
{
    assert(s && s->magic == CONSERV_DEVICE_SCREEN_MAGIC);
    if (!s->initialised) {
        /* No screen device available. */
        return;
    }
    assert(s->frameBuffer);
    memset((void*) s->frameBuffer, 0, s->width * s->height * sizeof(short));
}

void
device_screen_direct_puts(struct device_screen_state* s, int r, int c, const char* str)
{
    assert(s && s->magic == CONSERV_DEVICE_SCREEN_MAGIC);

    if (r < 0 || r >= s->height || c < 0 || c >= s->width) {
        /* Out of screen bounds. */
        return;
    }

    if (!s->initialised) {
        /* No screen device available. */
        return;
    }

    int len = strlen(str);
    for (int i = 0; i < len; i++) {
        if (c + i >= s->width) {
            break;
        }
        /* Write character to screen. The (7 << 8) colour is the default EGA framebuffer text
           gray colour. */
        s->frameBuffer[r * s->width + c + i] = ((short) str[i]) | (7 << 8);
    }
}

void
device_screen_write(struct device_screen_state* s, char* buf, int len)
{
    assert(s && s->magic == CONSERV_DEVICE_SCREEN_MAGIC);
    if (!s->initialised && !len) {
        return;
    }
    vterm_write(s->vterm, buf, len);
}