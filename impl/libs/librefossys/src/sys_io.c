/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <refos/error.h>
#include <refos-io/internal_state.h>
#include <refos-io/ipc_state.h>
#include <refos-io/filetable.h>
#include <refos-util/init.h>
#include <refos-rpc/data_client.h>
#include <refos-rpc/data_client_helper.h>
#include <utils/arith.h>
#include <autoconf.h>

#include <stdio.h>
#include <sys/uio.h>
#include <limits.h>
#include <errno.h>
#include <sel4/sel4.h>
#include <stdarg.h>
#include <fcntl.h>
#include <refos-util/dprintf.h>

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

#define REFOS_SYSIO_MAX_PATHLEN 256

static size_t
sys_platform_stdout_write(void *data, size_t count)
{
    char *cdata = data;

#if defined(SEL4_DEBUG_KERNEL) && defined(CONFIG_REFOS_SYS_FORCE_DEBUGPUTCHAR)
    for (size_t i = 0; i < count; i++) {
        seL4_DebugPutChar(cdata[i]);
    }
#else

    if (refosIOState.stdioWriteOverride != NULL) {
        /* Use overridden write function. */
        return refosIOState.stdioWriteOverride(data, count);
    }

    /* Use serial dataspace on Console server. */
    if (refosIOState.stdioDataspace && refosIOState.stdioSession.serverSession) {
        refosio_internal_save_IPC_buffer();
        for (size_t i = 0; i < count;) {
            int c = MIN(REFOS_DEFAULT_DSPACE_IPC_MAXLEN, count - i);
            int n = data_write(refosIOState.stdioSession.serverSession, refosIOState.stdioDataspace,
                               0, &cdata[i], c);
            if (!n) {
                /* An error occured. */
                refosio_internal_restore_IPC_buffer();
                return i;
            }

            i += n;
        }
        refosio_internal_restore_IPC_buffer();
    }

#endif

    return count;
}

static size_t
sys_platform_stdin_read(void *data, size_t count)
{
    assert(data && count);
    int c = refos_getc();
    if (c < 0) {
        return 0;
    }
    char cc = (char) c;
    memcpy(data, &cc, sizeof(char));
    return sizeof(char);
}

/* Writev syscall implementation for muslc. Only implemented for stdin and stdout. */
static long
_sys_writev(int fildes, struct iovec *iov, int iovcnt)
{
    long long sum = 0;
    ssize_t ret = 0;

    /* The iovcnt argument is valid if greater than 0 and less than or equal to IOV_MAX. */
    if (iovcnt <= 0 || iovcnt > IOV_MAX)
        return -EINVAL;
   
    /* The sum of iov_len is valid if less than or equal to SSIZE_MAX i.e. cannot overflow
       a ssize_t. */
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len < 0)
            return -EINVAL;

        sum += (long long)iov[i].iov_len;
        if (sum > SSIZE_MAX)
            return -EINVAL;
    }

    /* If all the iov_len members in the array are 0, return 0. */
    if (!sum)
        return 0;

    /* Write the buffer to console if the fd is for stdout or stderr. */
    if (fildes == STDOUT_FD || fildes == STDERR_FD) {
        for (int i = 0; i < iovcnt; i++) {
            ret += sys_platform_stdout_write(iov[i].iov_base, iov[i].iov_len);
        }
    } else if (fildes == STDIN_FD) {
        /* Can't write to stdin. */
        assert(!"Can't write to stdin.");
        return -EACCES;
    } else {
        for (int i = 0; i < iovcnt; i++) {
            if (iov[i].iov_len == 0) continue;
            int offset = 0;
            
            while (offset < iov[i].iov_len) {
                int nc = filetable_write(&refosIOState.fdTable, fildes, iov[i].iov_base + offset,
                                         iov[i].iov_len - offset);
                if (nc >= 0) {
                    assert(nc <= iov[i].iov_len - offset);
                    offset += nc;
                    ret += nc;
                } else {
                    ret = -EFAULT;
                    break;
                }
            }

            if (ret < 0) {
                break;
            }
        }
    }

    return ret;
}

long
sys_writev(va_list ap)
{
    int fildes = va_arg(ap, int);
    struct iovec *iov = va_arg(ap, struct iovec *);
    int iovcnt = va_arg(ap, int);
    return _sys_writev(fildes, iov, iovcnt);
}

long
sys_write(va_list ap) {
    int fildes = va_arg(ap, int);
    void* buf = va_arg(ap, void*);
    size_t count = va_arg(ap, size_t);

    struct iovec iov = {
        .iov_base = buf,
        .iov_len = count
    };

    return _sys_writev(fildes, &iov, 1);
}

static long
_sys_readv(int fildes, struct iovec *iov, int iovcnt)
{
    long long sum = 0;
    ssize_t ret = 0;
    
    /* The iovcnt argument is valid if greater than 0 and less than or equal to IOV_MAX. */
    if (iovcnt <= 0 || iovcnt > IOV_MAX)
        return -EINVAL;

    /* The sum of iov_len is valid if less than or equal to SSIZE_MAX i.e. cannot overflow
       a ssize_t. */
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len < 0)
            return -EINVAL;

        sum += (long long)iov[i].iov_len;
        if (sum > SSIZE_MAX)
            return -EINVAL;
    }
    
    /* If all the iov_len members in the array are 0, return 0. */
    if (!sum)
        return 0;
    
    /* Read the iov buffers. */
    if (fildes == STDIN_FD) {
        /* Read from STDIN. */
        for (int i = 0; i < iovcnt; i++) {
            if (iov[i].iov_len == 0) continue;
            ret += sys_platform_stdin_read(iov[i].iov_base, iov[i].iov_len);
            break;
        }
        return ret;
    } 

    /* Read from dataspace file. */
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len == 0) continue;
    
        int nc = filetable_read(&refosIOState.fdTable, fildes, iov[i].iov_base, iov[i].iov_len);
        if (nc < 0) {
            return -1;
        } else if (nc < iov[i].iov_len) {
            ret += nc;
            break;
        }
        ret += nc;
    }

    return ret;
}

long
sys_readv(va_list ap)
{
    int fildes = va_arg(ap, int);
    struct iovec *iov = va_arg(ap, struct iovec *);
    int iovcnt = va_arg(ap, int);
    return _sys_readv(fildes, iov, iovcnt);
}

long
sys_read(va_list ap) {
    int fildes = va_arg(ap, int);
    void* buf = va_arg(ap, void*);
    size_t count = va_arg(ap, size_t);

    struct iovec iov = {
        .iov_base = buf,
        .iov_len = count
    };

    return _sys_readv(fildes, &iov, 1);
}

long
sys_open(va_list ap)
{
    char *pathname = va_arg(ap, char*);
    int flags = va_arg(ap, int);
    int fd = -1;
    static char tempBufferPath[REFOS_SYSIO_MAX_PATHLEN];

    /* Handle the PWD environment variable. */
    char *pwd = getenv("PWD");
    if (pwd && strlen(pwd) > 0) {
        snprintf(tempBufferPath, REFOS_SYSIO_MAX_PATHLEN, "%s%s", getenv("PWD"), pathname);
        pathname = tempBufferPath;
    }

    /* Open dataspace file. */
    fd = filetable_dspace_open(&refosIOState.fdTable, pathname, flags, 0, 0x1000);
    switch (ROS_ERRNO()) {
        case ESUCCESS: break;
        case EINVALID: return -EFAULT;
        case EINVALIDPARAM: return -EACCES;
        case EFILENOTFOUND: return -EMFILE;
        case ESERVERNOTFOUND: return -EMFILE;
        default: return -EFAULT;
    }
    assert(fd);

    return fd;
}

long
_sys_lseek(int fildes, off_t offset, int whence)
{
    int newOffset = (int) offset;
    if (fildes == STDOUT_FD || fildes == STDERR_FD || fildes == STDIN_FD) {
        /* lseek for STDOUT / STDIN / STDERR makes no sense. */
        return newOffset;
    } 

    /* Perform lseek on filetable. */
    int error = filetable_lseek(&refosIOState.fdTable, fildes, &newOffset, whence);
    if (error != ESUCCESS) {
        return -EFAULT;
    }
    return newOffset;
}

long
sys_lseek(va_list ap)
{
    int fildes = va_arg(ap, int);
    off_t offset = va_arg(ap, int);
    int whence = va_arg(ap, int); 
    return _sys_lseek(fildes, offset , whence);
}

long
sys__llseek(va_list ap)
{
    int fildes = va_arg(ap, int);
    unsigned long offset_high = va_arg(ap, unsigned long);
    unsigned long offset_low = va_arg(ap, unsigned long);
    off_t *result = va_arg(ap, off_t*);
    int whence = va_arg(ap, int);

    if (!result) {
        return -1;
    }
    if (offset_high != 0) {
        return -1;
    }

    (*result) = (off_t) _sys_lseek(fildes, offset_low, whence);
    return 0;
}

long
sys_ioctl(va_list ap)
{
    /* muslc does some ioctl to stdout, so just allow these to silently go through */
    return 0;
}

long
sys_close(va_list ap)
{
    int fildes = va_arg(ap, int);
    if (fildes == STDOUT_FD || fildes == STDERR_FD || fildes == STDIN_FD) {
        /* close for STDOUT / STDIN / STDERR makes no sense. */
        return 0;
    }

    /* Perform close on filetable. */
    int error = filetable_close(&refosIOState.fdTable, fildes);
    if (error != ESUCCESS) {
        return -EIO;
    }
    return 0;
}
