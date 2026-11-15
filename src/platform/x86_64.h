/* x86-64 system call numbers (its own Linux table), restricted to the calls of research R5. */
#ifndef VO_PLATFORM_H
#define VO_PLATFORM_H

#define SYS_read 0
#define SYS_write 1
#define SYS_close 3
#define SYS_mmap 9
#define SYS_getcwd 79
#define SYS_getdents64 217
#define SYS_clock_gettime 228
#define SYS_exit_group 231
#define SYS_openat 257
#define SYS_unlinkat 263
#define SYS_readlinkat 267
#define SYS_ppoll 271
#define SYS_renameat2 316
#define SYS_statx 332

#define SYS_socket 41
#define SYS_connect 42
#define SYS_sendto 44
#define SYS_recvfrom 45

/* spec 011: serving */
#define SYS_rt_sigprocmask 14
#define SYS_bind 49
#define SYS_listen 50
#define SYS_setsockopt 54
#define SYS_accept4 288
#define SYS_signalfd4 289

#define O_DIRECTORY 0x10000

#endif
