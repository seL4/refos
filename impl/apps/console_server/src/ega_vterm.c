/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#include <autoconf.h>

#include "ega_vterm.h"
#include "state.h"

/*! @file
    @brief Virtual EGA terminal emulator. */

static void
vterm_buffer_puts_internal(vterm_state_t *s, int r, int c, uint32_t *buf, int len)
{
    assert(s && s->magic == VTERM_MAGIC);
    if (r < 0 || r >= s->height) {
        return;
    }
    if (c < 0 || c >= s->width) {
        return;
    }
    if (c + len >= s->width) {
        len = (s->width - 1) - c;
    }
    int n = 0;
    for (int i = c; i < c + len; i++) {
        if (buf[n] & ~0xFF) {
            /* UTF-8 character, cannot display. */
            buf[n] = (char) 127;
        }
        char ch = (char) buf[n++];
        s->buffer[r * s->width + i] = (uint16_t) (ch |
                ((s->fgColour & 0xF) << 8) | ((s->bgColour & 0xF) << 12));
    }
}

static int
vterm_internal_rgb_to_vterm_colour_index(int r, int g, int b)
{
    /* The values here should mirror the ones in pen.c. */
    static const VTermColor vtermANSIColors[] = {
        /* R    G    B */
        {   0,   0,   0 }, // black
        { 224,   0,   0 }, // red
        {   0, 224,   0 }, // green
        { 224, 224,   0 }, // yellow
        {   0,   0, 224 }, // blue
        { 224,   0, 224 }, // magenta
        {   0, 224, 224 }, // cyan
        { 224, 224, 224 }, // white == light grey
        { 128, 128, 128 }, // black
        { 255,  64,  64 }, // red
        {  64, 255,  64 }, // green
        { 255, 255,  64 }, // yellow
        {  64,  64, 255 }, // blue
        { 255,  64, 255 }, // magenta
        {  64, 255, 255 }, // cyan
        { 255, 255, 255 }, // white for real
    };
    
    static const int tableANSI2VGA[] = {
        VTERM_BLACK,
        VTERM_LOW_RED,
        VTERM_LOW_GREEN,
        VTERM_BROWN,
        VTERM_LOW_BLUE,
        VTERM_LOW_MAGENTA,
        VTERM_LOW_CYAN,
        VTERM_LIGHT_GRAY,
        VTERM_DARK_GRAY,
        VTERM_HIGH_RED,
        VTERM_HIGH_GREEN,
        VTERM_HIGH_YELLOW,
        VTERM_HIGH_BLUE,
        VTERM_HIGH_MAGENTA,
        VTERM_HIGH_CYAN,
        VTERM_HIGH_WHITE
    };
    
    const int vtermANSIColorsNum = 16;
    for (int i = 0; i < vtermANSIColorsNum; i++) {
        if (r == vtermANSIColors[i].red &&
                g == vtermANSIColors[i].green &&
                b == vtermANSIColors[i].blue) {
            return tableANSI2VGA[i];
        }
    }
    return VTERM_LIGHT_GRAY;
}

static int
vterm_internal_colour_to_vterm_colour_index(VTermColor col)
{
    return vterm_internal_rgb_to_vterm_colour_index(col.red, col.green, col.blue);
}

/* ----------------------------- Virtual Terminal Functions ------------------------------------- */

int
vterm_init(vterm_state_t *s, int width, int height, volatile uint16_t *buffer) 
{
    memset(s, 0, sizeof(vterm_state_t));

    s->magic = VTERM_MAGIC;
    s->width = width;
    s->height = height;
    s->buffer = buffer;
    s->fgColour = VTERM_LIGHT_GRAY;
    s->bgColour = VTERM_BLACK;
    s->autoRenderUpdate = true;

    /* Initialise virtual terminal. */
    dprintf("Initialising %d x %d Virtual Terminal object...\n", width, height);
    s->vt = vterm_new(s->height, s->width);
    if (!s->vt) {
        ROS_ERROR("Failed to create terminal.\n");
        return EINVALID;
    }
    
    /* Grab the virtual screen & state object. */
    s->vts = vterm_obtain_screen(s->vt);
    s->vtstate = vterm_obtain_state(s->vt);
    assert(s->vts && s->vtstate);
    
    /* Set parameters. */
    vterm_parser_set_utf8(s->vt, true);
    vterm_state_set_bold_highbright(s->vtstate, true);
    vterm_screen_reset(s->vts, 1);

    return ESUCCESS;
}

void vterm_deinit(vterm_state_t *s)
{
    assert(s && s->magic == VTERM_MAGIC);
    vterm_free(s->vt);
    memset(s, 0, sizeof(vterm_state_t));
}

void
vterm_write(vterm_state_t *s, char *buffer, int len)
{
    assert(s && s->magic == VTERM_MAGIC);
    for (int i = 0; i < len; i++) {
        vterm_push_bytes(s->vt, &buffer[i], 1);
        if (buffer[i] == '\n') {
            char cr = '\r';
            /* Convert \n to \n\r. */
            vterm_push_bytes(s->vt, &cr, 1);
        }
    }
    if (s->autoRenderUpdate) {
        vterm_render_buffer(s);
    }
}

void
vterm_printf(vterm_state_t *s, const char* fmt, ...)
{
    char temp[1024];
    va_list varListParser;
    va_start(varListParser, fmt);
      vsprintf(temp, fmt, varListParser);
    va_end(varListParser);
    vterm_write(s, temp, strlen(temp));
}

void
vterm_render_buffer(vterm_state_t *s)
{
    assert(s && s->magic == VTERM_MAGIC && s->buffer);
    
    int bufferHeight, bufferWidth;
    vterm_get_size(s->vt, &bufferHeight, &bufferWidth);
    
    for (int i = 0; i < bufferHeight; i++) {
        for (int j = 0; j < bufferWidth; ) {
            VTermPos pos = {
                .row = i,
                .col = j  
            };
            VTermScreenCell cell;
            vterm_screen_get_cell(s->vts, pos, &cell);
            s->fgColour = vterm_internal_colour_to_vterm_colour_index(cell.fg);
            s->bgColour = vterm_internal_colour_to_vterm_colour_index(cell.bg);
            vterm_buffer_puts_internal(s, i, j, cell.chars, cell.width);
            j += cell.width;
        }
    }
}
