/* Versioned packages from git hosts (spec 061, research R1): the versions of a repository are its
 * tags `[v]MAJOR.MINOR.PATCH`, read from the ref list of git's smart HTTP protocol, and one version is
 * fetched as a pack holding one commit (`deepen 1`) with its trees and blobs. No external program.
 * Everything read here is hostile: every offset is checked against the pack's end, every size against
 * what was inflated, the total against 64 MiB, and names against path_unsafe. Only the package's `.vo`
 * files and its root `vo.packages` are handed on; links and submodules in the package refuse it.
 * SHA-1 names objects inside one pack; what is trusted afterwards is the package's content hash.
 * ponytail: the whole one-commit pack is downloaded (16 MiB at most); `filter blob:none` and a second
 * request for the .vo blobs would lift that for large repositories. */
#include "vo.h"

#define ARENA (64L << 20)
#define MAX_OBJECTS 200000
#define GIT_MAX_FILES 10000
#define MAX_FILE (16L << 20)
#define MAX_CHAIN 50
#define MAX_DEPTH 64
enum { G_COMMIT = 1, G_TREE = 2, G_BLOB = 3, G_TAG = 4, G_OFS = 6, G_REF = 7 };

typedef struct Obj {
    long off;      /* where its header starts in the pack */
    long base_off; /* an OFS delta's base */
    char *data;    /* inflated: the object, or the delta until it is applied */
    long n;
    int type;  /* commit, tree, blob or tag (a delta: its base's, once applied) */
    int delta; /* G_OFS or G_REF until applied, then 0 */
    char base_id[20];
    char id[20];
} Obj;

static char *g_arena; /* every object's bytes */
static long g_at;
static Obj *g_objs;
static long g_n, g_objs_cap;
static int *g_table; /* id -> index + 1, open addressing */
static long g_mask, g_table_cap;
static char *g_why;

void git_release(void)
{
    pk_unmap(g_arena, ARENA);
    pk_unmap((char *)g_objs, g_objs_cap);
    pk_unmap((char *)g_table, g_table_cap);
    g_arena = 0, g_objs = 0, g_table = 0;
}

static void *bad(char *why)
{
    if (!g_why) g_why = why;
    return 0;
}

static Obj *by_id(char *id)
{
    for (long h = (uint32_t)((uint8_t)id[0] | (uint8_t)id[1] << 8 | (uint8_t)id[2] << 16 | (uint32_t)(uint8_t)id[3] << 24);; h++) {
        int k = g_table[h & g_mask];
        if (!k) return 0;
        if (mem_eq(g_objs[k - 1].id, id, 20)) return &g_objs[k - 1];
    }
}

/* Names o (the SHA-1 of `TYPE SIZE\0` and its bytes) and enters it in the table. */
static void name_object(Obj *o)
{
    static const char *const names[] = {0, "commit", "tree", "blob", "tag"};
    char head[32], digits[20];
    long k = 0, d = 0;
    for (const char *t = names[o->type]; *t; t++) head[k++] = *t;
    head[k++] = ' ';
    for (long v = o->n; v || !d; v /= 10) digits[d++] = (char)('0' + v % 10);
    while (d) head[k++] = digits[--d];
    head[k++] = 0;
    Sha1 c;
    sha1_start(&c);
    sha1_add(&c, head, k);
    sha1_add(&c, o->data, o->n);
    sha1_end(&c, o->id);
    if (by_id(o->id)) return; /* the same object twice: the first stays */
    long h = (uint32_t)((uint8_t)o->id[0] | (uint8_t)o->id[1] << 8 | (uint8_t)o->id[2] << 16 | (uint32_t)(uint8_t)o->id[3] << 24);
    while (g_table[h & g_mask]) h++;
    g_table[h & g_mask] = (int)(o - g_objs) + 1;
}

static Obj *by_offset(long off)
{
    long lo = 0, hi = g_n - 1;
    while (lo <= hi) {
        long mid = (lo + hi) / 2;
        if (g_objs[mid].off == off) return &g_objs[mid];
        if (g_objs[mid].off < off) lo = mid + 1;
        else hi = mid - 1;
    }
    return 0;
}

/* A size of a delta's head: 7 bits a byte, least significant first. -1 when it runs past the end. */
static long delta_size(const uint8_t *d, long n, long *at)
{
    long v = 0;
    for (int shift = 0; shift < 63; shift += 7) {
        if (*at >= n) return -1;
        int c = d[(*at)++];
        v |= (long)(c & 0x7f) << shift;
        if (!(c & 0x80)) return v;
    }
    return -1;
}

/* Applies delta o to base: the result takes o's place (in the arena). 0 when the delta is damaged. */
static int apply_delta(Obj *o, Obj *base)
{
    const uint8_t *d = (const uint8_t *)o->data;
    long n = o->n, at = 0;
    long from = delta_size(d, n, &at), to = delta_size(d, n, &at);
    if (from != base->n || to < 0) return bad("a delta does not fit its base: the pack is damaged") != 0;
    if (to > ARENA - g_at) return bad("it unpacks to more than 64 MiB") != 0;
    char *out = g_arena + g_at;
    long len = 0;
    while (at < n) {
        int c = d[at++];
        if (c & 0x80) { /* copy from the base: offset and size bytes as the bits say */
            long off = 0, size = 0;
            for (int i = 0; i < 4; i++)
                if (c & (1 << i)) {
                    if (at >= n) return bad("a delta ends early: the pack is damaged") != 0;
                    off |= (long)d[at++] << (8 * i);
                }
            for (int i = 0; i < 3; i++)
                if (c & (0x10 << i)) {
                    if (at >= n) return bad("a delta ends early: the pack is damaged") != 0;
                    size |= (long)d[at++] << (8 * i);
                }
            if (!size) size = 0x10000;
            if (off + size > base->n || size > to - len) return bad("a delta copies past its base: the pack is damaged") != 0;
            mem_copy(out + len, base->data + off, size);
            len += size;
        } else if (c) { /* insert the next c bytes */
            if (at + c > n || c > to - len) return bad("a delta ends early: the pack is damaged") != 0;
            mem_copy(out + len, (char *)d + at, c);
            at += c;
            len += c;
        } else {
            return bad("a delta has an unknown instruction: the pack is damaged") != 0;
        }
    }
    if (len != to) return bad("a delta gives another size than it says: the pack is damaged") != 0;
    o->data = out;
    o->n = to;
    o->type = base->type;
    o->delta = 0;
    g_at += to;
    return 1;
}

/* Reads every object of the pack into g_objs, inflated, and applies the deltas. */
static int read_pack(char *pack, long n)
{
    const uint8_t *p = (const uint8_t *)pack;
    if (n < 32 || !mem_eq(pack, "PACK", 4)) return bad("the answer is not a git pack") != 0;
    char sum[20];
    Sha1 c;
    sha1_start(&c);
    sha1_add(&c, pack, n - 20);
    sha1_end(&c, sum);
    if (!mem_eq(sum, pack + n - 20, 20)) return bad("the pack's checksum does not match: it is damaged or cut short") != 0;
    long version = (long)p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7];
    long count = (long)p[8] << 24 | p[9] << 16 | p[10] << 8 | p[11];
    if (version != 2) return bad("the pack has a version vo does not read") != 0;
    if (count > MAX_OBJECTS) return bad("the pack has more than 200,000 objects") != 0;
    long end = n - 20, at = 12;
    g_objs_cap = (count + 1) * (long)sizeof(Obj);
    for (g_mask = 1023; g_mask < 2 * count; g_mask = g_mask * 2 + 1) {}
    g_table_cap = (g_mask + 1) * (long)sizeof(int);
    g_arena = pk_map(ARENA);
    g_objs = (Obj *)pk_map(g_objs_cap);
    g_table = (int *)pk_map(g_table_cap);
    g_at = 0;
    g_n = 0;
    if (!g_arena || !g_objs || !g_table) return bad("there is no memory to unpack it") != 0;
    for (long i = 0; i < count; i++) {
        Obj *o = &g_objs[i];
        o->off = at;
        if (at >= end) return bad("the pack ends before its last object") != 0;
        int b = p[at++];
        int type = (b >> 4) & 7;
        long size = b & 15;
        for (int shift = 4; b & 0x80; shift += 7) {
            if (at >= end || shift > 56) return bad("an object's header is damaged") != 0;
            b = p[at++];
            size |= (long)(b & 0x7f) << shift;
        }
        if (type == G_OFS) { /* the distance back to its base: 7 bits a byte, most significant first, each +1 */
            if (at >= end) return bad("an object's header is damaged") != 0;
            b = p[at++];
            long back = b & 0x7f;
            while (b & 0x80) {
                if (at >= end || back > (1L << 40)) return bad("an object's header is damaged") != 0;
                b = p[at++];
                back = ((back + 1) << 7) | (b & 0x7f);
            }
            o->base_off = o->off - back;
            if (back <= 0 || o->base_off < 12) return bad("a delta's base is outside the pack") != 0;
        } else if (type == G_REF) {
            if (at + 20 > end) return bad("an object's header is damaged") != 0;
            mem_copy(o->base_id, pack + at, 20);
            at += 20;
        } else if (type < G_COMMIT || type > G_TAG) {
            return bad("the pack has an object of an unknown type") != 0;
        }
        long out_n = 0, used = 0;
        char *why = 0;
        if (zlib_unpack(pack + at, end - at, g_arena + g_at, ARENA - g_at, &out_n, &used, &why)) return bad(why) != 0;
        if (out_n != size) return bad("an object has another size than its header says: the pack is damaged") != 0;
        o->data = g_arena + g_at;
        o->n = out_n;
        o->type = type;
        o->delta = type >= G_OFS ? type : 0;
        g_at += out_n;
        at += used;
        g_n++;
    }
    if (at != end) return bad("the pack has data after its last object") != 0;
    for (long i = 0; i < g_n; i++)
        if (!g_objs[i].delta) name_object(&g_objs[i]);
    for (int pass = 0;; pass++) { /* a delta is applied once its base is whole: a pass per level of a chain */
        long left = 0, done = 0;
        for (long i = 0; i < g_n; i++) {
            Obj *o = &g_objs[i];
            if (!o->delta) continue;
            Obj *base = o->delta == G_OFS ? by_offset(o->base_off) : by_id(o->base_id);
            if (!base || base->delta || base == o) {
                left++;
                continue;
            }
            if (!apply_delta(o, base)) return 0;
            name_object(o);
            done++;
        }
        if (!left) return 1;
        if (!done) return bad("a delta's base is not in the pack") != 0;
        if (pass >= MAX_CHAIN) return bad("a chain of deltas is longer than 50") != 0;
    }
}

static int hex_id(char *p, long n, char *id)
{
    if (n < 40) return 0;
    for (int i = 0; i < 40; i++) {
        int c = p[i], v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (v < 0) return 0;
        id[i / 2] = (char)(i & 1 ? id[i / 2] | v : v << 4);
    }
    return 1;
}

/* The next entry of a tree at *at: 1 with its mode, name and id; 0 at the end; -1 when damaged. */
static int tree_entry(Obj *t, long *at, long *mode, char **name, long *name_n, char **id)
{
    if (*at >= t->n) return 0;
    long m = 0, i = *at;
    for (; i < t->n && t->data[i] != ' '; i++) {
        if (t->data[i] < '0' || t->data[i] > '7' || i - *at > 7) return -1;
        m = m * 8 + (t->data[i] - '0');
    }
    long start = ++i;
    while (i < t->n && t->data[i]) i++;
    if (i + 21 > t->n) return -1;
    *mode = m, *name = t->data + start, *name_n = i - start, *id = t->data + i + 1;
    *at = i + 21;
    return 1;
}

/* The package's files under tree t into out: `.vo` files anywhere, `vo.packages` at the root. */
static int collect(Obj *t, Str *prefix, int depth, Vec *out)
{
    long at = 0, mode, name_n;
    char *name, *id;
    if (depth > MAX_DEPTH) return bad("its folders are nested deeper than 64") != 0;
    for (;;) {
        int r = tree_entry(t, &at, &mode, &name, &name_n, &id);
        if (r < 0) return bad("a folder of the repository is damaged") != 0;
        if (!r) return 1;
        for (long i = 0; i < name_n; i++)
            if (name[i] == '/') return bad("an entry's name has a `/`") != 0;
        SB *sb = sb_new();
        sb_str(sb, prefix);
        sb_bytes(sb, name, name_n);
        Str *path = sb_finish(sb);
        char *why = path_unsafe(path->data, path->len);
        if (why) return bad(why) != 0;
        if (mode == 0120000) return bad("it has a link, which a package cannot hold") != 0;
        if (mode == 0160000) return bad("it has a submodule, which is not fetched") != 0;
        Obj *o = by_id(id);
        if (mode == 040000) {
            if (!o || o->type != G_TREE) return bad("a folder of the repository is missing from the pack") != 0;
            sb = sb_new();
            sb_str(sb, path);
            sb_byte(sb, '/');
            if (!collect(o, sb_finish(sb), depth + 1, out)) return 0;
            continue;
        }
        int is_vo = name_n >= 4 && mem_eq(name + name_n - 3, ".vo", 3);
        if (!is_vo && !(depth == 0 && name_n == 11 && mem_eq(name, "vo.packages", 11))) continue;
        if (!o || o->type != G_BLOB) return bad("a file of the package is missing from the pack") != 0;
        if (o->n > MAX_FILE) return bad("a file is larger than 16 MiB") != 0;
        if (out->n >= GIT_MAX_FILES) return bad("it has more than 10,000 files") != 0;
        TarFile *f = alloc(sizeof(TarFile));
        f->path = path;
        f->data = o->data;
        f->n = o->n;
        vec_push(out, f);
    }
}

/* The files of the package in a pack: the commit (or annotated tag) `want`, its tree, the folder
 * `folder` in it (0: the root). The files' bytes stay mapped until git_release. An empty list when the
 * folder does not exist or holds no file of a package; 0 with *why when the pack is refused. */
Vec *git_unpack(char *pack, long n, char *want, Str *folder, char **why)
{
    g_why = 0;
    Vec *out = vec_new();
    if (!read_pack(pack, n)) goto fail;
    Obj *o = by_id(want);
    for (int hop = 0; o && o->type == G_TAG && hop < 8; hop++) { /* an annotated tag names its object */
        char id[20];
        o = o->n > 47 && mem_eq(o->data, "object ", 7) && hex_id(o->data + 7, o->n - 7, id) ? by_id(id) : 0;
    }
    if (!o || o->type != G_COMMIT) {
        bad("the version's commit is not in the pack");
        goto fail;
    }
    char id[20];
    Obj *tree = o->n > 45 && mem_eq(o->data, "tree ", 5) && hex_id(o->data + 5, o->n - 5, id) ? by_id(id) : 0;
    if (!tree || tree->type != G_TREE) {
        bad("the version's folder is not in the pack");
        goto fail;
    }
    for (long start = 0; folder && start < folder->len;) { /* down to the package's folder */
        long end = start;
        while (end < folder->len && folder->data[end] != '/') end++;
        long at = 0, mode, name_n;
        char *name, *eid;
        Obj *next = 0;
        for (;;) {
            int r = tree_entry(tree, &at, &mode, &name, &name_n, &eid);
            if (r < 0) {
                bad("a folder of the repository is damaged");
                goto fail;
            }
            if (!r) return out; /* no such folder: nothing of a package here */
            if (mode != 040000 || name_n != end - start || !mem_eq(name, folder->data + start, name_n)) continue;
            next = by_id(eid);
            break;
        }
        if (!next || next->type != G_TREE) {
            bad("a folder of the repository is missing from the pack");
            goto fail;
        }
        tree = next;
        start = end + 1;
    }
    if (collect(tree, str_from_cstr(""), 0, out)) return out;
fail:
    *why = g_why;
    git_release();
    return 0;
}

/* ---- the two requests (git's smart HTTP protocol, version 0) ---- */

#define REFS "/info/refs?service=git-upload-pack"

/* The length of the pkt-line at p (its four hex digits, which count themselves), or -1. 0 is a flush. */
static long pkt_len(char *p, long left)
{
    long v = 0;
    if (left < 4) return -1;
    for (int i = 0; i < 4; i++) {
        int c = p[i], d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (d < 0) return -1;
        v = v * 16 + d;
    }
    return v == 0 ? 0 : v < 4 || v > left ? -1 : v;
}

/* `[v]MAJOR.MINOR.PATCH` exactly (no leading zeros, six digits a number at most): 1 with the numbers. */
int git_version_parse(char *p, long n, long *v)
{
    long i = n && p[0] == 'v' ? 1 : 0;
    for (int k = 0; k < 3; k++) {
        long start = i, x = 0;
        for (; i < n && p[i] >= '0' && p[i] <= '9'; i++) x = x * 10 + (p[i] - '0');
        if (i == start || i - start > 6 || (p[start] == '0' && i - start > 1)) return 0;
        if (k < 2 && (i >= n || p[i++] != '.')) return 0;
        v[k] = x;
    }
    return i == n;
}

static int version_cmp(long *a, long *b)
{
    for (int k = 0; k < 3; k++)
        if (a[k] != b[k]) return a[k] < b[k] ? -1 : 1;
    return 0;
}

/* The versions of the repository at `repo` (https://HOST/PATH), lowest first; *final_url is the
 * repository's address after redirects. 0 with *why when it cannot be asked or does not answer as git. */
Vec *git_versions(Str *repo, Str **final_url, Str **why)
{
    SB *sb = sb_new();
    sb_str(sb, repo);
    sb_cstr(sb, REFS);
    Str *end = 0, *body = fetch_request(sb_finish(sb), 0, 0, &end, why);
    if (!body) return 0;
    long suffix = cstr_len(REFS);
    if (end->len <= suffix || !mem_eq(end->data + end->len - suffix, REFS, suffix)) {
        *why = str_from_cstr("it redirects to something that is not a git repository");
        return 0;
    }
    *final_url = str_new(end->data, end->len - suffix);
    Vec *out = vec_new();
    Vec *plain = vec_new(); /* 1 for a tag written without `v` (a `v` tag of the same version wins) */
    long at = 0, lines = 0;
    while (at < body->len) {
        long len = pkt_len(body->data + at, body->len - at);
        if (len < 0) break;
        if (len == 0) {
            at += 4;
            continue;
        }
        char *p = body->data + at + 4;
        long n = len - 4;
        at += len;
        if (!lines++ && !(n > 0 && p[0] == '#')) break; /* the first line names the service */
        if (p[0] == '#') continue;
        if (n > 64 && p[64] == ' ') {
            *why = str_from_cstr("the repository names its objects by SHA-256, which vo does not read");
            return 0;
        }
        char id[20];
        if (n < 42 || p[40] != ' ' || !hex_id(p, n, id)) continue;
        long name_n = 0;
        char *name = p + 41;
        while (41 + name_n < n && name[name_n] && name[name_n] != '\n') name_n++;
        long v[3];
        if (name_n <= 10 || !mem_eq(name, "refs/tags/", 10) || !git_version_parse(name + 10, name_n - 10, v)) continue;
        long is_plain = name[10] != 'v', k = 0;
        while (k < out->n && version_cmp(((GitTag *)vec_get(out, k))->v, v)) k++;
        if (k < out->n && (is_plain || !plain->data[k].i)) continue; /* the version is there already */
        GitTag *t = k < out->n ? vec_get(out, k) : alloc(sizeof(GitTag));
        for (int j = 0; j < 3; j++) t->v[j] = v[j];
        mem_copy(t->id, id, 20);
        if (k == out->n) {
            vec_push(out, t);
            vec_push(plain, (void *)is_plain);
        } else {
            plain->data[k].i = is_plain;
        }
    }
    if (!lines || at < body->len) {
        *why = str_from_cstr("it does not answer as a git repository");
        return 0;
    }
    for (long i = 1; i < out->n; i++) /* insertion sort: lowest first */
        for (long j = i; j > 0 && version_cmp(((GitTag *)vec_get(out, j - 1))->v, ((GitTag *)vec_get(out, j))->v) > 0; j--) {
            void *t = out->data[j].p;
            out->data[j].p = out->data[j - 1].p;
            out->data[j - 1].p = t;
        }
    return out;
}

/* The package files of the tag or commit `id` of the repository (its address after redirects), under
 * `folder` (0: the root): as git_unpack gives them. 0 with *why. */
Vec *git_fetch(Str *repo, char *id, Str *folder, Str **why)
{
    static const char hex[] = "0123456789abcdef";
    SB *rq = sb_new();
    sb_cstr(rq, "0050want "); /* 4 + 5 + 40 + 31 */
    for (int i = 0; i < 20; i++) {
        sb_byte(rq, hex[(uint8_t)id[i] >> 4]);
        sb_byte(rq, hex[id[i] & 15]);
    }
    sb_cstr(rq, " shallow no-progress ofs-delta\n000ddeepen 1\n00000009done\n");
    SB *url = sb_new();
    sb_str(url, repo);
    sb_cstr(url, "/git-upload-pack");
    Str *body = fetch_request(sb_finish(url), sb_finish(rq), "application/x-git-upload-pack-request", 0, why);
    if (!body) return 0;
    long at = 0;
    while (at + 4 <= body->len && !mem_eq(body->data + at, "PACK", 4)) { /* `shallow` lines, a flush, NAK */
        long len = pkt_len(body->data + at, body->len - at);
        if (len < 0) break;
        if (len > 8 && mem_eq(body->data + at + 4, "ERR ", 4)) {
            long n = len - 8;
            while (n && (body->data[at + 8 + n - 1] == '\n' || body->data[at + 8 + n - 1] == '\r')) n--;
            *why = msg("the server refused: $1", str_new(body->data + at + 8, n), 0, 0);
            return 0;
        }
        at += len ? len : 4;
    }
    if (at + 4 > body->len || !mem_eq(body->data + at, "PACK", 4)) {
        *why = str_from_cstr("the server sent no pack for that version");
        return 0;
    }
    char *bad_why = 0;
    Vec *files = git_unpack(body->data + at, body->len - at, id, folder, &bad_why);
    if (!files) *why = str_from_cstr(bad_why);
    return files;
}
