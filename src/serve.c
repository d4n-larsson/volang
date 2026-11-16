/* HTTP serving for std.http.serve (spec 011 research R3-R6, R9): listening, reading one request
 * within the limits, replying, and calling the handler under a recovery point. The protocol itself
 * (request parsing, response text) is vo code in std/http.vo; recording and replay wrap these
 * functions (io_serve_*, src/replay.c).
 * ponytail: one request at a time, IPv4 only, Connection: close; concurrency and IPv6 when needed. */
#include "vo.h"

#define AF_INET 2
#define SOCK_STREAM 1
#define SOCK_NONBLOCK 0x800
#define SOCK_CLOEXEC 0x80000
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SIG_BLOCK 0
#define SIG_SETMASK 2
#define SIGINT 2
#define SIGTERM 15
#define POLLIN 1
#define POLLOUT 4
#define MSG_NOSIGNAL 0x4000
#define E_AGAIN 11
#define E_ACCES 13
#define E_ADDRINUSE 98
#define HEAD_MAX (64L << 10) /* the request line and headers (431 above) */
#define HEADERS_MAX 100      /* header lines (431 above) */
#define BODY_MAX (16L << 20) /* the body (413 above), as the client's response cap */
#define READ_MS 10000        /* to receive a whole request (408 after) */

typedef struct SockAddrIn {
    uint16_t family;
    uint8_t port[2]; /* network order */
    uint8_t addr[4];
    uint8_t zero[8];
} SockAddrIn;

static long sv_sig = -1;          /* signalfd for SIGINT and SIGTERM while serving */
static long sv_conn = -1;         /* the connection of the request being handled */
static unsigned long sv_old_mask; /* the signal mask before serving */
long serve_guard;                 /* 1 while a handler runs: rt_raise reports, then jumps here */

static Variant *sv_err(char *tmpl, Str *a)
{
    return make_result(1, (long)msg(tmpl, a, 0, 0));
}

/* result[int]: the listening socket for address ("PORT" or "0.0.0.0:PORT"), or err with the reason.
 * SIGINT and SIGTERM are blocked from here on and read through a signalfd (no handlers, R6). The
 * address is checked against the grant by the caller (io_serve_listen). */
Variant *rt_serve_listen(Str *address)
{
    long all = 0;
    long port = serve_address_port(address, &all);
    if (!port) return sv_err("`$1` is not a serve address: write \"PORT\" or \"0.0.0.0:PORT\"", address);
    long fd = vo_syscall3(SYS_socket, AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return sv_err("cannot open a socket for `$1`", address);
    int one = 1;
    vo_syscall5(SYS_setsockopt, fd, SOL_SOCKET, SO_REUSEADDR, (long)&one, sizeof one);
    SockAddrIn sa = {AF_INET, {(uint8_t)(port >> 8), (uint8_t)port}, {127, 0, 0, 1}, {0}};
    if (all) sa.addr[0] = sa.addr[3] = 0; /* 0.0.0.0 */
    long r = vo_syscall3(SYS_bind, fd, (long)&sa, sizeof sa);
    if (r == 0) r = vo_syscall2(SYS_listen, fd, 64);
    if (r < 0) {
        sys_close((int)fd);
        if (r == -E_ADDRINUSE) return sv_err("`$1` is already in use", address);
        if (r == -E_ACCES) return sv_err("no permission to serve on `$1`", address);
        return sv_err("cannot serve on `$1`", address);
    }
    unsigned long mask = (1UL << (SIGINT - 1)) | (1UL << (SIGTERM - 1));
    vo_syscall4(SYS_rt_sigprocmask, SIG_BLOCK, (long)&mask, (long)&sv_old_mask, 8);
    sv_sig = vo_syscall4(SYS_signalfd4, -1, (long)&mask, 8, SOCK_NONBLOCK | SOCK_CLOEXEC);
    serve_deadline(0);
    return make_result(0, fd);
}

/* While serving there is no deadline (each handler call has one, R5); after it, the program's
 * deadline starts again (on: 1). */
void serve_deadline(long on)
{
    deadline_ms = on && opt_timeout ? sys_clock_ms() + opt_timeout * 1000 : 0;
}

/* After the server stops: the signal mask back, the deadline for the rest of the program. */
static void sv_stop(long listener)
{
    sys_close((int)listener);
    if (sv_sig >= 0) sys_close((int)sv_sig);
    sv_sig = -1;
    vo_syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (long)&sv_old_mask, 0, 8);
    serve_deadline(1);
}

/* Waits for fd (and the stop signal): 1 fd ready, 2 a stop signal, 0 after ms (ms < 0: no limit). */
static int sv_wait(long fd, int events, long ms)
{
    struct {
        int fd;
        short events, revents;
    } p[2] = {{(int)fd, (short)events, 0}, {(int)sv_sig, POLLIN, 0}};
    long ts[2] = {ms / 1000, ms % 1000 * 1000000};
    long n = vo_syscall5(SYS_ppoll, (long)p, sv_sig >= 0 ? 2 : 1, ms < 0 ? 0 : (long)ts, 0, 8);
    if (n <= 0) return 0;
    if (p[1].revents) return 2;
    return 1;
}

static void sv_send(long fd, char *p, long n)
{
    long end = sys_clock_ms() + READ_MS;
    while (n > 0) {
        long w = vo_syscall6(SYS_sendto, fd, (long)p, n, MSG_NOSIGNAL, 0, 0);
        if (w == -E_AGAIN) {
            long left = end - sys_clock_ms();
            if (left <= 0) return;
            sys_wait_fd(fd, POLLOUT, left);
            continue;
        }
        if (w <= 0) return; /* the client went away */
        p += w;
        n -= w;
    }
}

/* Answers a request the handler never sees (400, 408, 413, 431) and closes the connection. */
static void sv_refuse(long fd, long status)
{
    static const char *const lines[] = {"400 Bad Request", "408 Request Timeout", "413 Content Too Large",
                                        "431 Request Header Fields Too Large"};
    const char *line = lines[status == 408 ? 1 : status == 413 ? 2 : status == 431 ? 3 : 0];
    SB *sb = sb_new();
    sb_cstr(sb, "HTTP/1.1 ");
    sb_cstr(sb, (char *)line);
    sb_cstr(sb, "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    sv_send(fd, sb->ptr, sb->len);
    sys_close((int)fd);
}

static int sv_lower_prefix(char *p, char *end, char *lower)
{
    for (; *lower; p++, lower++) {
        if (p >= end) return 0;
        int c = (unsigned char)*p;
        if (c >= 'A' && c <= 'Z') c += 32;
        if (c != *lower) return 0;
    }
    return 1;
}

/* ---- spec 013 (R4): one framing for requests (server) and responses (client) ----
 * http_frame decides where a message ends and decodes its body, incrementally (called again as bytes
 * arrive, resuming where it stopped: L9). The head must be ASCII text in CRLF lines; the head and all
 * chunk framing (size lines, extensions, trailers) count against HEAD_MAX, the decoded body against
 * BODY_MAX (H9). A finished message is the head (with its blank line) followed by the decoded body;
 * __http_part splits it without UTF-8 boundary checks, so vo code slices only ASCII (H7, M2). */

/* Frames buf[0..n) with state f (zeroed before the first call): >0 when the message is complete (then
 * f->out holds it), 0 when more bytes are needed, or -400/-413/-431. eof: no more bytes will come;
 * response: a body without a length runs to the end. */
#define BAD(reason) return (f->why = (reason), -400)
long http_frame(HttpFrame *f, char *buf, long n, int eof, int response)
{
    if (!f->head) { /* the blank line ending the head, searched from where the last call stopped */
        long i = f->scanned > 3 ? f->scanned - 3 : 0;
        for (; i + 3 < n; i++)
            if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') break;
        if (i + 3 >= n) {
            f->scanned = n;
            if (n > HEAD_MAX) return -431;
            if (eof) BAD("no end of headers");
            return 0;
        }
        f->head = i + 4;
        if (f->head > HEAD_MAX) return -431;
        long lines = 0; /* ASCII text in CRLF lines; header names and values are read by vo */
        for (long k = 0; k < f->head; k++) {
            int c = (unsigned char)buf[k];
            if (c == '\r') {
                if (buf[k + 1] != '\n') BAD("a CR without LF in the head");
            } else if (c == '\n') {
                if (!k || buf[k - 1] != '\r') BAD("an LF without CR in the head");
                lines++;
            } else if ((c < 0x20 && c != '\t') || c >= 0x7f) {
                BAD("a byte that is not ASCII text in the head");
            }
        }
        if (lines - 2 > HEADERS_MAX) return -431; /* the first line and the blank line are not headers */
        f->length = -1;
        for (char *p = buf; p < buf + f->head;) { /* Content-Length, Transfer-Encoding */
            char *le = p;
            while (le < buf + f->head && *le != '\n') le++;
            if (sv_lower_prefix(p, le, "content-length:")) {
                char *q = p + 15;
                while (q < le && (*q == ' ' || *q == '\t')) q++;
                if (q >= le || *q < '0' || *q > '9') BAD("bad Content-Length");
                f->length = 0;
                for (; q < le && *q >= '0' && *q <= '9'; q++) {
                    f->length = f->length * 10 + (*q - '0');
                    if (f->length > BODY_MAX) return -413;
                }
                while (q < le && (*q == ' ' || *q == '\t' || *q == '\r')) q++;
                if (q < le) BAD("bad Content-Length");
            } else if (sv_lower_prefix(p, le, "transfer-encoding:")) {
                for (char *q = p + 18; q + 7 <= le; q++)
                    if (sv_lower_prefix(q, le, "chunked")) f->chunked = 1;
            }
            p = le + 1;
        }
        f->at = f->head;
        if (f->chunked) { /* the decoded message: the head, then each chunk's data */
            f->body = sb_new();
            sb_bytes(f->body, buf, f->head);
        }
    }
    SB *body = f->body;
    if (!f->chunked) { /* the message as received: no copy but the result */
        long have = n - f->head;
        if (f->length < 0 && !response) f->length = 0; /* a request without a length has no body */
        if (f->length >= 0) {
            if (have < f->length) {
                if (eof) BAD("body shorter than Content-Length");
                return 0;
            }
            have = f->length;
        } else {          /* a response without a length: up to the end */
            if (have > BODY_MAX) return -413;
            if (!eof) return 0;
        }
        f->out = str_new(buf, f->head + have);
        return 1;
    }
    while (!f->trailers) { /* chunks: HEX[;ext]CRLF DATA CRLF ... 0 CRLF [trailers] CRLF */
        long at = f->at, size = 0, digits = 0;
        while (at < n && buf[at] != '\r' && buf[at] != ';') {
            int c = (unsigned char)buf[at++];
            int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (v < 0) BAD("bad chunk");
            if (++digits > 8) return -413;
            size = size * 16 + v;
        }
        while (at < n && buf[at] != '\r') { /* an extension: counted as framing */
            if (buf[at] == '\n') BAD("bad chunk");
            if (f->head + f->overhead + (++at - f->at) > HEAD_MAX) return -431;
        }
        if (at + 1 >= n) goto more;
        if (!digits || buf[at + 1] != '\n') BAD("bad chunk");
        at += 2;
        long overhead = f->overhead + (at - f->at);
        if (f->head + overhead > HEAD_MAX) return -431;
        if (size == 0) { /* the last chunk: trailers follow */
            f->overhead = overhead;
            f->at = at;
            f->trailers = 1;
            break;
        }
        if (n - at < size + 2) goto more; /* (the size line is parsed again when more bytes come) */
        if (body->len - f->head + size > BODY_MAX) return -413;
        if (buf[at + size] != '\r' || buf[at + size + 1] != '\n') BAD("bad chunk");
        sb_bytes(body, buf + at, size);
        f->overhead = overhead;
        f->at = at + size + 2;
    }
    for (;;) { /* trailer lines (ignored), then the blank line */
        long at = f->at, le = at;
        while (le < n && buf[le] != '\n') le++;
        if (le >= n) {
            if (f->head + f->overhead + (n - at) > HEAD_MAX) return -431;
            goto more;
        }
        if (le == at || buf[le - 1] != '\r') BAD("bad chunk");
        f->overhead += le + 1 - at;
        if (f->head + f->overhead > HEAD_MAX) return -431;
        f->at = le + 1;
        if (le == at + 1) goto done;
    }
more:
    if (eof) BAD("chunk cut short");
    return 0;
done:
    f->out = sb_finish(body);
    return 1;
}

/* Reads one request from a new connection: its framed message, or 0 with *status: the refusal (400,
 * 408, 413, 431), 0 when the client closed without sending anything (not a request: a readiness probe,
 * a port scanner), or 1 when a stop signal came (the server stops after closing the connection). The
 * deadline and the stop signal are checked on every read (H9). */
static Str *sv_read(long fd, long *status)
{
    HttpFrame f = {0};
    SB *sb = sb_new();
    GC_ROOT(&sb);
    GC_ROOT(&f.body);
    GC_ROOT(&f.out);
    long end = sys_clock_ms() + READ_MS;
    char buf[65536];
    Str *msg_ = 0;
    *status = 0;
    for (;;) {
        long left = end - sys_clock_ms();
        if (left <= 0) {
            *status = sb->len ? 408 : 0;
            break;
        }
        int w = sv_wait(fd, POLLIN, left);
        if (w == 2) {
            *status = 1;
            break;
        }
        if (w != 1) continue;
        long got = vo_syscall6(SYS_recvfrom, fd, (long)buf, sizeof buf, 0, 0, 0);
        if (got == -E_AGAIN) continue;
        if (got <= 0) { /* closed */
            if (!sb->len) break;
            long r = http_frame(&f, sb->ptr, sb->len, 1, 0);
            if (r > 0) msg_ = f.out;
            else *status = r < 0 ? -r : 400;
            break;
        }
        sb_bytes(sb, buf, got);
        long r = http_frame(&f, sb->ptr, sb->len, 0, 0);
        if (r > 0) {
            msg_ = f.out;
            break;
        }
        if (r < 0) {
            *status = -r;
            break;
        }
        long need = f.head + f.length; /* a known length: room for exactly the whole message */
        if (f.head && !f.chunked && f.length >= 0 && sb->cap < need) {
            char *p = alloc_bytes(need);
            mem_copy(p, sb->ptr, sb->len);
            sb->ptr = p;
            sb->cap = need;
        }
    }
    GC_UNROOT(3);
    return msg_;
}

/* result[str]: the next request (framed: head and decoded body), or ok("") when a stop signal came
 * (the server is then stopped). Requests refused by sv_read are answered here and never reach the
 * program. */
Variant *rt_serve_next(long listener)
{
    for (;;) {
        int w = sv_wait(listener, POLLIN, -1);
        if (w == 2) {
        stop:;
            char info[128]; /* struct signalfd_siginfo */
            vo_syscall3(SYS_read, sv_sig, (long)info, sizeof info);
            sv_stop(listener);
            return make_result(0, (long)str_from_cstr(""));
        }
        if (w != 1) continue;
        long fd = vo_syscall4(SYS_accept4, listener, 0, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) continue;
        long status;
        Str *req = sv_read(fd, &status);
        if (!req) {
            if (status > 1)
                sv_refuse(fd, status);
            else
                sys_close((int)fd);
            if (status == 1) goto stop;
            continue;
        }
        sv_conn = fd;
        return make_result(0, (long)req);
    }
}

/* __http_part(message, 0|1): the head (without its blank line) or the body of a framed message, cut
 * without UTF-8 boundary checks (spec 013: the body may hold any bytes). */
Str *rt_http_part(Str *m, long which)
{
    long head = -1;
    for (long i = 0; i + 3 < m->len; i++)
        if (m->data[i] == '\r' && m->data[i + 1] == '\n' && m->data[i + 2] == '\r' && m->data[i + 3] == '\n') {
            head = i;
            break;
        }
    if (head < 0) return which ? str_new("", 0) : m;
    return which ? str_new(m->data + head + 4, m->len - head - 4) : str_new(m->data, head);
}

/* Sends the response text on the current connection and closes it. */
void rt_serve_reply(Str *text)
{
    if (sv_conn < 0) return;
    sv_send(sv_conn, text->data, text->len);
    sys_close((int)sv_conn);
    sv_conn = -1;
}

/* result[Response]: the handler's response, or err(code) after a runtime error in it, whose report
 * went to stderr (R4). The handler runs with the --timeout deadline (R5); the interpreter state is
 * put back where this call started. */
Variant *rt_serve_call(void *fn, long request, Node *call)
{
    long jb[JB_SIZE / 8];
    GC_ROOT(&fn);
    GC_ROOT(&request);
    long saved_vsp = vsp, saved_frame = cur_frame, outer = recover_point, depth = tr_depth_get();
    long saved_gc_on = gc_on, saved_oom = oom_hook; /* spec 013 (H10): R0006 switches them off */
    volatile long result = 0;
    recover_point = (long)jb;
    serve_guard = 1;
    deadline_ms = opt_timeout ? sys_clock_ms() + opt_timeout * 1000 : 0;
    long status = jmp_save(jb); /* 0 ran through, 1 an expect failed (in a test), 2 runtime error */
    if (!status) {
        long r = call_value(fn, call, &request);
        result = (long)make_result(0, r);
    }
    serve_guard = 0;
    recover_point = outer;
    deadline_ms = 0;
    if (status) {
        vsp = saved_vsp;
        cur_frame = saved_frame;
        tr_depth_set(depth);
        rt_reporting = 0;
        gc_on = saved_gc_on; /* spec 013 (H10, L11): the server goes on as before the request */
        oom_hook = saved_oom;
        rt_err_extra = 0;
        if (status == 1 && outer) jmp_restore((void *)outer, 1); /* the enclosing test's failure */
        result = (long)make_result(1, (long)str_from_cstr((char *)rt_err_code));
    }
    GC_UNROOT(2);
    return (Variant *)result;
}
