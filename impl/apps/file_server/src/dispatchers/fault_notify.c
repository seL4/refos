/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <refos/refos.h>
#include <refos/error.h>
#include <refos/share.h>
#include <refos-rpc/proc_common.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>
#include <refos-util/serv_connect.h>
#include <utils/arith.h>

#include "dispatch.h"
#include "../state.h"
#include "../badge.h"

/*! @file
    @brief Handles process server client notifications.

    This module is responsible for handling any process server notifications, including client
    page faults (when acting as pager), dataspace content initialisation calls, and client
    death notification. Notifications use asynchronous one-way ring buffers, operating on top of
    an anonymous memory dataspace (ie. the notification buffer).
*/

/*! @brief Handles client page fault notifications.
    
    This function handles client page fault notifications from the process server. When we act as
    the pager, the process server delegates all page faults to us via this notification.
    We then choose a page to map, and map it.

    @param notification Structure containing the notification message, read from the notification
                        ring buffer.
    @return DISPATCH_SUCCESS if success, DISPATCHER_ERROR otherwise.
*/
static int
handle_fileserver_fault(struct proc_notification *notification)
{
    int error;

    dvprintf(COLOUR_M "## Fileserv Handling Notification VM fault delegation...\n" COLOUR_RESET);
    dvprintf("     Label: PROCSERV_NOTIFY_FAULT_DELEGATION\n");
    dvprintf("     winID: %d\n", notification->arg[0]);
    dvprintf("     win size: %d\n", notification->arg[1]);
    dvprintf("     fault address: 0x%x\n", notification->arg[2]);
    dvprintf("     window base: 0x%x\n", notification->arg[3]);
    dvprintf("     instruction: 0x%x\n", notification->arg[4]);
    dvprintf("     permission: 0x%x\n", notification->arg[5]);
    dvprintf("     pc: 0x%x\n", notification->arg[6]);

    seL4_Word winID = notification->arg[0];
    seL4_Word winSize = notification->arg[1];
    seL4_Word faultAddr = notification->arg[2];
    seL4_Word winBase = notification->arg[3];

    /* Look up the faulting window. */
    struct dataspace_association_info *dwa = dspace_window_find(&fileServ.dspaceTable, winID);
    if (!dwa) {
        ROS_ERROR("File Server does not know about this faulting window.");
        ROS_ERROR("  This paging delegation request will be ignored. This is most likely a bug.");
        ROS_ERROR("  Faulting client will be permanently blocked.");
        assert(!"handle_fileserver_fault bug.");
        return DISPATCH_SUCCESS;
    }

    /* Look up the dataspace associated with the faulting window. */
    struct fs_dataspace* dspace = dspace_get(&fileServ.dspaceTable, dwa->dataspaceID);
    if (!dspace) {
        ROS_ERROR("File Server could not find the dataspace associated with the faulting window.");
        ROS_ERROR("  This paging delegation request will be ignored. This is most likely a bug.");
        ROS_ERROR("  Faulting client will be permanently blocked.");
        assert(!"handle_fileserver_fault bug.");
        return DISPATCH_ERROR;
    }
    size_t faultAddrWinOffset = faultAddr - winBase;

    /* Allocate a frame to page client with. */
    vaddr_t pframe = pager_alloc_frame(&fileServ.pageFrameBlock);
    if (!pframe) {
        ROS_ERROR("File Server Out of memory handling VM fault. Paging not implemented.");
        ROS_ERROR("  Try increasing FILESERVER_MAX_PAGE_FRAMES.");
        ROS_ERROR("  Faulting client will be permanently blocked.");
        return DISPATCH_ERROR;
    }
    memset((void*) pframe, 0, REFOS_PAGE_SIZE);

    /* Copy any CPIO file content if there is data. */
    if (dspace->fileData) {
        /* Round faulting address down to page. */
        seL4_Word alignedFaultAddr = REFOS_PAGE_ALIGN(faultAddr);

        /* initFrameSkip is to compensate for the case where the window base is not page aligned,
           and the faulting address is the first page of the window, resulting in a region of
           page overlap that we can't avoid. Illustration:

                                      winBase   faultAddr
                                            ▼   ▼
                 |_______|_______|_______|__[―――◯|―――――――|―――――――]_______|_______|
                                         ▲  ◀――――― window ―――――――▶
                          alignedFaultAddr  
                                         ◀――▶ initFrameSkip = (winBase - alignedFaultAddr)
        */ 
        size_t initFrameSkip = (winBase > alignedFaultAddr) ? (winBase - alignedFaultAddr) : 0;

        /* dataspaceSkipWinOffset is for all cases except the (unsigned window base addr &
           first page fault) special case. Illustration:
           
                                      winBase                faultAddr
                                            ▼                ▼
                 |_______|_______|_______|__[――――|―――――――|―――◯―――]_______|_______|
                                         ▲  ◀――――― window ―――――――▶
                          alignedFaultAddr  
                                            ◀――――――――――――――――▶ dataspaceSkipWinOffset
                                              = (alignedFaultAddr - winBase)
        */ 
        size_t dataspaceSkipWinOffset = (winBase > alignedFaultAddr) ?
                0 : (alignedFaultAddr - winBase);

        /* nbytes is the number of bytes from the offset dataspace start that we should copy into
           the frame. There are 4 cases here:

            Case 1:
                 |_______|_______|_______|__[――――|―――――――|―――◯―――]_______|_______| 
                                            ◀――――▶ nbytes = (REFOS_PAGE_SIZE - initFrameSkip)

            Case 2:
                 |_______|_______|_______[―――――――|―――◯―――|―――――――]_______|_______| 
                                                 ◀―――――――▶ nbytes = REFOS_PAGE_SIZE

            Case 3:
                 |_______|_______|_______[―――――――|―――――――|―◯―××××]_______|_______| 
                                                         ◀―――▶ nbytes = fileDataSize -
                                                            dataspaceSkipWinOffset - dataspaceOffset
                        (× = indicates there is no more CPIO file data here)

            Case 3:
                 |_______|_______|_______[―――――――|―――――――|―◯――]__|_______|_______| 
                                                         ◀――――▶ nbytes =
                                                                winSize - dataspaceSkipWinOffset
        */
        size_t nbytes = MIN(REFOS_PAGE_SIZE - initFrameSkip,
                dspace->fileDataSize - dataspaceSkipWinOffset - dwa->dataspaceOffset);
        nbytes = MIN(nbytes, winSize - dataspaceSkipWinOffset);

        /* Check if nbytes is sane. */
        if ((dspace->fileDataSize - dataspaceSkipWinOffset - dwa->dataspaceOffset >
                dspace->fileDataSize) ||
            (dspace->fileDataSize - dataspaceSkipWinOffset > dspace->fileDataSize) ||
            (dspace->fileDataSize - dwa->dataspaceOffset > dspace->fileDataSize)) {
            ROS_ERROR("nbytes overflowed.\n");
            assert(!"nbytes overflowed. Fileserver bug.");
            pager_free_frame(&fileServ.pageFrameBlock, pframe);
            return DISPATCH_ERROR;
        }

        memcpy (
                (void*) (pframe + initFrameSkip),
                dspace->fileData + dwa->dataspaceOffset + dataspaceSkipWinOffset,
                nbytes
        );
    }

    /* Now map the frame into the client's vspace window. */
    dvprintf("    Mapping frame at  0x%x ―――▶ client 0x%x\n", (uint32_t) pframe,
            (uint32_t) faultAddr);
    error = proc_window_map(dwa->objectCap, faultAddrWinOffset, (seL4_Word) pframe);
    if (error) {
        ROS_ERROR("File Server Unexpected error while mapping frame!");
        ROS_ERROR("  Most likely a file server bug.");
        assert(!"proc_window_map error. Fileserver bug.");
        pager_free_frame(&fileServ.pageFrameBlock, pframe);
        return DISPATCH_ERROR;
    }

    dvprintf("    Successfully mapped frame...\n");
    return DISPATCH_SUCCESS;
}

/*! @brief Handles client content init notifications.

    Handles dataspace content init notifications from another dataserver. When we act as a data
    provider, the external dataserver (eg. process server for anon memory) notifies us with this
    notification for us to provide the content for it.
    We find the content that was asked for, and reply to the notification with data_provide_data.

    @param notification Structure containing the notification message, read from the notification
                        ring buffer.
    @return DISPATCH_SUCCESS if success, DISPATCHER_ERROR otherwise.
*/
static int
handle_fileserver_content_init(struct proc_notification *notification)
{
    dvprintf(COLOUR_M "## Fileserv Handling dspace content initialisation...\n" COLOUR_RESET);
    dvprintf("    Label: PROCSERV_NOTIFY_CONTENT_INIT\n");
    dvprintf("    dataID: %d\n", notification->arg[0]);
    dvprintf("    procDsOffset: %d\n", notification->arg[1]);
    dvprintf("    winOffset: %d\n", notification->arg[2]);

    seL4_Word dataID = notification->arg[0];
    seL4_Word destDataspaceOffset = notification->arg[1];

    /* Look up the dataspace --> dataspace association. */
    struct dataspace_association_info *dda = dspace_external_find(&fileServ.dspaceTable, dataID);
    if (!dda) {
        ROS_ERROR("File Server does not know about this content init dspace.");
        ROS_ERROR("  This paging delegation request will be ignored. This is most likely a bug.");
        ROS_ERROR("  Faulting client will be permanently blocked.");
        assert(!"handle_fileserver_fault bug.");
        return DISPATCH_ERROR;
    }

    struct fs_dataspace* dspace = dspace_get(&fileServ.dspaceTable, dda->dataspaceID);
    if (!dspace) {
        ROS_ERROR("File Server could not find the dataspace for content init.");
        ROS_ERROR("  This paging delegation request will be ignored. This is most likely a bug.");
        ROS_ERROR("  Faulting client will be permanently blocked.");
        assert(!"handle_fileserver_fault bug.");
        return DISPATCH_ERROR;
    }

    seL4_Word dataspaceOffset = destDataspaceOffset + dda->dataspaceOffset;

    /* Check the content size to copy. There are 2 cases here: either the size to copy ends
       with the next page boundary, or is cut short because we've ran out of file data. */
    size_t contentSize = MIN(dspace->fileDataSize - dataspaceOffset, REFOS_PAGE_SIZE);
    dvprintf("    Fault file source = 0x%x\n", (uint32_t) dataspaceOffset);

    /* Provide the data back to the process server who notified us. */
    assert(dataspaceOffset < dspace->fileDataSize);
    if (dspace->fileData) {
        int error = data_provide_data(
                REFOS_PROCSERV_EP, dda->objectCap,
                destDataspaceOffset, (char*)dspace->fileData + dataspaceOffset,
                contentSize, &fileServCommon->procServParamBuffer
        );
        if (error != ESUCCESS) {
            ROS_ERROR("File Server could not provide data.");
            ROS_ERROR("  Faulting client will be permanently blocked.");
            assert(!"handle_fileserver_fault bug.");
            return DISPATCH_ERROR;
        }
    }
    dvprintf("      Data ready. Content size is 0x%x\n", contentSize);
    dvprintf("      Successfully initialised content...\n");    

    return DISPATCH_SUCCESS;
}

/*! @brief Handles client death notifications.
    @param notification Structure containing the notification message, read from the notification
                        ring buffer.
    @return DISPATCH_SUCCESS if success, DISPATCHER_ERROR otherwise.
*/
static int
handle_fileserver_death_notification(struct proc_notification *notification)
{
    dprintf(COLOUR_M "## Fileserv Handling death notification...\n" COLOUR_RESET);
    dprintf("     Label: PROCSERV_NOTIFY_DEATH\n");
    dprintf("     deathID: %d\n", notification->arg[0]);

    /* Find the client and queue it for deletion. */
    client_queue_delete(&fileServCommon->clientTable, notification->arg[0]);
    return DISPATCH_SUCCESS;
}

int
dispatch_notification(srv_msg_t *m)
{
    if (m->badge != FS_ASYNC_NOTIFY_BADGE) {
        return DISPATCH_PASS;
    }

    srv_common_notify_handler_callbacks_t cb = {
        .handle_server_fault = handle_fileserver_fault,
        .handle_server_content_init = handle_fileserver_content_init,
        .handle_server_death_notification = handle_fileserver_death_notification
    };

    return srv_dispatch_notification(fileServCommon, cb);
}
