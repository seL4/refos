/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_IO_IPC_STATE_SAVE_RESTORE_H_
#define _REFOS_IO_IPC_STATE_SAVE_RESTORE_H_

#include <autoconf.h>
#include <sel4/sel4.h>
#include <stdlib.h>

#define IPC_SAVESTACK_MAXLEVELS 4
static seL4_IPCBuffer _refosioIPCBufferBackup[IPC_SAVESTACK_MAXLEVELS];
static uint32_t _refosioIPCBufferBackupStack = 0;

static inline void
refosio_internal_save_IPC_buffer(void)
{
    assert(_refosioIPCBufferBackupStack < IPC_SAVESTACK_MAXLEVELS);
    assert(_refosioIPCBufferBackupStack >= 0);
    seL4_IPCBuffer *ipcBuf = &_refosioIPCBufferBackup[_refosioIPCBufferBackupStack++];

#if defined(ARCH_ARM)
    memcpy(ipcBuf, seL4_GetIPCBuffer(), sizeof(seL4_IPCBuffer));
#elif defined(ARCH_IA32)
    seL4_Word *dest = (seL4_Word *) ipcBuf;
    for (int i = 0; i < sizeof(seL4_IPCBuffer) / sizeof(seL4_Word); i++) {
        #ifdef CONFIG_X86_64
            asm volatile ("movq %%gs:0(,%1,0x8), %0" : "=r"(*(dest + i)) : "r"(i) : "memory");
        #else
            asm volatile ("movl %%gs:0(,%1,0x4), %0" : "=r"(*(dest + i)) : "r"(i) : "memory");
        #endif
    }
#else
    #error "Unknown arch."
#endif
}

static inline void
refosio_internal_restore_IPC_buffer(void)
{
    assert(_refosioIPCBufferBackupStack <= IPC_SAVESTACK_MAXLEVELS);
    assert(_refosioIPCBufferBackupStack > 0);
    seL4_IPCBuffer *ipcBuf = &_refosioIPCBufferBackup[--_refosioIPCBufferBackupStack];

#if defined(ARCH_ARM)
    memcpy(seL4_GetIPCBuffer(), ipcBuf, sizeof(seL4_IPCBuffer));
#elif defined(ARCH_IA32)
    seL4_Word *src = (seL4_Word *) ipcBuf;
    for (int i = 0; i < sizeof(seL4_IPCBuffer) / sizeof(seL4_Word); i++) {
    #ifdef CONFIG_X86_64
            asm volatile ("movq %0, %%gs:0(,%1,0x8)" :: "r"(*(src + i)), "r"(i) : "memory");
    #else
            asm volatile ("movl %0, %%gs:0(,%1,0x4)" :: "r"(*(src + i)), "r"(i) : "memory");
    #endif
    }
#else
    #error "Unknown arch."
#endif
}

#endif /* _REFOS_IO_IPC_STATE_SAVE_RESTORE_H_ */