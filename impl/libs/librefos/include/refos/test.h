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
    @brief Testing helper library.

    This simple lightweight testing library provides a consistent testing output interface. Root-
    task unit tests, OS level tests and user level tests all use this library, so they output
    results in the same format.
*/

#ifndef _REFOS_TEST_H_
#define _REFOS_TEST_H_

#include <refos/refos.h>
#include <autoconf.h>

#define REFOS_TEST_HALT_ON_ASSERT 0
#define REFOS_TEST_VERBOSE_PRINT 0

#ifdef CONFIG_REFOS_RUN_TESTS

#define TEST_MAX_TESTS 128
#define TEST_TITLE_BUFFER_LEN 256

#if defined(CONFIG_REFOS_ANSI_COLOUR_OUTPUT)
    #define TEST_RED "\033[;1;31m"
    #define TEST_GREEN "\033[;1;32m"
    #define TEST_GREY "\033[;0;37m"
    #define TEST_RESET "\033[0m"
#else
    #define TEST_RED
    #define TEST_GREEN
    #define TEST_GREY
    #define TEST_RESET
#endif

#define tprintf(x...) fprintf(stdout, x)

/*! @brief The current test title. */
extern char *test_title;

/*! @brief Start a new unit test. Must be called first for every test.
    @param name String containing the test name, to be displayed on screen.
*/
void test_start(const char *name);

/*! @brief Internal function to fail the current test.

    This function is used to implement the  test_assert macro. Must be called after test_start() and
    before test_success(). Please use the test_assert() macro instead of directly calling this
    function.

    @param check String containing the condition being checked that failed.
    @param file The file that the failure occured in.
    @param func The function name that the failure occured in.
    @param line The line number into the file.
    @return -1, meaning failure.
*/
int test_fail(const char *check, const char *file, const char *func, int line);

/*! @brief Set the current test to success. This should be called the last thing after a unit test
           has been completed; it effectively ends the test.
    @return 0, meaning success.
*/
int test_success(void);

/*! @brief Print all the test results for every test so far, along with any failures. */
void test_print_log(void);

#if REFOS_TEST_HALT_ON_ASSERT
    #define test_assert assert
#else
    #define test_assert(e) if (!(e)) return test_fail(#e, __FILE__, __func__, __LINE__)
#endif

#if REFOS_TEST_VERBOSE_PRINT
    #define tvprintf printf
#else
    #define tvprintf(x...)
#endif

#endif /* CONFIG_REFOS_RUN_TESTS */

#endif /* _REFOS_TEST_H_ */
