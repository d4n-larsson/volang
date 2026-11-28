/* ARM64 system call numbers (the generic Linux table, shared with RISC-V): research R5. */
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
#define SYS_munmap 215
#define SYS_renameat2 276
#define SYS_statx 291

#define SYS_socket 198
#define SYS_socketpair 199 /* spec 063 */
#define SYS_sendmsg 211
#define SYS_recvmsg 212
#define SYS_connect 203
#define SYS_sendto 206
#define SYS_recvfrom 207

/* spec 011: serving */
#define SYS_signalfd4 74
#define SYS_rt_sigprocmask 135
#define SYS_bind 200
#define SYS_listen 201
#define SYS_setsockopt 208
#define SYS_getsockname 204
#define SYS_accept4 242

/* spec 013 */
#define SYS_getrandom 278

#define O_DIRECTORY 0x4000 /* 0x10000 is O_DIRECT here */
#define O_NOFOLLOW 0x8000

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

/* spec 017: file changes and running programs */
#define SYS_mkdirat 34
#define SYS_execve 221
#define SYS_setpgid 154
#define SYS_fchdir 50
#define SYS_close_range 436
#define SYS_pread64 67 /* spec 026: SQLite's file layer (src/sqlite_vfs.c) */
#define SYS_pwrite64 68
#define SYS_fsync 82
#define SYS_ftruncate 46

#define SYS_lseek 62 /* spec 034: vo build and the program part of a built executable (src/build.c) */
#define SYS_fchmod 52
#endif
