/* HTTP serving for std.http.serve (spec 011 research R3-R6, R9): listening, reading one request
 * within the limits, replying, and calling the handler under a recovery point. The protocol itself
 * (request parsing, response text) is vo code in std/http.vo; recording and replay wrap these
 * functions (io_serve_*, src/replay.c).
 * ponytail: IPv4 only, Connection: close; many connections are read at once (spec 016) but the handler
 * is called for one request at a time; IPv6 when needed. */
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
static long sv_conn = -1;         /* the index in sv_conns of the request being handled */
static unsigned long sv_old_mask; /* the signal mask before serving */
long serve_guard;                 /* 1 while a handler runs: rt_raise reports, then jumps here */
long sv_listener = -1;            /* the last listening socket (spec 028: serve_workers serves on it) */

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
    sv_listener = fd;
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
        /* RFC 9110 6.3: a 1xx, 204 or 304 response has no body, whatever its headers say (a 304 may
           carry the Content-Length of the representation it stands for) */
        if (response && f->head >= 12 && mem_eq(buf, "HTTP/1.", 7) && buf[8] == ' ' &&
            (buf[9] == '1' || mem_eq(buf + 9, "204", 3) || mem_eq(buf + 9, "304", 3))) {
            f->length = 0;
            f->chunked = 0;
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

/* ---- connections (spec 016, research R14) ----
 * Up to CONN_MAX connections are read at the same time, each within its own READ_MS; the request that
 * became complete first goes to the handler, one call at a time (clarification Q2). Responses are
 * queued and written by the same loop, so a slow reader holds up no one.
 * ponytail: 64 connections; more wait in the accept backlog. */
#define CONN_MAX 64
enum { C_FREE, C_READING, C_READY, C_HANDLING, C_WRITING };

typedef struct Conn {
    long fd;
    long state;
    long deadline; /* reading: the whole request by then (408); writing: the whole response */
    long seq;      /* when it became ready: requests are handled in this order */
    SB *in;        /* what has been received */
    HttpFrame f;   /* its framing state (f.out: the framed request, when complete) */
    Str *out;      /* the response being written */
    long sent;
    long extra;    /* spec 063: bytes received after the request (a taken connection must have none) */
} Conn;

static Conn sv_conns[CONN_MAX]; /* (globals are collector roots) */
static long sv_seq;

static void conn_close(Conn *c)
{
    if (c->state != C_FREE) sys_close((int)c->fd);
    *c = (Conn){.fd = -1, .state = C_FREE};
}

/* Writes what the socket takes now; closes the connection when all is sent or the client left. */
static void conn_write(Conn *c)
{
    while (c->sent < c->out->len) {
        long w = vo_syscall6(SYS_sendto, c->fd, (long)c->out->data + c->sent, c->out->len - c->sent, MSG_NOSIGNAL, 0, 0);
        if (w == -E_AGAIN) return;
        if (w <= 0) break; /* the client went away */
        c->sent += w;
    }
    conn_close(c);
}

/* Queues text on c (a response or a refusal). */
static void conn_reply(Conn *c, Str *text)
{
    c->state = C_WRITING;
    c->out = text;
    c->sent = 0;
    c->deadline = sys_clock_ms() + READ_MS;
    conn_write(c);
}

/* A request the handler never sees (400, 408, 413, 431): answered, then closed. */
static void conn_refuse(Conn *c, long status)
{
    static const char *const lines[] = {"400 Bad Request", "408 Request Timeout", "413 Content Too Large",
                                        "431 Request Header Fields Too Large", "503 Service Unavailable"};
    const char *line = lines[status == 408 ? 1 : status == 413 ? 2 : status == 431 ? 3 : status == 503 ? 4 : 0];
    SB *sb = sb_new();
    sb_cstr(sb, "HTTP/1.1 ");
    sb_cstr(sb, (char *)line);
    sb_cstr(sb, "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    conn_reply(c, sb_finish(sb));
}

/* What the connections hold of requests not yet handled, in bytes. */
static long conns_buffered(void)
{
    long n = 0;
    for (int i = 0; i < CONN_MAX; i++)
        if (sv_conns[i].state != C_FREE && sv_conns[i].in) n += sv_conns[i].in->cap;
    return n;
}

/* Reads what c has (the limits of sv_read before spec 016, unchanged): it becomes ready, is refused, is
 * closed (the client left without sending anything: not a request), or keeps reading. */
static void conn_read(Conn *c)
{
    char buf[65536];
    long got = vo_syscall6(SYS_recvfrom, c->fd, (long)buf, sizeof buf, 0, 0, 0);
    if (got == -E_AGAIN) return;
    long r;
    if (got <= 0) { /* closed */
        if (!c->in->len) {
            conn_close(c);
            return;
        }
        r = http_frame(&c->f, c->in->ptr, c->in->len, 1, 0);
        if (r <= 0) {
            conn_refuse(c, r < 0 ? -r : 400);
            return;
        }
    } else {
        sb_bytes(c->in, buf, got);
        r = http_frame(&c->f, c->in->ptr, c->in->len, 0, 0);
        if (r < 0) {
            conn_refuse(c, -r);
            return;
        }
        long need = c->f.head + c->f.length; /* a known length: room for exactly the whole message */
        int grow = !r && c->f.head && !c->f.chunked && c->f.length >= 0 && c->in->cap < need;
        /* All unhandled requests together stay under a quarter of the memory limit (and one request of
         * the largest size always fits): clients announcing or sending large bodies on many connections
         * are answered 503, where they used to end the server with R0006. */
        long budget = gc_limit / 4 > BODY_MAX + HEAD_MAX ? gc_limit / 4 : BODY_MAX + HEAD_MAX;
        if (!r && conns_buffered() + (grow ? need - c->in->cap : 0) > budget) {
            c->in = 0;
            conn_refuse(c, 503);
            return;
        }
        if (grow) {
            char *p = alloc_bytes(need);
            mem_copy(p, c->in->ptr, c->in->len);
            c->in->ptr = p;
            c->in->cap = need;
        }
        if (!r) return;
    }
    c->state = C_READY;
    c->seq = ++sv_seq;
}

/* The stop signal: queued responses are finished (each within its time), the rest closed. */
static void conns_stop(void)
{
    for (int i = 0; i < CONN_MAX; i++) {
        Conn *c = &sv_conns[i];
        if (c->state == C_WRITING) {
            sv_send(c->fd, c->out->data + c->sent, c->out->len - c->sent);
            c->state = C_READING; /* (closed below) */
        }
        if (c->state != C_FREE) conn_close(c);
    }
}

/* ---- the connection loop, shared by serve and serve_workers (spec 028) ---- */

/* The oldest complete request: its connection is handled from now on and *req gets the framed request;
 * the connection's index, or -1 when no request is complete. */
long sv_take_ready(Str **req)
{
    Conn *best = 0; /* the request that became complete first */
    for (int i = 0; i < CONN_MAX; i++) {
        Conn *c = &sv_conns[i];
        if (c->state == C_READY && (!best || c->seq < best->seq)) best = c;
    }
    if (!best) return -1;
    best->state = C_HANDLING;
    best->extra = !best->f.chunked && best->f.length >= 0 && best->in ? best->in->len - (best->f.head + best->f.length) : 0;
    *req = best->f.out;
    best->in = 0; /* the request is the handler's now */
    best->f.out = 0;
    best->f.body = 0;
    return best - sv_conns;
}

/* One round of waiting: accepts new connections (when accepting and there is room), reads requests,
 * writes responses, expires connections out of time; the extra file descriptors are polled too (their
 * revents filled in). 1 when a stop signal came (it is read), 0 otherwise. */
int sv_round(long listener, int accepting, SvPoll *extra, long nextra)
{
    struct {
        int fd;
        short events, revents;
    } p[CONN_MAX + 2 + SV_EXTRA_MAX];
    Conn *who[CONN_MAX + 2 + SV_EXTRA_MAX];
    long np = 0, now = sys_clock_ms(), wait = -1, open = 0;
    for (int i = 0; i < CONN_MAX; i++) open += sv_conns[i].state != C_FREE;
    p[np].fd = (int)sv_sig;
    p[np].events = POLLIN;
    p[np].revents = 0;
    who[np++] = 0;
    long lst = -1; /* the listener's place in p */
    if (accepting && open < CONN_MAX) {
        lst = np;
        p[np].fd = (int)listener;
        p[np].events = POLLIN;
        p[np].revents = 0;
        who[np++] = 0;
    }
    for (int i = 0; i < CONN_MAX; i++) {
        Conn *c = &sv_conns[i];
        if (c->state != C_READING && c->state != C_WRITING) continue;
        if (c->deadline <= now) { /* out of time: 408 for a part of a request, else just closed */
            if (c->state == C_READING && c->in->len) conn_refuse(c, 408); /* (then written in time) */
            else conn_close(c);
            continue;
        }
        long left = c->deadline - now;
        if (wait < 0 || left < wait) wait = left;
        p[np].fd = (int)c->fd;
        p[np].events = c->state == C_READING ? POLLIN : POLLOUT;
        p[np].revents = 0;
        who[np++] = c;
    }
    long first_extra = np;
    for (long k = 0; k < nextra; k++) {
        extra[k].revents = 0;
        p[np].fd = extra[k].fd;
        p[np].events = extra[k].events;
        p[np].revents = 0;
        who[np++] = 0;
    }
    if (sv_wake_at) { /* spec 055: the drain's end */
        long left = sv_wake_at > now ? sv_wake_at - now : 0;
        if (wait < 0 || left < wait) wait = left;
    }
    long ts[2] = {wait / 1000, wait % 1000 * 1000000};
    long n = vo_syscall5(SYS_ppoll, (long)p, np, wait < 0 ? 0 : (long)ts, 0, 8);
    if (n <= 0) return 0;
    if (p[0].revents) { /* a stop signal */
        char info[128]; /* struct signalfd_siginfo */
        vo_syscall3(SYS_read, sv_sig, (long)info, sizeof info);
        return 1;
    }
    for (long k = 0; k < nextra; k++) extra[k].revents = p[first_extra + k].revents;
    for (long i = 1; i < first_extra; i++) {
        if (!p[i].revents) continue;
        Conn *c = who[i];
        if (i == lst) { /* the listener: every waiting connection, while there is room */
            for (;;) {
                Conn *f = 0;
                for (int k = 0; k < CONN_MAX && !f; k++)
                    if (sv_conns[k].state == C_FREE) f = &sv_conns[k];
                if (!f) break;
                long fd = vo_syscall4(SYS_accept4, listener, 0, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (fd < 0) break;
                f->fd = fd;
                f->state = C_READING;
                f->deadline = sys_clock_ms() + READ_MS;
                f->in = sb_new();
            }
        } else if (c && c->state == C_READING) {
            conn_read(c);
        } else if (c && c->state == C_WRITING) {
            conn_write(c);
        }
    }
    return 0;
}

/* Queues text as the response on connection conn (being handled). */
void sv_reply_to(long conn, Str *text)
{
    if (conn >= 0 && conn < CONN_MAX && sv_conns[conn].state == C_HANDLING) conn_reply(&sv_conns[conn], text);
}

/* Spec 055: the drain. sv_wake_at: when sv_round must return at the latest (0: no limit). */
long sv_wake_at;

/* Requests read but not yet handled are answered 503 (the drain's limit passed); how many. */
long sv_refuse_ready(void)
{
    long n = 0;
    for (int i = 0; i < CONN_MAX; i++)
        if (sv_conns[i].state == C_READY) {
            conn_refuse(&sv_conns[i], 503);
            n++;
        }
    return n;
}

/* The record a stopped server writes (research R11): the limit in force, requests answered while draining,
 * requests cut off at the limit (level warn when any). */
void sv_stopped(long drained, long cut)
{
    sv_wake_at = 0;
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, "\"drain_s\":");
    sb_str(sb, int_to_str(opt_drain));
    sb_cstr(sb, ",\"drained\":");
    sb_str(sb, int_to_str(drained));
    sb_cstr(sb, ",\"cut_off\":");
    sb_str(sb, int_to_str(cut));
    Str *fields = sb_finish(sb);
    GC_ROOT(&fields);
    rt_log_write(cut ? 2 : 1, str_from_cstr("\"stopped\""), fields);
    GC_UNROOT(2);
}

/* Requests still to answer: complete, being handled, or being written. */
long sv_pending(void)
{
    long n = 0;
    for (int i = 0; i < CONN_MAX; i++) {
        long s = sv_conns[i].state;
        n += s == C_READY || s == C_HANDLING || s == C_WRITING;
    }
    return n;
}

/* Shutting down: connections whose request is not complete yet are closed (only requests already read
 * are answered). */
void sv_drop_partial(void)
{
    for (int i = 0; i < CONN_MAX; i++)
        if (sv_conns[i].state == C_READING) conn_close(&sv_conns[i]);
}

/* The end of serving: queued responses finished, the rest closed, the listener and the signals back. */
void sv_end(long listener)
{
    conns_stop();
    sv_stop(listener);
}

/* In a forked worker: its copies of the listener, the signal descriptor and the connections are closed
 * (the parent owns them), without touching the parent's. */
void sv_child_close(long listener)
{
    sys_close((int)listener);
    if (sv_sig >= 0) sys_close((int)sv_sig);
    for (int i = 0; i < CONN_MAX; i++)
        if (sv_conns[i].state != C_FREE) sys_close((int)sv_conns[i].fd);
}

/* A note on stderr (with --json, a `note` record), spec 028. */
void sv_note(Str *m)
{
    GC_ROOT(&m);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"note\",\"message\":");
        json_str_into(sb, m);
        sb_cstr(sb, "}\n");
    } else {
        sb_cstr(sb, "note: ");
        sb_str(sb, m);
        sb_byte(sb, '\n');
    }
    out_bytes(2, sb->ptr, sb->len);
    GC_UNROOT(2);
}

/* result[str]: the next request (framed: head and decoded body), or ok("") when a stop signal came
 * (the server is then stopped). Requests refused here never reach the program. */
Variant *rt_serve_next(long listener)
{
    static int noted;
    if (opt_workers && !noted) { /* spec 028: --workers is serve_workers' */
        noted = 1;
        sv_note(str_from_cstr("--workers has no effect: this program serves one request at a time (serve_workers runs several)"));
    }
    static long drain_end, drained; /* spec 055: after the first signal, requests already read are answered
                                       until --drain seconds pass (checked between requests) */
    long cut = 0;
    for (;;) {
        if (!drain_end && sys_wait_fd(sv_sig, POLLIN, 0) > 0) { /* a stop signal during the last handler */
            char info[128]; /* struct signalfd_siginfo */
            vo_syscall3(SYS_read, sv_sig, (long)info, sizeof info);
            drain_end = sys_clock_ms() + opt_drain * 1000;
            sv_wake_at = drain_end;
            sv_drop_partial();
            continue;
        }
        if (drain_end && sys_clock_ms() >= drain_end) {
            cut = sv_refuse_ready();
            break;
        }
        Str *req;
        long i = sv_take_ready(&req);
        if (i >= 0) {
            sv_conn = i;
            drained += drain_end != 0;
            return make_result(0, (long)req);
        }
        if (drain_end && !sv_pending()) break;
        if (sv_round(listener, !drain_end, 0, 0)) {
            if (drain_end) break; /* a second signal: stop at once */
            drain_end = sys_clock_ms() + opt_drain * 1000;
            sv_wake_at = drain_end;
            sv_drop_partial();
        }
    }
    conns_stop();
    sv_stop(listener);
    sv_stopped(drained, cut);
    drain_end = drained = 0;
    return make_result(0, (long)str_from_cstr(""));
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

/* Queues the response text on the connection being handled; rt_serve_next writes it and closes it. */
void rt_serve_reply(Str *text)
{
    sv_take_end(); /* spec 063: a connection the handler took is not answered here */
    if (sv_conn < 0) return;
    conn_reply(&sv_conns[sv_conn], text);
    sv_conn = -1;
}

/* result[Response]: the handler's response, or err(code) after a runtime error in it, whose report
 * went to stderr (R4). The handler runs with the --timeout deadline (R5); the interpreter state is
 * put back where this call started. */
Variant *rt_serve_call(void *fn, long request, Node *call)
{
    return rt_serve_call_n(fn, &request, 1, call);
}

/* The same with n argument values (spec 028: a worker's handler takes the setup's result too). */
Variant *rt_serve_call_n(void *fn, long *vals, long n, Node *call)
{
    long jb[JB_SIZE / 8];
    GC_ROOT(&fn);
    for (long k = 0; k < n; k++) GC_ROOT(&vals[k]);
    long saved_vsp = vsp, saved_frame = cur_frame, outer = recover_point, depth = tr_depth_get();
    long saved_gc_on = gc_on, saved_oom = oom_hook; /* spec 013 (H10): R0006 switches them off */
    long saved_guard = serve_guard; /* spec 029: std.web's call inside a server's handler call nests */
    volatile long result = 0;
    recover_point = (long)jb;
    serve_guard = 1;
    if (!saved_guard) deadline_ms = opt_timeout ? sys_clock_ms() + opt_timeout * 1000 : 0; /* (nested: the outer one goes on) */
    long status = jmp_save(jb); /* 0 ran through, 1 an expect failed (in a test), 2 runtime error */
    if (!status) {
        long r = call_value(fn, call, vals);
        result = (long)make_result(0, r);
    }
    serve_guard = saved_guard;
    recover_point = outer;
    if (!saved_guard) deadline_ms = 0; /* (nested: the outer call's deadline goes on) */
    if (status) {
        sv_handler_failed = status == 2; /* spec 063: a taken WebSocket is closed with 1011 */
        vsp = saved_vsp;
        cur_frame = saved_frame;
        tr_depth_set(depth);
        rt_reporting = 0;
        gc_on = saved_gc_on; /* spec 013 (H10, L11): the server goes on as before the request */
        oom_hook = saved_oom;
        rt_err_extra = 0;
        if (status == 1 && outer) rt_recover(1); /* the enclosing test's failure (recover_point is outer again) */
        result = (long)make_result(1, (long)str_from_cstr((char *)rt_err_code));
    }
    GC_UNROOT(n + 1);
    return (Variant *)result;
}

/* ---- spec 063 (research R1): a handler takes its request's connection ---- */

static long sv_taken = -1; /* the std.tcp id of the connection the running handler took, or -1 */
static long sv_taken_kind; /* 1 an event stream, 2 a WebSocket */
long sv_handler_failed;    /* set when a handler call stops with a runtime error (rt_serve_call_n) */

/* In this process (serve): the descriptor of the request being handled, its slot freed without closing
 * it; -1 with *why when there is none or it cannot be taken. */
static long sv_take_here(char **why)
{
    if (sv_conn < 0 || sv_conns[sv_conn].state != C_HANDLING || sv_taken >= 0) {
        *why = "no connection";
        return -1;
    }
    Conn *c = &sv_conns[sv_conn];
    if (c->extra > 0) {
        *why = "bytes after the request";
        return -1;
    }
    long fd = c->fd;
    *c = (Conn){.fd = -1, .state = C_FREE};
    sv_conn = -1;
    return fd;
}

/* Server side of serve_workers: the descriptor of connection conn for a worker, its slot freed (the
 * parent's copy is closed by the caller once sent); -1 with *why. */
long sv_give(long conn, char **why)
{
    if (conn < 0 || conn >= CONN_MAX || sv_conns[conn].state != C_HANDLING) {
        *why = "no connection";
        return -1;
    }
    Conn *c = &sv_conns[conn];
    if (c->extra > 0) {
        *why = "bytes after the request";
        return -1;
    }
    long fd = c->fd;
    *c = (Conn){.fd = -1, .state = C_FREE};
    return fd;
}

/* result[int]: the connection of the request being handled, moved into this process's std.tcp table
 * (kind 1 an event stream, 2 a WebSocket; max_open the route's bound, 0: half the workers). Errors:
 * "busy" (the bound is reached), "no connection" (outside a serving handler, or taken already), "bytes
 * after the request", "too many connections". Recorded as a serve_take event; a replay takes no
 * descriptor. */
Variant *rt_serve_take(long kind, long max_open)
{
    Str *name = str_from_cstr("request");
    GC_ROOT(&name);
    Variant *r;
    if (rcd_mode == 2) {
        Event *ev = rp_take(EVK_SERVE_TAKE, name);
        if (ev->result != EVR_OK) {
            r = make_result(1, (long)ev->value.s);
        } else {
            long id = tcp_adopt(-1, "request");
            sv_taken = id;
            sv_taken_kind = kind;
            r = id < 0 ? make_result(1, (long)str_from_cstr("too many connections")) : make_result(0, id);
        }
        GC_UNROOT(1);
        return r;
    }
    char *why = 0;
    long fd = sw_in_worker() ? sw_take(kind, max_open, &why) : sv_take_here(&why);
    long id = -1;
    if (fd >= 0) {
        id = tcp_adopt(fd, "request");
        if (id < 0) {
            sys_close((int)fd);
            why = "too many connections";
        }
    }
    if (id >= 0) {
        sv_taken = id;
        sv_taken_kind = kind;
        deadline_ms = 0; /* --timeout limits answering the request; a conversation is limited by its own time
                            limits, the client leaving, the route's bound and the server's stop */
        if (sw_in_worker()) {
            net_wake_set(sw_pair_fd(), 1); /* spec 063: the server's stop wakes the handler */
        } else {
            net_wake_set(sv_sig, 0);
            sv_gauge(1);
        }
        r = make_result(0, id);
    } else {
        r = make_result(1, (long)str_from_cstr(why));
    }
    if (rcd_mode == 1) {
        GC_ROOT(&r);
        rcd_event(EVK_SERVE_TAKE, name, id >= 0 ? EVR_OK : EVR_ERR, id >= 0 ? str_from_cstr("") : r->pay.s, 0);
        GC_UNROOT(1);
    }
    GC_UNROOT(1);
    return r;
}

/* Spec 063 (FR-015): the gauge http_conversations_open, set whenever the number held changes. */
void sv_gauge(long n)
{
    static const char key[] = "2\037http_conversations_open\037Conversations (event streams and WebSockets) being held open.\037\037\037";
    Str *k = str_new((char *)key, sizeof key - 1);
    double x = (double)n;
    long bits;
    mem_copy((char *)&bits, (char *)&x, 8);
    rt_metric(2, k, bits, (Node *)cur_node);
}

/* The handler returned (or failed): a connection it took and left open is closed (research R1). */
void sv_take_end(void)
{
    if (sv_taken >= 0) {
        tcp_finish(sv_taken, sv_taken_kind, (int)sv_handler_failed);
        if (!sw_in_worker() && rcd_mode != 2) sv_gauge(0);
    }
    sv_taken = -1;
    net_wake_set(-1, 0);
    sv_handler_failed = 0;
}
