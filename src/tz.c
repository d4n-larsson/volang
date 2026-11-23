/* Spec 048: the embedded tz database and the machine's zone, for std/time.vo.
 *
 * tz_blob (third_party/tzdata/zones.bin, contracts/timezones.md) is "VOTZ1 RELEASE COUNT\n", COUNT
 * lines "NAME OFFSET LENGTH\n" sorted by name, then the TZif files end to end. vo reads the files
 * (zone_from_bytes); C only hands the bytes out. */
#include "vo.h"

#define LOCAL_MAX (64L << 10) /* the largest /etc/localtime read */

static long tz_dec(const unsigned char **p)
{
    long v = 0;
    while (**p >= '0' && **p <= '9') v = v * 10 + (*(*p)++ - '0');
    return v;
}

/* The first index line; *count the number of lines. */
static const unsigned char *tz_index(long *count)
{
    const unsigned char *p = tz_blob + 6; /* after "VOTZ1 " */
    while (*p != ' ') p++;
    p++;
    *count = tz_dec(&p);
    return p + 1;
}

static const unsigned char *tz_data(void)
{
    static const unsigned char *data;
    if (!data) {
        long count;
        const unsigned char *p = tz_index(&count);
        while (count--) {
            while (*p != '\n') p++;
            p++;
        }
        data = p;
    }
    return data;
}

/* The release name, such as "2026f", as a C string in buf (for `vo version`: nothing is allocated). */
void tz_version(char buf[32])
{
    const unsigned char *p = tz_blob + 6;
    long n = 0;
    while (p[n] != ' ' && n < 31) {
        buf[n] = (char)p[n];
        n++;
    }
    buf[n] = 0;
}

/* The TZif bytes of a zone by its exact name, or 0. A name with a space or a newline finds nothing.
 * ponytail: linear over the index (597 names); binary search if it grows past a few thousand. */
static const unsigned char *tz_find(char *name, long n, long *len)
{
    long count;
    const unsigned char *p = tz_index(&count), *data = tz_data();
    if (n <= 0) return 0;
    for (long i = 0; i < n; i++)
        if (name[i] == ' ' || name[i] == '\n' || !name[i]) return 0;
    while (count--) {
        if (data - p > n && p[n] == ' ' && mem_eq((char *)p, name, n)) {
            p += n + 1;
            long off = tz_dec(&p);
            p++;
            *len = tz_dec(&p);
            return data + off;
        }
        while (*p != '\n') p++;
        p++;
    }
    return 0;
}

/* __tz(op, name) -> result[bytes]: 1 a zone's bytes (err("unknown")), 2 the header and index text. */
Variant *rt_tz(long op, Str *name)
{
    long len;
    const unsigned char *z;
    if (op == 2) return make_result(0, (long)str_new((char *)tz_blob, tz_data() - tz_blob));
    if (op == 1 && (z = tz_find(name->data, name->len, &len))) return make_result(0, (long)str_new((char *)z, len));
    return make_result(1, (long)str_from_cstr("unknown"));
}

/* __local_tz() -> bytes: "NAME\n" and the TZif bytes of the machine's zone, or no bytes (research R3):
 * nothing with a fixed clock; TZ (after an optional ':') when it names an embedded zone; else
 * /etc/localtime, named by its link target after "zoneinfo/" or "local". Read directly: the grant is
 * clock, and nothing else of the environment or the file system is reachable through it. */
Str *rt_local_tz(void)
{
    static char buf[256 + LOCAL_MAX + 1];
    long n = 0, len;
    if (clock_fixed) return str_new("", 0);
    for (char **env = (char **)envp; env && *env; env++) {
        char *e = *env;
        if (e[0] != 'T' || e[1] != 'Z' || e[2] != '=') continue;
        e += e[3] == ':' ? 4 : 3;
        long name_len = cstr_len(e);
        const unsigned char *z = name_len < 256 ? tz_find(e, name_len, &len) : 0;
        if (!z || len > LOCAL_MAX) break;
        for (long i = 0; i < name_len; i++) buf[n++] = e[i];
        buf[n++] = '\n';
        for (long i = 0; i < len; i++) buf[n++] = (char)z[i];
        return str_new(buf, n);
    }
    char link[1024];
    long l = sys_readlink("/etc/localtime", link, sizeof link);
    long at = -1;
    for (long i = 0; i + 9 <= l; i++)
        if (mem_eq(link + i, "zoneinfo/", 9)) at = i + 9;
    long ok = at >= 0 && l - at > 0 && l - at < 256 && rw_valid_utf8(link + at, l - at);
    for (long i = at; ok && i < l; i++)
        if (link[i] == '\n' || !link[i]) ok = 0;
    if (ok) {
        for (long i = at; i < l; i++) buf[n++] = link[i];
    } else {
        for (char *s = "local"; *s; s++) buf[n++] = *s;
    }
    buf[n++] = '\n';
    long fd = sys_open("/etc/localtime", O_RDONLY);
    if (fd < 0) return str_new("", 0);
    long got = 0, r;
    while (got <= LOCAL_MAX && (r = sys_read((int)fd, buf + n + got, LOCAL_MAX + 1 - got)) > 0) got += r;
    sys_close((int)fd);
    if (got == 0 || got > LOCAL_MAX) return str_new("", 0);
    return str_new(buf, n + got);
}
