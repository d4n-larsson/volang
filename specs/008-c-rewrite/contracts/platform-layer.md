# Contract: The Platform Layer

One file per architecture, `src/platform/<arch>.S`, at most 300 lines (SC-008), linted by SESI where
a pack exists (riscv64). Everything here is called from C with these prototypes:

```c
void _start(void);                       /* entry: maps ADDR_STACK (64 MiB), switches sp, calls vo_main */
int  vo_main(long argc, char **argv, char **envp);   /* C; returns the exit status for vo_exit */
long vo_syscall0(long n);
long vo_syscall1(long n, long a);
/* ... */
long vo_syscall6(long n, long a, long b, long c, long d, long e, long f);
__attribute__((returns_twice)) long jmp_save(void *buf);   /* 0 first, then jmp_restore's value */
__attribute__((noreturn)) void jmp_restore(void *buf, long value);
void gc_spill_registers(void *out);      /* while the native stack is scanned (R4); removed after stage 7 */
```

- System call numbers come from `src/platform/<arch>.h` (`SYS_openat`, ...). Only calls listed in
  research R5 are used; a call missing on one architecture is not used on any.
- No signal handlers exist (timeouts are deadline checks), so no `rt_sigreturn` trampoline is needed.
- No other assembly exists outside these files once stage 8 is done (FR-005).
