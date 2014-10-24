/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sel4/sel4.h>

#include <refos/refos.h>
#include <refos/error.h>
#include <refos-io/filetable.h>
#include <refos-io/internal_state.h>
#include <refos-rpc/serv_client.h>
#include <refos-rpc/serv_client_helper.h>
#include <refos-util/dprintf.h>

#define FD_TABLE_DEFAULT_SIZE 1024
#define FD_TABLE_ENTRY_TYPE_NONE 0
#define FD_TABLE_ENTRY_TYPE_DATASPACE 1

#define FD_TABLE_ENTRY_DATASPACE_MAGIC 0x4E6CC517
#define FD_TABLE_DATASPACE_IPC_MAXLEN 32

typedef struct fd_table_entry_dataspace_s {
    char type; /* FD_TABLE_ENTRY_TYPE. Inherited, must be first. */
    int magic;
    int fd;

    serv_connection_t connection;
    seL4_CPtr dspace;
    int32_t dspacePos;
    uint32_t dspaceSize;
} fd_table_entry_dataspace_t;

/* ----------------------------- Filetable OAT functions ---------------------------------------- */

static cvector_item_t
filetable_oat_create(coat_t *oat, int id, uint32_t arg[COAT_ARGS])
{
    char type = (char) arg[0];
    cvector_item_t item = NULL;

    fd_table_entry_dataspace_t *e = NULL;

    switch (type) {
        case FD_TABLE_ENTRY_TYPE_DATASPACE:
            /* Allocate and set a new dataspace FD entry struct. */
            e = (fd_table_entry_dataspace_t*) malloc(sizeof(fd_table_entry_dataspace_t));
            if (e){
                memset(e, 0, sizeof(fd_table_entry_dataspace_t));
                e->type = type;
                e->magic = FD_TABLE_ENTRY_DATASPACE_MAGIC;
                e->fd = id;
            }
            item = (cvector_item_t) e;
            break;
        default:
            printf("filetable_oat_create error: Unknown type.\n");
            break;
    }

    if (!item) {
        printf("filetable_oat_create error: Could not allocate structure.\n");
        return NULL;
    }

    return item;
}

static void
filetable_oat_delete(coat_t *oat, cvector_item_t *obj)
{
    char type = *((char*) obj);
    fd_table_entry_dataspace_t *e = NULL;

    switch(type) {
        case FD_TABLE_ENTRY_TYPE_DATASPACE:
            e = (fd_table_entry_dataspace_t*) obj;
            assert(e->type == FD_TABLE_ENTRY_TYPE_DATASPACE);
            assert(e->magic == FD_TABLE_ENTRY_DATASPACE_MAGIC);

            /* Delete dataspace. */
            if (e->connection.serverSession && e->dspace) {
                refos_err_t error = data_close(e->connection.serverSession, e->dspace);
                if (error != ESUCCESS) {
                    printf("filetable_oat_delete error: couldn't close dspace.\n");
                    return;
                }
                csfree_delete(e->dspace);
                e->dspace = 0;
            }

            /* Disconnect from server. */
            if (e->connection.serverSession) {
                serv_disconnect(&e->connection);
                e->connection.serverSession = 0;
            }

            e->magic = 0x0;
            free(e);
            break;
        default:
            printf("filetable_oat_delete error: Unknown type.\n");
            break;
    }
    
    return;
}

/* -------------------------- Filetable interface functions ------------------------------------- */

void
filetable_init(fd_table_t *fdt, uint32_t tableSize)
{
    assert(fdt);
    fdt->magic = FD_TABLE_MAGIC;
    fdt->tableSize = tableSize;

    /* Initialise FD allocation table. */
    memset(&fdt->table, 0, sizeof(coat_t));
    fdt->table.oat_create = filetable_oat_create;
    fdt->table.oat_delete = filetable_oat_delete;
    coat_init(&fdt->table, FD_TABLE_BASE, tableSize);
}

void
filetable_release(fd_table_t *fdt)
{
    assert(fdt && fdt->magic == FD_TABLE_MAGIC);
    coat_release(&fdt->table);
    fdt->magic = 0x0;
}

int
filetable_dspace_open(fd_table_t *fdt, char* filePath, int flags, int mode, int size)
{
    assert(fdt && fdt->magic == FD_TABLE_MAGIC);
    if (!filePath) {
        return -EFILENOTFOUND;
    }

    int error = -EINVALID;
    fd_table_entry_dataspace_t* e = NULL;
    uint32_t arg[COAT_ARGS];
    arg[0] = FD_TABLE_ENTRY_TYPE_DATASPACE;

    /* Allocate an ID, and the FD entry structure associated with it. */
    coat_alloc(&fdt->table, arg, (cvector_item_t *) &e);
    if (!e) {
        printf("filetable_open out of memory.\n");
        return -ENOMEM;
    }

    /* Connect to the dataspace server. */
    assert(e->magic == FD_TABLE_ENTRY_DATASPACE_MAGIC);
    e->connection = serv_connect_no_pbuffer(filePath);
    if (e->connection.error != ESUCCESS || !e->connection.serverSession) {
        error = -ESERVERNOTFOUND;
        goto exit1;
    }

    /* Open the dataspace on the server. */
    e->dspace = data_open(e->connection.serverSession,
            e->connection.serverMountPoint.dspaceName, flags, mode, size, &error);
    if (error || !e->dspace) {
        error = -EFILENOTFOUND;
        goto exit2;
    }

    e->dspaceSize = data_get_size(e->connection.serverSession, e->dspace);
    e->dspacePos = 0;
    return e->fd;

    /* Exit stack. */
exit2:
    serv_disconnect(&e->connection);
exit1:
    assert(e && e->fd);
    coat_free(&fdt->table, e->fd);
    assert(error < 0);
    return error;
}

int
filetable_close(fd_table_t *fdt, int fd)
{
    assert(fdt && fdt->magic == FD_TABLE_MAGIC);
    if (fd < FD_TABLE_BASE || fd >= fdt->tableSize) {
        return -EFILENOTFOUND;
    }
    coat_free(&fdt->table, fd);
    return ESUCCESS;
}

refos_err_t
filetable_lseek(fd_table_t *fdt, int fd, int *offset, int whence)
{
    assert(fdt && fdt->magic == FD_TABLE_MAGIC);
    if (!offset) {
        printf("filetable_lseek - NULL parameter.\n");
        return EINVALIDPARAM;
    }
    if (fd < FD_TABLE_BASE || fd >= fdt->tableSize) {
        return EFILENOTFOUND;
    }

    /* Retrieve the file descr entry. */
    cvector_item_t entry = coat_get(&fdt->table, fd);
    if (!entry) {
        return EFILENOTFOUND;
    }
    char type = *((char*) entry);

    /* lseek only support for dataspace entries. */
    if (type != FD_TABLE_ENTRY_TYPE_DATASPACE) {
        assert(!"lseek for this type unimplemented.");
        return EUNIMPLEMENTED;
    }

    fd_table_entry_dataspace_t *fdEntry = (fd_table_entry_dataspace_t*) entry;
    assert(fdEntry->magic == FD_TABLE_ENTRY_DATASPACE_MAGIC);

    switch(whence) {
        case SEEK_SET:
            fdEntry->dspacePos = (*offset);
            break;
        case SEEK_CUR:
            fdEntry->dspacePos += (*offset);
            break;
        case SEEK_END:
            fdEntry->dspacePos = fdEntry->dspaceSize + (*offset);
            break;
        default:
            return EINVALIDPARAM;  
    }

    if (fdEntry->dspacePos < 0) {
        fdEntry->dspacePos = 0;
    }
    if (fdEntry->dspacePos > fdEntry->dspaceSize) {
        fdEntry->dspacePos = fdEntry->dspaceSize;
    }

    (*offset) = fdEntry->dspacePos;
    return ESUCCESS;
}

static int
filetable_internal_read_write(fd_table_t *fdt, int fd, char *buffer, int bufferLen, bool read)
{
    assert(fdt && fdt->magic == FD_TABLE_MAGIC);
    if (!buffer || !bufferLen) {
        ROS_SET_ERRNO(ESUCCESS);
        return 0;
    }
    if (fd < FD_TABLE_BASE || fd >= fdt->tableSize) {
        ROS_SET_ERRNO(EFILENOTFOUND);
        return -EFILENOTFOUND;
    }

    /* Retrieve the file descr entry. */
    cvector_item_t entry = coat_get(&fdt->table, fd);
    if (!entry) {
        ROS_SET_ERRNO(EFILENOTFOUND);
        return -EFILENOTFOUND;
    }
    char type = *((char*) entry);

    /* Read / write only supported for dataspace entries. */
    if (type != FD_TABLE_ENTRY_TYPE_DATASPACE) {
        assert(!"read / write for this type unimplemented.");
        ROS_SET_ERRNO(EUNIMPLEMENTED);
        return -EUNIMPLEMENTED;
    }

    fd_table_entry_dataspace_t *fdEntry = (fd_table_entry_dataspace_t*) entry;
    assert(fdEntry->magic == FD_TABLE_ENTRY_DATASPACE_MAGIC);

    /* Cap length so we don't overrun IPC buffer.
       Currently read / write is implemented over IPC, and this is inefficient and somewhat hacky.
       In the future, file read / write using mapped shared memory should be implemented.
    */
    if (bufferLen > FD_TABLE_DATASPACE_IPC_MAXLEN) {
        bufferLen = FD_TABLE_DATASPACE_IPC_MAXLEN;
    }

    /* Perform the actual dataspace read / write operation. */
    assert(fdEntry->dspace);
    int nr = -EINVALID;
    if (read) {
        nr = data_read(fdEntry->connection.serverSession, fdEntry->dspace, fdEntry->dspacePos,
                       buffer, bufferLen);
    } else {
        nr = data_write(fdEntry->connection.serverSession, fdEntry->dspace, fdEntry->dspacePos,
                       buffer, bufferLen);
    }
    if (nr < 0) {
        ROS_SET_ERRNO(-nr);
        return nr;
    }

    /* Shift the dataspace position offset. */
    fdEntry->dspacePos += nr;
    if (fdEntry->dspacePos < 0) {
        fdEntry->dspacePos = 0;
    }
    if (read) {
        if (fdEntry->dspacePos > fdEntry->dspaceSize) {
            fdEntry->dspacePos = fdEntry->dspaceSize;
        }
    } else {
        fdEntry->dspaceSize = data_get_size(fdEntry->connection.serverSession, fdEntry->dspace);
    }

    ROS_SET_ERRNO(ESUCCESS);
    return nr;
}

int
filetable_read(fd_table_t *fdt, int fd, char *bufferDest, int bufferLen)
{
    return filetable_internal_read_write(fdt, fd, bufferDest, bufferLen, true);
}

int
filetable_write(fd_table_t *fdt, int fd, char *bufferSrc, int bufferLen)
{
    return filetable_internal_read_write(fdt, fd, bufferSrc, bufferLen, false);
}

seL4_CPtr
filetable_dspace_get(fd_table_t *fdt, int fd)
{
    assert(fdt && fdt->magic == FD_TABLE_MAGIC);
    if (fd < FD_TABLE_BASE || fd >= fdt->tableSize) {
        ROS_SET_ERRNO(EFILENOTFOUND);
        return -EFILENOTFOUND;
    }

    /* Retrieve the file descr entry. */
    cvector_item_t entry = coat_get(&fdt->table, fd);
    if (!entry) {
        ROS_SET_ERRNO(EFILENOTFOUND);
        return -EFILENOTFOUND;
    }
    char type = *((char*) entry);

    /* Read / write only supported for dataspace entries. */
    if (type != FD_TABLE_ENTRY_TYPE_DATASPACE) {
        assert(!"dspace_get for this type unsupported.");
        ROS_SET_ERRNO(EUNIMPLEMENTED);
        return -EUNIMPLEMENTED;
    }

    fd_table_entry_dataspace_t *fdEntry = (fd_table_entry_dataspace_t*) entry;
    assert(fdEntry->magic == FD_TABLE_ENTRY_DATASPACE_MAGIC);
    return fdEntry->dspace;
}

/* ----------------------- Refos IO default filetable functions --------------------------------- */

void
filetable_init_default(void)
{
    filetable_init(&refosIOState.fdTable, FD_TABLE_DEFAULT_SIZE);
}

void
filetable_deinit_default(void)
{
    filetable_release(&refosIOState.fdTable);
}
