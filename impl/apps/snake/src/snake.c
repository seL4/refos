/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*! @file
    @brief A simple port of Snake to run as a RefOS userland demo app.
    
    This simple port of the classic game Snake serves as an demo app for the high-level RefOS
    userland environment. It uses a UNIX-line environment, showing serial input / output
    functionality and timer functionality.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include <refos/refos.h>
#include <refos-io/stdio.h>
#include <refos-util/init.h>
#include <refos-util/dprintf.h>

/* ANSI Escape sequences. */
#define clrscr()              puts ("\e[2J\e[1;1H")
#define gotoxy(x,y)           printf("\e[%d;%dH", y, x); fflush(stdout)
#define hidecursor()          puts ("\e[?25l")
#define showcursor()          puts ("\e[?25h")
#define printblock(x)         printf ("\e[%dm  ", x); fflush(stdout)

/* Game environment defitions. */
#define NUM_ROWS 20
#define NUM_COLS 35
#define MAX_SNAKE_LENGTH 50
#define INITIAL_SNAKE_LENGTH 4
#define DELAY_AMOUNT 250

/* Screen buffer state. */
char buffer[NUM_ROWS][NUM_COLS];
char lastbuffer[NUM_ROWS][NUM_COLS];

/* Game state. */
int snakeX[MAX_SNAKE_LENGTH];
int snakeY[MAX_SNAKE_LENGTH];
int snakeLen;
int snakeDir;
int appleX, appleY;

void newGame(void);

/*! @brief Pick another random apple location. */
void
newApple(void)
{
    appleX = rand() % NUM_ROWS;
    appleY = rand() % NUM_COLS;
}

/*! @brief Step forward the game one discrete frame. */
void
stepGame(void)
{
    /* Shift the snake down one step. */
    for (int i = (snakeLen - 1); i >= 1; i--) {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
    }
    static int dr[] = {-1, 0, 1, 0 };
    static int dc[] = { 0, 1, 0,-1 };
    snakeX[0] += dr[snakeDir];
    snakeY[0] += dc[snakeDir];

    /* Warp around edge of screen. */
    if (snakeX[0] < 0) snakeX[0] += NUM_ROWS;
    if (snakeY[0] < 0) snakeY[0] += NUM_COLS;
    snakeX[0] %= NUM_ROWS;
    snakeY[0] %= NUM_COLS;

    /* Check if we have collided ourselves. */
    for (int i = 1; i < snakeLen; i++) {
        if (snakeX[i] < 0) continue;
        if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
            gotoxy (0, NUM_ROWS + 1);
            printf("Your snake has died.\n");
            newGame();
            return;
        }
    }

    /* Check if we have eaten the apple. */
    if (snakeX[0] == appleX && snakeY[0] == appleY) {
        if (snakeLen + 1 < MAX_SNAKE_LENGTH) {
            snakeX[snakeLen] = snakeY[snakeLen] = -1;
            snakeLen++;
            newApple();
        }
    }
}

/*! @brief Render the current game state to screen.
 
    Speeds up rendering by first checking screen buffer differences, and only re-writing the bits
    that have changed. Much faster as every gotoxy() is quite expensive.
*/
void
renderGame(void)
{
    /* Render the snake. */
    memset(buffer, 0, NUM_ROWS * NUM_COLS);
    for (int i = 0; i < snakeLen; i++) {
        if (snakeX[i] < 0) continue;
        buffer[snakeX[i]][snakeY[i]] = '#';
    }
    
    /* Render the apple. */
    buffer[appleX][appleY] = 'O';

    /* Draw the buffer to the screen. */
    for (int i = 0; i < NUM_ROWS; i++) {
        for (int j = 0; j < NUM_COLS; j++) {
            if (buffer[i][j] == lastbuffer[i][j]) continue;
            gotoxy(j, i);
            if (buffer[i][j] == 0) {
                printf(" ");
            } else if (buffer[i][j] == '#') {
                printf(COLOUR_G "█" COLOUR_RESET);
            } else {
                printf(COLOUR_R "◯" COLOUR_RESET);
            }
        }
    }

    /* Save the last buffer. */
    memcpy(lastbuffer, buffer, NUM_ROWS * NUM_COLS);
}

/*! @brief Starts (or restarts) a new game of snake, resetting snake and apple. */
void
newGame(void)
{
    /* Display new game message. */
    printf("      Press the space bar to continue...\n");
    while (1) {
        int c = refos_async_getc();
        rand();
        if (c == ' ') break;
    }

    /* Start the game. */
    clrscr();
    snakeLen = INITIAL_SNAKE_LENGTH;
    for (int i = 1; i < snakeLen; i++) {
        snakeX[i] = snakeY[i] = -1;
    }
    snakeDir = rand() % 4;
    snakeX[0] = rand() % NUM_ROWS;
    snakeY[0] = rand() % NUM_COLS;
    newApple();
}

/*! @brief Change the snake direction according to key pressed. */
void
handleInput(int c)
{
    if (c == 'w') {
        snakeDir = 0;
    } else if (c == 's') {
        snakeDir = 2;
    } else if (c == 'a') {
        snakeDir = 3;
    } else if (c == 'd') {
        snakeDir = 1;
    }
}

/*! @brief Output a nice big logo. */
static void
print_welcome_message(void)
{
    printf(
        "_______ __   _ _______ _     _ _______\n"
        "|______ | \\  | |_____| |____/  |______\n"
        "______| |  \\_| |     | |    \\_ |______\n"
    );
}


/*! @brief Snake main function. */
int
main()
{
    refos_initialise();
    srand(time(NULL));

    clrscr();
    hidecursor();
    print_welcome_message();
    newGame();

    while (true) {
        int c = refos_async_getc();
        if (c == 'q') {
            break;
        }
        handleInput(c);
        stepGame();
        renderGame();
        usleep(DELAY_AMOUNT * 1000);
    }

    showcursor();
}
