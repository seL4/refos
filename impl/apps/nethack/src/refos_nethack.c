/* Nethack as a whole is licensed under a GPL license.
   Please refer to LICENSE_GPL.txt for details.

   Source files were modified to be compatible with the RefOS build system, and to disable features
   that RefOS didn't fully support.
*/

#include <stdio.h>
#include <assert.h>
#include <refos-io/stdio.h>

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
