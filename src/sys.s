# Process startup, syscall wrappers, buffered output, file reading, arena allocator.
    .include "defs.inc"

    .section .data
    .align 3
    .globl oom_hook
oom_hook:   .dword oom_default
    .globl stack_floor
stack_floor: .dword 0
arena_ptr:  .dword 0
arena_end:  .dword 0
arena_total: .dword 0
    .globl arena_cap
arena_cap:  .dword 0x20000000       # 512 MiB

    .section .bss
    .align 4
out_buf1:   .space 65536
out_buf2:   .space 65536
out_len1:   .space 8
out_len2:   .space 8

    .section .rodata
oom_msg:    .string "error[T0003]: internal error: out of memory\n"

    .text
# void _start(void)
    .globl _start
_start:
    ld t0, 0(sp)                # argc
    addi t1, sp, 8              # argv
    li a0, 0
    li a1, 0x4000000            # 64 MiB private stack (research §9)
    li a2, 3                    # PROT_READ | PROT_WRITE
    li a3, 0x22                 # MAP_PRIVATE | MAP_ANONYMOUS
    li a4, -1
    li a5, 0
    li a7, SYS_mmap
    ecall
    li t2, -4096
    bgeu a0, t2, 1f             # mmap failed: keep the kernel stack, no floor check
    la t2, stack_floor
    li t3, 0x100000
    add t3, a0, t3
    sd t3, 0(t2)
    li t3, 0x4000000
    add sp, a0, t3
1:
    addi t2, t0, 1              # envp follows argv's NULL
    slli t2, t2, 3
    add t2, t1, t2
    la t3, envp
    sd t2, 0(t3)
    mv a0, t0
    mv a1, t1
    call vo_main
    call vo_exit
2:
    j 2b

    .section .bss
    .align 3
    .globl envp
envp:   .space 8
    .text

# void vo_exit(int code)
    .globl vo_exit
vo_exit:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd a0, 0(sp)
    call out_flush
    ld a0, 0(sp)
    li a7, SYS_exit_group
    ecall
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# long sys_read(int fd, void *buf, long n)
    .globl sys_read
sys_read:
    li a7, SYS_read
    ecall
    ret

# long sys_write(int fd, void *buf, long n)
    .globl sys_write
sys_write:
    li a7, SYS_write
    ecall
    ret

# long sys_open(char *path, int flags)
    .globl sys_open
sys_open:
    mv a2, a1
    mv a1, a0
    li a0, AT_FDCWD
    li a3, 0644
    li a7, SYS_openat
    ecall
    ret

# long sys_close(int fd)
    .globl sys_close
sys_close:
    li a7, SYS_close
    ecall
    ret

# long sys_getdents64(int fd, void *buf, long n)
    .globl sys_getdents64
sys_getdents64:
    li a7, SYS_getdents64
    ecall
    ret

# long sys_rename(char *from, char *to)
    .globl sys_rename
sys_rename:
    mv a3, a1
    mv a1, a0
    li a0, AT_FDCWD
    li a2, AT_FDCWD
    li a4, 0                    # flags
    li a7, SYS_renameat2
    ecall
    ret

# long sys_clock_ms(void)
    .globl sys_clock_ms
sys_clock_ms:
    addi sp, sp, -16
    li a0, 1                    # CLOCK_MONOTONIC
    mv a1, sp
    li a7, SYS_clock_gettime
    ecall
    ld t0, 0(sp)
    ld t1, 8(sp)
    li t2, 1000
    mul a0, t0, t2
    li t2, 1000000
    div t1, t1, t2
    add a0, a0, t1
    addi sp, sp, 16
    ret

# long sys_wait_stdin(long ms)
# Returns 1 when fd 0 is readable (or closed), 0 on timeout. ms < 0 waits forever.
    .globl sys_wait_stdin
sys_wait_stdin:
    addi sp, sp, -32
    sw zero, 0(sp)              # pollfd.fd = 0
    li t0, 1                    # POLLIN
    sh t0, 4(sp)
    sh zero, 6(sp)
    li a2, 0
    bltz a0, 1f
    li t1, 1000
    div t2, a0, t1
    rem t3, a0, t1
    li t1, 1000000
    mul t3, t3, t1
    sd t2, 16(sp)
    sd t3, 24(sp)
    addi a2, sp, 16
1:
    mv a0, sp
    li a1, 1
    li a3, 0
    li a4, 8
    li a7, SYS_ppoll
    ecall
    sltu a0, zero, a0           # > 0 -> 1; 0 -> 0 (errors count as readable)
    addi sp, sp, 32
    ret

# void write_all(int fd, char *p, long n)
    .globl write_all
write_all:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
1:
    blez s2, 2f
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call sys_write
    blez a0, 2f                 # error: give up rather than spin
    add s1, s1, a0
    sub s2, s2, a0
    j 1b
2:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void out_flush(void)
    .globl out_flush
out_flush:
    addi sp, sp, -16
    sd ra, 8(sp)
    li a0, 1
    la a1, out_buf1
    la t0, out_len1
    ld a2, 0(t0)
    sd zero, 0(t0)
    call write_all
    li a0, 2
    la a1, out_buf2
    la t0, out_len2
    ld a2, 0(t0)
    sd zero, 0(t0)
    call write_all
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void out_bytes(int fd, char *p, long n)
    .globl out_bytes
out_bytes:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    li t0, 1
    beq s0, t0, 1f
    li t0, 2
    beq s0, t0, 2f
    call write_all              # other fds: unbuffered
    j 9f
1:
    la s3, out_buf1
    la s4, out_len1
    j 3f
2:
    la s3, out_buf2
    la s4, out_len2
3:
    ld t0, 0(s4)
    add t1, t0, s2
    li t2, 65536
    ble t1, t2, 4f
    call out_flush              # buffer full: flush everything, keeps stdout/stderr ordered
    li t2, 65536
    blt s2, t2, 4f
    mv a0, s0                   # larger than the buffer: write directly
    mv a1, s1
    mv a2, s2
    call write_all
    j 9f
4:
    ld t0, 0(s4)
    add a0, s3, t0
    add t0, t0, s2
    sd t0, 0(s4)
    mv a1, s1
    mv a2, s2
    call mem_copy
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# void out_cstr(int fd, char *s)
    .globl out_cstr
out_cstr:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    mv a0, a1
    call cstr_len
    mv a2, a0
    mv a0, s0
    mv a1, s1
    call out_bytes
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void out_str(int fd, void *str)
    .globl out_str
out_str:
    addi sp, sp, -16
    sd ra, 8(sp)
    ld a2, STR_LEN(a1)
    addi a1, a1, STR_DATA
    call out_bytes
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# long read_whole_file(char *path, char **out)
# Returns the length, or a negative errno. The bytes are followed by a NUL.
    .globl read_whole_file
read_whole_file:
    addi sp, sp, -176
    sd ra, 168(sp)
    sd s0, 160(sp)
    sd s1, 152(sp)
    sd s2, 144(sp)
    sd s3, 136(sp)
    mv s1, a1
    li a1, O_RDONLY
    call sys_open
    bltz a0, 9f
    mv s0, a0
    mv a0, s0
    mv a1, sp                   # struct stat (128 bytes) at 0(sp)
    li a7, SYS_fstat
    ecall
    bltz a0, 8f
    ld t0, 48(sp)               # st_size (written by the kernel)
    mv s2, t0
    addi a0, s2, 1
    call alloc
    mv s3, a0
    li t0, 0
    sd t0, 128(sp)              # bytes read so far
1:
    ld t0, 128(sp)
    sub a2, s2, t0
    blez a2, 2f
    mv a0, s0
    add a1, s3, t0
    call sys_read
    bltz a0, 8f
    beqz a0, 2f
    ld t0, 128(sp)
    add t0, t0, a0
    sd t0, 128(sp)
    j 1b
2:
    mv a0, s0
    call sys_close
    sd s3, 0(s1)
    ld a0, 128(sp)
    j 9f
8:
    sd a0, 128(sp)
    mv a0, s0
    call sys_close
    ld a0, 128(sp)
9:
    ld ra, 168(sp)
    ld s0, 160(sp)
    ld s1, 152(sp)
    ld s2, 144(sp)
    ld s3, 136(sp)
    addi sp, sp, 176
    ret

# void *alloc(long size)
# ponytail: bump arena, never frees; long allocation-heavy runs hit the 512 MiB cap (R0006); upgrade: copying GC using the size:u32 object headers
    .globl alloc
alloc:
    addi a0, a0, 7
    andi a0, a0, -8
    la t0, arena_ptr
    ld t1, 0(t0)
    la t2, arena_end
    ld t3, 0(t2)
    add t4, t1, a0
    bgtu t4, t3, 1f
    beqz t1, 1f
    sd t4, 0(t0)
    mv a0, t1
    ret
1:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0                   # size
    li s1, 0x1000000            # 16 MiB chunk
    bleu s0, s1, 2f
    mv s1, s0
2:
    la t0, arena_total
    ld t1, 0(t0)
    add t1, t1, s1
    la t2, arena_cap
    ld t2, 0(t2)
    bgtu t1, t2, 8f
    li a0, 0
    mv a1, s1
    li a2, 3
    li a3, 0x22
    li a4, -1
    li a5, 0
    li a7, SYS_mmap
    ecall
    li t2, -4096
    bgeu a0, t2, 8f
    la t0, arena_total
    ld t1, 0(t0)
    add t1, t1, s1
    sd t1, 0(t0)
    add t1, a0, s0
    la t0, arena_ptr
    sd t1, 0(t0)
    add t1, a0, s1
    la t0, arena_end
    sd t1, 0(t0)
    j 9f
8:
    la t0, oom_hook
    ld t0, 0(t0)
    jalr t0
    li a0, 0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *alloc_obj(int kind, long size)
# size is the payload size; the result points at the 8-byte header.
    .globl alloc_obj
alloc_obj:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    addi s1, a1, 8
    mv a0, s1
    call alloc
    sw s1, 0(a0)
    sw s0, 4(a0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void oom_default(void)
    .globl oom_default
oom_default:
    addi sp, sp, -16
    sd ra, 8(sp)
    li a0, 2
    la a1, oom_msg
    call out_cstr
    li a0, EXIT_INTERNAL
    call vo_exit
    ld ra, 8(sp)
    addi sp, sp, 16
    ret
