/* Plain HTTP for std.http (spec 007 US5, research R9): `__http_exchange(host, port, request)`
 * resolves the host (an IPv4 literal, /etc/hosts, then one DNS query), connects, sends the request
 * and reads the response until the server closes, every wait bounded by the --timeout deadline
 * (R0009). Recording and replay wrap it (io_http_exchange, src/replay.c).
 * ponytail: IPv4 and the first nameserver of /etc/resolv.conf only, one request per connection;
 * IPv6, search domains and several servers when someone needs them. */
#include "vo.h"

#define AF_INET 2
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_NONBLOCK 0x800 /* O_NONBLOCK on x86-64, ARM64 and RISC-V */
#define MSG_NOSIGNAL 0x4000 /* no SIGPIPE: the toolchain has no signal handlers */
#define POLLIN 1
#define POLLOUT 4
#define E_AGAIN 11
#define E_INPROGRESS 115
#define E_ISCONN 106
#define E_CONNREFUSED 111
#define MAX_RESPONSE (16L << 20) /* ponytail: 16 MiB, the documented cap */
#define DNS_TRIES 3
#define DNS_WAIT_MS 2000

typedef struct SockAddrIn {
    uint16_t family;
    uint8_t port[2]; /* network order */
    uint8_t addr[4];
    uint8_t zero[8];
} SockAddrIn;

long net_timeout_next;
long net_cap_ms;
long net_capped;
int net_kind; /* spec 039: why the last net_open failed (NET_*), for std.tcp's typed errors */

/* Waits for events on fd: 1 when ready, 0 after cap_ms (cap_ms < 0: no cap of its own). The run's
 * deadline is the outer bound: past it, the run stops with R0009 at the current call. A request's
 * own timeout (net_cap_ms) ends the wait with 0 and net_capped set: the caller gives up with an err. */
int net_wait(long fd, int events, long cap_ms)
{
    long wait = cap_ms;
    if (deadline_ms) {
        long left = deadline_ms - sys_clock_ms();
        if (left <= 0) rt_timeout((Node *)cur_node);
        if (wait < 0 || left < wait) wait = left;
    }
    if (net_cap_ms) {
        long left = net_cap_ms - sys_clock_ms();
        if (left <= 0) {
            net_capped = 1;
            return 0;
        }
        if (wait < 0 || left < wait) wait = left;
    }
    if (sys_wait_fd(fd, events, wait)) return 1;
    if (deadline_ms && sys_clock_ms() >= deadline_ms) rt_timeout((Node *)cur_node);
    if (net_cap_ms && sys_clock_ms() >= net_cap_ms) net_capped = 1;
    return 0;
}

/* The err of a request that ran into its own timeout. */
Variant *net_timed_out(Str *host, long port)
{
    return net_err("the request to $1 timed out", host, port);
}

/* Parses a dotted IPv4 address (four decimal parts 0..255) of n bytes; 1 when it is one. */
int parse_ipv4(char *s, long n, uint8_t *out)
{
    long part = 0, value = -1;
    for (long i = 0; i <= n; i++) {
        if (i == n || s[i] == '.') {
            if (value < 0 || part > 3) return 0;
            out[part++] = (uint8_t)value;
            value = -1;
        } else if (s[i] >= '0' && s[i] <= '9') {
            value = (value < 0 ? 0 : value * 10) + (s[i] - '0');
            if (value > 255) return 0;
        } else {
            return 0;
        }
    }
    return part == 4;
}

static int net_space(int c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

static int lower_eq(char *a, long n, Str *b)
{
    if (n != b->len) return 0;
    for (long i = 0; i < n; i++) {
        int x = (unsigned char)a[i], y = (unsigned char)b->data[i];
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
    }
    return 1;
}

/* The first IPv4 address /etc/hosts gives host; 1 when found. */
static int hosts_lookup(Str *host, uint8_t *out)
{
    char *text;
    long n = read_whole_file("/etc/hosts", &text);
    if (n < 0) return 0;
    char *p = text, *end = text + n;
    while (p < end) {
        char *le = p;
        while (le < end && *le != '\n' && *le != '#') le++;
        char *q = p, *addr = 0; /* the line's first word is the address, the others are names */
        long addr_len = 0;
        while (q < le) {
            while (q < le && net_space((unsigned char)*q)) q++;
            char *w = q;
            while (q < le && !net_space((unsigned char)*q)) q++;
            if (q == w) break;
            if (!addr) {
                addr = w;
                addr_len = q - w;
            } else if (lower_eq(w, q - w, host) && parse_ipv4(addr, addr_len, out)) {
                return 1;
            }
        }
        while (le < end && *le != '\n') le++; /* past a comment */
        p = le + 1;
    }
    return 0;
}

/* The first `nameserver` of /etc/resolv.conf that is an IPv4 address; 1 when there is one. */
static int resolver(uint8_t *out)
{
    char *text;
    long n = read_whole_file("/etc/resolv.conf", &text);
    if (n < 0) return 0;
    char *p = text, *end = text + n;
    while (p < end) {
        char *le = p;
        while (le < end && *le != '\n') le++;
        if (le - p > 11 && mem_eq(p, "nameserver", 10) && net_space((unsigned char)p[10])) {
            char *a = p + 10;
            while (a < le && net_space((unsigned char)*a)) a++;
            char *b = a;
            while (b < le && !net_space((unsigned char)*b)) b++;
            if (parse_ipv4(a, b - a, out)) return 1;
        }
        p = le + 1;
    }
    return 0;
}

static long net_socket(int type)
{
    return vo_syscall3(SYS_socket, AF_INET, type | SOCK_NONBLOCK | 0x80000 /* SOCK_CLOEXEC */, 0);
}

static long net_connect(long fd, uint8_t *addr, long port)
{
    SockAddrIn sa = {AF_INET, {(uint8_t)(port >> 8), (uint8_t)port}, {addr[0], addr[1], addr[2], addr[3]}, {0}};
    return vo_syscall3(SYS_connect, fd, (long)&sa, sizeof sa);
}

/* One DNS A query for host to the resolver: 1 with the address, 0 for "no such host", -1 when
 * no usable answer came (no server, no reply, an error reply). */
static int dns_lookup(Str *host, uint8_t *out)
{
    uint8_t ns[4];
    if (!resolver(ns)) return -1;
    uint8_t q[512]; /* the query: header, QNAME, QTYPE A, QCLASS IN */
    static const uint8_t head[12] = {0x76, 0x6f, 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, 0};
    mem_copy((char *)q, (char *)head, 12);
    vo_syscall3(SYS_getrandom, (long)q, 2, 0); /* spec 013 (L8): a random ID (the fixed one if it fails) */
    long n = 12, label = 0;
    for (long i = 0; i <= host->len; i++) {
        if (i == host->len || host->data[i] == '.') {
            long len = i - label;
            if (len < 1 || len > 63 || n + len + 6 > 512) return 0; /* not a host name */
            q[n++] = (uint8_t)len;
            mem_copy((char *)q + n, host->data + label, len);
            n += len;
            label = i + 1;
        }
    }
    q[n++] = 0;
    q[n++] = 0;
    q[n++] = 1; /* A */
    q[n++] = 0;
    q[n++] = 1; /* IN */
    long fd = net_socket(SOCK_DGRAM);
    if (fd < 0) return -1;
    int found = -1;
    if (net_connect(fd, ns, 53) >= 0) {
        uint8_t r[512];
        for (int t = 0; t < DNS_TRIES && found < 0; t++) {
            if (vo_syscall6(SYS_sendto, fd, (long)q, n, MSG_NOSIGNAL, 0, 0) != n) break;
            if (!net_wait(fd, POLLIN, DNS_WAIT_MS)) { /* no reply yet: ask again */
                if (net_capped) break;
                continue;
            }
            long got = vo_syscall6(SYS_recvfrom, fd, (long)r, sizeof r, 0, 0, 0);
            /* our ID, a reply, and our question echoed (spec 013, L8) */
            if (got < n || r[0] != q[0] || r[1] != q[1] || !(r[2] & 0x80) || r[4] || r[5] != 1 ||
                !mem_eq((char *)r + 12, (char *)q + 12, n - 12))
                continue;
            int rcode = r[3] & 15;
            if (rcode == 3) { /* NXDOMAIN */
                found = 0;
                break;
            }
            if (rcode) break;
            long qd = r[4] << 8 | r[5], an = r[6] << 8 | r[7], p = 12;
            for (long k = 0; k < qd + an && p < got; k++) {
                while (p < got && r[p]) { /* the name: labels, or a compression pointer */
                    if ((r[p] & 0xc0) == 0xc0) {
                        p++;
                        break;
                    }
                    p += r[p] + 1;
                }
                p++;
                if (k < qd) { /* a question: QTYPE, QCLASS */
                    p += 4;
                    continue;
                }
                if (p + 10 > got) break;
                long type = r[p] << 8 | r[p + 1], cls = r[p + 2] << 8 | r[p + 3], rdlen = r[p + 8] << 8 | r[p + 9];
                p += 10;
                if (type == 1 && cls == 1 && rdlen == 4 && p + 4 <= got) {
                    mem_copy((char *)out, (char *)r + p, 4);
                    found = 1;
                    break;
                }
                p += rdlen;
            }
            if (found < 0) found = 0; /* an answer without an address */
        }
    }
    sys_close((int)fd);
    return found;
}

Variant *net_err(char *tmpl, Str *host, long port)
{
    SB *sb = sb_new();
    sb_str(sb, host);
    sb_byte(sb, ':');
    int_to_dec(sb, port);
    return make_result(1, (long)msg(tmpl, sb_finish(sb), 0, 0));
}

/* Resolves host and connects to port (spec 019: shared by HTTP and HTTPS): the connected,
 * non-blocking socket, or -1 with *err set to the err result saying why. */
long net_open(Str *host, long port, Variant **err)
{
    GC_ROOT(&host);
    uint8_t addr[4];
    *err = 0;
    if (!parse_ipv4(host->data, host->len, addr) && !hosts_lookup(host, addr)) {
        int r = dns_lookup(host, addr);
        if (net_capped) {
            net_kind = NET_TIMED_OUT;
            *err = net_timed_out(host, port);
            GC_UNROOT(1);
            return -1;
        }
        if (r <= 0) {
            net_kind = NET_NOT_FOUND;
            *err = make_result(1, (long)msg(r == 0 ? "unknown host `$1`" : "cannot resolve `$1`: no answer from a name server",
                                            host, 0, 0));
            GC_UNROOT(1);
            return -1;
        }
    }
    long fd = net_socket(SOCK_STREAM);
    if (fd < 0) {
        net_kind = NET_OTHER;
        *err = make_err("cannot open a socket");
        GC_UNROOT(1);
        return -1;
    }
    long c = net_connect(fd, addr, port);
    if (c == -E_INPROGRESS) { /* wait, then ask again: 0 or EISCONN when connected */
        net_wait(fd, POLLOUT, -1);
        c = net_connect(fd, addr, port);
        if (c == -E_ISCONN) c = 0;
    }
    if (c < 0) {
        sys_close((int)fd);
        net_kind = net_capped ? NET_TIMED_OUT : c == -E_CONNREFUSED ? NET_REFUSED : c == -101 || c == -113 ? NET_UNREACHABLE : NET_OTHER;
        *err = net_capped ? net_timed_out(host, port) : net_err(c == -E_CONNREFUSED ? "connection refused by $1" : "cannot connect to $1", host, port);
        GC_UNROOT(1);
        return -1;
    }
    GC_UNROOT(1);
    return fd;
}

/* result[str]: the raw response, or err with the reason. The host is not checked here: the
 * caller (io_http_exchange) checks it against the running grant first. */
Variant *rt_http_exchange(Str *host, long port, Str *request)
{
    GC_ROOT(&host);
    GC_ROOT(&request);
    Variant *result = 0;
    long fd = net_open(host, port, &result);
    if (fd < 0) {
        GC_UNROOT(2);
        return result;
    }
    for (long sent = 0; sent < request->len;) {
        long w = vo_syscall6(SYS_sendto, fd, (long)request->data + sent, request->len - sent, MSG_NOSIGNAL, 0, 0);
        if (w == -E_AGAIN) {
            net_wait(fd, POLLOUT, -1);
            if (!net_capped) continue;
            sys_close((int)fd);
            GC_UNROOT(2);
            return net_timed_out(host, port);
        }
        if (w <= 0) {
            sys_close((int)fd);
            GC_UNROOT(2);
            return net_err("the connection to $1 failed while sending", host, port);
        }
        sent += w;
    }
    SB *sb = sb_new();
    GC_ROOT(&sb);
    char buf[65536];
    for (;;) {
        long got = vo_syscall6(SYS_recvfrom, fd, (long)buf, sizeof buf, 0, 0, 0);
        if (got == -E_AGAIN) {
            net_wait(fd, POLLIN, -1);
            if (!net_capped) continue;
            result = net_timed_out(host, port);
            break;
        }
        if (got < 0) {
            result = net_err("the connection to $1 failed while reading", host, port);
            break;
        }
        if (got == 0) break; /* the server closed: the whole response */
        if (sb->len + got > MAX_RESPONSE) {
            result = net_err("the response from $1 is larger than 16 MiB", host, port);
            break;
        }
        sb_bytes(sb, buf, got);
    }
    sys_close((int)fd);
    if (!result) result = make_result(0, (long)sb_finish(sb));
    GC_UNROOT(3);
    return result;
}
