/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "dispatch.h"
#include "serv_dispatch.h"
#include "../state.h"
#include "../badge.h"
#include <sys/types.h>
#include <sys/stat.h> 
#include <utils/arith.h>
#include <fcntl.h>
#include <refos/error.h>
#include <refos-rpc/data_server.h>
#include <refos-rpc/data_client.h>
#include <refos-util/serv_connect.h>
#include <refos-util/serv_common.h>

/*! @file
    @brief Handles CPIO file server dataspace calls.

  This file contains the handlers for dataspace interface syscalls, and exposes the file server's
  CPIO contents through this interface. The methods here should implement the methods declared in
  refos-rpc/data_server.h.
*/

#define CPIO_RAMFS_MAX_CREATED_FILES 64
#define CPIO_RAMFS_MAX_FILESSIZE 40960
#define CPIO_RAMFS_MAX_FILENAME 32

/*! @brief Forward declaration of the CPIO archive.

    The CPIO archive is a simple format file archive stored inside a parent program's ELF section.
    This is a similar idea to something like creating a
    > const char data[] = { 0x3F, 0xFF, 0x23 ...etc}
*/
extern char _cpio_archive[];

/*! @brief Rather hacky minimal ramfs created files.
    
    This is a rather terrible hack to allow creation of writable files in CPIO fileserver as a sort
    of minimal crude RamFS. The functionality is nice for portability, although in the future a
    much better way to do this should be implemented.
*/
static char _ramfs_archive[CPIO_RAMFS_MAX_CREATED_FILES][CPIO_RAMFS_MAX_FILESSIZE];
static char _ramfs_filename[CPIO_RAMFS_MAX_CREATED_FILES][CPIO_RAMFS_MAX_FILENAME];
static int _ramfs_filesz[CPIO_RAMFS_MAX_CREATED_FILES];
static int _ramfs_curfile = 0; /* Incrementally allocated files. */

seL4_CPtr
data_open_handler(void *rpc_userptr , char* rpc_name , int rpc_flags , int rpc_mode , int rpc_size ,
                  int* rpc_errno)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    assert(c->magic == FS_CLIENT_MAGIC);

    if (!rpc_name) {
        SET_ERRNO_PTR(rpc_errno, EINVALIDPARAM);
        return 0;
    }

    /* Find file data in CPIO. */
    dprintf("Opening %s...\n", rpc_name);
    unsigned long fileDataSize = 0;
    char *fileData = cpio_get_file(_cpio_archive, rpc_name, &fileDataSize);
    bool fileCreated = false;

    if (fileData && (rpc_flags & O_ACCMODE) != O_RDONLY) {
        /* CPIO dataspaces require read only. */
        SET_ERRNO_PTR(rpc_errno, EACCESSDENIED);
        return 0;
    }

    if (!fileData) {
        for (int i = 0; i < _ramfs_curfile; i++) {
            if (!strcmp(rpc_name, _ramfs_filename[i])) {
                if ((rpc_flags & O_CREAT)) {
                    /* Re-create this file. */
                    memset(_ramfs_archive[i], 0, CPIO_RAMFS_MAX_FILESSIZE);
                    _ramfs_filesz[i] = 0;
                }
                fileData = _ramfs_archive[i];
                fileDataSize = _ramfs_filesz[i];
                fileCreated = true;
                break;
            }
        }
    }

    if (!fileData) {
        if ((rpc_flags & O_CREAT) == 0) {
            dprintf("File %s not found!\n", rpc_name);
            SET_ERRNO_PTR(rpc_errno, EFILENOTFOUND);
            return 0;
        }
        /* Assign new blank RAMFS file. */
        if (_ramfs_curfile >= CPIO_RAMFS_MAX_CREATED_FILES) {
            SET_ERRNO_PTR(rpc_errno, EACCESSDENIED);
            return 0;
        }
        dvprintf("Creating new file %s...\n", rpc_name);
        strncpy(_ramfs_filename[_ramfs_curfile], rpc_name, CPIO_RAMFS_MAX_FILENAME);
        fileData = _ramfs_archive[_ramfs_curfile++];
        fileDataSize = 0;
        fileCreated = true;
    }

    /* Allocate new dataspace structure. */
    struct fs_dataspace* nds = dspace_alloc(&fileServ.dspaceTable, c->deathID, fileData,
        (size_t) fileDataSize, O_RDONLY);
    if (!nds) {
        ROS_ERROR("data_open_handler failed to allocate dataspace.");
        SET_ERRNO_PTR(rpc_errno, ENOMEM);
        return 0;
    }
    nds->fileCreated = fileCreated;

    dvprintf("%s file %s OK ID %d...\n", fileCreated ? "Created" : "Opened", rpc_name, nds->dID);
    SET_ERRNO_PTR(rpc_errno, ESUCCESS);
    assert(nds->dataspaceCap);
    return nds->dataspaceCap;
}

refos_err_t
data_close_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c->magic == FS_CLIENT_MAGIC);

    /* Sanity check the dataspace cap. */
    if (seL4_MessageInfo_get_capsUnwrapped(m->message) != 0x00000001 ||
        seL4_MessageInfo_get_extraCaps(m->message) != 1) {
        dprintf("data_close_handler EINVALIDPARAM: bad caps.\n");
        return EINVALIDPARAM;
    }

    if (!dspace_get_badge(&fileServ.dspaceTable, rpc_dspace_fd)) {
        ROS_WARNING("data_close_handler: no such dataspace.");
        return EINVALIDPARAM;
    }

    dprintf("Closing dataspace ID %d...\n", rpc_dspace_fd- FS_DSPACE_BADGE_BASE);

    dspace_delete(&fileServ.dspaceTable, rpc_dspace_fd - FS_DSPACE_BADGE_BASE);
    return ESUCCESS;
}

int
data_read_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                  rpc_buffer_t rpc_buf , uint32_t rpc_count)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c->magic == FS_CLIENT_MAGIC);

    /* Sanity check the dataspace cap. */
    if (seL4_MessageInfo_get_capsUnwrapped(m->message) != 0x00000001 ||
        seL4_MessageInfo_get_extraCaps(m->message) != 1) {
        dprintf("data_read_handler EINVALIDPARAM: bad caps.\n");
        return EINVALIDPARAM;
    }

    struct fs_dataspace* dspace = dspace_get_badge(&fileServ.dspaceTable, rpc_dspace_fd);
    if (!dspace) {
        ROS_WARNING("data_read_handler: no such dataspace.");
        return 0;
    }
    assert(dspace->magic == FS_DATASPACE_MAGIC);
    assert(dspace->fileData);

    if (rpc_offset >= dspace->fileDataSize) {
        return 0;
    }
    uint32_t count = MIN(dspace->fileDataSize - rpc_offset, rpc_buf.count);
    memcpy(rpc_buf.data, dspace->fileData + rpc_offset, count);
    return count;
}

int
data_write_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_offset ,
                   rpc_buffer_t rpc_buf , uint32_t rpc_count)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c->magic == FS_CLIENT_MAGIC);

    /* Sanity check the dataspace cap. */
    if (seL4_MessageInfo_get_capsUnwrapped(m->message) != 0x00000001 ||
        seL4_MessageInfo_get_extraCaps(m->message) != 1) {
        dprintf("data_write_handler EINVALIDPARAM: bad caps.\n");
        return -EINVALIDPARAM;
    }

    struct fs_dataspace* dspace = dspace_get_badge(&fileServ.dspaceTable, rpc_dspace_fd);
    if (!dspace) {
        ROS_WARNING("data_write_handler: no such dataspace.");
        return 0;
    }
    assert(dspace->magic == FS_DATASPACE_MAGIC);
    assert(dspace->fileData);

    if (!dspace->fileCreated) {
        /* Tried to write to a read only CPIO file. */
        ROS_WARNING("data_write_handler: Tried to write to a read only CPIO file %d.", dspace->dID);
        return -EACCESSDENIED;
    }

    if (rpc_offset + rpc_buf.count > dspace->fileDataSize) {
        if (rpc_offset + rpc_buf.count > CPIO_RAMFS_MAX_FILESSIZE) {
            assert(!"File maxsize overflow.");
            return -ENOMEM;
        }
        dspace->fileDataSize = rpc_offset + rpc_buf.count;
    }
    for (int i = 0; i < _ramfs_curfile; i++) {
        if (_ramfs_archive[i] == dspace->fileData) {
            _ramfs_filesz[i] = dspace->fileDataSize;
            break;
        }
    }
    memcpy(dspace->fileData + rpc_offset, rpc_buf.data, rpc_buf.count);
    return rpc_buf.count;
}

int
data_getc_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , int rpc_block)
{
    assert(!"data_getc_handler not implemented");
    return EUNIMPLEMENTED;
}

off_t
data_lseek_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , off_t rpc_offset , int rpc_whence)
{
    assert(!"data_lseek_handler not implemented");
    return 0;
}

uint32_t
data_get_size_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c->magic == FS_CLIENT_MAGIC);

    /* Sanity check the dataspace cap. */
    if (seL4_MessageInfo_get_capsUnwrapped(m->message) != 0x00000001 ||
        seL4_MessageInfo_get_extraCaps(m->message) != 1) {
        dprintf("data_get_size_handler EINVALIDPARAM: bad caps.\n");
        return 0;
    }

    struct fs_dataspace* dspace = dspace_get_badge(&fileServ.dspaceTable, rpc_dspace_fd);
    if (!dspace) {
        ROS_WARNING("data_get_size_handler: no such dataspace.");
        return 0;
    }
    assert(dspace->magic == FS_DATASPACE_MAGIC);

    return (uint32_t) dspace->fileDataSize;
}

refos_err_t
data_expand_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , uint32_t rpc_size)
{
    return EUNIMPLEMENTED;
}

refos_err_t
data_datamap_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , seL4_CPtr rpc_memoryWindow ,
                     uint32_t rpc_offset)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c->magic == FS_CLIENT_MAGIC);

    if (seL4_MessageInfo_get_extraCaps(m->message) != 2 ||
        !(seL4_MessageInfo_get_capsUnwrapped(m->message) & 0x00000001)) {
        return EINVALIDPARAM;
    }

    /* Retrieve and validate dataspace badge. */
    seL4_Word dataspaceBadge = seL4_CapData_Badge_get_Badge(seL4_GetBadge(0));
    assert(dataspaceBadge == rpc_dspace_fd);
    struct fs_dataspace* dspace = dspace_get_badge(&fileServ.dspaceTable, dataspaceBadge);
    if (!dspace) {
        ROS_ERROR("data_datamap_handler error: no such dataspace.");
        return EINVALIDPARAM;
    }

    /* Copy out the memory window cap. Do not printf before the copyout. */
    seL4_CPtr memoryWindow = rpc_copyout_cptr(rpc_memoryWindow);
    if (!memoryWindow) {
        ROS_ERROR("data_datamap_handler error: invalid memory window.");
        return EINVALIDPARAM;
    }

    /* Ask the process server to be the pager for this window. */
    seL4_Word winID;
    int error = proc_register_as_pager(memoryWindow, fileServCommon->notifyClientFaultDeathAsyncEP,
                                       &winID);
    if (error != ESUCCESS) {
        csfree(memoryWindow);
        ROS_ERROR("data_datamap_handler error: failed to register as pager.");
        return EINVALID;
    }

    /* Set up fileserver window ID bookkeeping. */
    dprintf("Associating dataspace %d --> windowID %d\n", dspace->dID, winID);
    error = dspace_window_associate(&fileServ.dspaceTable, winID, dspace->dID, rpc_offset,
                                    memoryWindow);
    if (error != ESUCCESS) {
        // TODO: proc_unregister_as_pager
        csfree(memoryWindow);
        return error;
    }

    return ESUCCESS;
}

refos_err_t
data_dataunmap_handler(void *rpc_userptr , seL4_CPtr rpc_memoryWindow)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c->magic == FS_CLIENT_MAGIC);

    if (seL4_MessageInfo_get_extraCaps(m->message) != 1 ||
        !(seL4_MessageInfo_get_capsUnwrapped(m->message) & 0x00000001)) {
        return EINVALIDPARAM;
    }

    /* Copy out the memory window cap. Do not printf before the copyout. */
    seL4_CPtr memoryWindow = rpc_copyout_cptr(rpc_memoryWindow);
    if (!memoryWindow) {
        ROS_ERROR("data_datamap_handler error: invalid memory window.");
        return EINVALIDPARAM;
    }

    int winID = proc_window_getID(memoryWindow);
    if (winID < 0) {
        ROS_ERROR("data_datamap_handler error: invalid memory window ID.");
        csfree(memoryWindow);
        return EINVALIDPARAM;
    }

    // TODO: proc_unregister_as_pager

    /* Clear any fileserver window ID bookkeeping. */
    dspace_window_unassociate(&fileServ.dspaceTable, winID);
    csfree(memoryWindow);
    return ESUCCESS;
}

refos_err_t
data_init_data_handler(void *rpc_userptr , seL4_CPtr rpc_destDataspace ,
                       seL4_CPtr rpc_srcDataspace , uint32_t rpc_srcDataspaceOffset)
{
    struct srv_client *c = (struct srv_client *) rpc_userptr;
    srv_msg_t *m = (srv_msg_t *) c->rpcClient.userptr;
    assert(c->magic == FS_CLIENT_MAGIC);

    if (seL4_MessageInfo_get_extraCaps(m->message) != 2 ||
            seL4_MessageInfo_get_capsUnwrapped(m->message) != 0x00000002) {
        dvprintf("data_init_data_handler: invalid cap parameters: %d 0x%x !\n",
                seL4_MessageInfo_get_extraCaps(m->message),
                seL4_MessageInfo_get_capsUnwrapped(m->message));
        dvprintf("rpc_destDataspace %d rpc_srcDataspace %d\n", rpc_destDataspace, rpc_srcDataspace)
        return EINVALIDPARAM;
    }

    /* Retrieve and validate dataspace badge. */
    struct fs_dataspace* dspace = dspace_get_badge(&fileServ.dspaceTable, rpc_srcDataspace);
    if (!dspace) {
        ROS_ERROR("data_init_data_handler error: no such dest dataspace.");
        return EINVALIDPARAM;
    }

    /* Copyout the source dataspace cap. Do not printf before the copyout. */
    seL4_CPtr destDataspace = rpc_copyout_cptr(rpc_destDataspace);
    if (!destDataspace) {
        ROS_ERROR("data_init_data_handler error: could not copyout src dspace cap.");
        return ENOMEM;
    }

    /* Notify process server that we want to provide data for this dataspace. */
    uint32_t dataID = (uint32_t) -1;
    int error = data_have_data(REFOS_PROCSERV_EP, destDataspace, fileServCommon->notifyClientFaultDeathAsyncEP, &dataID);
    if (error != ESUCCESS || dataID == -1) {
        csfree_delete(destDataspace);
        ROS_ERROR("data_init_data_handler error: data_have_data failed.");
        return EINVALID;
    }

    /* Set up fileserver dataspace ID bookkeeping. */
    dprintf("Associating dataspace %d --> external dataspace %d\n", dspace->dID, dataID);
    error = dspace_external_associate(&fileServ.dspaceTable, dataID, dspace->dID,
                                      rpc_srcDataspaceOffset, destDataspace);
    if (error != ESUCCESS) {
        ROS_ERROR("Failed to associate dataspace.");
        data_unhave_data(REFOS_PROCSERV_EP, destDataspace);
        csfree_delete(destDataspace);
        return error;
    }

    return ESUCCESS;
}

refos_err_t
data_have_data_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd , seL4_CPtr rpc_faultNotifyEP ,
                       uint32_t* rpc_dataID)
{
    ROS_WARNING("CPIO Fileserver does not support data_have_data!");
    return EUNIMPLEMENTED;
}

refos_err_t
data_unhave_data_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd)
{
    ROS_WARNING("CPIO Fileserver does not support data_unhave_data!");
    return EUNIMPLEMENTED;
}

refos_err_t
data_provide_data_from_parambuffer_handler(void *rpc_userptr , seL4_CPtr rpc_dspace_fd ,
                                           uint32_t rpc_offset , uint32_t rpc_contentSize)
{
    ROS_WARNING("CPIO Fileserver does not support data_provide_data!");
    return EUNIMPLEMENTED;
}

int
check_dispatch_data(srv_msg_t *m, void **userptr)
{
    if (m->badge == SRV_UNBADGED) {
        return DISPATCH_PASS;
    }
    return check_dispatch_interface(m, userptr, RPC_DATA_LABEL_MIN, RPC_DATA_LABEL_MAX);
}
