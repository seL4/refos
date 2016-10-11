/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <refos/refos.h>
#include <refos-util/init.h>
#include <refos-io/stdio.h>
#include <refos-rpc/proc_client.h>
#include <refos-rpc/proc_client_helper.h>
#include <unistd.h>
#include <autoconf.h>

#include "print_time.h"

/*! @file
    @brief A simple terminal program for RefOS.

    This is a rather minimalistic and hacky terminal app for RefOS. A better console such as bash,
    or ash, may be ported in the future. The terminal app runs as the first userland program, and
    presents an interactive shell. It's possible to launcher another instance of the terminal
    program using the terminal program.
*/

#define TERMINAL_INPUT_ARG_LENGTH 10
#define TERMINAL_INPUT_ARG_COUNT 10
#define TERMINAL_INPUT_BUFFER_SIZE 512
#define TERMINAL_NEW_LINE_CHAR '\r'
#define TERMINAL_DELETE_CHAR 127
#define TERMINAL_CLEAR_SCREEN "\e[2J\e[1;1H"

static char *args[TERMINAL_INPUT_ARG_COUNT];
static char exitProgram = 0;
extern char **__environ; /*! < POSIX standard, from MUSLC lib. */

/*! @brief Print a heart-warming welcome banner. */
static void
terminal_startup_message(void)
{
    printf(" ______     ______     ______   ______     ______    \n"
           "/\\  == \\   /\\  ___\\   /\\  ___\\ /\\  __ \\   /\\  ___\\   \n"
           "\\ \\  __<   \\ \\  __\\   \\ \\  __\\ \\ \\ \\/\\ \\  \\ \\___  \\  \n"
           " \\ \\_\\ \\_\\  \\ \\_____\\  \\ \\_\\    \\ \\_____\\  \\/\\_____\\ \n"
           "  \\/_/ /_/   \\/_____/   \\/_/     \\/_____/   \\/_____/ \n\n"
           "-----------------------------------------------------\n"
           "    Built on the seL4 microkernel.\n"
           "    (c) Copyright 2016 Data61, CSIRO\n"
           "-----------------------------------------------------\n");
}

/*! @brief Print some quick help. */
static void
terminal_printhelp(void)
{
    printf("RefOS Terminal Help:\n"
           "    clear - Clear the screen.\n"
           "    help - Display this help screen.\n"
           "    exec - Run an executable.\n"
           #if CONFIG_APP_TETRIS
           "    exec fileserv/tetris - Run tetris game.\n"
           #endif
           #if CONFIG_APP_SNAKE
           "    exec fileserv/snake - Run snake game.\n"
           #endif
           #if CONFIG_APP_NETHACK
           "    exec fileserv/nethack - Run Nethack 3.4.3 game.\n"
           #endif
           #if CONFIG_APP_TEST_USER
           "    exec fileserv/test_user - Run RefOS userland tests.\n"
           #endif
           "    exec fileserv/terminal - Run another instance of RefOS terminal.\n"
           "    cd /fileserv/ - Change current working directory.\n"
           "    printenv - Print all environment variables.\n"
           "    setenv - Set an environment variable.\n"
           "    time - Display the current system time.\n"
           "    exit - Exit RefOS terminal.\n");
}

/*! @brief Execute a program. */
static void
terminal_exec(void)
{
    int status;
    if (!args[1]) {
        printf("exec: missing parameter.\n");
        return;
    }
    char tempBuffer[1024];
    snprintf(tempBuffer, 1024, "%s%s", getenv("PWD"), args[1]);
    proc_new_proc(tempBuffer, "", true, 71, &status);

    if (status == EFILENOTFOUND) {
        printf("-terminal: %s: application not found\n", args[1]);
    }
}

/*! @brief Evaluate a command. */
static void
terminal_evaluate_command(char *inputBuffer)
{
    if (!args[0]) return;

    /* Crazy string parsing can go here. */
    if (!strcmp(args[0], "exec")) {
        terminal_exec();
    } else if (!strcmp(args[0], "clear") || !strcmp(args[0], "cls") || !strcmp(args[0], "reset")) {
        printf("%s", TERMINAL_CLEAR_SCREEN);
    } else if (!strcmp(args[0], "exit") || !strcmp(args[0], "quit")) {
        exitProgram = 1;
    } else if (!strcmp(args[0], "help")) {
        terminal_printhelp();
    } else if (!strcmp(args[0], "time")) {
        time_t rawTime = time(NULL);
        struct tm *gmtTime = gmtime(&rawTime);
        struct tm *localTime = localtime(&rawTime);
        printf("Raw epoch time is %llu\n", (uint64_t) rawTime);
        printf("Current GMT time is %s", refos_print_time(gmtTime));
        printf("Current local time (%s) is %s", getenv("TZ"), refos_print_time(localTime));
    } else if (!strcmp(args[0], "printenv")) {
        for (int i = 0; __environ[i]; i++) {
            printf("%s\n", __environ[i]);
        }
    } else if (!strcmp(args[0], "setenv")) {
        if (args[1] && args[2]) {
            setenv(args[1], args[2], true);
        }
        else {
            printf("setenv: usage: setenv <VARIABLE> <VALUE>\n");
        }
    } else if (!strcmp(args[0], "cd") && args[1]) {
        setenv("PWD", args[1], true);
    } else {
        printf("-terminal: %s: command not found\n", inputBuffer);
    }
}

/*! @brief Input a new command. */
static void
terminal_get_input(char *inputBuffer)
{
    printf("refos:%s$ ", getenv("PWD"));
    fflush(stdout);

    int i = 0; inputBuffer[0] = '\0';
    for (i = 0; i < TERMINAL_INPUT_BUFFER_SIZE;) {
        inputBuffer[i] = getchar();
        if (inputBuffer[i] == TERMINAL_NEW_LINE_CHAR) {
            printf("\n");
            inputBuffer[i] = '\0';
            return;
        } else if (inputBuffer[i] == TERMINAL_DELETE_CHAR) {
            if (i > 0) {
                printf("\b \b");
                fflush(stdout);
                inputBuffer[--i] = '\0';
            }
        } else {
            printf("%c", inputBuffer[i]);
            fflush(stdout);
            i++;
        }
    }
    inputBuffer[TERMINAL_INPUT_BUFFER_SIZE] = '\0';
    printf("\n");
}

/*! @brief Reset the input buffer. */
static inline void
terminal_reset_input(void)
{
    for (int i = 0; i < TERMINAL_INPUT_ARG_COUNT; i++) {
        args[i] = NULL;
    }
}

/*! @brief Main terminal loop. */
void
terminal_mainloop(void)
{
    char inputBuffer[TERMINAL_INPUT_BUFFER_SIZE + 1];
    char *ptr;
    int i;

    while (!exitProgram) {
        terminal_reset_input();
        terminal_get_input(inputBuffer);

        ptr = strtok(inputBuffer, " ");
        i = 0;
        while (ptr != NULL) {
            args[i] = ptr;
            ptr = strtok(NULL, " ");
            i++;
        }

        terminal_evaluate_command(inputBuffer);
    }
}

/*! @brief Main terminal entry point. */
int main()
{
    /* Future Work 3:
       How the selfloader bootstraps user processes needs to be modified further. Changes were
       made to accomodate the new way that muslc expects process's stacks to be set up when
       processes start, but the one part of this that still needs to changed is how user processes
       find their system call table. Currently the selfloader sets up user processes so that
       the selfloader's system call table is used by user processes by passing the address of the
       selfloader's system call table to the user processes via the user process's environment
       variables. Ideally, user processes would use their own system call table.
    */

    uintptr_t address = strtoll(getenv("SYSTABLE"), NULL, 16);
    refos_init_selfload_child(address);
    refos_initialise();
    terminal_startup_message();
    terminal_mainloop();
    printf("\n");
    return 0;
}
