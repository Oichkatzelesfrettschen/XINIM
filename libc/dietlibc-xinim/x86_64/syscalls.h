/*
 * XINIM System Call Numbers for x86_64
 *
 * These are XINIM microkernel syscall numbers, NOT Linux syscall numbers.
 * This file replaces the standard dietlibc Linux syscalls with XINIM syscalls.
 */

/* Debug/Legacy Syscalls */
#define __NR_debug_write                         1
#define __NR_monotonic_ns                        2

/* File I/O (POSIX.1-2017) */
#define __NR_open                                3
#define __NR_close                               4
#define __NR_read                                5
#define __NR_write                               6
#define __NR_lseek                               7
#define __NR_stat                                8
#define __NR_fstat                               9
#define __NR_access                             10
#define __NR_dup                                11
#define __NR_dup2                               12
#define __NR_pipe                               13
#define __NR_ioctl                              14
#define __NR_fcntl                              15

/* Directory Operations */
#define __NR_mkdir                              16
#define __NR_rmdir                              17
#define __NR_chdir                              18
#define __NR_getcwd                             19
#define __NR_link                               20
#define __NR_unlink                             21
#define __NR_rename                             22
#define __NR_chmod                              23
#define __NR_chown                              24

/* Process Management */
#define __NR_exit                               25
#define __NR_fork                               26
#define __NR_execve                             27
#define __NR_wait4                              28
#define __NR_getpid                             29
#define __NR_getppid                            30
#define __NR_kill                               31
#define __NR_signal                             32
#define __NR_rt_sigaction                       33

/* Memory Management */
#define __NR_brk                                34
#define __NR_mmap                               35
#define __NR_munmap                             36
#define __NR_mprotect                           37

/* IPC (XINIM Lattice IPC) */
#define __NR_lattice_connect                    38
#define __NR_lattice_send                       39
#define __NR_lattice_recv                       40
#define __NR_lattice_close                      41

/* Time */
#define __NR_time                               42
#define __NR_gettimeofday                       43
#define __NR_clock_gettime                      44
#define __NR_nanosleep                          45

/* User/Group IDs */
#define __NR_getuid                             46
#define __NR_geteuid                            47
#define __NR_getgid                             48
#define __NR_getegid                            49
#define __NR_setuid                             50
#define __NR_setgid                             51

/* Unimplemented/TODO syscalls */
#define __NR_lstat                              -1
#define __NR_poll                               -1
#define __NR_pread                              -1
#define __NR_pwrite                             -1
#define __NR_readv                              -1
#define __NR_writev                             -1
#define __NR_select                             -1
#define __NR_sched_yield                        -1
#define __NR_mremap                             -1
#define __NR_msync                              -1
#define __NR_mincore                            -1
#define __NR_madvise                            -1
#define __NR_shmget                             -1
#define __NR_shmat                              -1
#define __NR_shmctl                             -1
#define __NR_pause                              -1
#define __NR_getitimer                          -1
#define __NR_alarm                              -1
#define __NR_setitimer                          -1

/* Networking syscalls - TODO: implement via VFS/network server */
#define __NR_sendfile                           -1
#define __NR_socket                             -1
#define __NR_connect                            -1
#define __NR_accept                             -1
#define __NR_sendto                             -1
#define __NR_recvfrom                           -1
#define __NR_sendmsg                            -1
#define __NR_recvmsg                            -1
#define __NR_shutdown                           -1
#define __NR_bind                               -1
#define __NR_listen                             -1
#define __NR_getsockname                        -1
#define __NR_getpeername                        -1
#define __NR_socketpair                         -1
#define __NR_setsockopt                         -1
#define __NR_getsockopt                         -1

/* Process management - partially implemented */
#define __NR_clone                              -1  /* TODO */
#define __NR_vfork                              -1  /* TODO: use fork */
#define __NR_uname                              -1  /* TODO */

/* System V IPC - use XINIM lattice IPC instead */
#define __NR_semget                             -1
#define __NR_semop                              -1
#define __NR_semctl                             -1
#define __NR_shmdt                              -1
#define __NR_msgget                             -1
#define __NR_msgsnd                             -1
#define __NR_msgrcv                             -1
#define __NR_msgctl                             -1

/* File operations - TODO */
#define __NR_flock                              -1
#define __NR_fsync                              -1
#define __NR_fdatasync                          -1
#define __NR_truncate                           -1
#define __NR_ftruncate                          -1
#define __NR_getdents                           -1
#define __NR_fchdir                             -1
#define __NR_creat                              -1
#define __NR_symlink                            -1
#define __NR_readlink                           -1
#define __NR_fchmod                             -1
#define __NR_fchown                             -1
#define __NR_lchown                             -1
#define __NR_umask                              -1

/* Resource management - TODO */
#define __NR_getrlimit                          -1
#define __NR_getrusage                          -1
#define __NR_sysinfo                            -1
#define __NR_times                              -1
#define __NR_ptrace                             -1
#define __NR_syslog                             -1

/* Process groups - TODO */
#define __NR_setpgid                            -1
#define __NR_getpgrp                            -1
#define __NR_setsid                             -1
#define __NR_setreuid                           -1
#define __NR_setregid                           -1
#define __NR_getgroups                          -1
#define __NR_setgroups                          -1
#define __NR_setresuid                          -1
#define __NR_getresuid                          -1
#define __NR_setresgid                          -1
#define __NR_getresgid                          -1
#define __NR_getpgid                            -1
#define __NR_setfsuid                           -1
#define __NR_setfsgid                           -1
#define __NR_getsid                             -1
#define __NR_capget                             -1
#define __NR_capset                             -1

/* Signals - TODO */
#define __NR_rt_sigpending                      -1
#define __NR_rt_sigtimedwait                    -1
#define __NR_rt_sigqueueinfo                    -1
#define __NR_rt_sigsuspend                      -1
#define __NR_rt_sigprocmask                     -1
#define __NR_rt_sigreturn                       -1
#define __NR_sigaltstack                        -1
#define __NR_tkill                              -1
#define __NR_tgkill                             -1

/* File system operations - TODO */
#define __NR_utime                              -1
#define __NR_mknod                              -1
#define __NR_ustat                              -1
#define __NR_statfs                             -1
#define __NR_fstatfs                            -1
#define __NR_sysfs                              -1

/* Scheduling - TODO */
#define __NR_getpriority                        -1
#define __NR_setpriority                        -1
#define __NR_sched_setparam                     -1
#define __NR_sched_getparam                     -1
#define __NR_sched_setscheduler                 -1
#define __NR_sched_getscheduler                 -1
#define __NR_sched_get_priority_max             -1
#define __NR_sched_get_priority_min             -1
#define __NR_sched_rr_get_interval              -1
#define __NR_sched_setaffinity                  -1
#define __NR_sched_getaffinity                  -1

/* Memory locking - TODO */
#define __NR_mlock                              -1
#define __NR_munlock                            -1
#define __NR_mlockall                           -1
#define __NR_munlockall                         -1

/* System administration - TODO */
#define __NR_vhangup                            -1
#define __NR_modify_ldt                         -1
#define __NR_pivot_root                         -1
#define __NR__sysctl                            -1
#define __NR_prctl                              -1
#define __NR_arch_prctl                         -1
#define __NR_adjtimex                           -1
#define __NR_setrlimit                          -1
#define __NR_chroot                             -1
#define __NR_sync                               -1
#define __NR_acct                               -1
#define __NR_settimeofday                       -1
#define __NR_mount                              -1
#define __NR_umount2                            -1
#define __NR_swapon                             -1
#define __NR_swapoff                            -1
#define __NR_reboot                             -1
#define __NR_sethostname                        -1
#define __NR_setdomainname                      -1
#define __NR_iopl                               -1
#define __NR_ioperm                             -1

/* Obsolete/not supported */
#define __NR_create_module                      -1
#define __NR_init_module                        -1
#define __NR_delete_module                      -1
#define __NR_get_kernel_syms                    -1
#define __NR_query_module                       -1
#define __NR_quotactl                           -1
#define __NR_nfsservctl                         -1
#define __NR_getpmsg                            -1
#define __NR_putpmsg                            -1
#define __NR_afs_syscall                        -1
#define __NR_tuxcall                            -1
#define __NR_security                           -1
#define __NR_uselib                             -1
#define __NR_personality                        -1

/* Thread/process management - TODO */
#define __NR_gettid                             -1
#define __NR_set_tid_address                    -1
#define __NR_exit_group                         -1

/* Extended attributes - TODO */
#define __NR_readahead                          -1
#define __NR_setxattr                           -1
#define __NR_lsetxattr                          -1
#define __NR_fsetxattr                          -1
#define __NR_getxattr                           -1
#define __NR_lgetxattr                          -1
#define __NR_fgetxattr                          -1
#define __NR_listxattr                          -1
#define __NR_llistxattr                         -1
#define __NR_flistxattr                         -1
#define __NR_removexattr                        -1
#define __NR_lremovexattr                       -1
#define __NR_fremovexattr                       -1

/* Advanced features - TODO */
#define __NR_futex                              -1
#define __NR_set_thread_area                    -1
#define __NR_io_setup                           -1
#define __NR_io_destroy                         -1
#define __NR_io_getevents                       -1
#define __NR_io_submit                          -1
#define __NR_io_cancel                          -1
#define __NR_get_thread_area                    -1
#define __NR_lookup_dcookie                     -1
#define __NR_epoll_create                       -1
#define __NR_epoll_ctl_old                      -1
#define __NR_epoll_wait_old                     -1
#define __NR_remap_file_pages                   -1
#define __NR_getdents64                         -1
#define __NR_restart_syscall                    -1
#define __NR_semtimedop                         -1
#define __NR_fadvise64                          -1

/* Timers - TODO */
#define __NR_timer_create                       -1
#define __NR_timer_settime                      -1
#define __NR_timer_gettime                      -1
#define __NR_timer_getoverrun                   -1
#define __NR_timer_delete                       -1
#define __NR_clock_settime                      -1
#define __NR_clock_getres                       -1
#define __NR_clock_nanosleep                    -1

/* Advanced epoll/polling - TODO */
#define __NR_epoll_wait                         -1
#define __NR_epoll_ctl                          -1
#define __NR_utimes                             -1
#define __NR_vserver                            -1

/* NUMA - TODO */
#define __NR_mbind                              -1
#define __NR_set_mempolicy                      -1
#define __NR_get_mempolicy                      -1

/* Message queues - use lattice IPC */
#define __NR_mq_open                            -1
#define __NR_mq_unlink                          -1
#define __NR_mq_timedsend                       -1
#define __NR_mq_timedreceive                    -1
#define __NR_mq_notify                          -1
#define __NR_mq_getsetattr                      -1

/* Modern syscalls - TODO */
#define __NR_kexec_load                         -1
#define __NR_waitid                             -1
#define __NR_add_key                            -1
#define __NR_request_key                        -1
#define __NR_keyctl                             -1
#define __NR_ioprio_set                         -1
#define __NR_ioprio_get                         -1
#define __NR_inotify_init                       -1
#define __NR_inotify_add_watch                  -1
#define __NR_inotify_rm_watch                   -1
#define __NR_migrate_pages                      -1
#define __NR_openat                             -1
#define __NR_mkdirat                            -1
#define __NR_mknodat                            -1
#define __NR_fchownat                           -1
#define __NR_futimesat                          -1
#define __NR_newfstatat                         -1
#define __NR_unlinkat                           -1
#define __NR_renameat                           -1
#define __NR_linkat                             -1
#define __NR_symlinkat                          -1
#define __NR_readlinkat                         -1
#define __NR_fchmodat                           -1
#define __NR_faccessat                          -1
#define __NR_pselect6                           -1
#define __NR_ppoll                              -1
#define __NR_unshare                            -1
#define __NR_set_robust_list                    -1
#define __NR_get_robust_list                    -1
#define __NR_splice                             -1
#define __NR_tee                                -1
#define __NR_sync_file_range                    -1
#define __NR_vmsplice                           -1
#define __NR_move_pages                         -1
#define __NR_utimensat                          -1
#define __NR_epoll_pwait                        -1
#define __NR_signalfd                           -1
#define __NR_timerfd                            -1
#define __NR_eventfd                            -1
#define __NR_fallocate                          -1
#define __NR_timerfd_settime                    -1
#define __NR_timerfd_gettime                    -1
#define __NR_accept4                            -1
#define __NR_signalfd4                          -1
#define __NR_eventfd2                           -1
#define __NR_epoll_create1                      -1
#define __NR_dup3                               -1
#define __NR_pipe2                              -1
#define __NR_inotify_init1                      -1
#define __NR_preadv                             -1
#define __NR_pwritev                            -1
#define __NR_rt_tgsigqueueinfo                  -1
#define __NR_perf_event_open                    -1
#define __NR_recvmmsg                           -1
#define __NR_fanotify_init                      -1
#define __NR_fanotify_mark                      -1
#define __NR_prlimit64                          -1
#define __NR_name_to_handle_at                  -1
#define __NR_open_by_handle_at                  -1
#define __NR_clock_adjtime                      -1
#define __NR_syncfs                             -1
#define __NR_sendmmsg                           -1
#define __NR_setns                              -1
#define __NR_getcpu                             -1
#define __NR_process_vm_readv                   -1
#define __NR_process_vm_writev                  -1
#define __NR_kcmp                               -1
#define __NR_finit_module                       -1
#define __NR_sched_setattr                      -1
#define __NR_sched_getattr                      -1
#define __NR_renameat2                          -1
#define __NR_seccomp                            -1
#define __NR_getrandom                          -1
#define __NR_memfd_create                       -1
#define __NR_kexec_file_load                    -1
#define __NR_bpf                                -1
#define __NR_execveat                           -1
#define __NR_userfaultfd                        -1
#define __NR_membarrier                         -1
#define __NR_mlock2                             -1
#define __NR_copy_file_range                    -1
#define __NR_preadv2                            -1
#define __NR_pwritev2                           -1
#define __NR_pkey_mprotect                      -1
#define __NR_pkey_alloc                         -1
#define __NR_pkey_free                          -1
#define __NR_statx                              -1
#define __NR_io_pgetevents                      -1
#define __NR_rseq                               -1

#if defined(__PIE__)

#define syscall_weak(name,wsym,sym) \
.text; \
.type wsym,@function; \
.weak wsym; \
.hidden wsym; \
wsym: ; \
.type sym,@function; \
.global sym; \
.hidden sym; \
sym: \
.ifge __NR_##name-256 ; \
	mov	$__NR_##name,%ax; \
	jmp	__unified_syscall_16bit@PLT;  \
.else ; \
	mov	$__NR_##name,%al; \
	jmp	__unified_syscall@PLT; \
.endif

#define syscall(name,sym) \
.text; \
.type sym,@function; \
.global sym; \
.hidden sym; \
sym: \
.ifge __NR_##name-256 ; \
	mov	$__NR_##name,%ax; \
	jmp	__unified_syscall_16bit@PLT; \
.else ; \
	mov	$__NR_##name,%al; \
	jmp	__unified_syscall@PLT; \
.endif

#elif defined(__PIC__)

#define syscall_weak(name,wsym,sym) \
.text; \
.type wsym,@function; \
.weak wsym; \
wsym: ; \
.type sym,@function; \
.global sym; \
sym: \
.ifge __NR_##name-256 ; \
	mov	$__NR_##name,%ax; \
	jmp	__unified_syscall_16bit@PLT;  \
.else ; \
	mov	$__NR_##name,%al; \
	jmp	__unified_syscall@PLT; \
.endif

#define syscall(name,sym) \
.text; \
.type sym,@function; \
.global sym; \
sym: \
.ifge __NR_##name-256 ; \
	mov	$__NR_##name,%ax; \
	jmp	__unified_syscall_16bit@PLT; \
.else ; \
	mov	$__NR_##name,%al; \
	jmp	__unified_syscall@PLT; \
.endif

#else

#define syscall_weak(name,wsym,sym) \
.text; \
.type wsym,@function; \
.weak wsym; \
wsym: ; \
.type sym,@function; \
.global sym; \
sym: \
.ifge __NR_##name-256 ; \
	mov	$__NR_##name,%ax; \
	jmp	__unified_syscall_16bit; \
.else ; \
	mov	$__NR_##name,%al; \
	jmp	__unified_syscall; \
.endif

#define syscall(name,sym) \
.text; \
.type sym,@function; \
.global sym; \
sym: \
.ifge __NR_##name-256 ; \
	mov	$__NR_##name,%ax; \
	jmp	__unified_syscall_16bit; \
.else ; \
	mov	$__NR_##name,%al; \
	jmp	__unified_syscall; \
.endif
#endif
