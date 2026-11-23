/* std.tcp (spec 039): connections and listeners behind the two primitives `__tcp` and `__tcp_io`.
 * A vo value holds only an id into the table here (slot + 256 * generation), so a value kept after
 * close never reaches the connection that reuses its slot. The table and every read buffer are
 * mapped, not static and not in the collected heap: the collector never scans or moves them, and a
 * task (a fork) gets its copy with everything else. Sockets, name lookup and waiting are std.http's
 * (net.c), TLS sessions are tls.c's. An error is the text `KIND: reason`; std/tcp.vo turns the kind
 * into its variant.
 * Recording (research R6) happens where the program waits for the outside: how a connection opened,
 * each piece received, each failed write. In a replay an entry has no socket and its buffer is
 * filled from the recorded pieces, so the three reads are the same code in both modes.
 * ponytail: IPv4 and the first address of a name, as net.c; one accepted connection per call. */
#include "vo.h"

#define TCP_MAX 256
#define READ_MAX (16L << 20)
#define PIECE 65536L
#define MS_MAX 600000L
#define POLLIN 1
#define POLLOUT 4
#define E_AGAIN 11
#define E_PIPE 32
#define E_ADDRINUSE 98
#define E_CONNRESET 104
#define E_ACCES 13
#define MSG_NOSIGNAL 0x4000
#define SOCK_FLAGS 0x80800 /* SOCK_NONBLOCK | SOCK_CLOEXEC */

enum { K_FREE, K_CONN, K_LISTEN };
enum { OP_CONNECT = 1, OP_CONNECT_TLS, OP_LISTEN, OP_ACCEPT, OP_START_TLS, OP_CLOSE, OP_PORT, OP_CLOSE_LISTENER };
enum { IO_READ = 1, IO_EXACT, IO_UNTIL, IO_WRITE, IO_PEER };

typedef struct Entry {
    int kind;
    int gen;      /* part of the id; incremented at close */
    long fd;      /* -1 in a replay */
    long effect;  /* EF_NET or EF_SERVE: what it was opened under, checked on every use */
    int accepted; /* from accept: no start_tls */
    int is_tls;
    int eof;      /* the peer closed; buffered bytes may remain */
    long port;    /* a listener's port; the port a connection was made to */
    void *tls;    /* the session (0 in a replay) */
    char *buf;    /* mapped; unread bytes are buf[pos..len) */
    long pos, len, cap;
    char label[280]; /* "host:port" of the peer, or the listener's address: the name in recordings */
} Entry;

typedef struct SockAddr {
    uint16_t family;
    uint8_t port[2];
    uint8_t addr[4];
    uint8_t zero[8];
} SockAddr;

static Entry *tab;

static char *pages(long n)
{
    long p = vo_syscall6(SYS_mmap, 0, n, 3 /* read, write */, 0x4022 /* private, anonymous, no reserve */, -1, 0);
    return p < 0 && p > -4096 ? 0 : (char *)p;
}

/* `KIND: reason` as an err result. */
static Variant *fail_s(char *kind, Str *reason)
{
    GC_ROOT(&reason);
    SB *sb = sb_new();
    sb_cstr(sb, kind);
    sb_cstr(sb, ": ");
    sb_str(sb, reason);
    GC_UNROOT(1);
    return make_result(1, (long)sb_finish(sb));
}

static Variant *fail(char *kind, char *reason)
{
    return fail_s(kind, str_from_cstr(reason));
}

static Variant *closed(void)
{
    return fail("closed", "the connection is closed");
}

static Entry *entry_of(long id, int kind)
{
    if (!tab || id < 0) return 0;
    Entry *e = &tab[id % TCP_MAX];
    return e->kind == kind && e->gen == id / TCP_MAX ? e : 0;
}

static long id_of(Entry *e)
{
    return (e - tab) + (long)e->gen * TCP_MAX;
}

/* A free entry made into one of kind, or 0 when all 256 are in use (or there is no memory). */
static Entry *entry_new(int kind, long fd, long effect, char *label, long n)
{
    if (!tab) {
        tab = (Entry *)pages(TCP_MAX * (long)sizeof(Entry));
        if (!tab) return 0;
    }
    for (long i = 0; i < TCP_MAX; i++) {
        Entry *e = &tab[i];
        if (e->kind != K_FREE) continue;
        int gen = e->gen + 1;
        *e = (Entry){0};
        e->kind = kind;
        e->gen = gen;
        e->fd = fd;
        e->effect = effect;
        if (n > (long)sizeof e->label - 1) n = sizeof e->label - 1;
        mem_copy(e->label, label, n);
        return e;
    }
    return 0;
}

static void entry_free(Entry *e)
{
    if (e->tls) tls_close(e->tls, !e->eof);
    if (e->fd >= 0) sys_close((int)e->fd);
    if (e->buf) vo_syscall2(SYS_munmap, (long)e->buf, e->cap);
    e->tls = 0;
    e->buf = 0;
    e->kind = K_FREE;
}

static Str *label_of(Entry *e)
{
    return str_from_cstr(e->label);
}

static long tcp_ms; /* the call's time limit: what std/tcp.vo set with `__http_timeout` just before
                       (builtins take three arguments), taken when the primitive is entered */

/* Starts the call's own time limit; 0 when it is out of range. Called after everything that can stop
 * the program (R0010), so a stopped call leaves no limit behind for the next test. */
static int limit_start(void)
{
    if (tcp_ms < 1 || tcp_ms > MS_MAX) return 0;
    net_capped = 0;
    net_cap_ms = rcd_mode == 2 ? 0 : sys_clock_ms() + tcp_ms;
    return 1;
}

static Variant *bad_limit(void)
{
    return fail("invalid", "the time limit must be 1 to 600000 ms");
}

/* Records an outcome under the entry's label (ok: the value; err: the error text), and returns r. */
static Variant *note(long kind, Str *name, Variant *r, Str *ok_value)
{
    if (rcd_mode != 1) return r;
    GC_ROOT(&r);
    GC_ROOT(&name);
    GC_ROOT(&ok_value);
    if (!ok_value) ok_value = str_from_cstr("");
    rcd_event(kind, name, r && r->tag ? EVR_ERR : EVR_OK, r && r->tag ? r->pay.s : ok_value, 0);
    GC_UNROOT(3);
    return r;
}

/* ---- receiving ---- */

/* Room for one more piece after the unread bytes; 0 when memory cannot be mapped. */
static int room(Entry *e, long need)
{
    if (e->cap - e->len >= need) return 1;
    long have = e->len - e->pos;
    if (e->pos && e->cap - have >= need) { /* slide the unread bytes to the start */
        for (long i = 0; i < have; i++) e->buf[i] = e->buf[e->pos + i];
        e->pos = 0;
        e->len = have;
        return 1;
    }
    long cap = e->cap ? e->cap * 2 : PIECE;
    while (cap - have < need) cap *= 2;
    char *b = pages(cap);
    if (!b) return 0;
    mem_copy(b, e->buf + e->pos, have);
    if (e->buf) vo_syscall2(SYS_munmap, (long)e->buf, e->cap);
    e->buf = b;
    e->cap = cap;
    e->pos = 0;
    e->len = have;
    return 1;
}

/* Waits for the peer once and appends what came to the buffer: 0, or the err result (timed out,
 * closed, other). The call's time limit is running. */
static Variant *receive(Entry *e)
{
    if (e->eof) return closed();
    Str *name = label_of(e);
    GC_ROOT(&name);
    Variant *r = 0;
    if (rcd_mode == 2) {
        Event *ev = rp_take(EVK_TCP_RECV, name);
        Str *v = ev->value.s;
        if (ev->result != EVR_OK) {
            if (v->len >= 6 && mem_eq(v->data, "closed", 6)) e->eof = 1;
            r = make_result(1, (long)v);
        } else if (!room(e, v->len)) {
            r = fail("other", "out of memory for the connection's buffer");
        } else {
            mem_copy(e->buf + e->len, v->data, v->len);
            e->len += v->len;
        }
        GC_UNROOT(1);
        return r;
    }
    if (!room(e, PIECE)) {
        GC_UNROOT(1);
        return fail("other", "out of memory for the connection's buffer");
    }
    long got;
    for (;;) {
        if (e->tls) {
            got = tls_read(e->tls, e->buf + e->len, PIECE);
            if (got == TLS_DEADLINE) rt_timeout((Node *)cur_node);
            break;
        }
        got = vo_syscall6(SYS_recvfrom, e->fd, (long)(e->buf + e->len), PIECE, 0, 0, 0);
        if (got != -E_AGAIN) break;
        if (!net_wait(e->fd, POLLIN, -1) && net_capped) break;
    }
    Str *piece = 0;
    GC_ROOT(&piece);
    if (net_capped && got <= 0) {
        r = fail("timed_out", "nothing arrived within the time limit");
    } else if (got == 0 || got == -E_CONNRESET || got == -E_PIPE) {
        e->eof = 1;
        r = fail("closed", "the peer closed the connection");
    } else if (got == TLS_TAMPERED) {
        r = fail("tls", "the connection was tampered with");
    } else if (got < 0) {
        r = fail("other", "the connection failed while reading");
    } else {
        if (rcd_mode == 1) piece = str_new(e->buf + e->len, got);
        e->len += got;
    }
    r = note(EVK_TCP_RECV, name, r, piece);
    GC_UNROOT(2);
    return r;
}

/* Takes the first n unread bytes as the ok result. */
static Variant *take(Entry *e, long n)
{
    Str *out = str_new(e->buf + e->pos, n);
    e->pos += n;
    if (e->pos == e->len) e->pos = e->len = 0;
    return make_result(0, (long)out);
}

static Variant *read_some(Entry *e, long max)
{
    if (max < 1 || max > READ_MAX) return fail("invalid", "the size must be 1 to 16777216 bytes");
    if (e->len == e->pos) {
        Variant *r = receive(e);
        if (r) return r;
    }
    long have = e->len - e->pos;
    return take(e, have < max ? have : max);
}

static Variant *read_exact(Entry *e, long n)
{
    if (n < 1 || n > READ_MAX) return fail("invalid", "the size must be 1 to 16777216 bytes");
    while (e->len - e->pos < n) {
        Variant *r = receive(e);
        if (r) return r;
    }
    return take(e, n);
}

static Variant *read_until(Entry *e, Str *d)
{
    if (d->len < 1) return fail("invalid", "the delimiter is empty");
    GC_ROOT(&d);
    long looked = 0; /* unread bytes already known to hold no delimiter start */
    Variant *r = 0;
    for (;;) {
        long have = e->len - e->pos;
        char *p = e->buf + e->pos;
        for (; looked + d->len <= have; looked++)
            if (p[looked] == d->data[0] && mem_eq(p + looked, d->data, d->len)) break;
        if (looked + d->len <= have) {
            r = take(e, looked + d->len);
            break;
        }
        if (have >= READ_MAX) {
            r = fail("too_large", "16 MiB arrived without the delimiter");
            break;
        }
        r = receive(e);
        if (r) break;
    }
    GC_UNROOT(1);
    return r;
}

/* ---- sending ---- */

static Variant *send_all(Entry *e, Str *data)
{
    Str *name = label_of(e);
    if (rcd_mode == 2) {
        if (rt_testing()) { /* spec 054: a test's recording holds every send, compared with what is sent now */
            Event *ev = rp_take(EVK_TCP_SEND, name);
            if (ev->result != EVR_OK) return make_result(1, (long)ev->value.s);
            if (!str_eq(ev->value.s, data)) rp_mismatch_sent(name, 0);
            return make_result(0, (long)str_new("", 0));
        }
        Event *ev = rp_peek(EVK_TCP_SEND, name);
        return ev ? make_result(1, (long)ev->value.s) : make_result(0, (long)str_new("", 0));
    }
    GC_ROOT(&name);
    GC_ROOT(&data);
    Variant *r = 0;
    if (e->eof) {
        r = fail("closed", "the peer closed the connection");
    } else if (e->tls) {
        long w = tls_write(e->tls, data->data, data->len);
        if (w == TLS_DEADLINE) rt_timeout((Node *)cur_node);
        if (w == TLS_CAPPED) r = fail("timed_out", "the peer did not take the bytes within the time limit");
        if (w == TLS_TAMPERED) r = fail("tls", "the connection was tampered with");
        if (w == TLS_FAILED) {
            e->eof = 1;
            r = fail("closed", "the peer closed the connection");
        }
    } else {
        for (long sent = 0; sent < data->len && !r;) {
            long w = vo_syscall6(SYS_sendto, e->fd, (long)data->data + sent, data->len - sent, MSG_NOSIGNAL, 0, 0);
            if (w == -E_AGAIN) {
                if (!net_wait(e->fd, POLLOUT, -1) && net_capped)
                    r = fail("timed_out", "the peer did not take the bytes within the time limit");
            } else if (w == -E_PIPE || w == -E_CONNRESET) {
                e->eof = 1;
                r = fail("closed", "the peer closed the connection");
            } else if (w <= 0) {
                r = fail("other", "the connection failed while writing");
            } else {
                sent += w;
            }
        }
    }
    if (r) note(EVK_TCP_SEND, name, r, 0);
    else if (rt_testing()) note(EVK_TCP_SEND, name, 0, data); /* spec 054: the bytes, for review and replay */
    GC_UNROOT(2);
    return r ? r : make_result(0, (long)str_new("", 0));
}

/* ---- opening ---- */

static Str *host_port(Str *host, long port, int tls)
{
    GC_ROOT(&host);
    SB *sb = sb_new();
    sb_str(sb, host);
    sb_byte(sb, ':');
    int_to_dec(sb, port);
    if (tls) sb_cstr(sb, " (TLS)");
    GC_UNROOT(1);
    return sb_finish(sb);
}

/* The kind of a handshake failure: tls, unless the limit passed or the arena is full. */
static Variant *tls_fail(Variant *err, int full)
{
    return fail_s(net_capped ? "timed_out" : full ? "too_many" : "tls", err->pay.s);
}

static Variant *tcp_connect(Str *host, long port, int tls)
{
    if (host->len < 1 || host->len > 253) return fail("invalid", "the host is empty or longer than 253 bytes");
    for (long i = 0; i < host->len; i++)
        if ((unsigned char)host->data[i] <= ' ' || host->data[i] == '/' || host->data[i] == ':')
            return fail("invalid", "the host is a name or an IPv4 address, without a scheme, port or path");
    if (port < 1 || port > 65535) return fail("invalid", "the port must be 1 to 65535");
    if (tcp_ms < 1 || tcp_ms > MS_MAX) return bad_limit();
    GC_ROOT(&host);
    rt_check_effect(EF_NET);
    rt_check_net(host, port);
    limit_start();
    Str *name = host_port(host, port, tls);
    GC_ROOT(&name);
    Variant *r = 0;
    GC_ROOT(&r);
    long fd = -1;
    void *session = 0;
    if (rcd_mode == 2) {
        Event *ev = rp_take(EVK_TCP_OPEN, name);
        if (ev->result != EVR_OK) r = make_result(1, (long)ev->value.s);
    } else {
        fd = net_open(host, port, &r);
        if (fd < 0) {
            static char *const kinds[] = {"other", "not_found", "refused", "timed_out", "unreachable", "other"};
            r = fail_s(kinds[net_kind], r->pay.s);
        } else if (tls) {
            int full = 0;
            session = tls_open(fd, host, port, 0, &r, &full);
            if (!session) {
                r = tls_fail(r, full);
                sys_close((int)fd);
                fd = -1;
            }
        } else {
            int one = 1;
            vo_syscall6(SYS_setsockopt, fd, 6 /* IPPROTO_TCP */, 1 /* TCP_NODELAY */, (long)&one, sizeof one, 0);
        }
    }
    net_cap_ms = 0;
    if (!r) {
        Entry *e = entry_new(K_CONN, fd, EF_NET, name->data, name->len - (tls ? 6 : 0));
        if (e) {
            e->tls = session;
            e->is_tls = tls;
            e->port = port;
            note(EVK_TCP_OPEN, name, 0, 0);
            r = make_result(0, id_of(e));
            GC_UNROOT(3);
            return r;
        }
        if (session) tls_close(session, 1);
        if (fd >= 0) sys_close((int)fd);
        r = fail("too_many", "256 connections and listeners are open");
    }
    note(EVK_TCP_OPEN, name, r, 0);
    GC_UNROOT(3);
    return r;
}

static Variant *tcp_start_tls(long id, Str *host)
{
    Entry *e = entry_of(id, K_CONN);
    if (!e) return closed();
    rt_check_effect(e->effect);
    if (e->accepted) return fail("invalid", "an accepted connection cannot start TLS");
    if (e->is_tls) return fail("invalid", "the connection is already TLS");
    if (e->len != e->pos) return fail("invalid", "bytes received before TLS are waiting to be read");
    if (host->len < 1 || host->len > 253) return fail("invalid", "the host is empty or longer than 253 bytes");
    if (!limit_start()) return bad_limit();
    GC_ROOT(&host);
    Str *name = label_of(e);
    GC_ROOT(&name);
    Variant *r = 0;
    GC_ROOT(&r);
    if (rcd_mode == 2) {
        Event *ev = rp_take(EVK_TCP_TLS, name);
        if (ev->result != EVR_OK) r = make_result(1, (long)ev->value.s);
    } else {
        int full = 0;
        e->tls = tls_open(e->fd, host, e->port, 0, &r, &full);
        if (!e->tls) r = tls_fail(r, full);
    }
    net_cap_ms = 0;
    if (!r) e->is_tls = 1;
    note(EVK_TCP_TLS, name, r, 0);
    if (!r) r = make_result(0, 0);
    GC_UNROOT(3);
    return r;
}

static Variant *tcp_listen(Str *address)
{
    long all = 0;
    long port = serve_address_parse(address, &all);
    if (port < 0 || address->len > 200) return fail("invalid", "the address is \"PORT\" or \"0.0.0.0:PORT\"");
    GC_ROOT(&address);
    rt_check_effect(EF_SERVE);
    rt_check_name(EF_SERVE, address);
    Variant *r = 0;
    GC_ROOT(&r);
    long fd = -1;
    if (rcd_mode == 2) {
        Event *ev = rp_take(EVK_TCP_LISTEN, address);
        Str *v = ev->value.s;
        if (ev->result != EVR_OK) {
            r = make_result(1, (long)v);
        } else {
            port = 0;
            for (long i = 0; i < v->len; i++) port = port * 10 + (v->data[i] - '0');
        }
    } else {
        fd = vo_syscall3(SYS_socket, 2 /* AF_INET */, 1 /* SOCK_STREAM */ | SOCK_FLAGS, 0);
        int one = 1;
        SockAddr sa = {2, {(uint8_t)(port >> 8), (uint8_t)port}, {127, 0, 0, 1}, {0}};
        if (all) sa.addr[0] = sa.addr[3] = 0;
        int len = sizeof sa;
        long rc = fd;
        if (rc >= 0) vo_syscall5(SYS_setsockopt, fd, 1 /* SOL_SOCKET */, 2 /* SO_REUSEADDR */, (long)&one, sizeof one);
        if (rc >= 0) rc = vo_syscall3(SYS_bind, fd, (long)&sa, sizeof sa);
        if (rc >= 0) rc = vo_syscall2(SYS_listen, fd, 64);
        if (rc >= 0) rc = vo_syscall3(SYS_getsockname, fd, (long)&sa, (long)&len);
        if (rc < 0) {
            if (fd >= 0) sys_close((int)fd);
            fd = -1;
            r = rc == -E_ADDRINUSE ? fail("in_use", "the port is already in use")
                : rc == -E_ACCES   ? fail("other", "no permission to listen on that port")
                                   : fail("other", "cannot listen on that address");
        } else {
            port = (long)sa.port[0] << 8 | sa.port[1];
        }
    }
    Str *value = 0;
    GC_ROOT(&value);
    if (!r) {
        Entry *e = entry_new(K_LISTEN, fd, EF_SERVE, address->data, address->len);
        if (e) {
            e->port = port;
            value = int_to_str(port);
            note(EVK_TCP_LISTEN, address, 0, value);
            r = make_result(0, id_of(e));
            GC_UNROOT(3);
            return r;
        }
        if (fd >= 0) sys_close((int)fd);
        r = fail("too_many", "256 connections and listeners are open");
    }
    note(EVK_TCP_LISTEN, address, r, 0);
    GC_UNROOT(3);
    return r;
}

static Variant *tcp_accept(long id)
{
    Entry *l = entry_of(id, K_LISTEN);
    if (!l) return fail("closed", "the listener is closed");
    rt_check_effect(l->effect);
    if (!limit_start()) return bad_limit();
    Str *name = label_of(l);
    GC_ROOT(&name);
    Variant *r = 0;
    GC_ROOT(&r);
    Str *peer = 0;
    GC_ROOT(&peer);
    long fd = -1;
    if (rcd_mode == 2) {
        Event *ev = rp_take(EVK_TCP_ACCEPT, name);
        if (ev->result != EVR_OK)
            r = make_result(1, (long)ev->value.s);
        else
            peer = ev->value.s;
    } else {
        SockAddr sa = {0};
        int len = sizeof sa;
        for (;;) {
            fd = vo_syscall4(SYS_accept4, l->fd, (long)&sa, (long)&len, SOCK_FLAGS);
            if (fd != -E_AGAIN) break;
            if (!net_wait(l->fd, POLLIN, -1) && net_capped) break;
        }
        if (fd < 0) {
            r = net_capped ? fail("timed_out", "no connection arrived within the time limit")
                           : fail("other", "cannot accept a connection");
        } else {
            int one = 1;
            vo_syscall6(SYS_setsockopt, fd, 6 /* IPPROTO_TCP */, 1 /* TCP_NODELAY */, (long)&one, sizeof one, 0);
            SB *sb = sb_new();
            for (int i = 0; i < 4; i++) {
                if (i) sb_byte(sb, '.');
                int_to_dec(sb, sa.addr[i]);
            }
            sb_byte(sb, ':');
            int_to_dec(sb, (long)sa.port[0] << 8 | sa.port[1]);
            peer = sb_finish(sb);
        }
    }
    net_cap_ms = 0;
    if (!r) {
        Entry *e = entry_new(K_CONN, fd, EF_SERVE, peer->data, peer->len);
        if (e) {
            e->accepted = 1;
            note(EVK_TCP_ACCEPT, name, 0, peer);
            r = make_result(0, id_of(e));
            GC_UNROOT(3);
            return r;
        }
        if (fd >= 0) sys_close((int)fd);
        r = fail("too_many", "256 connections and listeners are open");
    }
    note(EVK_TCP_ACCEPT, name, r, 0);
    GC_UNROOT(3);
    return r;
}

/* ---- the primitives ---- */

/* __tcp(op, a, text) -> result[int] */
Variant *rt_tcp(long op, long a, Str *text)
{
    tcp_ms = net_timeout_next;
    net_timeout_next = 0;
    switch (op) {
    case OP_CONNECT: return tcp_connect(text, a, 0);
    case OP_CONNECT_TLS: return tcp_connect(text, a, 1);
    case OP_LISTEN: return tcp_listen(text);
    case OP_ACCEPT: return tcp_accept(a);
    case OP_START_TLS: return tcp_start_tls(a, text);
    case OP_CLOSE:
    case OP_CLOSE_LISTENER: { /* harmless on a closed one */
        Entry *e = entry_of(a, op == OP_CLOSE ? K_CONN : K_LISTEN);
        if (e) {
            rt_check_effect(e->effect);
            entry_free(e);
        }
        return make_result(0, 0);
    }
    case OP_PORT: {
        Entry *e = entry_of(a, K_LISTEN);
        return e ? make_result(0, e->port) : fail("closed", "the listener is closed");
    }
    }
    return fail("invalid", "an unknown operation");
}

/* __tcp_io(op + 8 * n, id, data) -> result[bytes]: n is the size of a sized read */
Variant *rt_tcp_io(long op, long id, Str *data)
{
    long n = op >> 3;
    op &= 7;
    tcp_ms = net_timeout_next;
    net_timeout_next = 0;
    Entry *e = entry_of(id, K_CONN);
    if (!e) return closed();
    rt_check_effect(e->effect);
    if (op == IO_PEER) return make_result(0, (long)label_of(e));
    if (!limit_start()) return bad_limit();
    GC_ROOT(&data);
    Variant *r = op == IO_READ    ? read_some(e, n)
                 : op == IO_EXACT ? read_exact(e, n)
                 : op == IO_UNTIL ? read_until(e, data)
                 : op == IO_WRITE ? send_all(e, data)
                                  : fail("invalid", "an unknown operation");
    net_cap_ms = 0;
    GC_UNROOT(1);
    return r;
}
