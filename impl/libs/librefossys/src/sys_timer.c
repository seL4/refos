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
#include <sel4/sel4.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <refos-io/timer.h>
#include <refos-io/internal_state.h>
#include <refos-io/ipc_state.h>
#include <refos-io/filetable.h>
#include <refos-util/dprintf.h>

void
refos_init_timer(char *dspacePath)
{
    assert(dspacePath);

    /* We can happily use the cstdio file interface here, as the file table should have
       been set up already. If not, then the user needs to simply set one up manually. */
    refosIOState.timerFD = fopen(dspacePath, "r+");
    if (!refosIOState.timerFD) {
        seL4_DebugPrintf("Could not initialise timer file.");
    }
}

long
sys_nanosleep(va_list ap)
{
    struct timespec  *req = va_arg(ap, struct timespec *);
    struct timespec  *rem = va_arg(ap, struct timespec *);

    if (req->tv_sec == 0 && req->tv_nsec == 0) {
        /* No sleep to perform. */
        return 0;
    }

    if (!refosIOState.timerFD) {
        assert(!"sys_nanosleep not supported");
        return -1;
    }

    uint64_t ns = (uint64_t) rem->tv_nsec;
    ns += rem->tv_sec * 1000000000UL;

    int res = fwrite(&ns, sizeof(uint64_t), 1, refosIOState.timerFD);
    fflush(refosIOState.timerFD);

    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return res < 0 ? -1 : 0;
}

long
sys_clock_gettime(va_list ap)
{
    clockid_t clk_id = va_arg(ap, clockid_t);
    struct timespec *tp = va_arg(ap, struct timespec *);
    if (!tp) {
        return -1;
    }
    if (clk_id == CLOCK_PROCESS_CPUTIME_ID || clk_id == CLOCK_THREAD_CPUTIME_ID) {
        seL4_DebugPrintf("WARNING: sys_clock_gettime CPU time feature not supported.\n");
        return -1;
    }
    if (!refosIOState.timerFD) {
        assert(!"sys_clock_gettime not supported");
        return -1;
    }

    uint64_t ns = 0;

    /* We directly use filetable_read interface here, to avoid buffering issues with using fread.
       This is a rather crude way of implementing clock_gettime. A far better way would be to
       datamap a dataspace as a shared buffer, then simply read off that.
    */
    int res = filetable_read(&refosIOState.fdTable, fileno(refosIOState.timerFD),
            (char*) &ns, sizeof(uint64_t));

    if (res >= sizeof(uint64_t)) {
        tp->tv_sec = ns / 1000000000UL;
        tp->tv_nsec = ns % 1000000000UL;
    } else {
        tp->tv_sec = 0;
        tp->tv_nsec = 0;
    }

    return res > 0 ? 0 : -1;
}
