/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <refos/test.h>

/*! @file
    @brief Testing helper library. */

#ifdef CONFIG_REFOS_RUN_TESTS

/*! @brief The width at which the test results are aligned to. This should be larger than the
           lngest test name to ensure alignment. Too large and it may wrap around the terminal. */
#define REFOS_TEST_WIDTH 80

struct test_failure_log {
    int line;
    const char *file;
    const char *func;
    const char *check;
};

char *test_title = "";

static struct test_failure_log _testsFailLog[TEST_MAX_TESTS];
static unsigned int _testsFailed = 0;

void
test_start(const char *name)
{
    static char temp_buffer[TEST_TITLE_BUFFER_LEN];
    temp_buffer[0] = '\0';
    sprintf(temp_buffer, "%s | Testing %s ", test_title, name);
    tprintf("%s", temp_buffer);
    for (unsigned int i = strlen(temp_buffer); i < REFOS_TEST_WIDTH; i++) {
        tprintf(".");
    }
    tprintf(" ");
}

int
test_fail(const char *check, const char *file, const char *func, int line)
{
    tprintf(TEST_RED "FAILED" TEST_RESET ".\n");
    _testsFailLog[_testsFailed].check = check;
    _testsFailLog[_testsFailed].file = file;
    _testsFailLog[_testsFailed].func = func;
    _testsFailLog[_testsFailed].line = line;
    _testsFailed++;
    return -1;
}

int
test_success()
{
    tprintf(TEST_GREEN "PASSED" TEST_RESET ".\n");
    return 0;
}

void
test_print_log(void)
{
    for (unsigned int i = 0; i < _testsFailed; i++) {
        tprintf("%s | Test assert failed: %s, function %s, "
                "file %s, line %d.\n", test_title, _testsFailLog[i].check, _testsFailLog[i].func,
                _testsFailLog[i].file, _testsFailLog[i].line);
    }
    if (_testsFailed == 0) {
        tprintf("%s | All is well in the universe.\n", test_title);
    }
}

#endif /* CONFIG_REFOS_RUN_TESTS */
