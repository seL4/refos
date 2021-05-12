/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <bits/syscall.h>
#include <syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sel4/sel4.h>
#include <refos-util/init.h>
#include <refos-util/cspace.h>
#include <refos-util/walloc.h>
#include <refos-util/dprintf.h>
#include <refos/refos.h>
#include <refos/vmlayout.h>
#include <autoconf.h>
#include "stdio_copy.h"

/*! @file
    @brief RefOS client environment initialisation helper functions. */

/* Forward declarations to avoid spectacular circular library dependency header soup. */
extern void refosio_init_morecore(struct sl_procinfo_s *procInfo);
extern void refos_init_timer(char *dspacePath);
extern void filetable_init_default(void);

/* Static buffer for the cspace allocator, to avoid malloc() circular dependency disaster. */
#define REFOS_UTIL_CSPACE_STATIC_SIZE 0x8000
static char _refosUtilCSpaceStatic[REFOS_UTIL_CSPACE_STATIC_SIZE];
static char *_refosEnv[1];
extern char **__environ; /*! < POSIX standard, from MUSLC lib. */

/* Provide default weak links to dprintf. */
const char* dprintfServerName __attribute__((weak)) = "????????";
int dprintfServerColour __attribute__((weak)) = 33;

/* Future Work 3:
   How the selfloader bootstraps user processes needs to be modified further. Changes were
   made to accomodate the new way that muslc expects process's stacks to be set up when
   processes start, but the one part of this that still needs to changed is how user processes
   find their system call table. Currently the selfloader sets up user processes so that
   the selfloader's system call table is used by user processes by passing the address of the
   selfloader's system call table to the user processes via the user process's environment
   variables. Ideally, user processes would use their own system call table.
*/

typedef long (*syscall_t)(va_list);

long sys_restart_syscall(va_list ap);
long sys_exit(va_list ap);
long sys_fork(va_list ap);
long sys_read(va_list ap);
long sys_write(va_list ap);
long sys_open(va_list ap);
long sys_close(va_list ap);
long sys_creat(va_list ap);
long sys_link(va_list ap);
long sys_unlink(va_list ap);
long sys_execve(va_list ap);
long sys_chdir(va_list ap);
long sys_mknod(va_list ap);
long sys_chmod(va_list ap);
long sys_lchown(va_list ap);
long sys_lseek(va_list ap);
long sys_getpid(va_list ap);
long sys_mount(va_list ap);
long sys_setuid(va_list ap);
long sys_getuid(va_list ap);
long sys_ptrace(va_list ap);
long sys_pause(va_list ap);
long sys_access(va_list ap);
long sys_nice(va_list ap);
long sys_sync(va_list ap);
long sys_kill(va_list ap);
long sys_rename(va_list ap);
long sys_mkdir(va_list ap);
long sys_rmdir(va_list ap);
long sys_dup(va_list ap);
long sys_pipe(va_list ap);
long sys_times(va_list ap);
long sys_brk(va_list ap);
long sys_setgid(va_list ap);
long sys_getgid(va_list ap);
long sys_geteuid(va_list ap);
long sys_getegid(va_list ap);
long sys_acct(va_list ap);
long sys_umount2(va_list ap);
long sys_ioctl(va_list ap);
long sys_fcntl(va_list ap);
long sys_setpgid(va_list ap);
long sys_umask(va_list ap);
long sys_chroot(va_list ap);
long sys_ustat(va_list ap);
long sys_dup2(va_list ap);
long sys_getppid(va_list ap);
long sys_getpgrp(va_list ap);
long sys_setsid(va_list ap);
long sys_sigaction(va_list ap);
long sys_setreuid(va_list ap);
long sys_setregid(va_list ap);
long sys_sigsuspend(va_list ap);
long sys_sigpending(va_list ap);
long sys_sethostname(va_list ap);
long sys_setrlimit(va_list ap);
long sys_getrusage(va_list ap);
long sys_gettimeofday(va_list ap);
long sys_settimeofday(va_list ap);
long sys_getgroups(va_list ap);
long sys_setgroups(va_list ap);
long sys_symlink(va_list ap);
long sys_readlink(va_list ap);
long sys_uselib(va_list ap);
long sys_swapon(va_list ap);
long sys_reboot(va_list ap);
long sys_munmap(va_list ap);
long sys_truncate(va_list ap);
long sys_ftruncate(va_list ap);
long sys_fchmod(va_list ap);
long sys_fchown(va_list ap);
long sys_getpriority(va_list ap);
long sys_setpriority(va_list ap);
long sys_statfs(va_list ap);
long sys_fstatfs(va_list ap);
long sys_syslog(va_list ap);
long sys_setitimer(va_list ap);
long sys_getitimer(va_list ap);
long sys_stat(va_list ap);
long sys_lstat(va_list ap);
long sys_fstat(va_list ap);
long sys_vhangup(va_list ap);
long sys_wait4(va_list ap);
long sys_swapoff(va_list ap);
long sys_sysinfo(va_list ap);
long sys_fsync(va_list ap);
long sys_sigreturn(va_list ap);
long sys_clone(va_list ap);
long sys_setdomainname(va_list ap);
long sys_uname(va_list ap);
long sys_adjtimex(va_list ap);
long sys_mprotect(va_list ap);
long sys_sigprocmask(va_list ap);
long sys_init_module(va_list ap);
long sys_delete_module(va_list ap);
long sys_quotactl(va_list ap);
long sys_getpgid(va_list ap);
long sys_fchdir(va_list ap);
long sys_bdflush(va_list ap);
long sys_sysfs(va_list ap);
long sys_personality(va_list ap);
long sys_setfsuid(va_list ap);
long sys_setfsgid(va_list ap);
long sys_getdents(va_list ap);
long sys__newselect(va_list ap);
long sys_flock(va_list ap);
long sys_msync(va_list ap);
long sys_readv(va_list ap);
long sys_writev(va_list ap);
long sys_getsid(va_list ap);
long sys_fdatasync(va_list ap);
long sys__sysctl(va_list ap);
long sys_mlock(va_list ap);
long sys_munlock(va_list ap);
long sys_mlockall(va_list ap);
long sys_munlockall(va_list ap);
long sys_sched_setparam(va_list ap);
long sys_sched_getparam(va_list ap);
long sys_sched_setscheduler(va_list ap);
long sys_sched_getscheduler(va_list ap);
long sys_sched_yield(va_list ap);
long sys_sched_get_priority_max(va_list ap);
long sys_sched_get_priority_min(va_list ap);
long sys_sched_rr_get_interval(va_list ap);
long sys_nanosleep(va_list ap);
long sys_mremap(va_list ap);
long sys_setresuid(va_list ap);
long sys_getresuid(va_list ap);
long sys_poll(va_list ap);
long sys_nfsservctl(va_list ap);
long sys_setresgid(va_list ap);
long sys_getresgid(va_list ap);
long sys_prctl(va_list ap);
long sys_rt_sigreturn(va_list ap);
long sys_rt_sigaction(va_list ap);
long sys_rt_sigprocmask(va_list ap);
long sys_rt_sigpending(va_list ap);
long sys_rt_sigtimedwait(va_list ap);
long sys_rt_sigqueueinfo(va_list ap);
long sys_rt_sigsuspend(va_list ap);
long sys_pread64(va_list ap);
long sys_pwrite64(va_list ap);
long sys_chown(va_list ap);
long sys_getcwd(va_list ap);
long sys_capget(va_list ap);
long sys_capset(va_list ap);
long sys_sigaltstack(va_list ap);
long sys_sendfile(va_list ap);
long sys_vfork(va_list ap);
long sys_ugetrlimit(va_list ap);
long sys_mmap2(va_list ap);
long sys_truncate64(va_list ap);
long sys_ftruncate64(va_list ap);
long sys_stat64(va_list ap);
long sys_lstat64(va_list ap);
long sys_fstat64(va_list ap);
long sys_lchown32(va_list ap);
long sys_getuid32(va_list ap);
long sys_getgid32(va_list ap);
long sys_geteuid32(va_list ap);
long sys_getegid32(va_list ap);
long sys_setreuid32(va_list ap);
long sys_setregid32(va_list ap);
long sys_getgroups32(va_list ap);
long sys_setgroups32(va_list ap);
long sys_fchown32(va_list ap);
long sys_setresuid32(va_list ap);
long sys_getresuid32(va_list ap);
long sys_setresgid32(va_list ap);
long sys_getresgid32(va_list ap);
long sys_chown32(va_list ap);
long sys_setuid32(va_list ap);
long sys_setgid32(va_list ap);
long sys_setfsuid32(va_list ap);
long sys_setfsgid32(va_list ap);
long sys_getdents64(va_list ap);
long sys_pivot_root(va_list ap);
long sys_mincore(va_list ap);
long sys_madvise(va_list ap);
long sys_fcntl64(va_list ap);
long sys_gettid(va_list ap);
long sys_readahead(va_list ap);
long sys_setxattr(va_list ap);
long sys_lsetxattr(va_list ap);
long sys_fsetxattr(va_list ap);
long sys_getxattr(va_list ap);
long sys_lgetxattr(va_list ap);
long sys_fgetxattr(va_list ap);
long sys_listxattr(va_list ap);
long sys_llistxattr(va_list ap);
long sys_flistxattr(va_list ap);
long sys_removexattr(va_list ap);
long sys_lremovexattr(va_list ap);
long sys_fremovexattr(va_list ap);
long sys_tkill(va_list ap);
long sys_sendfile64(va_list ap);
long sys_futex(va_list ap);
long sys_sched_setaffinity(va_list ap);
long sys_sched_getaffinity(va_list ap);
long sys_io_setup(va_list ap);
long sys_io_destroy(va_list ap);
long sys_io_getevents(va_list ap);
long sys_io_submit(va_list ap);
long sys_io_cancel(va_list ap);
long sys_exit_group(va_list ap);
long sys_lookup_dcookie(va_list ap);
long sys_epoll_create(va_list ap);
long sys_epoll_ctl(va_list ap);
long sys_epoll_wait(va_list ap);
long sys_remap_file_pages(va_list ap);
long sys_set_tid_address(va_list ap);
long sys_timer_create(va_list ap);
long sys_timer_settime(va_list ap);
long sys_timer_gettime(va_list ap);
long sys_timer_getoverrun(va_list ap);
long sys_timer_delete(va_list ap);
long sys_clock_settime(va_list ap);
long sys_clock_gettime(va_list ap);
long sys_clock_getres(va_list ap);
long sys_clock_nanosleep(va_list ap);
long sys_statfs64(va_list ap);
long sys_fstatfs64(va_list ap);
long sys_tgkill(va_list ap);
long sys_utimes(va_list ap);
long sys_fadvise64_64(va_list ap);
long sys_mq_open(va_list ap);
long sys_mq_unlink(va_list ap);
long sys_mq_timedsend(va_list ap);
long sys_mq_timedreceive(va_list ap);
long sys_mq_notify(va_list ap);
long sys_mq_getsetattr(va_list ap);
long sys_waitid(va_list ap);
long sys_add_key(va_list ap);
long sys_request_key(va_list ap);
long sys_keyctl(va_list ap);
long sys_vserver(va_list ap);
long sys_ioprio_set(va_list ap);
long sys_ioprio_get(va_list ap);
long sys_inotify_init(va_list ap);
long sys_inotify_add_watch(va_list ap);
long sys_inotify_rm_watch(va_list ap);
long sys_mbind(va_list ap);
long sys_get_mempolicy(va_list ap);
long sys_set_mempolicy(va_list ap);
long sys_openat(va_list ap);
long sys_mkdirat(va_list ap);
long sys_mknodat(va_list ap);
long sys_fchownat(va_list ap);
long sys_futimesat(va_list ap);
long sys_fstatat64(va_list ap);
long sys_unlinkat(va_list ap);
long sys_renameat(va_list ap);
long sys_linkat(va_list ap);
long sys_symlinkat(va_list ap);
long sys_readlinkat(va_list ap);
long sys_fchmodat(va_list ap);
long sys_faccessat(va_list ap);
long sys_pselect6(va_list ap);
long sys_ppoll(va_list ap);
long sys_unshare(va_list ap);
long sys_set_robust_list(va_list ap);
long sys_get_robust_list(va_list ap);
long sys_splice(va_list ap);
long sys_tee(va_list ap);
long sys_vmsplice(va_list ap);
long sys_move_pages(va_list ap);
long sys_getcpu(va_list ap);
long sys_epoll_pwait(va_list ap);
long sys_kexec_load(va_list ap);
long sys_utimensat(va_list ap);
long sys_signalfd(va_list ap);
long sys_timerfd_create(va_list ap);
long sys_eventfd(va_list ap);
long sys_fallocate(va_list ap);
long sys_timerfd_settime(va_list ap);
long sys_timerfd_gettime(va_list ap);
long sys_signalfd4(va_list ap);
long sys_eventfd2(va_list ap);
long sys_epoll_create1(va_list ap);
long sys_dup3(va_list ap);
long sys_pipe2(va_list ap);
long sys_inotify_init1(va_list ap);
long sys_preadv(va_list ap);
long sys_pwritev(va_list ap);
long sys_prlimit64(va_list ap);
long sys_name_to_handle_at(va_list ap);
long sys_open_by_handle_at(va_list ap);
long sys_clock_adjtime(va_list ap);
long sys_syncfs(va_list ap);
long sys_sendmmsg(va_list ap);
long sys_setns(va_list ap);
long sys_process_vm_readv(va_list ap);
long sys_process_vm_writev(va_list ap);
#ifdef CONFIG_ARCH_ARM
long sys_pciconfig_iobase(va_list ap);
long sys_pciconfig_read(va_list ap);
long sys_pciconfig_write(va_list ap);
long sys_socket(va_list ap);
long sys_bind(va_list ap);
long sys_connect(va_list ap);
long sys_listen(va_list ap);
long sys_accept(va_list ap);
long sys_getsockname(va_list ap);
long sys_getpeername(va_list ap);
long sys_socketpair(va_list ap);
long sys_send(va_list ap);
long sys_sendto(va_list ap);
long sys_recv(va_list ap);
long sys_recvfrom(va_list ap);
long sys_shutdown(va_list ap);
long sys_setsockopt(va_list ap);
long sys_getsockopt(va_list ap);
long sys_sendmsg(va_list ap);
long sys_recvmsg(va_list ap);
long sys_semop(va_list ap);
long sys_semget(va_list ap);
long sys_semctl(va_list ap);
long sys_msgsnd(va_list ap);
long sys_msgrcv(va_list ap);
long sys_msgget(va_list ap);
long sys_msgctl(va_list ap);
long sys_shmat(va_list ap);
long sys_shmdt(va_list ap);
long sys_shmget(va_list ap);
long sys_shmctl(va_list ap);
long sys_semtimedop(va_list ap);
long sys_sync_file_range2(va_list ap);
long sys_accept4(va_list ap);
long sys_rt_tgsigqueueinfo(va_list ap);
long sys_perf_event_open(va_list ap);
long sys_recvmmsg(va_list ap);
long sys_fanotify_init(va_list ap);
long sys_fanotify_mark(va_list ap);
#endif /* CONFIG_ARCH_ARM */

void
refos_init_selfload_child(uintptr_t parent_syscall_table_address)
{
    /* Point the selfloaded process to the parent's system call table. */
    syscall_t *syscall_table = (syscall_t *) parent_syscall_table_address;

    syscall_table[__NR_restart_syscall] = sys_restart_syscall;
    syscall_table[__NR_exit] = sys_exit;
    syscall_table[__NR_fork] = sys_fork;
    syscall_table[__NR_read] = sys_read;
    syscall_table[__NR_write] = sys_write;
    syscall_table[__NR_open] = sys_open;
    syscall_table[__NR_close] = sys_close;
    syscall_table[__NR_creat] = sys_creat;
    syscall_table[__NR_link] = sys_link;
    syscall_table[__NR_unlink] = sys_unlink;
    syscall_table[__NR_execve] = sys_execve;
    syscall_table[__NR_chdir] = sys_chdir;
    syscall_table[__NR_mknod] = sys_mknod;
    syscall_table[__NR_chmod] = sys_chmod;
    syscall_table[__NR_lchown] = sys_lchown;
    syscall_table[__NR_lseek] = sys_lseek;
    syscall_table[__NR_getpid] = sys_getpid;
    syscall_table[__NR_mount] = sys_mount;
    syscall_table[__NR_setuid] = sys_setuid;
    syscall_table[__NR_getuid] = sys_getuid;
    syscall_table[__NR_ptrace] = sys_ptrace;
    syscall_table[__NR_pause] = sys_pause;
    syscall_table[__NR_access] = sys_access;
    syscall_table[__NR_nice] = sys_nice;
    syscall_table[__NR_sync] = sys_sync;
    syscall_table[__NR_kill] = sys_kill;
    syscall_table[__NR_rename] = sys_rename;
    syscall_table[__NR_mkdir] = sys_mkdir;
    syscall_table[__NR_rmdir] = sys_rmdir;
    syscall_table[__NR_dup] = sys_dup;
    syscall_table[__NR_pipe] = sys_pipe;
    syscall_table[__NR_times] = sys_times;
    syscall_table[__NR_brk] = sys_brk;
    syscall_table[__NR_setgid] = sys_setgid;
    syscall_table[__NR_getgid] = sys_getgid;
    syscall_table[__NR_geteuid] = sys_geteuid;
    syscall_table[__NR_getegid] = sys_getegid;
    syscall_table[__NR_acct] = sys_acct;
    syscall_table[__NR_umount2] = sys_umount2;
    syscall_table[__NR_ioctl] = sys_ioctl;
    syscall_table[__NR_fcntl] = sys_fcntl;
    syscall_table[__NR_setpgid] = sys_setpgid;
    syscall_table[__NR_umask] = sys_umask;
    syscall_table[__NR_chroot] = sys_chroot;
    syscall_table[__NR_ustat] = sys_ustat;
    syscall_table[__NR_dup2] = sys_dup2;
    syscall_table[__NR_getppid] = sys_getppid;
    syscall_table[__NR_getpgrp] = sys_getpgrp;
    syscall_table[__NR_setsid] = sys_setsid;
    syscall_table[__NR_sigaction] = sys_sigaction;
    syscall_table[__NR_setreuid] = sys_setreuid;
    syscall_table[__NR_setregid] = sys_setregid;
    syscall_table[__NR_sigsuspend] = sys_sigsuspend;
    syscall_table[__NR_sigpending] = sys_sigpending;
    syscall_table[__NR_sethostname] = sys_sethostname;
    syscall_table[__NR_setrlimit] = sys_setrlimit;
    syscall_table[__NR_getrusage] = sys_getrusage;
    syscall_table[__NR_gettimeofday] = sys_gettimeofday;
    syscall_table[__NR_settimeofday] = sys_settimeofday;
    syscall_table[__NR_getgroups] = sys_getgroups;
    syscall_table[__NR_setgroups] = sys_setgroups;
    syscall_table[__NR_symlink] = sys_symlink;
    syscall_table[__NR_readlink] = sys_readlink;
    syscall_table[__NR_uselib] = sys_uselib;
    syscall_table[__NR_swapon] = sys_swapon;
    syscall_table[__NR_reboot] = sys_reboot;
    syscall_table[__NR_munmap] = sys_munmap;
    syscall_table[__NR_truncate] = sys_truncate;
    syscall_table[__NR_ftruncate] = sys_ftruncate;
    syscall_table[__NR_fchmod] = sys_fchmod;
    syscall_table[__NR_fchown] = sys_fchown;
    syscall_table[__NR_getpriority] = sys_getpriority;
    syscall_table[__NR_setpriority] = sys_setpriority;
    syscall_table[__NR_statfs] = sys_statfs;
    syscall_table[__NR_fstatfs] = sys_fstatfs;
    syscall_table[__NR_syslog] = sys_syslog;
    syscall_table[__NR_setitimer] = sys_setitimer;
    syscall_table[__NR_getitimer] = sys_getitimer;
    syscall_table[__NR_stat] = sys_stat;
    syscall_table[__NR_lstat] = sys_lstat;
    syscall_table[__NR_fstat] = sys_fstat;
    syscall_table[__NR_vhangup] = sys_vhangup;
    syscall_table[__NR_wait4] = sys_wait4;
    syscall_table[__NR_swapoff] = sys_swapoff;
    syscall_table[__NR_sysinfo] = sys_sysinfo;
    syscall_table[__NR_fsync] = sys_fsync;
    syscall_table[__NR_sigreturn] = sys_sigreturn;
    syscall_table[__NR_clone] = sys_clone;
    syscall_table[__NR_setdomainname] = sys_setdomainname;
    syscall_table[__NR_uname] = sys_uname;
    syscall_table[__NR_adjtimex] = sys_adjtimex;
    syscall_table[__NR_mprotect] = sys_mprotect;
    syscall_table[__NR_sigprocmask] = sys_sigprocmask;
    syscall_table[__NR_init_module] = sys_init_module;
    syscall_table[__NR_delete_module] = sys_delete_module;
    syscall_table[__NR_quotactl] = sys_quotactl;
    syscall_table[__NR_getpgid] = sys_getpgid;
    syscall_table[__NR_fchdir] = sys_fchdir;
    syscall_table[__NR_bdflush] = sys_bdflush;
    syscall_table[__NR_sysfs] = sys_sysfs;
    syscall_table[__NR_personality] = sys_personality;
    syscall_table[__NR_setfsuid] = sys_setfsuid;
    syscall_table[__NR_setfsgid] = sys_setfsgid;
    syscall_table[__NR_getdents] = sys_getdents;
    syscall_table[__NR__newselect] = sys__newselect;
    syscall_table[__NR_flock] = sys_flock;
    syscall_table[__NR_msync] = sys_msync;
    syscall_table[__NR_readv] = sys_readv;
    syscall_table[__NR_writev] = sys_writev;
    syscall_table[__NR_getsid] = sys_getsid;
    syscall_table[__NR_fdatasync] = sys_fdatasync;
    syscall_table[__NR__sysctl] = sys__sysctl;
    syscall_table[__NR_mlock] = sys_mlock;
    syscall_table[__NR_munlock] = sys_munlock;
    syscall_table[__NR_mlockall] = sys_mlockall;
    syscall_table[__NR_munlockall] = sys_munlockall;
    syscall_table[__NR_sched_setparam] = sys_sched_setparam;
    syscall_table[__NR_sched_getparam] = sys_sched_getparam;
    syscall_table[__NR_sched_setscheduler] = sys_sched_setscheduler;
    syscall_table[__NR_sched_getscheduler] = sys_sched_getscheduler;
    syscall_table[__NR_sched_yield] = sys_sched_yield;
    syscall_table[__NR_sched_get_priority_max] = sys_sched_get_priority_max;
    syscall_table[__NR_sched_get_priority_min] = sys_sched_get_priority_min;
    syscall_table[__NR_sched_rr_get_interval] = sys_sched_rr_get_interval;
    syscall_table[__NR_nanosleep] = sys_nanosleep;
    syscall_table[__NR_mremap] = sys_mremap;
    syscall_table[__NR_setresuid] = sys_setresuid;
    syscall_table[__NR_getresuid] = sys_getresuid;
    syscall_table[__NR_poll] = sys_poll;
    syscall_table[__NR_nfsservctl] = sys_nfsservctl;
    syscall_table[__NR_setresgid] = sys_setresgid;
    syscall_table[__NR_getresgid] = sys_getresgid;
    syscall_table[__NR_prctl] = sys_prctl;
    syscall_table[__NR_rt_sigreturn] = sys_rt_sigreturn;
    syscall_table[__NR_rt_sigaction] = sys_rt_sigaction;
    syscall_table[__NR_rt_sigprocmask] = sys_rt_sigprocmask;
    syscall_table[__NR_rt_sigpending] = sys_rt_sigpending;
    syscall_table[__NR_rt_sigtimedwait] = sys_rt_sigtimedwait;
    syscall_table[__NR_rt_sigqueueinfo] = sys_rt_sigqueueinfo;
    syscall_table[__NR_rt_sigsuspend] = sys_rt_sigsuspend;
    syscall_table[__NR_pread64] = sys_pread64;
    syscall_table[__NR_pwrite64] = sys_pwrite64;
    syscall_table[__NR_chown] = sys_chown;
    syscall_table[__NR_getcwd] = sys_getcwd;
    syscall_table[__NR_capget] = sys_capget;
    syscall_table[__NR_capset] = sys_capset;
    syscall_table[__NR_sigaltstack] = sys_sigaltstack;
    syscall_table[__NR_sendfile] = sys_sendfile;
    syscall_table[__NR_vfork] = sys_vfork;
    syscall_table[__NR_ugetrlimit] = sys_ugetrlimit;
    syscall_table[__NR_mmap2] = sys_mmap2;
    syscall_table[__NR_truncate64] = sys_truncate64;
    syscall_table[__NR_ftruncate64] = sys_ftruncate64;
    syscall_table[__NR_stat64] = sys_stat64;
    syscall_table[__NR_lstat64] = sys_lstat64;
    syscall_table[__NR_fstat64] = sys_fstat64;
    syscall_table[__NR_lchown32] = sys_lchown32;
    syscall_table[__NR_getuid32] = sys_getuid32;
    syscall_table[__NR_getgid32] = sys_getgid32;
    syscall_table[__NR_geteuid32] = sys_geteuid32;
    syscall_table[__NR_getegid32] = sys_getegid32;
    syscall_table[__NR_setreuid32] = sys_setreuid32;
    syscall_table[__NR_setregid32] = sys_setregid32;
    syscall_table[__NR_getgroups32] = sys_getgroups32;
    syscall_table[__NR_setgroups32] = sys_setgroups32;
    syscall_table[__NR_fchown32] = sys_fchown32;
    syscall_table[__NR_setresuid32] = sys_setresuid32;
    syscall_table[__NR_getresuid32] = sys_getresuid32;
    syscall_table[__NR_setresgid32] = sys_setresgid32;
    syscall_table[__NR_getresgid32] = sys_getresgid32;
    syscall_table[__NR_chown32] = sys_chown32;
    syscall_table[__NR_setuid32] = sys_setuid32;
    syscall_table[__NR_setgid32] = sys_setgid32;
    syscall_table[__NR_setfsuid32] = sys_setfsuid32;
    syscall_table[__NR_setfsgid32] = sys_setfsgid32;
    syscall_table[__NR_getdents64] = sys_getdents64;
    syscall_table[__NR_pivot_root] = sys_pivot_root;
    syscall_table[__NR_mincore] = sys_mincore;
    syscall_table[__NR_madvise] = sys_madvise;
    syscall_table[__NR_fcntl64] = sys_fcntl64;
    syscall_table[__NR_gettid] = sys_gettid;
    syscall_table[__NR_readahead] = sys_readahead;
    syscall_table[__NR_setxattr] = sys_setxattr;
    syscall_table[__NR_lsetxattr] = sys_lsetxattr;
    syscall_table[__NR_fsetxattr] = sys_fsetxattr;
    syscall_table[__NR_getxattr] = sys_getxattr;
    syscall_table[__NR_lgetxattr] = sys_lgetxattr;
    syscall_table[__NR_fgetxattr] = sys_fgetxattr;
    syscall_table[__NR_listxattr] = sys_listxattr;
    syscall_table[__NR_llistxattr] = sys_llistxattr;
    syscall_table[__NR_flistxattr] = sys_flistxattr;
    syscall_table[__NR_removexattr] = sys_removexattr;
    syscall_table[__NR_lremovexattr] = sys_lremovexattr;
    syscall_table[__NR_fremovexattr] = sys_fremovexattr;
    syscall_table[__NR_tkill] = sys_tkill;
    syscall_table[__NR_sendfile64] = sys_sendfile64;
    syscall_table[__NR_futex] = sys_futex;
    syscall_table[__NR_sched_setaffinity] = sys_sched_setaffinity;
    syscall_table[__NR_sched_getaffinity] = sys_sched_getaffinity;
    syscall_table[__NR_io_setup] = sys_io_setup;
    syscall_table[__NR_io_destroy] = sys_io_destroy;
    syscall_table[__NR_io_getevents] = sys_io_getevents;
    syscall_table[__NR_io_submit] = sys_io_submit;
    syscall_table[__NR_io_cancel] = sys_io_cancel;
    syscall_table[__NR_exit_group] = sys_exit_group;
    syscall_table[__NR_lookup_dcookie] = sys_lookup_dcookie;
    syscall_table[__NR_epoll_create] = sys_epoll_create;
    syscall_table[__NR_epoll_ctl] = sys_epoll_ctl;
    syscall_table[__NR_epoll_wait] = sys_epoll_wait;
    syscall_table[__NR_remap_file_pages] = sys_remap_file_pages;
    syscall_table[__NR_set_tid_address] = sys_set_tid_address;
    syscall_table[__NR_timer_create] = sys_timer_create;
    syscall_table[__NR_timer_settime] = sys_timer_settime;
    syscall_table[__NR_timer_gettime] = sys_timer_gettime;
    syscall_table[__NR_timer_getoverrun] = sys_timer_getoverrun;
    syscall_table[__NR_timer_delete] = sys_timer_delete;
    syscall_table[__NR_clock_settime] = sys_clock_settime;
    syscall_table[__NR_clock_gettime] = sys_clock_gettime;
    syscall_table[__NR_clock_getres] = sys_clock_getres;
    syscall_table[__NR_clock_nanosleep] = sys_clock_nanosleep;
    syscall_table[__NR_statfs64] = sys_statfs64;
    syscall_table[__NR_fstatfs64] = sys_fstatfs64;
    syscall_table[__NR_tgkill] = sys_tgkill;
    syscall_table[__NR_utimes] = sys_utimes;
    syscall_table[__NR_fadvise64_64] = sys_fadvise64_64;
    syscall_table[__NR_mq_open] = sys_mq_open;
    syscall_table[__NR_mq_unlink] = sys_mq_unlink;
    syscall_table[__NR_mq_timedsend] = sys_mq_timedsend;
    syscall_table[__NR_mq_timedreceive] = sys_mq_timedreceive;
    syscall_table[__NR_mq_notify] = sys_mq_notify;
    syscall_table[__NR_mq_getsetattr] = sys_mq_getsetattr;
    syscall_table[__NR_waitid] = sys_waitid;
    syscall_table[__NR_add_key] = sys_add_key;
    syscall_table[__NR_request_key] = sys_request_key;
    syscall_table[__NR_keyctl] = sys_keyctl;
    syscall_table[__NR_vserver] = sys_vserver;
    syscall_table[__NR_ioprio_set] = sys_ioprio_set;
    syscall_table[__NR_ioprio_get] = sys_ioprio_get;
    syscall_table[__NR_inotify_init] = sys_inotify_init;
    syscall_table[__NR_inotify_add_watch] = sys_inotify_add_watch;
    syscall_table[__NR_inotify_rm_watch] = sys_inotify_rm_watch;
    syscall_table[__NR_mbind] = sys_mbind;
    syscall_table[__NR_get_mempolicy] = sys_get_mempolicy;
    syscall_table[__NR_set_mempolicy] = sys_set_mempolicy;
    syscall_table[__NR_openat] = sys_openat;
    syscall_table[__NR_mkdirat] = sys_mkdirat;
    syscall_table[__NR_mknodat] = sys_mknodat;
    syscall_table[__NR_fchownat] = sys_fchownat;
    syscall_table[__NR_futimesat] = sys_futimesat;
    syscall_table[__NR_fstatat64] = sys_fstatat64;
    syscall_table[__NR_unlinkat] = sys_unlinkat;
    syscall_table[__NR_renameat] = sys_renameat;
    syscall_table[__NR_linkat] = sys_linkat;
    syscall_table[__NR_symlinkat] = sys_symlinkat;
    syscall_table[__NR_readlinkat] = sys_readlinkat;
    syscall_table[__NR_fchmodat] = sys_fchmodat;
    syscall_table[__NR_faccessat] = sys_faccessat;
    syscall_table[__NR_pselect6] = sys_pselect6;
    syscall_table[__NR_ppoll] = sys_ppoll;
    syscall_table[__NR_unshare] = sys_unshare;
    syscall_table[__NR_set_robust_list] = sys_set_robust_list;
    syscall_table[__NR_get_robust_list] = sys_get_robust_list;
    syscall_table[__NR_splice] = sys_splice;
    syscall_table[__NR_tee] = sys_tee;
    syscall_table[__NR_vmsplice] = sys_vmsplice;
    syscall_table[__NR_move_pages] = sys_move_pages;
    syscall_table[__NR_getcpu] = sys_getcpu;
    syscall_table[__NR_epoll_pwait] = sys_epoll_pwait;
    syscall_table[__NR_kexec_load] = sys_kexec_load;
    syscall_table[__NR_utimensat] = sys_utimensat;
    syscall_table[__NR_signalfd] = sys_signalfd;
    syscall_table[__NR_timerfd_create] = sys_timerfd_create;
    syscall_table[__NR_eventfd] = sys_eventfd;
    syscall_table[__NR_fallocate] = sys_fallocate;
    syscall_table[__NR_timerfd_settime] = sys_timerfd_settime;
    syscall_table[__NR_timerfd_gettime] = sys_timerfd_gettime;
    syscall_table[__NR_signalfd4] = sys_signalfd4;
    syscall_table[__NR_eventfd2] = sys_eventfd2;
    syscall_table[__NR_epoll_create1] = sys_epoll_create1;
    syscall_table[__NR_dup3] = sys_dup3;
    syscall_table[__NR_pipe2] = sys_pipe2;
    syscall_table[__NR_inotify_init1] = sys_inotify_init1;
    syscall_table[__NR_preadv] = sys_preadv;
    syscall_table[__NR_pwritev] = sys_pwritev;
    syscall_table[__NR_prlimit64] = sys_prlimit64;
    syscall_table[__NR_name_to_handle_at] = sys_name_to_handle_at;
    syscall_table[__NR_open_by_handle_at] = sys_open_by_handle_at;
    syscall_table[__NR_clock_adjtime] = sys_clock_adjtime;
    syscall_table[__NR_syncfs] = sys_syncfs;
    syscall_table[__NR_sendmmsg] = sys_sendmmsg;
    syscall_table[__NR_setns] = sys_setns;
    syscall_table[__NR_process_vm_readv] = sys_process_vm_readv;
    syscall_table[__NR_process_vm_writev] = sys_process_vm_writev;
#ifdef CONFIG_ARCH_ARM
    syscall_table[__NR_pciconfig_iobase] = sys_pciconfig_iobase;
    syscall_table[__NR_pciconfig_read] = sys_pciconfig_read;
    syscall_table[__NR_pciconfig_write] = sys_pciconfig_write;
    syscall_table[__NR_socket] = sys_socket;
    syscall_table[__NR_bind] = sys_bind;
    syscall_table[__NR_connect] = sys_connect;
    syscall_table[__NR_listen] = sys_listen;
    syscall_table[__NR_accept] = sys_accept;
    syscall_table[__NR_getsockname] = sys_getsockname;
    syscall_table[__NR_getpeername] = sys_getpeername;
    syscall_table[__NR_socketpair] = sys_socketpair;
    syscall_table[__NR_send] = sys_send;
    syscall_table[__NR_sendto] = sys_sendto;
    syscall_table[__NR_recv] = sys_recv;
    syscall_table[__NR_recvfrom] = sys_recvfrom;
    syscall_table[__NR_shutdown] = sys_shutdown;
    syscall_table[__NR_setsockopt] = sys_setsockopt;
    syscall_table[__NR_getsockopt] = sys_getsockopt;
    syscall_table[__NR_sendmsg] = sys_sendmsg;
    syscall_table[__NR_recvmsg] = sys_recvmsg;
    syscall_table[__NR_semop] = sys_semop;
    syscall_table[__NR_semget] = sys_semget;
    syscall_table[__NR_semctl] = sys_semctl;
    syscall_table[__NR_msgsnd] = sys_msgsnd;
    syscall_table[__NR_msgrcv] = sys_msgrcv;
    syscall_table[__NR_msgget] = sys_msgget;
    syscall_table[__NR_msgctl] = sys_msgctl;
    syscall_table[__NR_shmat] = sys_shmat;
    syscall_table[__NR_shmdt] = sys_shmdt;
    syscall_table[__NR_shmget] = sys_shmget;
    syscall_table[__NR_shmctl] = sys_shmctl;
    syscall_table[__NR_semtimedop] = sys_semtimedop;
    syscall_table[__NR_sync_file_range2] = sys_sync_file_range2;
    syscall_table[__NR_accept4] = sys_accept4;
    syscall_table[__NR_rt_tgsigqueueinfo] = sys_rt_tgsigqueueinfo;
    syscall_table[__NR_perf_event_open] = sys_perf_event_open;
    syscall_table[__NR_recvmmsg] = sys_recvmmsg;
    syscall_table[__NR_fanotify_init] = sys_fanotify_init;
    syscall_table[__NR_fanotify_mark] = sys_fanotify_mark;
#endif /* CONFIG_ARCH_ARM */
}

void
refos_initialise_os_minimal(void)
{
    /* Initialise userspace allocator helper libraries. */
    csalloc_init(PROCCSPACE_ALLOC_REGION_START, PROCCSPACE_ALLOC_REGION_END);
    walloc_init(PROCESS_WALLOC_START, PROCESS_WALLOC_END);
}

void refos_initialise_selfloader(void)
{
    /* Initialise userspace allocator helper libraries. */
    csalloc_init(PROCCSPACE_SELFLOADER_CSPACE_START, PROCCSPACE_SELFLOADER_CSPACE_END);
    walloc_init(PROCESS_SELFLOADER_RESERVED_READELF,
        PROCESS_SELFLOADER_RESERVED_READELF + PROCESS_SELFLOADER_RESERVED_READELF_SIZE);
}

void
refos_initialise_timer(void)
{
    /* Temporarily use seL4_DebugPutChar before printf is set up. On release kernel this will
       do nothing. */
    refos_seL4_debug_override_stdout();

    /* We first initialise the cspace allocator statically, as MMap and heap (which malloc 
       depend on) needs this. */
    csalloc_init_static(PROCCSPACE_ALLOC_REGION_START, PROCCSPACE_ALLOC_REGION_END,
            _refosUtilCSpaceStatic, REFOS_UTIL_CSPACE_STATIC_SIZE);

    /* Initialise dynamic MMap and heap. */
    refosio_init_morecore(refos_static_param_procinfo());

    /* Initialise userspace allocator helper libraries. */
    walloc_init(PROCESS_WALLOC_START, PROCESS_WALLOC_END);

    /* Write to the STDIO output device. */
    refos_override_stdio(NULL, NULL);
    refos_setup_dataspace_stdio(REFOS_DEFAULT_STDIO_DSPACE);

    /* Initialise file descriptor table. */
    filetable_init_default();

    /* Initialise default environment variables. */
    _refosEnv[0] = NULL;
    __environ = _refosEnv;
    setenv("SHELL", "/fileserv/terminal", true);
    setenv("PWD", "/", true);
    #ifdef CONFIG_REFOS_TIMEZONE
        setenv("TZ", CONFIG_REFOS_TIMEZONE, true);
    #endif
}

void
refos_initialise(void)
{
    /* Temporarily use seL4_DebugPutChar before printf is set up. On release kernel this will
       do nothing. */
    refos_seL4_debug_override_stdout();

    /* We first initialise the cspace allocator statically, as MMap and heap (which malloc 
       depend on) needs this. */
    csalloc_init_static(PROCCSPACE_ALLOC_REGION_START, PROCCSPACE_ALLOC_REGION_END,
            _refosUtilCSpaceStatic, REFOS_UTIL_CSPACE_STATIC_SIZE);

    /* Initialise dynamic MMap and heap. */
    refosio_init_morecore(refos_static_param_procinfo());

    /* Initialise userspace allocator helper libraries. */
    walloc_init(PROCESS_WALLOC_START, PROCESS_WALLOC_END);

    /* Write to the STDIO output device. */
    refos_override_stdio(NULL, NULL);
    refos_setup_dataspace_stdio(REFOS_DEFAULT_STDIO_DSPACE);

    /* Initialise file descriptor table. */
    filetable_init_default();

    /* Initialise timer so we can sleep. */
    refos_init_timer(REFOS_DEFAULT_TIMER_DSPACE);

    /* Initialise default environment variables. */
    _refosEnv[0] = NULL;
    __environ = _refosEnv;
    setenv("SHELL", "/fileserv/terminal", true);
    setenv("PWD", "/", true);
    #ifdef CONFIG_REFOS_TIMEZONE
        setenv("TZ", CONFIG_REFOS_TIMEZONE, true);
    #endif
}

char *
refos_static_param(void)
{
    return (char*)(PROCESS_STATICPARAM_ADDR);
}

struct sl_procinfo_s *
refos_static_param_procinfo(void)
{
    return (struct sl_procinfo_s *)(PROCESS_STATICPARAM_PROCINFO_ADDR);
}
