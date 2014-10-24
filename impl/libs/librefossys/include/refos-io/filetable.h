/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_IO_FILETABLE_H_
#define _REFOS_IO_FILETABLE_H_

#include <stdint.h>
#include <refos/refos.h>
#include <refos/error.h>
#include <data_struct/coat.h>

#define FD_TABLE_MAGIC 0xA6B1063F
#define FD_TABLE_BASE 3 /* 0, 1 and 2 are stdin, stdout and stderr. */

typedef struct fd_table_s {
    coat_t table; /* fd_table_entry_*_t, Inherited, must be first. */
    uint32_t tableSize;
    uint32_t magic;
} fd_table_t;

void filetable_init(fd_table_t *fdt, uint32_t tableSize);

void filetable_release(fd_table_t *fdt);

int filetable_dspace_open(fd_table_t *fdt, char* filePath, int flags, int mode, int size);

int filetable_close(fd_table_t *fdt, int fd);

refos_err_t filetable_lseek(fd_table_t *fdt, int fd, int *offset, int whence);

int filetable_read(fd_table_t *fdt, int fd, char *bufferDest, int bufferLen);

int filetable_write(fd_table_t *fdt, int fd, char *bufferSrc, int bufferLen);

seL4_CPtr filetable_dspace_get(fd_table_t *fdt, int fd);

void filetable_init_default(void);

void filetable_deinit_default(void);

#endif /* _REFOS_IO_FILETABLE_H_ */
