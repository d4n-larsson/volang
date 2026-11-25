/* I/O builtins: stdout/stderr, buffered stdin with deadline-aware waits, files, args, exit (ported
 * from src/rt_io.s, spec 008). */
#include "vo.h"

static char in_buf[65536];
static long in_pos;
static long in_len;
static long in_eof;

/* spec 018: what a test writes to standard output, kept with its report (research R4).
 * ponytail: a static 64 KiB per test (it survives the per-test heap reset); further bytes are only
 * counted. A growable buffer outside the heap if tests need longer output. */
char cap_buf[CAP_MAX];
long cap_len;
long cap_dropped;
long cap_on;

void cap_bytes(char *p, long n)
{
    long room = CAP_MAX - cap_len;
    long k = n < room ? n : room;
    mem_copy(cap_buf + cap_len, p, k);
    cap_len += k;
    cap_dropped += n - k;
}

void rt_print(int fd, Str *s, int newline)
{
    if (fd == 1 && cap_on) {
        cap_bytes(s->data, s->len);
        if (newline) cap_bytes("\n", 1);
        return;
    }
    out_str(fd, s);
    if (newline) out_cstr(fd, "\n");
}

/* Refills the stdin buffer; 0 at end of input. Waits no longer than the deadline (R0009). */
int stdin_fill(Node *node)
{
    if (in_eof) return 0;
    if (worker_self) rt_raise_c("R0007", "A worker cannot read standard input: the program reads it and sends what the worker needs.", node); /* spec 047 */
    out_flush();
    long deadline = deadline_ms;
    long wait = -1;
    if (deadline) {
        wait = deadline - sys_clock_ms();
        if (wait <= 0) {
            rt_timeout(node);
            return 0;
        }
    }
    if (!sys_wait_stdin(wait)) {
        rt_timeout(node);
        return 0;
    }
    long got = sys_read(0, in_buf, sizeof in_buf);
    if (got <= 0) {
        in_eof = 1;
        return 0;
    }
    in_len = got;
    in_pos = 0;
    return 1;
}

/* result[str]: the next line without its newline, or err("eof"). */
Variant *rt_read_line(Node *node)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    int any = 0; /* read anything? */
    for (;;) {
        if (in_pos >= in_len) {
            if (stdin_fill(node)) continue;
            if (!any) {
                GC_UNROOT(1);
                return make_err("eof");
            }
            break;
        }
        int c = (unsigned char)in_buf[in_pos++];
        any = 1;
        if (c == '\n') break;
        sb_byte(sb, c);
    }
    if (sb->len && sb->ptr[sb->len - 1] == '\r') sb->len--; /* a Windows line ending: drop the \r too */
    GC_UNROOT(1);
    return make_result(0, (long)sb_finish(sb));
}

Str *rt_read_all(Node *node)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    do {
        if (in_pos < in_len) {
            long from = in_pos;
            in_pos = in_len;
            sb_bytes(sb, in_buf + from, in_len - from);
        }
    } while (stdin_fill(node));
    GC_UNROOT(1);
    return sb_finish(sb);
}

Variant *rt_read_file(Str *path)
{
    char *text;
    long fd = rt_open_file(EF_FREAD, path, O_RDONLY | O_NONBLOCK); /* spec 013: checked, beneath its scope */
    long len = fd < 0 ? fd : read_whole_fd(fd, &text);
    if (len >= 0) return make_result(0, (long)str_new(text, len));
    return make_result(1, (long)msg("cannot read $1", path, 0, 0));
}

Variant *rt_write_file(Str *path, Str *text)
{
    long fd = rt_open_file(EF_FWRITE, path, O_WRCREAT); /* spec 013: checked, beneath its scope */
    if (fd < 0) return make_result(1, (long)msg("cannot write $1", path, 0, 0));
    write_all((int)fd, text->data, text->len);
    sys_close((int)fd);
    return make_result(0, 0);
}

List *rt_args(void)
{
    List *l = list_new(prog_argc);
    GC_ROOT(&l);
    for (long i = 0; i < prog_argc; i++) list_push(l, (long)str_from_cstr(((char **)prog_argv)[i]));
    GC_UNROOT(1);
    return l;
}

void rt_exit(long code, Node *node)
{
    if (code >= 0 && code <= 63) {
        gc_report(2, "run", 0); /* --mem-stats: the run ends here */
        vo_exit((int)code);
        return;
    }
    rt_raise("R0008", msg("Exit status $1 is outside 0 to 63.", int_to_str(code), 0, 0), node);
}
