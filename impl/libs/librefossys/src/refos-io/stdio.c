/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <refos-io/stdio.h>
#include <refos-io/internal_state.h>
#include <refos-io/ipc_state.h>
#include <autoconf.h>

#define DPRINTF_SERVER_NAME ""
#include <refos-util/dprintf.h>

refos_io_internal_state_t refosIOState;
bool refos_stdio_translate_stdin_cr;

static size_t
refos_seL4_debug_override_writev(void *data, size_t count)
{
    #if defined(SEL4_DEBUG_KERNEL)
    for (int i = 0; i < count; i++) {
        seL4_DebugPutChar(((char*)data)[i]);
    }
    #endif
    return count;
}

void
refos_seL4_debug_override_stdout(void)
{
    refos_override_stdio(NULL, refos_seL4_debug_override_writev);
}

void
refos_override_stdio(stdio_read_fn_t readfn, stdio_write_fn_t writefn)
{
    refosIOState.stdioReadOverride = readfn;
    refosIOState.stdioWriteOverride = writefn;
}

void
refos_setup_dataspace_stdio(char *dspacePath)
{
#if defined(SEL4_DEBUG_KERNEL) && defined(CONFIG_REFOS_SYS_FORCE_DEBUGPUTCHAR)
    return;
#else
    /* Find the path and connect to it. */
    refosIOState.stdioSession = serv_connect_no_pbuffer(dspacePath);
    if (!refosIOState.stdioSession.error == ESUCCESS || !refosIOState.stdioSession.serverSession) {
        seL4_DebugPrintf("Failed to connect to [%s]. Error: %d %s.\n", dspacePath,
                refosIOState.stdioSession.error, refos_error_str(refosIOState.stdioSession.error));
        #if defined(SEL4_DEBUG_KERNEL)
        seL4_DebugHalt();
        #endif
        while (1);
    }

    /* Open the dataspace at the Resolved mount point. */
    int error;
    refosIOState.stdioDataspace = data_open(refosIOState.stdioSession.serverSession,
            refosIOState.stdioSession.serverMountPoint.dspaceName, 0, 0, 0, &error);
    if (error || !refosIOState.stdioDataspace) {
        seL4_DebugPrintf("Failed to open dataspace [%s].\n",
                refosIOState.stdioSession.serverMountPoint.dspaceName);
        #if defined(SEL4_DEBUG_KERNEL)
        seL4_DebugHalt();
        #endif
        while (1);
    }
#endif
}

int
refos_async_getc(void)
{
    if (!refosIOState.stdioDataspace || !refosIOState.stdioSession.serverSession) {
        seL4_DebugPrintf("refos_async_getc used without setting up stdin. Ignoring.\n");
        return -1;
    }
    int c = data_getc(refosIOState.stdioSession.serverSession, refosIOState.stdioDataspace, false);
    if (refos_stdio_translate_stdin_cr && c == '\r') {
        c = '\n';
    }
    return c;
}

int
refos_getc(void)
{
    if (!refosIOState.stdioDataspace || !refosIOState.stdioSession.serverSession) {
        seL4_DebugPrintf("refos_getc used without setting up stdin. Ignoring.\n");
        return -1;
    }
    int c = data_getc(refosIOState.stdioSession.serverSession, refosIOState.stdioDataspace, true);
    if (refos_stdio_translate_stdin_cr && c == '\r') {
        c = '\n';
    }
    return c;
}