/* Fetching an archive for `vo package ENTRY NAME URL` (spec 041, research R1): one GET over the HTTPS
 * client std.http uses (tls_exchange: certificates verified, 16 MiB at most), with up to five
 * redirects. Only https:// URLs are fetched, and none with credentials. The `#PATH` of a package URL
 * is the caller's: it is never part of what is asked here. */
#include "vo.h"

#define REDIRECTS 5
#define FETCH_MS 60000

static Str *text3(char *a, Str *b, char *c)
{
    SB *sb = sb_new();
    sb_cstr(sb, a);
    if (b) sb_str(sb, b);
    sb_cstr(sb, c);
    return sb_finish(sb);
}

static int lower_starts(char *p, long n, char *lit)
{
    long k = cstr_len(lit);
    if (n < k) return 0;
    for (long i = 0; i < k; i++) {
        int c = (unsigned char)p[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        if (c != lit[i]) return 0;
    }
    return 1;
}

/* Splits https://HOST[:PORT]/TARGET: 0 with *why when the URL cannot be fetched. */
static int url_parts(Str *url, Str **host, long *port, Str **target, Str **why)
{
    if (!lower_starts(url->data, url->len, "https://")) {
        *why = text3("`", url, "` is not an https:// URL");
        return 0;
    }
    char *p = url->data + 8;
    long n = url->len - 8, cut = 0;
    for (long i = 0; i < n; i++)
        if ((unsigned char)p[i] <= ' ' || p[i] == 0x7f) {
            *why = text3("`", url, "` has a space or control character");
            return 0;
        }
    while (cut < n && p[cut] != '/' && p[cut] != '?') cut++;
    long hn = cut;
    *port = 443;
    for (long i = 0; i < cut; i++) {
        if (p[i] == '@') {
            *why = str_from_cstr("a URL with a user name or password is not fetched");
            return 0;
        }
        if (p[i] == ':') {
            hn = i;
            long v = 0;
            for (long k = i + 1; k < cut; k++) {
                if (p[k] < '0' || p[k] > '9' || v > 65535) v = 1L << 20;
                else v = v * 10 + (p[k] - '0');
            }
            if (i + 1 == cut || v < 1 || v > 65535) {
                *why = text3("`", url, "` has a bad port");
                return 0;
            }
            *port = v;
            break;
        }
    }
    if (!hn) {
        *why = text3("`", url, "` has no host");
        return 0;
    }
    SB *h = sb_new();
    for (long i = 0; i < hn; i++) sb_byte(h, p[i] >= 'A' && p[i] <= 'Z' ? p[i] + 32 : p[i]);
    *host = sb_finish(h);
    SB *t = sb_new();
    if (cut == n || p[cut] != '/') sb_byte(t, '/');
    sb_bytes(t, p + cut, n - cut);
    *target = sb_finish(t);
    return 1;
}

/* The value of header `name` (lower case, with its colon) in a response head, or 0. */
static Str *head_value(Str *head, char *name)
{
    long i = 0;
    while (i < head->len) {
        long e = i;
        while (e < head->len && head->data[e] != '\n') e++;
        if (lower_starts(head->data + i, e - i, name)) {
            long a = i + cstr_len(name), b = e;
            while (a < b && (head->data[a] == ' ' || head->data[a] == '\t')) a++;
            while (b > a && (head->data[b - 1] == '\r' || head->data[b - 1] == ' ')) b--;
            return str_new(head->data + a, b - a);
        }
        i = e + 1;
    }
    return 0;
}

/* The body at url (without a fragment), or 0 with *why saying what failed. */
Str *fetch_url(Str *url, Str **why)
{
    *why = 0;
    for (int hop = 0; hop <= REDIRECTS; hop++) {
        Str *host, *target;
        long port;
        if (!url_parts(url, &host, &port, &target, why)) return 0;
        SB *rq = sb_new();
        sb_cstr(rq, "GET ");
        sb_str(rq, target);
        sb_cstr(rq, " HTTP/1.1\r\nHost: ");
        sb_str(rq, host);
        if (port != 443) {
            sb_byte(rq, ':');
            int_to_dec(rq, port);
        }
        sb_cstr(rq, "\r\nConnection: close\r\nUser-Agent: vo\r\nAccept-Encoding: identity\r\n\r\n");
        net_capped = 0;
        net_cap_ms = sys_clock_ms() + FETCH_MS;
        Variant *r = tls_exchange(host, port, sb_finish(rq));
        net_cap_ms = 0;
        if (r->tag) {
            *why = r->pay.s;
            return 0;
        }
        HttpFrame f = {0};
        Str *raw = r->pay.s;
        long st = http_frame(&f, raw->data, raw->len, 1, 1);
        if (st <= 0) {
            *why = str_from_cstr(st == -413 ? "the download is larger than 16 MiB" : "the server's answer is not a valid HTTP response");
            return 0;
        }
        Str *head = rt_http_part(f.out, 0);
        long status = 0, i = 0;
        while (i < head->len && head->data[i] != ' ') i++;
        for (i++; i < head->len && head->data[i] >= '0' && head->data[i] <= '9'; i++) status = status * 10 + (head->data[i] - '0');
        if (status == 200) return rt_http_part(f.out, 1);
        Str *to = head_value(head, "location:");
        if (!to || (status != 301 && status != 302 && status != 303 && status != 307 && status != 308)) {
            *why = text3("the server answered ", int_to_str(status), "");
            return 0;
        }
        if (to->len && to->data[0] == '/') { /* a path on the same host */
            SB *sb = sb_new();
            sb_cstr(sb, "https://");
            sb_str(sb, host);
            if (port != 443) {
                sb_byte(sb, ':');
                int_to_dec(sb, port);
            }
            sb_str(sb, to);
            to = sb_finish(sb);
        } else if (!lower_starts(to->data, to->len, "https://")) {
            *why = text3("it redirects to `", to, "`, which is not an https:// URL");
            return 0;
        }
        long hash = 0; /* a fragment in a redirect is not sent on */
        while (hash < to->len && to->data[hash] != '#') hash++;
        url = str_new(to->data, hash);
    }
    *why = str_from_cstr("more than 5 redirects");
    return 0;
}
