/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "state.h"
#include "common.h"
#include <simple/simple.h>
#include <sel4platsupport/platsupport.h>
#include <autoconf.h>
#include <refos/refos.h>
#include <refos-rpc/rpc.h>

#include <syscall_stubs_sel4.h>
MUSLC_SYSCALL_TABLE;

/*! @file
    @brief Global statuc struct & helper functions for process server. */

#define PROCSERV_IRQ_HANDLER_HASHTABLE_SIZE 32
#ifndef CONFIG_PROCSERV_INITIAL_MEM_SIZE
    #define CONFIG_PROCSERV_INITIAL_MEM_SIZE (4096 * 32)
#endif

static char _procservInitialMemPool[CONFIG_PROCSERV_INITIAL_MEM_SIZE];
struct procserv_state procServ;
const char* dprintfServerName = "PROCSERV";
int dprintfServerColour = 32;

uint32_t faketime() {
    return procServ.faketime++;
}

static void procserv_nameserv_callback_free_cap(seL4_CPtr cap);

/*! @brief display a heartwarming welcome message.
    @param info The Bootinfo structure.
*/
static void
initialise_welcome_message(seL4_BootInfo *info)
{
    dprintf("================= RefOS Version 2.0 =================\n");
    dprintf("  Built on "__DATE__" "__TIME__".\n");
    dprintf("  © Copyright 2014, NICTA\n");
    dprintf("=====================================================\n");
    

    /* Print boot info from kernel. */
    dprintf("Node ID: %d (of %d)\n",info->nodeID, info->numNodes);
    dprintf("initThreadCNode size: %d slots\n", (1 << info->initThreadCNodeSizeBits) );
    dprintf("initMemPool size: %d bytes (%d pages)\n", CONFIG_PROCSERV_INITIAL_MEM_SIZE,
            (CONFIG_PROCSERV_INITIAL_MEM_SIZE / REFOS_PAGE_SIZE));
    
    dprintf("\n");
    dprintf("--- Capability Details ---\n");
    dprintf("Type              Start      End\n");
    dprintf("Empty             0x%08x 0x%08x\n", info->empty.start, info->empty.end);
    dprintf("Shared frames     0x%08x 0x%08x\n", info->sharedFrames.start, info->sharedFrames.end);
    dprintf("User image frames 0x%08x 0x%08x\n", info->userImageFrames.start,
            info->userImageFrames.end);
    dprintf("User image PTs    0x%08x 0x%08x\n", info->userImagePTs.start, info->userImagePTs.end);
    dprintf("Untypeds          0x%08x 0x%08x\n", info->untyped.start, info->untyped.end);
    dprintf("Device Regions    0x%08x 0x%08x\n", info->deviceUntyped.start, info->deviceUntyped.end);
    
    dprintf("\n");
    dprintf("--- Untyped Details ---\n");
    dprintf("Untyped Slot       Paddr      Bits\n");
    int numUntyped = info->untyped.end - info->untyped.start;
    for (int i = 0; i < numUntyped; i++) {
        dprintf("%3x     0x%08x 0x%08x %d\n", i, info->untyped.start+i, info->untypedPaddrList[i],
                info->untypedSizeBitsList[i]);
    }
    
    dprintf("\n");
    dprintf("--- Device Regions: %d ---\n", info->deviceUntyped.end - info->deviceUntyped.start);
    dprintf("CSlot \t Device Addr \t Size\n");
    int numDevices = info->deviceUntyped.end - info->deviceUntyped.start;
    for (int i = 0; i < numDevices; i++) {
        dprintf("%3x \t 0x%08x \t %d\n", info->deviceUntyped.start + i,
                info->untypedPaddrList[numUntyped + i], info->untypedSizeBitsList[numUntyped + i]);
    }

}

/*! @brief Initialises the kernel object allocator.
    @param info The BootInfo struct passed in from the kernel.
    @param s The process server global state.
 */
static void
initialise_allocator(seL4_BootInfo *info, struct procserv_state *s)
{
    assert(info && s);
    int error = -1;
    memset(s, 0, sizeof(struct procserv_state));
    (void) error;

    /* Create and initialise allocman allocator, and create a virtual kernel allocator (VKA)
       interface from it. */
    s->allocman = bootstrap_use_bootinfo(info, CONFIG_PROCSERV_INITIAL_MEM_SIZE,
            _procservInitialMemPool);
    assert(s->allocman);
    allocman_make_vka(&s->vka, s->allocman);
    s->vkaPtr = &s->vka;

    /* Manage our own root server VSpace using this newly created allocator. */
    error = sel4utils_bootstrap_vspace_with_bootinfo_leaky(&s->vspace, &s->vspaceData,
            seL4_CapInitThreadPD, &s->vka, info);
    assert(!error);

    /* Initialise libsel4simple, which abstracts away which kernel version we are running on */
    #ifdef CONFIG_KERNEL_STABLE
        simple_stable_init_bootinfo(&s->simpleEnv, info);
    #else
        simple_default_init_bootinfo(&s->simpleEnv, info);
    #endif
}

/*! @brief Initialise the process server modules.
    @param s The process server global state.
 */
static void
initialise_modules(struct procserv_state *s)
{
    pd_init(&s->PDList);
    pid_init(&s->PIDList);
    w_init(&s->windowList);
    ram_dspace_init(&s->dspaceList);
    nameserv_init(&s->nameServRegList, procserv_nameserv_callback_free_cap);
}

/*! @brief Initialise the process server.
    @param info The BootInfo struct passed in from the kernel.
    @param s The process server global state to initialise.
 */
void
initialise(seL4_BootInfo *info, struct procserv_state *s)
{
    initialise_allocator(info, s);
    int error;
    (void) error;

    /* Enable printf and then print welcome message. */
    platsupport_serial_setup_simple(&s->vspace, &s->simpleEnv, &s->vka);
    initialise_welcome_message(info);

    /* Set up process server global objects. */
    dprintf("Allocating main process server endpoint...\n");
    error = vka_alloc_endpoint(&s->vka, &s->endpoint);
    assert(!error);

    /* Initialise recieving cslot. */
    dprintf("Setting recv cslot...\n");
    error = vka_cspace_alloc_path(&s->vka, &s->IPCCapRecv);
    assert(!error);
    rpc_setup_recv_cspace(s->IPCCapRecv.root, s->IPCCapRecv.capPtr, s->IPCCapRecv.capDepth);

    /* Initialise miscellaneous states. */
    dprintf("Initialising process server modules...\n");
    initialise_modules(s);
    chash_init(&s->irqHandlerList, PROCSERV_IRQ_HANDLER_HASHTABLE_SIZE);
    s->unblockClientFaultPID = PID_NULL;

    /* Procserv initialised OK. */
    dprintf("OK.\n");
    dprintf("==========================================\n\n");
}

cspacepath_t
procserv_mint_badge(int badge)
{
    cspacepath_t path, pathSrc;
    memset(&path, 0, sizeof(cspacepath_t));
    int error = vka_cspace_alloc_path(&procServ.vka, &path);
    if (error) {
        ROS_WARNING("procserv_mint_badge could not allocate a cslot.");
        return path;
    }
    vka_cspace_make_path(&procServ.vka, procServ.endpoint.cptr, &pathSrc);
    error = vka_cnode_mint(
            &path, &pathSrc, seL4_CanGrant | seL4_CanWrite,
            seL4_CapData_Badge_new(badge)
    );
    if (error) {
        ROS_WARNING("procserv_mint_badge could not mint endpoint cap.");
        vka_cspace_free(&procServ.vka, path.capPtr);
        memset(&path, 0, sizeof(cspacepath_t));
        return path;
    }
    return path;
}

int
procserv_frame_write(seL4_CPtr frame, const char* src, size_t len, size_t offset)
{
    if (offset + len > REFOS_PAGE_SIZE) {
        ROS_ERROR("procserv_frame_write invalid offset and length.");
        return EINVALIDPARAM;
    }
    char* addr = (char*) vspace_map_pages(&procServ.vspace, &frame, NULL, seL4_AllRights, 1,
                                          seL4_PageBits, true);
    if (!addr) {
        ROS_ERROR ("procserv_frame_write couldn't map frame.");
        return ENOMEM;
    }
    memcpy((void*)(addr + offset), (void*) src, len);
    procserv_flush(&frame, 1);
    vspace_unmap_pages(&procServ.vspace, addr, 1, seL4_PageBits, VSPACE_PRESERVE);
    return ESUCCESS;
}

int
procserv_frame_read(seL4_CPtr frame, const char* dst, size_t len, size_t offset)
{
    if (offset + len > REFOS_PAGE_SIZE) {
        ROS_ERROR("procserv_frame_read invalid offset and length.");
        return EINVALIDPARAM;
    }

    char* addr = (char*) vspace_map_pages(&procServ.vspace, &frame, NULL, seL4_AllRights, 1,
                                          seL4_PageBits, true);
    if (!addr) {
        ROS_ERROR ("procserv_frame_read couldn't map frame.");
        return ENOMEM;
    }
    procserv_flush(&frame, 1);
    memcpy((void*) dst, (void*)(addr + offset), len);
    vspace_unmap_pages(&procServ.vspace, addr, 1, seL4_PageBits, VSPACE_PRESERVE);
    return ESUCCESS;
}

/*! @brief The free EP cap callback function, used by the nameserv implementation helper library.
    @param cap The endpoint cap to free.
 */
static void
procserv_nameserv_callback_free_cap(seL4_CPtr cap)
{
    if (!cap) {
        ROS_WARNING("procserv_nameserv_callback_free_cap called on NULL cap!");
        return;
    }
    cspacepath_t path;
    vka_cspace_make_path(&procServ.vka, cap, &path);

    /* Name server service does not revoke the given anon caps, clients get to keep them. */
    vka_cnode_delete(&path);
    vka_cspace_free(&procServ.vka, cap);
}

cspacepath_t
procserv_find_device(void *paddr, int size)
{
    cspacepath_t path;
    path.capPtr = 0;

    /* Figure out device size in bits. */
    int sizeBits = -1;
    for (int i = 0; i < 32; i++) {
        if ((1 << i) == size) {
            sizeBits = i;
            break;
        }
    }
    if (sizeBits == -1) {
        ROS_ERROR("procserv_find_device invalid size 0x%x!\n", size);
        return path;
    }

    /* Allocate a cslot. */
    int error = vka_cspace_alloc_path(&procServ.vka, &path);
    if (error) {
        ROS_ERROR("procserv_find_device failed to allocate cslot.");
        path.capPtr = 0;
        return path;
    }

    /* Perform the device lookup. */
    error = simple_get_frame_cap(&procServ.simpleEnv, paddr, sizeBits, &path);
    if (error) {
        vka_cspace_free(&procServ.vka, path.capPtr);
        path.capPtr = 0;
        return path;
    }

    assert(path.capPtr);
    return path;
}

void
procserv_flush(seL4_CPtr *frame, int nFrames)
{
#ifdef CONFIG_ARCH_ARM
    if (!frame) {
        return;
    }
    for (int i = 0; i < nFrames; i++) {
        if (!frame[i]) {
            continue;
        }
        seL4_ARM_Page_Unify_Instruction(frame[i], 0, REFOS_PAGE_SIZE);
    }
#endif /* CONFIG_ARCH_ARM */
}

seL4_CPtr
procserv_get_irq_handler(int irq)
{
    /* Check whether we have already made a handler for this IRQ. */
    seL4_CPtr existingHandler = (seL4_CPtr) chash_get(&procServ.irqHandlerList, irq);
    if (existingHandler) {
        return existingHandler;
    }

    /* Allocate a new cslot to store the IRQ handler. */
    cspacepath_t handler;
    int error = vka_cspace_alloc_path(&procServ.vka, &handler);
    if (error) {
        ROS_WARNING("procserv_get_irq_handler could not allocate IRQ handler cslot.");
        return (seL4_CPtr) 0;
    }

    /* Get the handler. */
    error = seL4_IRQControl_Get(seL4_CapIRQControl, irq,
            handler.root, handler.capPtr, handler.capDepth);
    if (error) {
        ROS_WARNING("procserv_get_irq_handler could not get IRQ handler for irq %u.\n", irq);
        vka_cspace_free(&procServ.vka, handler.capPtr);
        return (seL4_CPtr) 0;
    }

    chash_set(&procServ.irqHandlerList, irq, (chash_item_t) handler.capPtr);

    return handler.capPtr;
}
