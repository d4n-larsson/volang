/* Spec 017: std.fs's primitive __fs(op, a, b) (research R5-R7).
 *
 * Every path is checked against the grant first (rt_check_path, R0010), then reached through
 * rt_open_parent: from the deepest granted scope directory, one component at a time without following
 * links, so a link swapped in after the check cannot redirect the operation (A4). The operation is
 * then the *at system call on that parent. Failures that depend on the file system are err values
 * with the fixed texts of contracts/codes.md. The ok texts (data-model.md) are decoded by std/fs.vo:
 *   1 info       "m" missing, "f<size>", "d", "o"
 *   2 list_dir   entries joined by "/": "<kind>[l] <name>", sorted by name bytes, non-UTF-8 names left out
 *   3 append_file (a, text), 4 make_dir, 5 delete, 6 rename (a to b): "" */
#include "vo.h"

#define STATX_SIZE 0x200
#define STATX_TYPE 0x1
#define STATX_SIZE_OFF 40 /* struct statx: stx_size */
#define STATX_MODE_OFF 28 /* struct statx: stx_mode (u16) */
#define AT_SYMLINK_NOFOLLOW 0x100
#define S_IFMT 0170000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFLNK 0120000
#define E_NOENT 2
#define E_NOTDIR 20
#define E_LOOP 40
#define DT_UNKNOWN 0
#define DT_DIR 4
#define DT_REG 8
#define DT_LNK 10

static const struct {
    int code;
    const char *text;
} fs_texts[] = {
    {2, "not found"},       {17, "already exists"},          {39, "not empty"},          {20, "not a directory"},
    {21, "is a directory"}, {13, "permission denied"},       {1, "permission denied"},   {28, "no space left"},
    {18, "on another file system"}, {30, "read-only file system"},
};

/* err(text) for a negative errno r. */
static Variant *fs_err(long r)
{
    for (unsigned long i = 0; i < sizeof fs_texts / sizeof fs_texts[0]; i++)
        if (fs_texts[i].code == -r) return make_result(1, (long)str_from_cstr((char *)fs_texts[i].text));
    return make_result(1, (long)msg("error $1", int_to_str(-r), 0, 0));
}

/* stx_mode of name in dir (0 and *err on failure); follow: through a final link. */
static long fs_stat(long dir, char *name, int follow, long *size, long *err)
{
    long stx[32]; /* struct statx, 256 bytes */
    long r = vo_syscall5(SYS_statx, dir, (long)name, follow ? 0 : AT_SYMLINK_NOFOLLOW, STATX_TYPE | STATX_SIZE,
                         (long)stx);
    if (r < 0) {
        *err = r;
        return 0;
    }
    if (size) *size = *(long *)((char *)stx + STATX_SIZE_OFF);
    return *(uint16_t *)((char *)stx + STATX_MODE_OFF) & S_IFMT;
}

/* R0007 for an empty path or one with a NUL byte. */
static void fs_check_text(Str *path)
{
    int bad = path->len == 0;
    for (long i = 0; i < path->len; i++) bad |= path->data[i] == 0;
    if (bad) rt_raise("R0007", str_from_cstr("A path must be non-empty and without NUL bytes."), (void *)cur_node);
}

/* The info text of a path that passed the check ("m", "f<size>", "d", "o"), or err. */
static Variant *fs_info(long bit, Str *path)
{
    Str *name = 0;
    GC_ROOT(&name);
    long parent = rt_open_parent(bit, path, &name, 0);
    long err = 0, size = 0, mode = 0;
    if (parent >= 0) {
        mode = fs_stat(parent, name->data, 0, &size, &err);
        /* unscoped: the target. Scoped, the name is the resolved path's and no link (rt_open_parent):
           one here was swapped in after the check, and is reported as the link it is, not followed */
        if (mode == S_IFLNK && !rt_bit_scoped(bit)) mode = fs_stat(parent, name->data, 1, &size, &err);
        sys_close((int)parent);
    } else {
        err = parent;
    }
    GC_UNROOT(1);
    if (err == -E_NOENT || err == -E_NOTDIR) return make_result(0, (long)str_from_cstr("m"));
    if (err) return fs_err(err);
    if (mode == S_IFREG) return make_result(0, (long)msg("f$1", int_to_str(size), 0, 0));
    return make_result(0, (long)str_from_cstr(mode == S_IFDIR ? "d" : "o"));
}

#define O_WRAPPEND 0x441 /* O_WRONLY | O_CREAT | O_APPEND (the same on all three platforms) */
#define AT_REMOVEDIR 0x200
#define RENAME_NOREPLACE 1
#define E_EXIST 17
#define E_NOTEMPTY 39
#define E_INVAL 22

/* Appends text to a file that passed the check, creating it. */
static Variant *fs_append(Str *path, Str *text)
{
    Str *name = 0;
    GC_ROOT(&name);
    int scoped = rt_bit_scoped(EF_FWRITE);
    long parent = rt_open_parent(EF_FWRITE, path, &name, 0);
    long fd = parent;
    if (parent >= 0) {
        fd = vo_syscall4(SYS_openat, parent, (long)name->data, O_WRAPPEND | O_CLOEXEC | (scoped ? O_NOFOLLOW : 0), 0644);
        sys_close((int)parent);
    }
    GC_UNROOT(1);
    if (fd == -E_LOOP && scoped) rt_denied(EF_FWRITE, path); /* a link where the check saw none */
    if (fd < 0) return fs_err(fd);
    long w = 0;
    for (long done = 0; done < text->len && w >= 0; done += w) w = sys_write((int)fd, text->data + done, text->len - done);
    sys_close((int)fd);
    return w < 0 ? fs_err(w) : make_result(0, 0);
}

/* mkdir of one path that the grant allows (the scope directory itself: from its resolved parent).
 * 0, or a negative errno; *kind gets the mode of what was already there. */
static long fs_mkdir_one(Str *path, long *kind)
{
    Str *name = 0;
    GC_ROOT(&path);
    GC_ROOT(&name);
    long parent;
    if (rt_bit_scoped(EF_FWRITE)) {
        Str *resolved = rt_resolve(path); /* the scope directory itself: its parent outside the scope */
        Str *dir = 0;
        GC_ROOT(&resolved); /* both read after allocations below (they were collected under them) */
        GC_ROOT(&dir);
        long cut = resolved ? resolved->len : 0;
        while (cut > 0 && resolved->data[cut - 1] != '/') cut--;
        int is_scope = 0;
        if (resolved) {
            Str *pname = 0;
            GC_ROOT(&pname);
            long fd = rt_open_parent(EF_FWRITE, path, &pname, 0);
            is_scope = fd == -E_NOENT || (fd >= 0 && pname->len == 1 && pname->data[0] == '.');
            if (fd >= 0 && !is_scope) {
                name = pname;
                parent = fd;
            } else if (fd >= 0) {
                sys_close((int)fd);
            }
            if (fd < 0 && !is_scope) {
                GC_UNROOT(5);
                return fd;
            }
            GC_UNROOT(1);
        }
        if (is_scope) {
            dir = cut > 1 ? str_new(resolved->data, cut - 1) : str_from_cstr("/");
            name = str_new(resolved->data + cut, resolved->len - cut);
            parent = vo_syscall4(SYS_openat, AT_FDCWD, (long)dir->data, O_PATH | O_DIRECTORY | O_CLOEXEC, 0);
        }
        if (!resolved) parent = -E_NOENT;
        GC_UNROOT(2);
    } else {
        parent = rt_open_parent(EF_FWRITE, path, &name, 0);
    }
    long r = parent;
    if (parent >= 0) {
        r = vo_syscall3(SYS_mkdirat, parent, (long)name->data, 0755);
        if (r == -E_EXIST) {
            long err = 0;
            *kind = fs_stat(parent, name->data, 1, 0, &err);
        }
        sys_close((int)parent);
    }
    GC_UNROOT(2);
    return r;
}

/* Creates a directory that passed the check, with its missing parents below (and including) the scope. */
static Variant *fs_make_dir(Str *path)
{
    GC_ROOT(&path);
    long end = path->len;
    while (end > 1 && path->data[end - 1] == '/') end--;
    long r = 0;
    Str *prefix = 0; /* rooted: the checks below allocate before they root their own copy */
    GC_ROOT(&prefix);
    for (long i = 0; i <= end; i++) {
        if (i < end && path->data[i] != '/') continue;
        if (i == 0 || path->data[i - 1] == '/') continue; /* "/", "//" */
        prefix = str_new(path->data, i);
        if (!rt_path_allowed(EF_FWRITE, prefix)) continue; /* above the scope: must exist already */
        long kind = 0;
        r = fs_mkdir_one(prefix, &kind);
        if (r == -E_EXIST) r = kind == S_IFDIR ? 0 : i == end ? -E_EXIST : -E_NOTDIR;
        if (r < 0) break;
    }
    GC_UNROOT(2);
    return r < 0 ? fs_err(r) : make_result(0, 0);
}

/* Deletes a file, link or empty directory that passed the check (never a scope directory). */
static Variant *fs_delete(Str *path)
{
    Str *name = 0;
    GC_ROOT(&name);
    long parent = rt_open_parent(EF_FDELETE, path, &name, 1);
    long r = parent;
    if (parent >= 0) {
        long err = 0;
        long mode = fs_stat(parent, name->data, 0, 0, &err);
        r = err ? err : vo_syscall3(SYS_unlinkat, parent, (long)name->data, mode == S_IFDIR ? AT_REMOVEDIR : 0);
        if (mode == S_IFDIR && r == -E_EXIST) r = -E_NOTEMPTY;
        sys_close((int)parent);
    }
    GC_UNROOT(1);
    return r < 0 ? fs_err(r) : make_result(0, 0);
}

/* Renames from (file.delete) to to (file.write), never onto an existing name or a scope directory. */
static Variant *fs_rename(Str *from, Str *to)
{
    Str *n1 = 0, *n2 = 0;
    GC_ROOT(&from);
    GC_ROOT(&to);
    GC_ROOT(&n1);
    GC_ROOT(&n2);
    long p1 = rt_open_parent(EF_FDELETE, from, &n1, 1);
    long p2 = p1 < 0 ? p1 : rt_open_parent(EF_FWRITE, to, &n2, 1);
    long r = p2;
    Str *r1 = rt_resolve_for(EF_FDELETE, from);
    GC_ROOT(&r1);
    Str *r2 = rt_resolve(to);
    int into = r1 && r2 && r2->len > r1->len && path_under(r2, r1); /* a directory into itself */
    if (p1 >= 0 && p2 >= 0 && !into)
        r = vo_syscall5(SYS_renameat2, p1, (long)n1->data, p2, (long)n2->data, RENAME_NOREPLACE);
    if (p1 >= 0) sys_close((int)p1);
    if (p2 >= 0 && p1 >= 0) sys_close((int)p2);
    GC_UNROOT(5);
    if (into && r >= 0) return make_result(1, (long)str_from_cstr("cannot move a directory into itself"));
    if (r == -E_INVAL) return make_result(1, (long)str_from_cstr("not supported on this file system"));
    if (r == -E_NOTEMPTY) r = -E_EXIST;
    return r < 0 ? fs_err(r) : make_result(0, 0);
}

static char dents[65536]; /* getdents64 buffer */

/* The listing text of a directory that passed the check, or err. */
static Variant *fs_list(Str *path)
{
    Str *name = 0;
    List *items = 0; /* "name\0kind": sorts by name, since no name holds a NUL byte */
    Str *item = 0;
    GC_ROOT(&path);
    GC_ROOT(&name);
    GC_ROOT(&items);
    GC_ROOT(&item);
    int scoped = rt_bit_scoped(EF_FREAD);
    long parent = rt_open_parent(EF_FREAD, path, &name, 0);
    long dir = parent;
    if (parent >= 0) {
        dir = vo_syscall4(SYS_openat, parent, (long)name->data,
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | (scoped ? O_NOFOLLOW : 0), 0);
        sys_close((int)parent);
    }
    if (dir == -E_LOOP && scoped) rt_denied(EF_FREAD, path); /* a link where the check saw none */
    if (dir < 0) {
        GC_UNROOT(4);
        return fs_err(dir);
    }
    items = list_new(16);
    long n;
    while ((n = sys_getdents64((int)dir, dents, sizeof dents)) > 0) {
        for (long off = 0; off < n;) {
            char *d = dents + off;
            off += *(uint16_t *)(d + 16); /* d_reclen */
            int type = (unsigned char)d[18];
            char *nm = d + 19;
            long len = cstr_len(nm);
            if ((len == 1 && nm[0] == '.') || (len == 2 && nm[0] == '.' && nm[1] == '.')) continue;
            if (utf8_validate(nm, len) >= 0) continue; /* not UTF-8: left out (no bytes type yet) */
            long size = 0, err = 0, mode = 0;
            char kind = 'o';
            int link = 0;
            if (type == DT_UNKNOWN || type == DT_REG) {
                mode = fs_stat(dir, nm, 0, &size, &err);
                type = mode == S_IFREG ? DT_REG : mode == S_IFDIR ? DT_DIR : mode == S_IFLNK ? DT_LNK : -1;
            }
            if (type == DT_REG) kind = 'f';
            if (type == DT_DIR) kind = 'd';
            if (type == DT_LNK) { /* the target's kind when it lies inside the grant, else other */
                link = 1;
                SB *sb = sb_new();
                GC_ROOT(&sb);
                sb_str(sb, path);
                if (path->data[path->len - 1] != '/') sb_byte(sb, '/');
                sb_bytes(sb, nm, len);
                GC_UNROOT(1);
                Str *full = sb_finish(sb);
                GC_ROOT(&full);
                if (rt_path_allowed(EF_FREAD, full)) {
                    Variant *v = fs_info(EF_FREAD, full);
                    Str *t = v->tag ? 0 : v->pay.s;
                    if (t && t->data[0] == 'f') {
                        kind = 'f';
                        size = 0;
                        for (long k = 1; k < t->len; k++) size = size * 10 + (t->data[k] - '0');
                    } else if (t && t->data[0] == 'd') {
                        kind = 'd';
                    }
                }
                GC_UNROOT(1);
            }
            SB *sb = sb_new();
            GC_ROOT(&sb);
            sb_bytes(sb, nm, len);
            sb_byte(sb, 0);
            sb_byte(sb, kind);
            if (kind == 'f') int_to_dec(sb, size);
            if (link) sb_byte(sb, 'l');
            GC_UNROOT(1);
            item = sb_finish(sb);
            list_push(items, (long)item);
        }
    }
    sys_close((int)dir);
    if (n < 0) {
        GC_UNROOT(4);
        return fs_err(n);
    }
    list_sort(items, TY_STR);
    SB *out = sb_new();
    GC_ROOT(&out);
    for (long i = 0; i < items->len; i++) {
        Str *s = (Str *)items->data[i].p;
        long cut = 0;
        while (s->data[cut]) cut++;
        if (i) sb_byte(out, '/');
        sb_bytes(out, s->data + cut + 1, s->len - cut - 1);
        sb_byte(out, ' ');
        sb_bytes(out, s->data, cut);
    }
    GC_UNROOT(5);
    return make_result(0, (long)sb_finish(out));
}

/* Checks the effect and the paths (R0010), then runs operation op; while recording, writes its event;
 * while replaying, returns the recorded result instead and touches nothing (the paths are still
 * checked, lexically). */
Variant *io_fs(long op, Str *a, Str *b)
{
    static const long kinds[] = {0, EVK_INFO, EVK_LIST_DIR, EVK_APPEND_FILE, EVK_MAKE_DIR, EVK_DELETE, EVK_RENAME};
    static const long bits[] = {0, EF_FREAD, EF_FREAD, EF_FWRITE, EF_FWRITE, EF_FDELETE, EF_FDELETE};
    if (op < 1 || op > 6) return make_result(1, (long)str_from_cstr("not supported"));
    GC_ROOT(&a);
    GC_ROOT(&b);
    fs_check_text(a);
    if (op == 6) fs_check_text(b);
    long kind = kinds[op], bit = bits[op];
    rt_check_effect(bit);
    if (op == 6) rt_check_effect(EF_FWRITE);
    Variant *r;
    if (rcd_mode == 2) {
        rt_check_path(bit, a);
        if (op == 6) rt_check_path(EF_FWRITE, b);
        r = io_replay(kind, a, op == 6 ? b : 0, 0);
    } else {
        io_pend(kind, a);
        rt_check_path(bit, a);
        if (op == 6) rt_check_path(EF_FWRITE, b);
        switch (op) {
        case 1: r = fs_info(EF_FREAD, a); break;
        case 2: r = fs_list(a); break;
        case 3: r = fs_append(a, b); break;
        case 4: r = fs_make_dir(a); break;
        case 5: r = fs_delete(a); break;
        default: r = fs_rename(a, b); break;
        }
        io_record(kind, a, op == 6 ? b : 0, 0, r);
    }
    GC_UNROOT(2);
    return r;
}

/* spec 021: __fsb(op, path, data): std.fs's bytes functions, through the text functions' own code
 * (the same grant checks, link rules, errors and recordings): 1 read_bytes (the file's bytes, not
 * checked as text), 2 write_bytes, 3 append_bytes. */
Variant *io_fsb(long op, Str *path, Str *data)
{
    switch (op) {
    case 1: rt_check_effect(EF_FREAD); return io_read_file(path);
    case 2: rt_check_effect(EF_FWRITE); return io_write_file(path, data);
    case 3: return io_fs(3, path, data);
    default: return make_result(1, (long)str_from_cstr("not supported"));
    }
}

/* ---- spec 040: a file in pieces, with no file kept open ----
 * `__fs_block(path, offset, size)` reads up to size bytes at offset: the grant check and link rules
 * of read_bytes, open, read, close. A result shorter than size is the end of the file. Every block is
 * one `file_block` event in a recording, written when it is read, so a stream of any length never
 * accumulates. `__fs_lines(path, offset)` reads a block and returns the whole lines in it, split here
 * because a byte loop in vo would be a hundred times slower. Only regular files have offsets. */

#define BLOCK_LINES (256L << 10)
#define LINE_MAX (16L << 20)
#define AT_EMPTY_PATH 0x1000

/* Up to size bytes of path at offset, or 0 with *err the err result. */
static Str *fs_block_read(Str *path, long offset, long size, Variant **err)
{
    GC_ROOT(&path);
    long fd = rt_open_file(EF_FREAD, path, O_RDONLY | O_NONBLOCK);
    long bad = 0, got = 0; /* bad: an errno, -1 for a file that is not a regular one */
    char *buf = 0;
    Str *s = 0;
    int dir = 0;
    if (fd >= 0) {
        long stx[32]; /* struct statx, 256 bytes */
        long r = vo_syscall5(SYS_statx, fd, (long)"", AT_EMPTY_PATH, STATX_TYPE, (long)stx);
        long kind = r < 0 ? 0 : *(uint16_t *)((char *)stx + STATX_MODE_OFF) & S_IFMT;
        dir = kind == S_IFDIR;
        if (kind != S_IFREG) bad = -1;
    }
    if (fd >= 0 && !bad) {
        buf = (char *)vo_syscall6(SYS_mmap, 0, size, 3 /* read, write */, 0x4022 /* private, anonymous, no reserve */, -1, 0);
        if ((long)buf < 0 && (long)buf > -4096) {
            bad = -12;
        } else {
            while (got < size) {
                long r = vo_syscall4(SYS_pread64, fd, (long)buf + got, size - got, offset + got);
                if (r <= 0) {
                    if (r < 0 && !got) bad = r;
                    break;
                }
                got += r;
            }
            if (!bad) s = str_new(buf, got);
            vo_syscall2(SYS_munmap, (long)buf, size);
        }
    }
    if (fd >= 0) sys_close((int)fd);
    if (!s)
        *err = fd >= 0 && bad == -1 && !dir ? make_err("not a regular file: read it whole with read_file")
                                            : make_result(1, (long)msg("cannot read $1", path, 0, 0));
    GC_UNROOT(1);
    return s;
}

/* The block through the grant check, the recording and the replay. grow: a block of exactly `size`
 * bytes without a line end is read again, twice as large, while it may still be one line. */
static Str *fs_block(Str *path, long offset, long size, int grow, Variant **err)
{
    GC_ROOT(&path);
    *err = 0;
    fs_check_text(path);
    Str *s = 0;
    GC_ROOT(&s);
    if (rcd_mode == 2) {
        rt_check_path(EF_FREAD, path);
        Event *e = rp_take(EVK_FILE_BLOCK, path);
        if (e->result == EVR_DENIED || e->port != offset) rp_mismatch_now(EVK_FILE_BLOCK, path);
        if (e->result == EVR_OK)
            s = e->value.s;
        else
            *err = make_result(1, (long)e->value.s);
    } else {
        io_pend(EVK_FILE_BLOCK, path);
        rt_check_path(EF_FREAD, path);
        io_pend(0, 0);
        for (;;) {
            s = fs_block_read(path, offset, size, err);
            if (!s || !grow || s->len < size || size > LINE_MAX) break;
            int nl = 0;
            for (long i = 0; i < s->len && !nl; i++) nl = s->data[i] == '\n';
            if (nl) break;
            size *= 2;
        }
        if (rcd_mode == 1) rcd_event(EVK_FILE_BLOCK, path, s ? EVR_OK : EVR_ERR, s ? s : (*err)->pay.s, offset);
    }
    GC_UNROOT(2);
    return s;
}

/* __fs_block(path, offset, size) -> result[bytes] */
Variant *io_fs_block(Str *path, long offset, long size)
{
    if (offset < 0 || size < 1 || size > LINE_MAX) return make_err("the chunk size must be 1 to 16777216");
    Variant *err;
    Str *s = fs_block(path, offset, size, 0, &err);
    return s ? make_result(0, (long)s) : err;
}

/* __fs_lines(path, offset) -> result[list[str]]: the offset after the lines (decimal text), then
 * the whole lines of the block at offset, without `\n` or `\r\n`. At the end of the file the rest is
 * a line; no line at all means the end. A line that is not UTF-8, or longer than 16 MiB, is an err
 * when it is the first of the block; otherwise the lines before it are returned and the next call
 * starts at it (so the caller, which counts lines, can name its number). */
Variant *io_fs_lines(Str *path, long offset)
{
    if (offset < 0) return make_err("bad offset");
    Variant *err;
    Str *s = fs_block(path, offset, BLOCK_LINES, 1, &err);
    if (!s) return err;
    GC_ROOT(&s);
    List *out = list_new(64);
    GC_ROOT(&out);
    list_push(out, 0); /* the next offset, set at the end */
    long start = 0, why = 0; /* why: 1 not UTF-8, 2 too long */
    while (start < s->len) {
        long end = start;
        while (end < s->len && s->data[end] != '\n') end++;
        int closed = end < s->len;
        if (!closed && out->len > 1) break; /* the tail without a line end: the next block's first line */
        long n = end - start;
        if (n > LINE_MAX) {
            why = 2;
            break;
        }
        if (closed && n && s->data[end - 1] == '\r') n--;
        if (utf8_validate(s->data + start, n) >= 0) {
            why = 1;
            break;
        }
        list_push(out, (long)str_new(s->data + start, n));
        start = end + closed;
    }
    Variant *r;
    if (why && out->len == 1) {
        r = make_err(why == 1 ? "not UTF-8" : "longer than 16 MiB");
    } else {
        out->data[0].p = int_to_str(offset + start);
        r = make_result(0, (long)out);
    }
    GC_UNROOT(2);
    return r;
}
