/* RISC-V system call numbers (the generic Linux table, shared with ARM64): research R5. */
#ifndef VO_PLATFORM_H
#define VO_PLATFORM_H

#define SYS_getcwd 17
#define SYS_unlinkat 35
#define SYS_openat 56
#define SYS_close 57
#define SYS_getdents64 61
#define SYS_read 63
#define SYS_write 64
#define SYS_ppoll 73
#define SYS_readlinkat 78
#define SYS_exit_group 94
#define SYS_clock_gettime 113
#define SYS_mmap 222
#define SYS_renameat2 276
#define SYS_statx 291

#define SYS_socket 198
#define SYS_connect 203
#define SYS_sendto 206
#define SYS_recvfrom 207

/* spec 011: serving */
#define SYS_signalfd4 74
#define SYS_rt_sigprocmask 135
#define SYS_bind 200
#define SYS_listen 201
#define SYS_setsockopt 208
#define SYS_accept4 242

/* spec 013 */
#define SYS_getrandom 278

#define O_DIRECTORY 0x10000
#define O_NOFOLLOW 0x20000

/* spec 016: tasks (one process each) */
#define SYS_clone 220
#define SYS_wait4 260
#define SYS_pipe2 59
#define SYS_dup3 24
#define SYS_fcntl 25
#define SYS_prctl 167
#define SYS_kill 129
#define SYS_getppid 173
#define SYS_getpid 172

#endif
