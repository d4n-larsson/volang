/* Process start-up, system call wrappers, buffered output, file reading and the permanent arena
   (spec 008 T049). The machine-specific part is src/platform/<arch>.S. */
#include "vo.h"

#define AT_EMPTY_PATH 0x1000
#define STATX_SIZE 0x200
#define STATX_SIZE_OFF 40 /* struct statx: stx_size */
#define OUT_CAP 65536

long oom_hook = (long)oom_default;
long stack_floor = ADDR_STACK + 0x100000; /* checked by deep recursions (research R7) */
long arena_ptr;                           /* the permanent arena's bump pointer (scanned up to here as a root) */
long arena_cap = 0x20000000;              /* 512 MiB */
long envp;
static long arena_end, arena_total;
static char out_buf[2][OUT_CAP]; /* fds 1 and 2 */
static long out_len[2];

/* Called by _start on the kernel's stack: maps the native stack and the C root stack (research
   R4), returns the native stack's top. */
long sys_start(void)
{
    map_fixed(ADDR_ROOTS, ROOTS_CAP * 8, MAP_NORESERVE);
    return (long)map_fixed(ADDR_STACK, STACK_SIZE, 0) + STACK_SIZE;
}

void vo_exit(int code)
{
    code = (int)exit_hooks(code); /* spec 006: completes the trace and the recording; may change the status */
    out_flush();
    vo_syscall1(SYS_exit_group, code);
    for (;;) {}
}

long sys_read(int fd, void *buf, long n)
{
    return vo_syscall3(SYS_read, fd, (long)buf, n);
}

long sys_write(int fd, void *buf, long n)
{
    return vo_syscall3(SYS_write, fd, (long)buf, n);
}

long sys_open(char *path, int flags)
{
    return vo_syscall4(SYS_openat, AT_FDCWD, (long)path, flags, 0644);
}

long sys_close(int fd)
{
    return vo_syscall1(SYS_close, fd);
}

long sys_getdents64(int fd, void *buf, long n)
{
    return vo_syscall3(SYS_getdents64, fd, (long)buf, n);
}

long sys_rename(char *from, char *to)
{
    return vo_syscall5(SYS_renameat2, AT_FDCWD, (long)from, AT_FDCWD, (long)to, 0);
}

long sys_unlink(char *path)
{
    return vo_syscall3(SYS_unlinkat, AT_FDCWD, (long)path, 0);
}

/* The link's target length in buf (not NUL-terminated), or a negative error. */
long sys_readlink(char *path, char *buf, long size)
{
    return vo_syscall4(SYS_readlinkat, AT_FDCWD, (long)path, (long)buf, size);
}

/* The working directory's length including its NUL, or a negative error. */
long sys_getcwd(char *buf, long size)
{
    return vo_syscall2(SYS_getcwd, (long)buf, size);
}

static void clock_get(long clock, long ts[2])
{
    vo_syscall2(SYS_clock_gettime, clock, (long)ts);
}

long sys_clock_ns(void)
{
    long ts[2];
    clock_get(1, ts); /* CLOCK_MONOTONIC */
    return ts[0] * 1000000000 + ts[1];
}

long sys_clock_ms(void)
{
    long ts[2];
    clock_get(1, ts);
    return ts[0] * 1000 + ts[1] / 1000000;
}

/* Milliseconds since the Unix epoch (CLOCK_REALTIME): `now_ms` (spec 007, std.time.now). */
long sys_clock_real_ms(void)
{
    long ts[2];
    clock_get(0, ts);
    return ts[0] * 1000 + ts[1] / 1000000;
}

/* 1 when fd 0 is readable (or closed), 0 on timeout. ms < 0 waits forever. */
long sys_wait_stdin(long ms)
{
    struct {
        int fd;
        short events, revents;
    } pfd = {0, 1, 0}; /* POLLIN */
    long ts[2] = {ms / 1000, ms % 1000 * 1000000};
    long r = vo_syscall5(SYS_ppoll, (long)&pfd, 1, ms < 0 ? 0 : (long)ts, 0, 8);
    return r != 0; /* errors count as readable */
}

/* 1 when a write failed, 0 when everything was written. */
long write_checked(int fd, char *p, long n)
{
    while (n > 0) {
        long r = sys_write(fd, p, n);
        if (r <= 0) return 1;
        p += r;
        n -= r;
    }
    return 0;
}

void write_all(int fd, char *p, long n)
{
    write_checked(fd, p, n); /* an error: give up rather than spin */
}

void out_flush(void)
{
    for (int i = 0; i < 2; i++) {
        long n = out_len[i];
        out_len[i] = 0;
        write_all(i + 1, out_buf[i], n);
    }
}

/* Buffered for fds 1 and 2 (flushed together, which keeps them ordered); other fds unbuffered. */
void out_bytes(int fd, char *p, long n)
{
    if (fd != 1 && fd != 2) {
        write_all(fd, p, n);
        return;
    }
    long *len = &out_len[fd - 1];
    if (*len + n > OUT_CAP) {
        out_flush();
        if (n >= OUT_CAP) {
            write_all(fd, p, n);
            return;
        }
    }
    mem_copy(out_buf[fd - 1] + *len, p, n);
    *len += n;
}

void out_cstr(int fd, char *s)
{
    out_bytes(fd, s, cstr_len(s));
}

void out_str(int fd, void *str)
{
    out_bytes(fd, ((Str *)str)->data, ((Str *)str)->len);
}

/* The length, or a negative errno. The bytes are followed by a NUL. */
long read_whole_file(char *path, char **out)
{
    long fd = sys_open(path, O_RDONLY);
    if (fd < 0) return fd;
    long stx[32]; /* struct statx, 256 bytes */
    long r = vo_syscall5(SYS_statx, fd, (long)"", AT_EMPTY_PATH, STATX_SIZE, (long)stx);
    if (r >= 0) {
        long size = *(long *)((char *)stx + STATX_SIZE_OFF);
        char *data = alloc_bytes(size + 1);
        long got = 0;
        while (got < size && (r = sys_read((int)fd, data + got, size - got)) > 0) got += r;
        if (r >= 0) {
            sys_close((int)fd);
            *out = data;
            return got;
        }
    }
    sys_close((int)fd);
    return r;
}

/* Maps zeroed read-write memory at exactly addr (flags: extra MAP_* bits), or stops with T0003. */
void *map_fixed(long addr, long size, long flags)
{
    long r = vo_syscall6(SYS_mmap, addr, size, 3 /* PROT_READ | PROT_WRITE */,
                         flags | MAP_FIXED_NOREPLACE | 0x22 /* MAP_PRIVATE | MAP_ANONYMOUS */, -1, 0);
    if (r == addr) return (void *)r;
    char hex[17];
    for (int i = 0; i < 16; i++) hex[i] = "0123456789abcdef"[((unsigned long)addr >> (60 - 4 * i)) & 15];
    hex[16] = '\n';
    out_cstr(2, "error[T0003]: cannot map memory at 0x");
    out_bytes(2, hex, 17);
    vo_exit(EXIT_INTERNAL);
    return 0;
}

/* The permanent arena: a bump allocator over contiguous 16 MiB chunks from ADDR_PERM, never freed. */
void *perm_alloc(long size)
{
    size = (size + 7) & -8;
    if (!arena_ptr) arena_ptr = arena_end = ADDR_PERM; /* first use: empty, at its fixed address */
    if (arena_ptr + size > arena_end) {                 /* grow right after the end, by whole chunks */
        long grow = (arena_ptr + size - arena_end + 0xFFFFFF) & -0x1000000;
        if ((unsigned long)(arena_total + grow) > (unsigned long)arena_cap) {
            ((void (*)(void))oom_hook)();
            return 0;
        }
        map_fixed(arena_end, grow, 0);
        arena_total += grow;
        arena_end += grow;
    }
    long p = arena_ptr;
    arena_ptr += size;
    return (void *)p;
}

/* Plain memory: the permanent arena before a program runs and inside perm brackets, otherwise a
   collected OBJ_RAW buffer (spec 003). */
void *alloc(long size)
{
    if (!gc_on || gc_perm) return perm_alloc(size);
    return (char *)gc_alloc(size + 8, OBJ_RAW) + 8;
}

/* Like alloc, for bytes that never hold pointers (never scanned by the collector). */
void *alloc_bytes(long size)
{
    if (!gc_on || gc_perm) return perm_alloc(size);
    return (char *)gc_alloc(size + 8, OBJ_BYTES) + 8;
}

/* Until the matching perm_end, allocations go to the permanent arena (interned types, caches). */
void perm_begin(void)
{
    gc_perm++;
}

void perm_end(void)
{
    gc_perm--;
}

/* size is the payload size; the result points at the 8-byte header. */
void *alloc_obj(int kind, long size)
{
    if (gc_on && !gc_perm) return gc_alloc(size + 8, kind); /* writes the header */
    ObjHdr *h = perm_alloc(size + 8);
    h->size = (uint32_t)(size + 8);
    h->kind = (uint32_t)kind;
    return h;
}

void oom_default(void)
{
    out_cstr(2, "error[T0003]: internal error: out of memory\n");
    vo_exit(EXIT_INTERNAL);
}

/* ---- buffered output files (spec 006: traces and recordings) ---- */

/* Creates or truncates path for writing; a buffered writer, or 0 when it cannot be created. Its
   memory is permanent (it lives until vo_exit). */
void *fbuf_open(char *path)
{
    long fd = sys_open(path, O_WRCREAT);
    if (fd < 0) return 0;
    FBuf *fb = perm_alloc(FB_SIZE);
    fb->buf = perm_alloc(FB_CAP);
    fb->fd = fd;
    fb->len = 0;
    fb->err = 0;
    return fb;
}

/* Writes the buffer out; a failed write sets err (reported by the caller at exit). */
void fbuf_flush(void *fbp)
{
    FBuf *fb = fbp;
    long n = fb->len;
    fb->len = 0;
    if (write_checked((int)fb->fd, fb->buf, n)) fb->err = 1;
}

void fbuf_write(void *fbp, char *p, long n)
{
    FBuf *fb = fbp;
    if (fb->len + n > FB_CAP) {
        fbuf_flush(fb);
        if (n >= FB_CAP) { /* larger than the buffer: straight out */
            if (write_checked((int)fb->fd, p, n)) fb->err = 1;
            return;
        }
    }
    mem_copy(fb->buf + fb->len, p, n);
    fb->len += n;
}
