# I/O builtins: stdout/stderr, buffered stdin with deadline-aware waits, files, args, exit.
    .include "defs.inc"

    .section .bss
    .align 3
in_buf:     .space 65536
in_pos:     .space 8
in_len:     .space 8
in_eof:     .space 8

    .section .rodata
r0008: .string "R0008"
m_exit:     .string "Exit status $1 is outside 0-63."
m_eof:      .string "eof"
m_cannot_read:  .string "cannot read $1"
m_cannot_write: .string "cannot write $1"
s_nl:       .string "\n"

    .text
# void rt_print(int fd, void *s, int newline)
    .globl rt_print
rt_print:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a2
    call out_str
    beqz s1, 1f
    mv a0, s0
    la a1, s_nl
    call out_cstr
1:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# int stdin_fill(void *node)
# Refills the stdin buffer; 0 at end of input. Waits no longer than the deadline (R0009).
    .globl stdin_fill
stdin_fill:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la t0, in_eof
    ld t0, 0(t0)
    bnez t0, 8f
    call out_flush
    la t0, deadline_ms
    ld s1, 0(t0)
    li a0, -1
    beqz s1, 1f
    call sys_clock_ms
    sub a0, s1, a0
    blez a0, 7f
1:
    call sys_wait_stdin
    beqz a0, 7f
    li a0, 0
    la a1, in_buf
    li a2, 65536
    call sys_read
    blez a0, 6f
    la t0, in_len
    sd a0, 0(t0)
    la t0, in_pos
    sd zero, 0(t0)
    li a0, 1
    j 9f
6:
    la t0, in_eof
    li t1, 1
    sd t1, 0(t0)
    j 8f
7:
    mv a0, s0
    call rt_timeout
8:
    li a0, 0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *rt_read_line(void *node)
# result[str]: the next line without its newline, or err("eof").
    .globl rt_read_line
rt_read_line:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    call sb_new
    mv s1, a0
    li s2, 0                    # read anything?
1:
    la t0, in_pos
    ld t1, 0(t0)
    la t2, in_len
    ld t2, 0(t2)
    blt t1, t2, 2f
    mv a0, s0
    call stdin_fill
    beqz a0, 5f
    j 1b
2:
    la t3, in_buf
    add t3, t3, t1
    lbu a1, 0(t3)
    addi t1, t1, 1
    sd t1, 0(t0)
    li s2, 1
    li t4, 10
    beq a1, t4, 6f
    mv a0, s1
    call sb_byte
    j 1b
5:
    bnez s2, 6f
    la a0, m_eof
    call make_err
    j 9f
6:
    ld t0, SB_LEN(s1)           # a Windows line ending: drop the \r too
    beqz t0, 61f
    ld t1, SB_PTR(s1)
    add t1, t1, t0
    lbu t1, -1(t1)
    li t2, 13
    bne t1, t2, 61f
    addi t0, t0, -1
    sd t0, SB_LEN(s1)
61:
    mv a0, s1
    call sb_finish
    mv a1, a0
    li a0, 0
    call make_result
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void *rt_read_all(void *node)
    .globl rt_read_all
rt_read_all:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    call sb_new
    mv s1, a0
1:
    la t0, in_pos
    ld t1, 0(t0)
    la t2, in_len
    ld t2, 0(t2)
    bge t1, t2, 2f
    la a1, in_buf
    add a1, a1, t1
    sub a2, t2, t1
    sd t2, 0(t0)
    mv a0, s1
    call sb_bytes
2:
    mv a0, s0
    call stdin_fill
    bnez a0, 1b
    mv a0, s1
    call sb_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *rt_read_file(void *path)
    .globl rt_read_file
rt_read_file:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a0
    addi a0, s0, STR_DATA
    addi a1, sp, 0
    call read_whole_file
    bltz a0, 1f
    mv a1, a0
    ld a0, 0(sp)
    call str_new
    mv a1, a0
    li a0, 0
    call make_result
    j 9f
1:
    la a0, m_cannot_read
    mv a1, s0
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    li a0, 1
    call make_result
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# void *rt_write_file(void *path, void *text)
    .globl rt_write_file
rt_write_file:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    addi a0, s0, STR_DATA
    li a1, O_WRCREAT
    call sys_open
    bltz a0, 1f
    mv s2, a0
    addi a1, s1, STR_DATA
    ld a2, STR_LEN(s1)
    call write_all
    mv a0, s2
    call sys_close
    li a0, 0
    li a1, 0
    call make_result
    j 9f
1:
    la a0, m_cannot_write
    mv a1, s0
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    li a0, 1
    call make_result
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *rt_args(void)
    .globl rt_args
rt_args:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    la t0, prog_argc
    ld a0, 0(t0)
    call list_new
    mv s0, a0
    li s1, 0
1:
    la t0, prog_argc
    ld t0, 0(t0)
    bge s1, t0, 9f
    la t0, prog_argv
    ld t0, 0(t0)
    slli t1, s1, 3
    add t0, t0, t1
    ld a0, 0(t0)
    call str_from_cstr
    mv a1, a0
    mv a0, s0
    call list_push
    addi s1, s1, 1
    j 1b
9:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

    .section .rodata
s_exit_run: .string "run"
    .text
# void rt_exit(long code, void *node)
    .globl rt_exit
rt_exit:
    bltz a0, 1f
    li t0, 63
    bgt a0, t0, 1f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd a0, 0(sp)
    li a0, 2                    # --mem-stats: the run ends here
    la a1, s_exit_run
    li a2, 0
    call gc_report
    ld a0, 0(sp)
    call vo_exit
    ld ra, 8(sp)
    addi sp, sp, 16
    ret
1:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a1
    call int_to_str
    mv a1, a0
    la a0, m_exit
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    la a0, r0008
    mv a2, s0
    call rt_raise
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
