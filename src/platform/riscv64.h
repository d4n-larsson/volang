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

#define O_DIRECTORY 0x10000

#endif
