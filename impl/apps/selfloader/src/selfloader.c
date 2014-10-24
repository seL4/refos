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
    @brief Process selfloader system application for RefOS.

   Selfloader is a tiny system application that starts in the same address space as the application
   being boosted. Selfloader then acts on behalf of the starting process, setting up its vspace and
   loading the ELF segments before jumping into the entry point of the process.  In order to avoid
   vspace address conflicts with the booting process, the Sselfloader process linker script compiles
   it into a very high RefOS reserved region, away from the vspace of the booting process.

   See \ref Bootup
*/

#include <string.h>
#include "selfloader.h"

#include <refos/refos.h>
#include <refos/share.h>
#include <refos/vmlayout.h>
#include <refos-util/init.h>
#include <refos-util/walloc.h>
#include <refos-io/morecore.h>
#include <refos-io/stdio.h>
#include <syscall_stubs_sel4.h>
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

/*! @brief Jumps into loaded ELF program in current vspace.
   @param file The mapped ELF header containing entry point to jump into.
 */
static inline void
sl_elf_start(char *file)
{
    dprintf("=============== Jumping into ELF program ==================\n");
    seL4_Word entryPoint = elf_getEntryPoint(file);
    seL4_Word stackPointer = PROCESS_STACK_BOT + PROCESS_RLIMIT_STACK;
    #ifdef ARCH_ARM
        asm volatile ("mov sp, %0" :: "r" (stackPointer));
        asm volatile ("mov pc, %0" :: "r" (entryPoint));
    #elif defined(ARCH_IA32)
        asm volatile ("movl %0, %%esp" :: "r" (stackPointer));
        void (*entryPointer)(void) = (void(*)(void)) entryPoint;
        entryPointer();
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
    SET_MUSLC_SYSCALL_TABLE;
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
