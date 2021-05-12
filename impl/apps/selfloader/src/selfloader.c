/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*! @file
    @brief Process selfloader system application for RefOS.

   Selfloader is a tiny system application that starts in the same address space as the application
   being boosted. Selfloader then acts on behalf of the starting process, setting up its vspace and
   loading the ELF segments before jumping into the entry point of the process.  In order to avoid
   vspace address conflicts with the booting process, the Sselfloader process linker script compiles
   it into a very high RefOS reserved region, away from the vspace of the booting process.

   See \ref Startup
*/

#include <string.h>
#include "selfloader.h"

#include <elf/elf.h>
#include <refos/refos.h>
#include <refos/share.h>
#include <refos/vmlayout.h>
#include <refos-util/init.h>
#include <refos-util/walloc.h>
#include <refos-io/morecore.h>
#include <refos-io/stdio.h>
#include <utils/arith.h>

/*! Global self-loader state. */
struct sl_state selfloaderState;
const char* dprintfServerName = "SELFLOADER";
int dprintfServerColour = 36;

/*! A little mini morecore region for the selfloader, since the libraries selfloader use would
    like to have malloc() available. */
static char slMiniMorecoreRegion[SELFLOADER_MINI_MORECORE_REGION_SIZE];

/*! The maximum length of the ELF header we support. */
#define SELFLOADER_ELF_HEADER_SIZE REFOS_PAGE_SIZE

/*! @brief Selfloader's system call table. */
extern uintptr_t __vsyscall_ptr;

/*! @brief The spawned process uses this system call table */
extern long (*syscall_table[])(va_list);

// TODO(xmc): standardise this with a macro.
/*! Helper function to round up to the next page boundary. */
static int
sl_roundup_page(int addr)
{
    return REFOS_PAGE_ALIGN(addr) + ((addr) % REFOS_PAGE_SIZE ? REFOS_PAGE_SIZE : 0);
}

/*! @brief Open the ELF header dataspace, and map it so we can read it.
   @param fsSession Resolved mount point and session of ELF header dataspace.
   @return ESUCCESS on success, refos_err_t otherwise.
 */
static int
sl_setup_elf_header(serv_connection_t* fsSession)
{
    assert(fsSession && fsSession->serverSession);
    assert(fsSession->serverMountPoint.success);

    dvprintf("Opening file [%s] on mountpoint [%s]..\n",
            fsSession->serverMountPoint.dspaceName,
            fsSession->serverMountPoint.nameservPathPrefix);

    selfloaderState.elfFileHeader = data_open_map(fsSession->serverSession,
            fsSession->serverMountPoint.dspaceName, 0, 0, SELFLOADER_ELF_HEADER_SIZE, -1);
    if (selfloaderState.elfFileHeader.err != ESUCCESS) {
        ROS_ERROR("sl_setup_elf_header failed to open file.");
        return selfloaderState.elfFileHeader.err;
    }
    return ESUCCESS;
}

/*! @brief Helper function to map out a zero vspace segment. Simply opens an anonymous dataspace
          and maps it at the right vaddr.
   @param start The start / base of the zero segment region window.
   @param size The size of the zero segment dataspace.
   @param windowSize The size of zero segment region window. Could be slightly different to the
                     dataspace region, due to page alignment.
   @param out[out] Optional output struct to store windows & dataspace capabilities. If this is
                   set to NULL, the created window and dataspace caps are simply used then
                   discarded.
   @return ESUCCESS on success, refos_err_t otherwise.
 */
static int
sl_create_zero_segment(seL4_Word start, seL4_Word size, seL4_Word windowSize, sl_dataspace_t *out)
{
    int error = EINVALID;

    sl_dataspace_t *elfSegment = out;
    if (!elfSegment) {
        elfSegment = &selfloaderState.elfSegment;
    }
    assert(!elfSegment->dataspace);
    assert(!elfSegment->window);

    dprintf("Loading zero segment 0x%08x --> 0x%08x \n", start, start + windowSize);

    /* Open a anon ram dataspace on procserv. */
    dvprintf("    Creating zero segment dataspace ...\n");
    elfSegment->dataspace = data_open(REFOS_PROCSERV_EP, "anon", 0, 0, size, &error);
    if (error != ESUCCESS) {
        ROS_ERROR("Failed to open zero segment anon dspace.");
        return error;
    }

    /* Create the zero-initalised window for this segment. */
    dvprintf("    Creating zero segment window ...\n");
    elfSegment->window = proc_create_mem_window(start, windowSize);
    if (!elfSegment->window || ROS_ERRNO() != ESUCCESS) {
        ROS_ERROR("Failed to create ELF zero segment window.");
        return error;
    }

    /* Map the zero dataspace into the window. */
    dvprintf("    Data mapping zero segment ...\n");
    error = data_datamap(REFOS_PROCSERV_EP, elfSegment->dataspace, elfSegment->window, 0);
    if (error != ESUCCESS) {
        ROS_ERROR("Failed to datamap ELF zero segment.");
        return error;
    }

    elfSegment->vaddr = (uint32_t) start;
    elfSegment->size = (uint32_t) windowSize;
    if (!out) {
        /* Delete our caps as we no longer need them. */
        csfree_delete(elfSegment->dataspace);
        csfree_delete(elfSegment->window);
        elfSegment->dataspace = 0;
        elfSegment->window = 0;
        elfSegment->vaddr = 0;
        elfSegment->size = 0;
    }

    return ESUCCESS;
}

/*! @brief Load an ELF segment region into current vspace.
   @param si The ELF segment infor structure, read from the ELF header.
   @param fsSession The dataserver session containing ELF file contents.
   @return ESUCCESS on success, refos_err_t otherwise.
*/
static int
sl_elf_load_region(struct sl_elf_segment_info si, serv_connection_t* fsSession)
{
    int error;
    assert(fsSession);

    dprintf("Loading elf segment 0x%08x --> 0x%08x \n",
            (unsigned int)si.vaddr, (unsigned int)(si.vaddr + si.segmentSize));

    data_mapping_t *elfFile = &selfloaderState.elfFileHeader;
    sl_dataspace_t *elfSegment = &selfloaderState.elfSegment;
    assert(elfFile->err == ESUCCESS && elfFile->vaddr != NULL);
    assert(elfSegment->dataspace == 0 && elfSegment->window == 0);

    /* Calculate the page-alignment correction offset. */
    seL4_Word alignCorrectionOffset = si.vaddr - REFOS_PAGE_ALIGN(si.vaddr);
    if (alignCorrectionOffset > si.source) {
        ROS_ERROR("Invalid elf segment vaddr.");
        return EINVALID;
    }

    /* Open an anon ram dataspace on procserv. */
    dvprintf("    Opening dataspace...\n");
    elfSegment->dataspace = data_open(REFOS_PROCSERV_EP, "anon", 0, 0, si.fileSize, &error);
    if (error != ESUCCESS) {
        ROS_ERROR("Failed to open ELF segment anon dataspace.");
        return error;
    }

    /* Initialise segment content with ELF content. */
    dvprintf("    Initialising dataspace contents...\n");
    error = data_init_data(fsSession->serverSession, elfSegment->dataspace, elfFile->dataspace,
                           si.source - alignCorrectionOffset);
    if (error) {
        ROS_ERROR("Failed to init data for ELF segment.");
        return error;
    }

    /* Calculate the page-aligned window end position. */
    int windowEnd = sl_roundup_page(si.vaddr + si.fileSize);
    assert(windowEnd > si.vaddr && si.vaddr + si.fileSize <= windowEnd);
    int windowSize = windowEnd - si.vaddr;
    assert(((si.vaddr + windowSize) % REFOS_PAGE_SIZE) == 0);

    /* Create the file-initialised window for this data initialised segment anon dspace. */
    dvprintf("    Creating memory window ...");
    elfSegment->window = proc_create_mem_window(REFOS_PAGE_ALIGN(si.vaddr), windowSize);
    if (!elfSegment->window || ROS_ERRNO() != ESUCCESS) {
        ROS_ERROR("Failed to create ELF segment window.");
        return ROS_ERRNO();
    }

    /* Map the correct region of the file into this segment. */
    dvprintf("    Mapping dataspace ...\n");
    error = data_datamap(REFOS_PROCSERV_EP, elfSegment->dataspace, elfSegment->window, 0);
    if (error) {
        ROS_ERROR("Failed to datamap ELF initialised segment anon dataspace.");
        return error;
    }

    /* Clean up the capabilities to this segment so we can re-use the structure. */
    csfree_delete(elfSegment->window);
    csfree_delete(elfSegment->dataspace);
    elfSegment->window = 0;
    elfSegment->dataspace = 0;

    if (si.segmentSize < si.fileSize) {
        ROS_ERROR("Invalid segment size in ELF file. Check for corruption.");
        return EINVALID;
    }
    if (windowSize >= si.segmentSize) {
        return ESUCCESS;
    }

    /* Fill out the remaining un-initialised portion of the segment with a zero dataspace. */
    seL4_Word zeroSegment = si.segmentSize - windowSize;
    assert(zeroSegment > 0);
    zeroSegment = sl_roundup_page(zeroSegment);
    error = sl_create_zero_segment(si.vaddr + windowSize, zeroSegment, zeroSegment, NULL);
    if (error) {
        return error;
    }

    return ESUCCESS;
}

/*! @brief Load given ELF file into the current vspace.
    @param file Pointer to mapped ELF header contents.
    @param fsSession The dataserver session containing the ELF tile.
    @return ESUCCESS on success, refos_err_t otherwise.
*/
static int
sl_elf_load(char *file, serv_connection_t* fsSession)
{
    int error;
    assert(fsSession);

    /* Ensure that the ELF file looks sane. */
    error = elf_checkFile(file);
    if (error) {
        ROS_ERROR("ELF header check error.");
        return error;
    }

    /* Look at header and loop through all segments. */
    struct sl_elf_segment_info si = {};
    int numHeaders = elf_getNumProgramHeaders(file);
    for (int i = 0; i < numHeaders; i++) {
        /* Skip non-loadable segments (such as debugging data). */
        if (elf_getProgramHeaderType(file, i) != PT_LOAD) {
            continue;
        }

        /* Fetch information about this segment. */
        si.source = elf_getProgramHeaderOffset(file, i);
        si.fileSize = elf_getProgramHeaderFileSize(file, i);
        si.segmentSize = elf_getProgramHeaderMemorySize(file, i);
        si.vaddr = elf_getProgramHeaderVaddr(file, i);
        si.flags = elf_getProgramHeaderFlags(file, i);

        error = sl_elf_load_region(si, fsSession);
        if (error) {
            ROS_ERROR("Failed to load ELF region.");
            return error;
        }
    }

    /* Save the end of the program. */
    selfloaderState.endOfProgram = si.vaddr + si.segmentSize;
    selfloaderState.heapRegionStart = REFOS_PAGE_ALIGN(selfloaderState.endOfProgram + 0x2800);

    /* Create and map zeroed stack segment. */
    error = sl_create_zero_segment(PROCESS_STACK_BOT, PROCESS_RLIMIT_STACK, PROCESS_RLIMIT_STACK,
                                   &selfloaderState.stackRegion);
    if (error) {
        return error;
    }

    /* Create and map zeroed heap segment. */
    error = sl_create_zero_segment(selfloaderState.heapRegionStart, PROCESS_HEAP_INITIAL_SIZE,
                                   PROCESS_HEAP_INITIAL_SIZE, &selfloaderState.heapRegion);
    if (error) {
        return error;
    }

    return ESUCCESS;
}

/*! @brief Load boot & ELF information into static address.
   Load information about the process and its ELF regions into the pre-allocated static
   buffer, for the process to read after we jump into it. This done so the process knows where
   its own ELF segment ends, and where its heap region can start.
*/
static void
sl_setup_bootinfo_buffer(void)
{
    struct sl_procinfo_s *procInfo = refos_static_param_procinfo();
    procInfo->magic = SELFLOADER_PROCINFO_MAGIC;
    procInfo->elfSegmentEnd = (uint32_t) selfloaderState.endOfProgram;
    procInfo->heapRegion = selfloaderState.heapRegion;
    procInfo->stackRegion = selfloaderState.stackRegion;
}

/*! @brief Push onto the stack.
   @param stack_top The current top of the stack.
   @param buf The buffer to be pushed onto the stack.
   @param n The size of the buffer to be pushed onto the stack.
   @return The new top of the stack.
*/
static uintptr_t stack_write(uintptr_t stack_top, void *buf, size_t n)
{
    uintptr_t new_stack_top = stack_top - n;
    memcpy((void*)new_stack_top, buf, n);
    return new_stack_top;
}

/*! @brief Push a constant onto the stack.
   @param stack_top The current top of the stack.
   @param value The value to be pushed onto the stack.
   @return The new top of the stack.
*/
static uintptr_t stack_write_constant(uintptr_t stack_top, long value) {
    return stack_write(stack_top, &value, sizeof(value));
}

/*! @brief Push arguments onto the stack.
   @param stack_top The current top of the stack.
   @param data_len The number of arguments to push to the stack.
   @param dest_data Pointer to the arguments to be pushed to the stack.
   @param dest_data When the function returns contains each of the arguments to be pushed.
   @return The new top of the stack.
*/
static uintptr_t stack_copy_args(uintptr_t stack_top, int data_len, char *data[], uintptr_t *dest_data) {
    uintptr_t new_stack_top = stack_top;
    for (int i = 0; i < data_len; i++) {
        new_stack_top = stack_write(new_stack_top, data[i], strlen(data[i]) + 1);
        dest_data[i] = new_stack_top;
        new_stack_top = ROUND_DOWN(new_stack_top, 4);
    }
    return new_stack_top;
}

/*! @brief Initialise the stack for how musllibc expects it.
   @param elf The mapped ELF header containing entry point to jump into.
   @param stack_top The current top of the stack.
   @return The new top of the stack.
*/
static uintptr_t system_v_init(char *elf, uintptr_t stack_top)
{
    uintptr_t sysinfo = __vsyscall_ptr;
    void *ipc_buffer = seL4_GetIPCBuffer();

    char ipc_buf_env[ENV_STR_SIZE];
    snprintf(ipc_buf_env, ENV_STR_SIZE, "IPCBUFFER=%p", ipc_buffer);

    /* Future Work 3:
       How the selfloader bootstraps user processes needs to be modified further. Changes were
       made to accomodate the new way that muslc expects process's stacks to be set up when
       processes start, but the one part of this that still needs to changed is how user processes
       find their system call table. Currently the selfloader sets up user processes so that
       the selfloader's system call table is used by user processes by passing the address of the
       selfloader's system call table to the user processes via the user process's environment
       variables. Ideally, user processes would use their own system call table.
    */

    char system_call_table_env[ENV_STR_SIZE];
    snprintf(system_call_table_env, ENV_STR_SIZE, "SYSTABLE=%x", (unsigned int) syscall_table);

    char *envp[] = {ipc_buf_env, system_call_table_env};
    int envc = 2;
    uintptr_t dest_envp[envc];
    Elf_auxv_t auxv[] = { {.a_type = AT_SYSINFO, .a_un = {sysinfo} } };

    stack_top = stack_copy_args(stack_top, envc, envp, dest_envp);

    size_t stack_size = 5 * sizeof(seL4_Word) + /* constants */
                        sizeof(dest_envp) + /* aux */
                        sizeof(auxv[0]); /* env */

    uintptr_t future_stack_top = stack_top - stack_size;
    uintptr_t rounded_future_stack_top = ROUND_DOWN(future_stack_top, sizeof(seL4_Word) * 2);
    ptrdiff_t stack_rounding = future_stack_top - rounded_future_stack_top;
    stack_top -= stack_rounding;

    /* NULL terminate aux */
    stack_top = stack_write_constant(stack_top, 0);
    stack_top = stack_write_constant(stack_top, 0);

    /* Write aux */
    stack_top = stack_write(stack_top, auxv, sizeof(auxv[0]));

    /* NULL terminate env */
    stack_top = stack_write_constant(stack_top, 0);

    /* Write env */
    stack_top = stack_write(stack_top, dest_envp, sizeof(dest_envp));

    /* NULL terimnate args (we don't have any) */
    stack_top = stack_write_constant(stack_top, 0);

    /* Write argument count (we don't have any args) */
    stack_top = stack_write_constant(stack_top, 0);

    return stack_top;
}

/*! @brief Jumps into loaded ELF program in current vspace.
   @param file The mapped ELF header containing entry point to jump into.
 */
static inline void
sl_elf_start(char *file)
{
    seL4_Word entryPoint = elf_getEntryPoint(file);
    seL4_Word stackPointer = PROCESS_STACK_BOT + PROCESS_RLIMIT_STACK;

    /* Future Work 3:
       How the selfloader bootstraps user processes needs to be modified further. Changes were
       made to accomodate the new way that muslc expects process's stacks to be set up when
       processes start, but the one part of this that still needs to changed is how user processes
       find their system call table. Currently the selfloader sets up user processes so that
       the selfloader's system call table is used by user processes by passing the address of the
       selfloader's system call table to the user processes via the user process's environment
       variables. Ideally, user processes would use their own system call table.
    */

    stackPointer = system_v_init(file, stackPointer);

    dprintf("=============== Jumping into ELF program ==================\n");

    #ifdef ARCH_ARM
        asm volatile ("mov sp, %0" :: "r" (stackPointer));
        asm volatile ("mov pc, %0" :: "r" (entryPoint));
    #elif defined(ARCH_IA32)
        asm volatile ("movl %0, %%esp" :: "r" (stackPointer));
        asm volatile ("jmp %0 " :: "r" (entryPoint));
    #else
        #error "Unknown architecture."
    #endif /* ARCH_ARM */
}

/*! @brief Fake time generator used by dprintf(). */
uint32_t
faketime() {
    static uint32_t faketime = 0;
    return faketime++;
}

/*! @brief Selfloader main function. */
int
main(void)
{
    /* Future Work 4:
       Eventually RefOS should be changed so that processes that are started
       by the process server do not require that the their system call table be
       explicitly referenced in the code like this. Without expliciting referencing
       __vsyscall_ptr in main(), the compiler optimizes away __vsyscall_ptr
       and then processes started by the process server can't find their system call
       table. Each of the four places in RefOS where this explicit reference is
       required is affected by a custom linker script (linker.lds), so it is possible
       that the custom linker script (and then likely other things too) needs to be
       modified. Also note that the ROS_ERROR() and assert() inside this if statement
       would not actually be able to execute if __vsyscall_ptr() were ever not set.
       The purpose of these calls to ROS_ERROR() and assert() is to show future
       developers that __vsyscall_ptr needs to be defined.
    */
    if (! __vsyscall_ptr) {
        ROS_ERROR("Selfloader could not find system call table.");
        assert("!Selfloader could not find system call table.");
        return 0;
    }

    refos_seL4_debug_override_stdout();
    dprintf(COLOUR_C "--- Starting RefOS process selfloader ---\n" COLOUR_RESET);
    refosio_setup_morecore_override(slMiniMorecoreRegion, SELFLOADER_MINI_MORECORE_REGION_SIZE);
    refos_initialise_selfloader();
    int error = 0;

    /* Sanity check the given ELF file path. */
    char *filePath = refos_static_param();
    if (!filePath || strlen(filePath) == 0) {
        ROS_ERROR("Invalid file path.");
        return EINVALIDPARAM;
    }

    /* Connect to the file server. */
    dprintf("    Connect to the server for [%s]\n", filePath);
    selfloaderState.fileservConnection = serv_connect(filePath);
    if (selfloaderState.fileservConnection.error != ESUCCESS) {
        ROS_ERROR("Error while connecting to file server.\n");
        return error;
    }

    /* Now map the ELF file header for reading. */
    dprintf("    Mapping the ELF header for reading... [%s]\n", filePath);
    error = sl_setup_elf_header(&selfloaderState.fileservConnection);
    if (error) {
        ROS_ERROR("Failed to open ELF header.")
        return error;
    }

    /* Read and load the sections of the ELF file. */
    error = sl_elf_load(selfloaderState.elfFileHeader.vaddr, &selfloaderState.fileservConnection);
    if (error) {
        return error;
    }

    /* We don't need the file server session any more. */
    serv_disconnect(&selfloaderState.fileservConnection);

    /* Set up bootinfo and jump into ELF entry! */
    sl_setup_bootinfo_buffer();
    sl_elf_start(selfloaderState.elfFileHeader.vaddr);

    ROS_ERROR("ERROR: Should not ever be here!\n");
    assert(!"Something is wrong. Should not be here.");
    while(1);

    return 0;
}
