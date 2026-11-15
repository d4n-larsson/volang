// The ARM64 platform layer (spec 008 research R5, contracts/platform-layer.md): process entry,
// system calls and non-local exits. Everything else is C (src/sys.c). AAPCS64.

// Linked first (Makefile), so this label is the start of .data: the collector scans the globals
// from here to _end (research R4).
    .data
    .globl vo_data_begin
vo_data_begin:

    .text
// void _start(void)
// sys_start maps the native stack and returns its top; vo_main's status goes to vo_exit.
    .globl _start
_start:
    mov x29, #0                 // the outermost frame
    mov x30, #0
    mov x19, sp                 // argc, argv..., NULL, envp...
    bl sys_start
    mov sp, x0
    ldr x0, [x19]               // argc
    add x1, x19, #8             // argv
    add x2, x1, x0, lsl #3
    add x2, x2, #8              // envp follows argv's NULL
    bl vo_main
    bl vo_exit
1:
    b 1b

// long vo_syscall0(long n) ... long vo_syscall6(long n, long a, long b, long c, long d, long e, long f)
// One body: the kernel ignores the registers a call does not use.
    .globl vo_syscall0, vo_syscall1, vo_syscall2, vo_syscall3, vo_syscall4, vo_syscall5, vo_syscall6
vo_syscall0:
vo_syscall1:
vo_syscall2:
vo_syscall3:
vo_syscall4:
vo_syscall5:
vo_syscall6:
    mov x8, x0
    mov x0, x1
    mov x1, x2
    mov x2, x3
    mov x3, x4
    mov x4, x5
    mov x5, x6
    svc #0
    ret

// long jmp_save(void *buf)
// Saves x19-x30, sp, the C root-stack depth gc_roots_n and d8-d15 (176 of JB_SIZE's bytes);
// returns 0 now and the jmp_restore value later.
    .globl jmp_save
jmp_save:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]
    mov x2, sp
    adrp x3, gc_roots_n         // roots pushed by C frames that a restore skips are dropped
    ldr x3, [x3, :lo12:gc_roots_n]
    stp x2, x3, [x0, #96]
    stp d8, d9, [x0, #112]
    stp d10, d11, [x0, #128]
    stp d12, d13, [x0, #144]
    stp d14, d15, [x0, #160]
    mov x0, #0
    ret

// void jmp_restore(void *buf, long value)
    .globl jmp_restore
jmp_restore:
    ldp x2, x3, [x0, #96]
    adrp x4, gc_roots_n
    str x3, [x4, :lo12:gc_roots_n]
    ldp x19, x20, [x0, #0]
    ldp x21, x22, [x0, #16]
    ldp x23, x24, [x0, #32]
    ldp x25, x26, [x0, #48]
    ldp x27, x28, [x0, #64]
    ldp x29, x30, [x0, #80]
    ldp d8, d9, [x0, #112]
    ldp d10, d11, [x0, #128]
    ldp d12, d13, [x0, #144]
    ldp d14, d15, [x0, #160]
    mov sp, x2
    mov x0, x1
    ret

    .section .note.GNU-stack, "", @progbits
