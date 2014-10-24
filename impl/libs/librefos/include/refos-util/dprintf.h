/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_DEBUG_
#define _REFOS_DEBUG_

#include <autoconf.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sel4/sel4.h>

/*! @file
    @brief Common RefOS debugging output helper functions.

    This file defines shared common RefOS debugging output functions. Using a common debugging
    interface ensures a consistent debug output format, and a centralised control to turn them on
    or off.
*/

// Console colours.
#ifndef COLOUR
    #ifdef CONFIG_REFOS_ANSI_COLOUR_OUTPUT
        #define COLOUR "\033[;1;%dm"
        #define COLOUR_R "\033[;1;31m"
        #define COLOUR_G "\033[;1;32m"
        #define COLOUR_Y "\033[;1;33m"
        #define COLOUR_B "\033[;1;34m"
        #define COLOUR_M "\033[;1;35m"
        #define COLOUR_C "\033[;1;36m"
        #define COLOUR_W "\033[;1;37m"
        #define COLOUR_NORM_R "\033[;0;31m"
        #define COLOUR_NORM_G "\033[;0;32m"
        #define COLOUR_NORM_Y "\033[;0;33m"
        #define COLOUR_NORM_B "\033[;0;34m"
        #define COLOUR_NORM_M "\033[;0;35m"
        #define COLOUR_NORM_C "\033[;0;36m"
        #define COLOUR_NORM_W "\033[;0;37m"
        #define COLOUR_RESET "\033[0m"
    #else
        #define COLOUR "%d "
        #define COLOUR_R
        #define COLOUR_G
        #define COLOUR_Y
        #define COLOUR_B
        #define COLOUR_M
        #define COLOUR_C
        #define COLOUR_W
        #define COLOUR_NORM_R
        #define COLOUR_NORM_G
        #define COLOUR_NORM_Y
        #define COLOUR_NORM_B
        #define COLOUR_NORM_M
        #define COLOUR_NORM_C
        #define COLOUR_NORM_W
        #define COLOUR_RESET
    #endif
#endif

#define RXOS_SEL4_DEBUGPRINT_BUFFER_MAXLEN 512

/*! @brief Debug printf that uses seL4_DebugPutChar, and not dependent on c-library printf.

    This debugging function is useful for outputting debugging information beforely using
    seL4_DebugPutChar() without going through stdio; which is convienient for when stdio is not yet
    set up. Requires debug kernel to be enabled, does nothing on release builds.

    @param fmt The printf format string.
*/
static inline void seL4_DebugPrintf(char *fmt, ...) {
#if defined(SEL4_DEBUG_KERNEL)
    static char buf[RXOS_SEL4_DEBUGPRINT_BUFFER_MAXLEN];
    buf[0] = '\0';
    
    va_list varListParser;
    va_start(varListParser, fmt);
      vsprintf(buf, fmt, varListParser);
    va_end(varListParser);

    buf[RXOS_SEL4_DEBUGPRINT_BUFFER_MAXLEN - 1] = '\0';
    uint32_t len = strlen(buf);
    for (uint32_t i = 0; i < len; i++) {
        seL4_DebugPutChar(buf[i]);
    }
#endif
}

extern uint32_t faketime() __attribute__((weak)); 
extern const char* dprintfServerName;
extern int dprintfServerColour;

// Enable verbose debugging output.
#ifdef CONFIG_REFOS_DEBUG_VERBOSE
    #define DLOG_VERBOSE 1
#else 
    #define DLOG_VERBOSE 0
#endif

#define ROS_ERROR(...) printf(COLOUR "ERROR" COLOUR_RESET " %s(): ", \
    31, __FUNCTION__); printf(__VA_ARGS__); printf("\n");

#define ROS_WARNING(...) printf(COLOUR "WARNING" COLOUR_RESET " %s(): ", \
    33, __FUNCTION__); printf(__VA_ARGS__); printf("\n");

#ifdef CONFIG_REFOS_DEBUG
    #define dprintf(...) printf("[00.%u] " COLOUR "%s | " \
        COLOUR_RESET " %s(): ", \
        faketime ? faketime() : 0, \
        dprintfServerColour, dprintfServerName, __FUNCTION__); \
        printf(__VA_ARGS__);
#else
    #define dprintf(...)
#endif

#if DLOG_VERBOSE
    #define dvprintf dprintf
#else
    #define dvprintf(...)
#endif

/*! @brief Print seL4 error information.
    @param error A seL4 error code.
 */
static inline void
print_seL4_error(int error)
{
    printf("--- SEL4 ERROR INFO ---\n");
    const char *label_str = NULL;

    switch(error) {
    case 0: label_str = "No_error"; break;
    case seL4_InvalidArgument: label_str = "seL4_InvalidArgument"; break;
    case seL4_InvalidCapability: label_str = "seL4_InvalidCapability"; break;
    case seL4_IllegalOperation: label_str = "seL4_IllegalOperation"; break;
    case seL4_RangeError: label_str = "seL4_RangeError"; break;
    case seL4_AlignmentError: label_str = "seL4_AlignmentError"; break;
    case seL4_FailedLookup: label_str = "seL4_FailedLookup"; break;
    case seL4_DeleteFirst: label_str = "seL4_DeleteFirst"; break;
    case seL4_RevokeFirst: label_str = "seL4_RevokeFirst"; break;
    case seL4_NotEnoughMemory: label_str = "seL4_NotEnoughMemory"; break;
    default: label_str = "seL4_Unknown"; break;
    }

    (void) label_str;
    printf("%s (%d)\n", label_str, error);
    for (int a = 0; a < 4; a++) {
        printf("IPCBuffer[%d] = %d\n", a, seL4_GetMR(a));
    }
}

#endif /* _REFOS_DEBUG_ */
