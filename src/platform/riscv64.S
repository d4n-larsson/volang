# The RISC-V platform layer (spec 008 research R5, contracts/platform-layer.md): process entry,
# system calls and non-local exits. Everything else is C (src/sys.c).

# Linked first (Makefile), so this label is the start of .data: the collector scans the globals
# from here to _end (research R4).
    .data
    .globl vo_data_begin
vo_data_begin:

    .text
# void _start(void) sesi: allow clobbers_callee_saved
# The process entry has no caller whose s registers it could clobber. sys_start maps the native
# stack and returns its top; vo_main's status goes to vo_exit.
    .globl _start
_start:
    mv s0, sp                   # argc, argv..., NULL, envp...
    mv a0, sp
    call sys_start
    mv sp, a0
    ld a0, 0(s0)                # argc
    addi a1, s0, 8              # argv
    slli a2, a0, 3
    add a2, a1, a2
    addi a2, a2, 8              # envp follows argv's NULL
    call vo_main
    call vo_exit
1:
    j 1b

# long vo_syscall0(long n)
    .globl vo_syscall0
vo_syscall0:
    mv a7, a0
    ecall
    ret

# long vo_syscall1(long n, long a)
    .globl vo_syscall1
vo_syscall1:
    mv a7, a0
    mv a0, a1
    ecall
    ret

# long vo_syscall2(long n, long a, long b)
    .globl vo_syscall2
vo_syscall2:
    mv a7, a0
    mv a0, a1
    mv a1, a2
    ecall
    ret

# long vo_syscall3(long n, long a, long b, long c)
    .globl vo_syscall3
vo_syscall3:
    mv a7, a0
    mv a0, a1
    mv a1, a2
    mv a2, a3
    ecall
    ret

# long vo_syscall4(long n, long a, long b, long c, long d)
    .globl vo_syscall4
vo_syscall4:
    mv a7, a0
    mv a0, a1
    mv a1, a2
    mv a2, a3
    mv a3, a4
    ecall
    ret

# long vo_syscall5(long n, long a, long b, long c, long d, long e)
    .globl vo_syscall5
vo_syscall5:
    mv a7, a0
    mv a0, a1
    mv a1, a2
    mv a2, a3
    mv a3, a4
    mv a4, a5
    ecall
    ret

# long vo_syscall6(long n, long a, long b, long c, long d, long e, long f)
    .globl vo_syscall6
vo_syscall6:
    mv a7, a0
    mv a0, a1
    mv a1, a2
    mv a2, a3
    mv a3, a4
    mv a4, a5
    mv a5, a6
    ecall
    ret

# long jmp_save(void *buf)
# Saves ra, sp, s0-s11, the C root-stack depth gc_roots_n and fs0-fs11 (JB_SIZE, 216 bytes);
# returns 0 now and the jmp_restore value later.
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
    la t0, gc_roots_n           # roots pushed by C frames that a restore skips are dropped
    ld t0, 0(t0)
    sd t0, 112(a0)
    fsd fs0, 120(a0)
    fsd fs1, 128(a0)
    fsd fs2, 136(a0)
    fsd fs3, 144(a0)
    fsd fs4, 152(a0)
    fsd fs5, 160(a0)
    fsd fs6, 168(a0)
    fsd fs7, 176(a0)
    fsd fs8, 184(a0)
    fsd fs9, 192(a0)
    fsd fs10, 200(a0)
    fsd fs11, 208(a0)
    li a0, 0
    ret

# void jmp_restore(void *buf, long value) sesi: allow clobbers_callee_saved, not_restored
# Reloads ra, sp and the callee-saved registers from jmp_save's buffer on purpose: that is the
# whole point (sesi-linter-issues.md #5).
    .globl jmp_restore
jmp_restore:
    ld t0, 112(a0)
    la t1, gc_roots_n
    sd t0, 0(t1)
    fld fs0, 120(a0)
    fld fs1, 128(a0)
    fld fs2, 136(a0)
    fld fs3, 144(a0)
    fld fs4, 152(a0)
    fld fs5, 160(a0)
    fld fs6, 168(a0)
    fld fs7, 176(a0)
    fld fs8, 184(a0)
    fld fs9, 192(a0)
    fld fs10, 200(a0)
    fld fs11, 208(a0)
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
