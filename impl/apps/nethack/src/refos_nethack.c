/* Nethack as a whole is licensed under a GPL license.
   Please refer to LICENSE_GPL.txt for details.

   Source files were modified to be compatible with the RefOS build system, and to disable features
   that RefOS didn't fully support.
*/

#include <stdio.h>
#include <assert.h>
#include <refos-io/stdio.h>
#include <refos/refos.h>

#define REFOS_NETHACK_WIZARD_MODE 0

int
has_colors()
{
    return 1;
}

char _argv0[] = "fileserv/nethack";
#if REFOS_NETHACK_WIZARD_MODE
char _argv1[] = "-D";
#endif

int
main(int argc, char **argv)
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
    refos_stdio_translate_stdin_cr = true;
    refos_initialise();
    printf("RefOS nethack.\n");
    setenv("PWD", "fileserv/", true);

#if REFOS_NETHACK_WIZARD_MODE
    char *_argv[] = {_argv0, _argv1};
    int _argc = 2;
#else
    char *_argv[] = {_argv0 };
    int _argc = 1;
#endif

    nethack_main(_argc, _argv);
}
