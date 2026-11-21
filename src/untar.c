/* tar for `vo package ENTRY NAME URL` (spec 041, research R3): walks the 512-byte headers of an
 * archive held in memory and returns its regular files. The archive is hostile, so it is refused as a
 * whole for anything that is not a plain file or a directory with a safe relative path: links,
 * devices and FIFOs, absolute paths, `..`, control bytes. Nothing is written here.
 * Read: ustar (name and prefix), GNU long names (`L`), pax paths (`x` with `path=`), pax global
 * headers (`g`, skipped). ponytail: pax `size=` is not read (files over 8 GiB are far past the limit). */
#include "vo.h"

#define BLOCK 512
#define MAX_ENTRIES 10000
#define MAX_PATH 4096
#define MAX_PART 255
#define MAX_FILE (16L << 20)

/* What is wrong with a path inside an archive, or 0: it must be relative, without empty, `.`-only
 * trailing or `..` components, and without control bytes. */
char *path_unsafe(char *p, long n)
{
    if (n == 0) return "an entry has no name";
    if (n > MAX_PATH) return "an entry's path is longer than 4096 bytes";
    if (p[0] == '/') return "an entry has an absolute path";
    long start = 0;
    for (long i = 0; i <= n; i++) {
        if (i < n && ((unsigned char)p[i] < 0x20 || p[i] == 0x7f)) return "an entry's path has a control character";
        if (i < n && p[i] != '/') continue;
        long len = i - start;
        if (len == 2 && p[start] == '.' && p[start + 1] == '.') return "an entry's path has a `..` component";
        if (len > MAX_PART) return "an entry's name is longer than 255 bytes";
        if (len == 0 && i < n) return "an entry's path has an empty component";
        start = i + 1;
    }
    return 0;
}

/* An octal field of n bytes (spaces and NULs end it): its value, or -1. */
static long octal(char *p, long n)
{
    long v = 0, i = 0;
    while (i < n && p[i] == ' ') i++;
    if (i < n && ((unsigned char)p[i] & 0x80)) return -1; /* the base-256 form: nothing that large is allowed */
    long digits = 0;
    for (; i < n && p[i] >= '0' && p[i] <= '7'; i++, digits++) {
        if (v > (1L << 50)) return -1;
        v = v * 8 + (p[i] - '0');
    }
    for (; i < n; i++)
        if (p[i] != ' ' && p[i] != 0) return -1;
    return digits ? v : 0;
}

static long field_len(char *p, long max)
{
    long n = 0;
    while (n < max && p[n]) n++;
    return n;
}

/* The `path=` value of a pax header's records ("LEN key=value\n" each), or 0. */
static Str *pax_path(char *p, long n)
{
    Str *found = 0;
    long at = 0;
    while (at < n) {
        long len = 0, i = at;
        while (i < n && p[i] >= '0' && p[i] <= '9' && len < (1L << 30)) len = len * 10 + (p[i++] - '0');
        if (i >= n || p[i] != ' ' || len < i - at + 2 || at + len > n) return 0; /* damaged: no path from it */
        char *rec = p + i + 1;
        long rn = at + len - (i + 1) - 1; /* without the newline */
        if (rn > 5 && mem_eq(rec, "path=", 5)) found = str_new(rec + 5, rn - 5);
        at += len;
    }
    return found;
}

/* The regular files of the tar archive buf[0..n), in order (TarFile each), or 0 with *why. */
Vec *untar(char *buf, long n, char **why)
{
    Vec *files = vec_new();
    Str *pending = 0; /* a long name or pax path for the next entry */
    long at = 0, entries = 0;
    *why = 0;
    while (at + BLOCK <= n) {
        char *h = buf + at;
        int zero = 1;
        for (int i = 0; i < BLOCK && zero; i++) zero = h[i] == 0;
        if (zero) break; /* the end of the archive */
        long sum = 0;
        for (int i = 0; i < BLOCK; i++) sum += i >= 148 && i < 156 ? ' ' : (unsigned char)h[i];
        if (octal(h + 148, 8) != sum) {
            *why = at ? "a header's checksum does not match: the archive is damaged" : "it is not a tar archive";
            return 0;
        }
        long size = octal(h + 124, 12);
        if (size < 0) {
            *why = "an entry's size cannot be read";
            return 0;
        }
        long blocks = (size + BLOCK - 1) / BLOCK * BLOCK;
        char *data = h + BLOCK;
        if (at + BLOCK + blocks > n || size > n) {
            *why = "the archive ends in the middle of an entry";
            return 0;
        }
        at += BLOCK + blocks;
        if (++entries > MAX_ENTRIES) {
            *why = "the archive has more than 10000 entries";
            return 0;
        }
        int type = h[156];
        if (type == 'g') continue; /* a global pax header: comments */
        if (type == 'x') {
            Str *p = pax_path(data, size);
            if (p) pending = p;
            continue;
        }
        if (type == 'L') {
            pending = str_new(data, field_len(data, size));
            continue;
        }
        Str *path = pending;
        pending = 0;
        if (!path) {
            SB *sb = sb_new();
            long pre = mem_eq(h + 257, "ustar", 5) ? field_len(h + 345, 155) : 0;
            if (pre) {
                sb_bytes(sb, h + 345, pre);
                sb_byte(sb, '/');
            }
            sb_bytes(sb, h, field_len(h, 100));
            path = sb_finish(sb);
        }
        long len = path->len;
        char *p = path->data;
        while (len >= 2 && p[0] == '.' && p[1] == '/') { /* a leading ./ */
            p += 2;
            len -= 2;
        }
        int dir = type == '5' || (len && p[len - 1] == '/');
        while (len && p[len - 1] == '/') len--;
        if (dir && (len == 0 || (len == 1 && p[0] == '.'))) continue; /* the archive's root */
        if (type != '0' && type != 0 && type != '5') {
            *why = type == '1' || type == '2' ? "it has a link (only files and directories are accepted)"
                                               : "it has a device, a FIFO or another special entry";
            return 0;
        }
        char *bad = path_unsafe(p, len);
        if (bad) {
            *why = bad;
            return 0;
        }
        if (dir) continue;
        if (size > MAX_FILE) {
            *why = "a file in it is larger than 16 MiB";
            return 0;
        }
        TarFile *f = alloc(sizeof(TarFile));
        f->path = str_new(p, len);
        f->data = data;
        f->n = size;
        vec_push(files, f);
    }
    if (!entries) {
        *why = n ? "it is not a tar archive" : "the archive is empty";
        return 0;
    }
    return files;
}

/* The package's files among an archive's: without a folder that holds everything (as forges make),
 * under `inner` when given (its prefix dropped), and only `.vo` files. */
Vec *untar_select(Vec *files, Str *inner)
{
    long top = -1; /* the length of the common first component, 0 when there is none */
    for (long i = 0; i < files->n && top != 0; i++) {
        TarFile *f = vec_get(files, i);
        long k = 0;
        while (k < f->path->len && f->path->data[k] != '/') k++;
        if (k == f->path->len) top = 0; /* a file at the root */
        else if (top < 0) top = k;
        else if (top != k || !mem_eq(f->path->data, ((TarFile *)vec_get(files, 0))->path->data, k)) top = 0;
    }
    Vec *out = vec_new();
    for (long i = 0; i < files->n; i++) {
        TarFile *f = vec_get(files, i);
        char *p = f->path->data;
        long n = f->path->len;
        if (top > 0) {
            p += top + 1;
            n -= top + 1;
        }
        if (inner) {
            if (n <= inner->len || p[inner->len] != '/' || !mem_eq(p, inner->data, inner->len)) continue;
            p += inner->len + 1;
            n -= inner->len + 1;
        }
        if (n < 4 || !mem_eq(p + n - 3, ".vo", 3)) continue;
        TarFile *g = alloc(sizeof(TarFile));
        g->path = str_new(p, n);
        g->data = f->data;
        g->n = f->n;
        vec_push(out, g);
    }
    return out;
}
