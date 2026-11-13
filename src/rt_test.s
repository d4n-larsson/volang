# expect / expect_eq: on failure, record it and return to the test runner's recovery point.
    .include "defs.inc"

    .section .bss
    .align 3
    .globl tf_node
tf_node:        .space 8            # the failing expect call
    .globl tf_msg
tf_msg:         .space 8
    .globl tf_expected
tf_expected:    .space 8            # rendered values (str) or 0
    .globl tf_actual
tf_actual:      .space 8

    .section .rodata
m_expect:       .string "`expect` failed: the condition is false."
m_expect_eq:    .string "`expect_eq` failed: the values differ."

    .text
# void rt_expect(long cond, void *node)
    .globl rt_expect
rt_expect:
    bnez a0, 1f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a1
    la a0, m_expect
    call str_from_cstr
    la t0, tf_msg
    sd a0, 0(t0)
    la t0, tf_node
    sd s0, 0(t0)
    la t0, tf_expected
    sd zero, 0(t0)
    la t0, tf_actual
    sd zero, 0(t0)
    la t0, recover_point
    ld a0, 0(t0)
    li a1, 1
    call jmp_restore
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
1:
    ret

# void rt_expect_eq(long actual, long expected, void *node)
    .globl rt_expect_eq
rt_expect_eq:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    ld t0, N_B(s2)              # the first argument's type
    ld t0, VEC_DATA(t0)
    ld t0, 0(t0)
    ld s3, N_TYPE(t0)
    mv a2, s3
    call values_equal
    bnez a0, 9f
    call sb_new
    sd a0, 0(sp)
    mv a1, s0
    mv a2, s3
    call render_value
    ld a0, 0(sp)
    call sb_finish
    la t0, tf_actual
    sd a0, 0(t0)
    call sb_new
    sd a0, 0(sp)
    mv a1, s1
    mv a2, s3
    call render_value
    ld a0, 0(sp)
    call sb_finish
    la t0, tf_expected
    sd a0, 0(t0)
    la a0, m_expect_eq
    call str_from_cstr
    la t0, tf_msg
    sd a0, 0(t0)
    la t0, tf_node
    sd s2, 0(t0)
    la t0, recover_point
    ld a0, 0(t0)
    li a1, 1
    call jmp_restore
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret
