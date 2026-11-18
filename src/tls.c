/* HTTPS for std.http (spec 019): vo's layer over the vendored Mbed TLS (third_party/mbedtls,
 * constitution 1.4.0). Every call into the library is in this file.
 *
 * The library runs without a C library through its own hooks (research R2): memory comes from one
 * mapped region through its buffer allocator, entropy from getrandom, wall-clock time from
 * clock_gettime. Its memory lives outside vo's collected heap, so the collector never sees it. */
#include "vo.h"

#include "mbedtls/memory_buffer_alloc.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/platform_util.h"
#include "psa/crypto.h"

/* ponytail: one 4 MiB region per process for the trust store (about 0.4 MiB for the 120 roots of a
 * common store) and connections (research R2, measured in implementation: the allocator zeroes the
 * whole region at startup, so 16 MiB cost 15 ms); grow it if a very large trust store needs more. */
#define TLS_ARENA (4L << 20)

static int tls_ready;

/* ---- hooks the library calls (research R2) ---- */

/* Entropy for the random generator: getrandom, the whole buffer (EINTR and short reads looped). */
int mbedtls_platform_get_entropy(psa_driver_get_entropy_flags_t flags, size_t *estimate_bits,
                                 unsigned char *output, size_t output_size)
{
    if (flags != 0) return PSA_ERROR_NOT_SUPPORTED;
    size_t got = 0;
    while (got < output_size) {
        long r = vo_syscall3(SYS_getrandom, (long)(output + got), (long)(output_size - got), 0);
        if (r == -4) continue; /* EINTR */
        if (r <= 0) return PSA_ERROR_INSUFFICIENT_ENTROPY;
        got += (size_t)r;
    }
    *estimate_bits = 8 * output_size;
    return 0;
}

/* Seconds since the Unix epoch, for certificate validity (the machine's clock: --clock-ms only
 * fixes the program's clock, research R9). */
static mbedtls_time_t tls_time(mbedtls_time_t *t)
{
    mbedtls_time_t now = (mbedtls_time_t)(sys_clock_real_ms() / 1000);
    if (t) *t = now;
    return now;
}

mbedtls_ms_time_t mbedtls_ms_time(void)
{
    return (mbedtls_ms_time_t)sys_clock_ms();
}

/* UTC calendar fields of a time (the days-to-civil algorithm, valid for every year after 1970). */
struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *tt, struct tm *tm_buf)
{
    long long t = (long long)*tt;
    long long days = t / 86400, secs = t % 86400;
    if (secs < 0) {
        secs += 86400;
        days--;
    }
    tm_buf->tm_hour = (int)(secs / 3600);
    tm_buf->tm_min = (int)(secs % 3600 / 60);
    tm_buf->tm_sec = (int)(secs % 60);
    tm_buf->tm_wday = (int)((days % 7 + 11) % 7); /* 1970-01-01 was a Thursday */
    long long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    long long d = doy - (153 * mp + 2) / 5 + 1;
    long long m = mp < 10 ? mp + 3 : mp - 9;
    long long y = yoe + era * 400 + (m <= 2);
    tm_buf->tm_year = (int)(y - 1900);
    tm_buf->tm_mon = (int)(m - 1);
    tm_buf->tm_mday = (int)d;
    static const int before[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    tm_buf->tm_yday = before[m - 1] + (int)d - 1 + (leap && m > 2);
    tm_buf->tm_isdst = 0;
    return tm_buf;
}

/* Maps the TLS memory and starts the library once per process (forked tasks inherit it). */
void tls_init(void)
{
    if (tls_ready) return;
    unsigned char *arena = (unsigned char *)vo_syscall6(SYS_mmap, 0, TLS_ARENA, 3 /* read, write */,
                                                       0x4022 /* private, anonymous, no reserve */, -1, 0);
    if ((long)arena < 0 && (long)arena > -4096) {
        out_cstr(2, "error[T0003]: cannot map memory for TLS\n");
        vo_exit(EXIT_INTERNAL);
    }
    mbedtls_memory_buffer_alloc_init(arena, TLS_ARENA);
    mbedtls_platform_set_time(tls_time);
    if (psa_crypto_init() != PSA_SUCCESS) {
        out_cstr(2, "error[T0003]: the TLS library could not start\n");
        vo_exit(EXIT_INTERNAL);
    }
    tls_ready = 1;
}

/* ---- trust anchors (research R5, R6) ---- */

#include "mbedtls/net_sockets.h" /* (its error codes only; vo has its own sockets) */
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

static mbedtls_x509_crt anchors; /* the machine's store (first use) plus every --ca-file (startup) */
static int anchors_init, store_tried;
static const char *const store_paths[] = {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
                                          "/etc/ssl/cert.pem", 0};

/* Parses PEM (or DER) certificates of the file at path into the anchors: the number of certificates
 * read, or -1 when the file cannot be read. Certificates that do not parse are skipped. */
static long anchors_add_file(char *path)
{
    tls_init();
    if (!anchors_init) {
        mbedtls_x509_crt_init(&anchors);
        anchors_init = 1;
    }
    char *text;
    long len = read_whole_file(path, &text);
    if (len < 0) return -1;
    unsigned char *buf = mbedtls_calloc(1, (size_t)len + 1); /* PEM parsing wants a final NUL */
    if (!buf) return 0;
    mem_copy((char *)buf, text, len);
    long before = 0;
    for (mbedtls_x509_crt *c = &anchors; c && c->raw.len; c = c->next) before++;
    mbedtls_x509_crt_parse(&anchors, buf, (size_t)len + 1);
    mbedtls_free(buf);
    long after = 0;
    for (mbedtls_x509_crt *c = &anchors; c && c->raw.len; c = c->next) after++;
    return after - before;
}

/* --ca-file FILE, at startup: T0002 when it cannot be read, T0013 when it holds no certificate. */
void tls_add_ca_file(char *path)
{
    long n = anchors_add_file(path);
    Str *p = str_from_cstr(path);
    if (n < 0) {
        toolchain_error((int)opt_json, (int)report_fd, "T0002", msg("Cannot read the certificate file `$1`.", p, 0, 0), p);
        vo_exit(EXIT_NOINPUT);
    }
    if (n == 0) {
        toolchain_error((int)opt_json, (int)report_fd, "T0013",
                        msg("`$1` holds no certificate vo can read: give a PEM file with one or more `BEGIN CERTIFICATE` blocks.",
                            p, 0, 0), p);
        vo_exit(EXIT_USAGE);
    }
}

/* The machine's trust store, once per process: 1 when there are anchors to verify against. */
static int anchors_ready(void)
{
    if (!store_tried) {
        store_tried = 1;
        for (int i = 0; store_paths[i]; i++)
            if (anchors_add_file((char *)store_paths[i]) > 0) break;
    }
    return anchors_init && anchors.raw.len;
}

/* ---- one HTTPS exchange (research R3, R4, R7) ---- */

typedef struct TlsIo {
    long fd;
    int timed_out;   /* the --timeout deadline passed inside the library: R0009 after cleanup */
    int self_signed; /* the server's certificate is its own issuer (research R7) */
    int depth;       /* the deepest certificate of the chain verification saw */
    int is_ip;       /* the host is an IPv4 address: checked here, not sent as SNI (RFC 6066) */
    uint8_t ip[4];
} TlsIo;

/* Called for each certificate of the chain being verified (the flags are left as they are): notes
 * whether the server sent a single certificate that issued itself. */
static int tls_verify_note(void *ctx, mbedtls_x509_crt *crt, int depth, uint32_t *flags)
{
    (void)flags;
    TlsIo *io = ctx;
    if (depth > io->depth) io->depth = depth;
    if (depth == 0)
        io->self_signed = crt->issuer_raw.len == crt->subject_raw.len &&
                          mem_eq((char *)crt->issuer_raw.p, (char *)crt->subject_raw.p, (long)crt->subject_raw.len);
    if (depth == 0 && io->is_ip) { /* an address must be one of the certificate's IP entries (RFC 9525) */
        int found = 0;
        for (const mbedtls_x509_sequence *s = &crt->subject_alt_names; s && !found; s = s->next)
            found = s->buf.tag == (MBEDTLS_ASN1_CONTEXT_SPECIFIC | 7) && s->buf.len == 4 &&
                    mem_eq((char *)s->buf.p, (char *)io->ip, 4);
        if (!found) *flags |= MBEDTLS_X509_BADCERT_CN_MISMATCH; /* (only ever adds a failure) */
    }
    return 0;
}

/* Waits for the socket under the run's deadline; 0 when the deadline passed. */
static int tls_wait(TlsIo *io, int events)
{
    long wait = -1;
    if (deadline_ms) {
        wait = deadline_ms - sys_clock_ms();
        if (wait <= 0) {
            io->timed_out = 1;
            return 0;
        }
    }
    if (sys_wait_fd(io->fd, events, wait)) return 1;
    io->timed_out = deadline_ms != 0;
    return !io->timed_out;
}

static int tls_send(void *ctx, const unsigned char *buf, size_t len)
{
    TlsIo *io = ctx;
    for (;;) {
        long w = vo_syscall6(SYS_sendto, io->fd, (long)buf, (long)len, 0x4000 /* MSG_NOSIGNAL */, 0, 0);
        if (w >= 0) return (int)w;
        if (w != -11) return MBEDTLS_ERR_NET_SEND_FAILED; /* not EAGAIN */
        if (!tls_wait(io, 4 /* POLLOUT */)) return MBEDTLS_ERR_SSL_TIMEOUT;
    }
}

static int tls_recv(void *ctx, unsigned char *buf, size_t len)
{
    TlsIo *io = ctx;
    for (;;) {
        long r = vo_syscall6(SYS_recvfrom, io->fd, (long)buf, (long)len, 0, 0, 0);
        if (r >= 0) return (int)r; /* 0: the peer closed */
        if (r != -11) return MBEDTLS_ERR_NET_RECV_FAILED;
        if (!tls_wait(io, 1 /* POLLIN */)) return MBEDTLS_ERR_SSL_TIMEOUT;
    }
}

/* The err for a failed handshake (research R7: the first row that applies wins). */
static Variant *tls_failure(mbedtls_ssl_context *ssl, TlsIo *io, int ret, Str *host, long port)
{
    uint32_t flags = mbedtls_ssl_get_verify_result(ssl);
    if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED && flags != (uint32_t)-1 && flags) {
        int self_signed = io->self_signed && io->depth == 0;
        if (flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED)
            return net_err(self_signed ? "the certificate of $1 is self-signed and not trusted"
                                       : "the certificate of $1 is not trusted", host, port);
        if (flags & MBEDTLS_X509_BADCERT_CN_MISMATCH) return net_err("the certificate of $1 is for another host", host, port);
        if (flags & MBEDTLS_X509_BADCERT_EXPIRED) return net_err("the certificate of $1 has expired", host, port);
        if (flags & MBEDTLS_X509_BADCERT_FUTURE) return net_err("the certificate of $1 is not valid yet", host, port);
        if (flags & (MBEDTLS_X509_BADCERT_BAD_MD | MBEDTLS_X509_BADCERT_BAD_PK | MBEDTLS_X509_BADCERT_BAD_KEY))
            return net_err("$1 offers no TLS version or algorithm that vo accepts", host, port);
        return net_err("the certificate of $1 is not trusted", host, port);
    }
    if (ret == MBEDTLS_ERR_SSL_BAD_PROTOCOL_VERSION || ret == MBEDTLS_ERR_SSL_HANDSHAKE_FAILURE ||
        ret == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)
        return net_err("$1 offers no TLS version or algorithm that vo accepts", host, port);
    if (ret == MBEDTLS_ERR_SSL_INVALID_MAC) return net_err("the connection to $1 was tampered with", host, port);
    return net_err("the TLS handshake with $1 failed", host, port);
}

/* result[str]: the raw response of request over a verified TLS connection to host:port, or err. The
 * host is checked against the grant by the caller (io_https_exchange); no request byte is sent before
 * the server's certificate is verified. */
Variant *tls_exchange(Str *host, long port, Str *request)
{
    GC_ROOT(&host);
    GC_ROOT(&request);
    Variant *result = 0;
    GC_ROOT(&result);
    tls_init();
    if (!anchors_ready()) {
        result = make_err("no trusted certificate authorities found (looked in /etc/ssl/certs/ca-certificates.crt, "
                          "/etc/pki/tls/certs/ca-bundle.crt, /etc/ssl/cert.pem)");
        GC_UNROOT(3);
        return result;
    }
    long fd = net_open(host, port, &result);
    if (fd < 0) {
        GC_UNROOT(3);
        return result;
    }
    int one = 1; /* TCP_NODELAY: the handshake's last message and the request go out at once, not 40 ms
                    apart (Nagle's algorithm waiting for the server's delayed acknowledgement) */
    vo_syscall6(SYS_setsockopt, fd, 6 /* IPPROTO_TCP */, 1 /* TCP_NODELAY */, (long)&one, sizeof one, 0);
    char name[256]; /* the host as a C string, for SNI and name verification */
    long n = host->len < 255 ? host->len : 255;
    mem_copy(name, host->data, n);
    name[n] = 0;
    TlsIo io = {fd, 0, 0, 0, 0, {0}};
    io.is_ip = parse_ipv4(host->data, host->len, io.ip);
    mbedtls_ssl_config conf;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config_init(&conf);
    mbedtls_ssl_init(&ssl);
    static const uint16_t groups[] = {MBEDTLS_SSL_IANA_TLS_GROUP_X25519, MBEDTLS_SSL_IANA_TLS_GROUP_SECP256R1,
                                      MBEDTLS_SSL_IANA_TLS_GROUP_SECP384R1, MBEDTLS_SSL_IANA_TLS_GROUP_NONE};
    static const char *alpn[] = {"http/1.1", 0};
    /* research R4, in order of preference: ChaCha20-Poly1305 first, which is fast without AES
       instructions (vo uses none), then AES-GCM; forward-secret and authenticated only */
    static const int suites[] = {
        MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256, MBEDTLS_TLS1_3_AES_128_GCM_SHA256, MBEDTLS_TLS1_3_AES_256_GCM_SHA384,
        MBEDTLS_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, MBEDTLS_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
        MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384, 0};
    int ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT);
    if (!ret) {
        mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED); /* always (FR-006) */
        mbedtls_ssl_conf_ca_chain(&conf, &anchors, 0);
        mbedtls_ssl_conf_verify(&conf, tls_verify_note, &io);
        mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_2);
        mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
        mbedtls_ssl_conf_groups(&conf, groups);
        mbedtls_ssl_conf_ciphersuites(&conf, suites);
        ret = mbedtls_ssl_conf_alpn_protocols(&conf, alpn);
    }
    if (!ret) ret = mbedtls_ssl_setup(&ssl, &conf);
    if (!ret) ret = mbedtls_ssl_set_hostname(&ssl, io.is_ip ? 0 : name); /* SNI and name check, or the IP check above */
    if (!ret) {
        mbedtls_ssl_set_bio(&ssl, &io, tls_send, tls_recv, 0);
        do ret = mbedtls_ssl_handshake(&ssl);
        while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
    }
    if (!ret && mbedtls_ssl_get_verify_result(&ssl) != 0) ret = MBEDTLS_ERR_X509_CERT_VERIFY_FAILED; /* (never: required) */
    if (ret && !io.timed_out) result = tls_failure(&ssl, &io, ret, host, port);
    for (long sent = 0; !ret && sent < request->len;) { /* verified: the request */
        int w = mbedtls_ssl_write(&ssl, (const unsigned char *)request->data + sent, (size_t)(request->len - sent));
        if (w == MBEDTLS_ERR_SSL_WANT_READ || w == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (w < 0) {
            ret = w;
            if (!io.timed_out)
                result = w == MBEDTLS_ERR_SSL_INVALID_MAC ? net_err("the connection to $1 was tampered with", host, port)
                                                          : net_err("the connection to $1 failed while sending", host, port);
        } else {
            sent += w;
        }
    }
    SB *sb = sb_new();
    GC_ROOT(&sb);
    unsigned char buf[16384];
    while (!ret) { /* the response, until the server closes */
        int r = mbedtls_ssl_read(&ssl, buf, sizeof buf);
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (r == 0 || r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || r == MBEDTLS_ERR_SSL_CONN_EOF) break;
        if (r < 0) {
            ret = r;
            if (!io.timed_out)
                result = r == MBEDTLS_ERR_SSL_INVALID_MAC ? net_err("the connection to $1 was tampered with", host, port)
                                                          : net_err("the connection to $1 failed while reading", host, port);
            break;
        }
        if (sb->len + r > (16L << 20)) {
            ret = -1;
            result = net_err("the response from $1 is larger than 16 MiB", host, port);
            break;
        }
        sb_bytes(sb, (char *)buf, r);
    }
    if (!ret) mbedtls_ssl_close_notify(&ssl);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    sys_close((int)fd);
    if (io.timed_out) rt_timeout((Node *)cur_node); /* R0009, after everything was freed */
    if (!result) result = make_result(0, (long)sb_finish(sb));
    GC_UNROOT(4);
    return result;
}
