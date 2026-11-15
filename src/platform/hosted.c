/* The instrumented build's entry (spec 008 research R9, T063): `make instrumented` links the toolchain
 * with the C library only for the ASan and UBSan runtimes. The C library's start code calls main; the
 * system calls stay raw (src/platform/x86_64.S), and every fixed region moves up by ADDR_BASE. */
#include "vo.h"

struct vo_rlimit {
    unsigned long cur, max;
};
int getrlimit(int resource, struct vo_rlimit *rl);
int setrlimit(int resource, const struct vo_rlimit *rl);
int execv(const char *path, char *const argv[]);
void __asan_handle_no_return(void);
void jmp_restore_raw(void *buf, long value) NORETURN;

#define RLIMIT_STACK 3
#define HOSTED_STACK (1UL << 30) /* instrumented frames are large: room for the R7 depths */

/* ASan must forget the poisoned frames that the restore skips. */
void jmp_restore(void *buf, long value)
{
    __asan_handle_no_return();
    jmp_restore_raw(buf, value);
}

int main(int argc, char **argv, char **env)
{
    struct vo_rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) == 0 && rl.cur < HOSTED_STACK && rl.max >= HOSTED_STACK) {
        rl.cur = HOSTED_STACK; /* the main stack grows to the limit in force at exec */
        if (setrlimit(RLIMIT_STACK, &rl) == 0) execv("/proc/self/exe", argv);
    }
    unsigned long size = getrlimit(RLIMIT_STACK, &rl) == 0 && rl.cur < HOSTED_STACK ? rl.cur : HOSTED_STACK;
    sys_start(); /* the root stack (and an unused native stack) */
    stack_floor = (long)__builtin_frame_address(0) - (long)size + 0x200000;
    vo_exit(vo_main(argc, argv, env));
    return 0;
}
