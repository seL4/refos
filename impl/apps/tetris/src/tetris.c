/* Micro Tetris, based on an obfuscated tetris, 1989 IOCCC Best Game
 *
 * Copyright (c) 1989 John Tromp <john.tromp@gmail.com>
 * Copyright (c) 2009, 2010 Joachim Nilsson <troglobit@gmail.com>
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
 *
 * See the following URLs for more information, first John Tromp's page about
 * the game http://homepages.cwi.nl/~tromp/tetris.html then there's the entry
 * page at IOCCC http://www.ioccc.org/1989/tromp.hint
 */

/*! @file
    @brief RefOS port of Micro Tetris.
    
    <h1> RefOS Tetris </h2>
    
    A classic Tetris clone, heavily based on (a port of)
    Micro Tetris by Joachim Nilsson <joachim.nilsson@vmlinux.org>,
    which itself is based on the 1989 IOCCC Best Game entry "An obfuscated
    tetris" by John Tromp <john.tromp@gmail.com>.
    
    Website about original IOCCC entry:
    http://homepages.cwi.nl/~tromp/tetris.html
    
    Website about Micro Tetris:
    http://freecode.com/projects/micro-tetris 
    http://troglobit.com/tetris.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "io.h"

/* Game Settings Definitions. */
#define LINES_PER_LEVEL 5
#define INITIAL_DELAY_MS 300
#define LEVEL_DECREASE_DELAY_MS 18
#define RANDOM_SEED 0x177A0929
#define MAX_LEVEL 8

/* Board points. */
#define      TL     -B_COLS-1       /* Top left. */
#define      TC     -B_COLS         /* Top center. */
#define      TR     -B_COLS+1       /* Top right. */
#define      ML     -1              /* Middle left. */
#define      MR     1               /* Middle right. */
#define      BL     B_COLS-1        /* Bottom left. */
#define      BC     B_COLS          /* Bottom center. */
#define      BR     B_COLS+1        /* Bottom right. */

/* Controls. */
#define DEFAULT_KEYS "jkl pq"
#define KEY_LEFT   0
#define KEY_RIGHT  2
#define KEY_ROTATE 1
#define KEY_DROP   3
#define KEY_PAUSE  4
#define KEY_QUIT   5

/* Game variables. */
char exitGame = 0;
char *keys = DEFAULT_KEYS;
int level = 1;
int points = 0;
int linesCleared = 0;
int pos = 17;

/* Settings. */
char enablePreview = 1;
char enableScoreBoard = 1;

/*! @brief Peek preview of next shape. */
int *peekShape = NULL;

/* Screen buffers. */
int board[B_SIZE], shadow[B_SIZE];
int preview[B_COLS * 10], shadowPreview[B_COLS * 10];

/*! @brief Current shape. */
int *shape;

/*! @brief Shapes list. */
int shapes[] = {
    7,  TL,  TC,  MR,
    8,  TR,  TC,  ML,
    9,  ML,  MR,  BC,
    3,  TL,  TC,  ML,
   12,  ML,  BL,  MR,
   15,  ML,  BR,  MR,
   18,  ML,  MR,   2,           /* sticks out */
    0,  TC,  ML,  BL,
    1,  TC,  MR,  BR,
   10,  TC,  MR,  BC,
   11,  TC,  ML,  MR,
    2,  TC,  ML,  BC,
   13,  TC,  BC,  BR,
   14,  TR,  ML,  MR,
    4,  TL,  TC,  BC,
   16,  TR,  TC,  BC,
   17,  TL,  MR,  ML,
    5,  TC,  BC,  BL,
    6,  TC,  BC,  2 * B_COLS,   /* sticks out */
};
int colours[] = {
    A_BG_R,
    A_BG_C,
    A_BG_Y,
    A_BG_W,
    A_BG_G,
    A_BG_B,
    A_BG_M,
    A_BG_R, 
    A_BG_C,
    A_BG_Y,
    A_BG_Y,
    A_BG_Y,
    A_BG_G,
    A_BG_G,
    A_BG_G,
    A_BG_B,
    A_BG_B,
    A_BG_B,
    A_BG_M,
};

/*! @brief Render the online help. */
void
show_online_help(void)
{
    static int start = 11;
    textattr(RESETATTR);
    gotoxy (26 + 28, start);
    puts("j     - left");
    gotoxy (26 + 28, start + 1);
    puts("k     - rotate");
    gotoxy (26 + 28, start + 2);
    puts("l     - right");
    gotoxy (26 + 28, start + 3);
    puts("space - drop");
    gotoxy (26 + 28, start + 4);
    puts("p     - pause");
    gotoxy (26 + 28, start + 5);
    puts("q     - quit");
}

/*! @brief Gets the corresponding colour of the shape. */
int
get_col(int *shape)
{
    return colours[(shape - shapes) / 4];
}

/*! @brief Render and update the current screen output. */
int
update(void)
{
    int x, y; 
    
    /* Render the next piece preview. */
    if (enablePreview) {
        static int start = 5;
        memset (preview, 0, sizeof(preview));
        
        int c = get_col(peekShape);
        preview[2 * B_COLS + 1] = c;
        preview[2 * B_COLS + 1 + peekShape[1]] = c;
        preview[2 * B_COLS + 1 + peekShape[2]] = c;
        preview[2 * B_COLS + 1 + peekShape[3]] = c;
    
        for (y = 0; y < 4; y++) {
            for (x = 0; x < B_COLS; x++) {
                if (preview[y * B_COLS + x] - shadowPreview[y * B_COLS + x]) {
                    shadowPreview[y * B_COLS + x] = preview[y * B_COLS + x];
                    gotoxy (x * 2 + 26 + 28, start + y);
                    printblock(preview[y * B_COLS + x]);
                }
            }
        }
    }
    
    /* Render the actual board. */
    for (y = 1; y < B_ROWS - 1; y++) {
        for (x = 0; x < B_COLS; x++) {
            if (board[y * B_COLS + x] - shadow[y * B_COLS + x]) {
                shadow[y * B_COLS + x] = board[y * B_COLS + x];
                gotoxy (x * 2 + 28, y);
                printblock(board[y * B_COLS + x]);
            }
        }
    }
    
    /* Update points and level. */
    while (linesCleared >= LINES_PER_LEVEL) {
        linesCleared -= LINES_PER_LEVEL;
        level++;
        if (level > MAX_LEVEL) level = MAX_LEVEL;
    }
    
    /* Render the scoreboard. */
    if (enableScoreBoard) {
        textattr(RESETATTR);
        gotoxy (26 + 28, 2);
        printf ("Level  : %d", level);
        fflush(stdout);
        gotoxy (26 + 28, 3);
        printf ("Points : %d", points);
        fflush(stdout);
    }
    
    if (enablePreview) {
        gotoxy (26 + 28, 5);
        printf ("Preview:");
        fflush(stdout);
    }
    
    gotoxy (26 + 28, 10);
    printf ("Keys:");
    fflush(stdout);
    return 0;
}

/*! @brief Returns whether a piece fits in given board position. */
int
fits_in(int *shape, int pos) {
    if (board[pos] || board[pos + shape[1]] || board[pos + shape[2]] ||
            board[pos + shape[3]]) {
        return 0;
    }
    return 1;
}

/*! @brief Place a piece on the board at a given position. */
void
place(int *shape, int pos, int b)
{
    board[pos] = b;
    board[pos + shape[1]] = b;
    board[pos + shape[2]] = b;
    board[pos + shape[3]] = b;
}

/*! @brief Randomly select a next shape. */
int
*next_shape(void)
{
    int *next = peekShape;
    peekShape = &shapes[rand() % 7 * 4];
    if (!next) next = &shapes[rand() % 7 * 4];
    return next;
}

/*! @brief Update game state forward a frame. */
void
game_frame(int c)
{
    int j;
    int *backup;
    
    if (c == 0) {
        if (fits_in (shape, pos + B_COLS)) {
            pos += B_COLS;
        } else {
            place(shape, pos, get_col(shape));
            ++points;
            for (j = 0; j < 252; j = B_COLS * (j / B_COLS + 1)) {
                for (; board[++j];) {
                    if (j % B_COLS == 10) {
                        linesCleared++;
                        
                        for (; j % B_COLS; board[j--] = 0);
                        update();
                        for (; --j; board[j + B_COLS] = board[j]);
                        update();
                    }
                }
            }
            shape = next_shape();
            if (!fits_in (shape, pos = 17)) c = keys[KEY_QUIT];
        }
    }

    if (c == keys[KEY_LEFT]) {
        if (!fits_in (shape, --pos)) ++pos;
    }
    if (c == keys[KEY_ROTATE]) {
        backup = shape;
        shape = &shapes[4 * *shape]; /* Rotate */
        /* Check if it fits, if not restore shape from backup. */
        if (!fits_in (shape, pos)) shape = backup;
    }

    if (c == keys[KEY_RIGHT]) {
        if (!fits_in (shape, ++pos)) --pos;
    }
    if (c == keys[KEY_DROP]) {
        for (; fits_in (shape, pos + B_COLS); ++points) pos += B_COLS;
    }
    if (c == keys[KEY_PAUSE] || c == keys[KEY_QUIT]) {
        exitGame = 1;
        return;
    }

    place(shape, pos, get_col(shape));
    update();
    place(shape, pos, RESETATTR);
}

static void
print_welcome_message(void)
{
    printf(
        "  ████████╗███████╗████████╗██████╗ ██╗███████╗\n"
        "  ╚══██╔══╝██╔════╝╚══██╔══╝██╔══██╗██║██╔════╝\n"
        "     ██║   █████╗     ██║   ██████╔╝██║███████╗\n"
        "     ██║   ██╔══╝     ██║   ██╔══██╗██║╚════██║\n"
        "     ██║   ███████╗   ██║   ██║  ██║██║███████║\n"
        "     ╚═╝   ╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝╚══════╝\n"
    );
}

int
main()
{
    int c, i, *ptr;
    int delay = INITIAL_DELAY_MS;
    refos_initialise();
    
    peekShape = NULL;

    /* Initialise board. */
    ptr = board;
    for (i = B_SIZE - 1; i>=0; i--) {
        *ptr++ = i < 25 || i % B_COLS < 2 ? A_BG_W : RESETATTR;
    }
    
    srand((unsigned int) RANDOM_SEED);

    clrscr();

    print_welcome_message();
    printf("      Press the space bar to continue...\n");
    show_online_help();

    while (1) {
        c = io_nonblock_getkey();
        rand();
        if (c == ' ') break;
    }
    clrscr();
    show_online_help();

    /* Main game loop. */
    shape = next_shape();
    while (!exitGame) {
        c = io_nonblock_getkey();
        rand();
        if (c >= 0) {
            game_frame(c);
        } else {
            game_frame(0);
            usleep((delay - level * LEVEL_DECREASE_DELAY_MS) * 1000);
        }
    }

    gotoxy (0, 25);
    printf("Game over! You reached %d points.\n", points);
} 
