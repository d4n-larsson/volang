# Non-local exits for `try` propagation and test recovery (setjmp/longjmp).
# jmp_restore deliberately reloads ra, sp and s0-s11 from a buffer written by jmp_save:
# that is the whole point, so the callee-saved reports for it are expected.
    .include "defs.inc"

    .text
# long jmp_save(void *buf)
# Saves ra, sp and s0-s11 (14 words); returns 0 now and the jmp_restore value later.
    .globl jmp_save
jmp_save:
    sd ra, 0(a0)
    sd sp, 8(a0)
    sd s0, 16(a0)
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)
    li a0, 0
    ret

# void jmp_restore(void *buf, long value)
    .globl jmp_restore
jmp_restore:
    ld ra, 0(a0)
    ld sp, 8(a0)
    ld s0, 16(a0)
    ld s1, 24(a0)
    ld s2, 32(a0)
    ld s3, 40(a0)
    ld s4, 48(a0)
    ld s5, 56(a0)
    ld s6, 64(a0)
    ld s7, 72(a0)
    ld s8, 80(a0)
    ld s9, 88(a0)
    ld s10, 96(a0)
    ld s11, 104(a0)
    mv a0, a1
    ret
