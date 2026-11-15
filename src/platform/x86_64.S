# The x86-64 platform layer (spec 008 research R5, contracts/platform-layer.md): process entry,
# system calls and non-local exits. Everything else is C (src/sys.c). AT&T syntax, System V ABI.

# Linked first (Makefile), so this label is the start of .data: the collector scans the globals
# from here to _end (research R4).
    .data
    .globl vo_data_begin
vo_data_begin:

    .text
# void _start(void)
# sys_start maps the native stack and returns its top; vo_main's status goes to vo_exit.
    .globl _start
_start:
    xor %ebp, %ebp              # the outermost frame
    mov %rsp, %rbx              # argc, argv..., NULL, envp...
    call sys_start              # rsp is 16-byte aligned at entry, as a call expects
    mov %rax, %rsp
    mov (%rbx), %rdi            # argc
    lea 8(%rbx), %rsi           # argv
    lea 16(%rbx,%rdi,8), %rdx   # envp follows argv's NULL
    call vo_main
    mov %eax, %edi
    call vo_exit
1:
    jmp 1b

# long vo_syscall0(long n) ... long vo_syscall6(long n, long a, long b, long c, long d, long e, long f)
# One body: the kernel ignores the registers a call does not use, and 8(%rsp) is always readable.
    .globl vo_syscall0, vo_syscall1, vo_syscall2, vo_syscall3, vo_syscall4, vo_syscall5, vo_syscall6
vo_syscall0:
vo_syscall1:
vo_syscall2:
vo_syscall3:
vo_syscall4:
vo_syscall5:
vo_syscall6:
    mov %rdi, %rax
    mov %rsi, %rdi
    mov %rdx, %rsi
    mov %rcx, %rdx
    mov %r8, %r10               # the fourth argument is r10 for syscall (rcx is clobbered)
    mov %r9, %r8
    mov 8(%rsp), %r9
    syscall
    ret

# long jmp_save(void *buf)
# Saves the return address, rsp, rbx, rbp, r12-r15 and the C root-stack depth gc_roots_n (72 of
# JB_SIZE's bytes); returns 0 now and the jmp_restore value later. No callee-saved SSE registers exist.
    .globl jmp_save
jmp_save:
    mov (%rsp), %rax
    mov %rax, 0(%rdi)
    lea 8(%rsp), %rax           # rsp as the caller sees it after the return
    mov %rax, 8(%rdi)
    mov %rbx, 16(%rdi)
    mov %rbp, 24(%rdi)
    mov %r12, 32(%rdi)
    mov %r13, 40(%rdi)
    mov %r14, 48(%rdi)
    mov %r15, 56(%rdi)
    mov gc_roots_n(%rip), %rax  # roots pushed by C frames that a restore skips are dropped
    mov %rax, 64(%rdi)
    xor %eax, %eax
    ret

# void jmp_restore(void *buf, long value)
    .globl jmp_restore
jmp_restore:
    mov 64(%rdi), %rax
    mov %rax, gc_roots_n(%rip)
    mov 16(%rdi), %rbx
    mov 24(%rdi), %rbp
    mov 32(%rdi), %r12
    mov 40(%rdi), %r13
    mov 48(%rdi), %r14
    mov 56(%rdi), %r15
    mov 8(%rdi), %rsp
    mov %rsi, %rax
    jmp *0(%rdi)

    .section .note.GNU-stack, "", @progbits
