/* Spec 034: `vo build` and the program part of a built executable.
 *
 * A built executable is a toolchain binary followed by a payload (the program's source files as the
 * loader saw them) and a 64-byte trailer (specs/034-vo-build/data-model.md):
 *   | base | payload | magic[16] | payload offset u64 | payload length u64 | sha256(payload)[32] |
 * The payload is a sequence of records: u64 kind, u64 length, bytes (little-endian).
 * Every start looks for the trailer in its own file (app_start); with one, the command line becomes
 * the toolchain command the executable stands for and the loader takes its files from the payload. */
#include "vo.h"

#define TRAILER 64
#define K_VERSION 1 /* the toolchain's version text */
#define K_ENTRY 2   /* the entry path */
#define K_PATH 3    /* a file's path; its bytes follow (K_BYTES) */
#define K_BYTES 4
#define K_MANIFEST 5 /* vo.packages' path; its bytes follow */
#define PKG_ROOT "/vo-app" /* stands for the manifest's directory, so a build does not depend on where it ran */

static const char magic[16] = "VO-PROGRAM-v1";
const char vo_version_text[] = "vo 0.6.0 schema 1\n";

long ld_app;
Str *app_entry;
long app_manifest = -1;
long opt_app_effects;

static void put_u64(SB *sb, unsigned long v)
{
    for (int i = 0; i < 8; i++, v >>= 8) sb_byte(sb, (int)(v & 0xff));
}

static unsigned long get_u64(char *p)
{
    unsigned long v = 0;
    for (int i = 7; i >= 0; i--) v = v << 8 | (unsigned char)p[i];
    return v;
}

static void put_record(SB *sb, long kind, char *p, long n)
{
    put_u64(sb, (unsigned long)kind);
    put_u64(sb, (unsigned long)n);
    sb_bytes(sb, p, n);
}

/* ---- a built executable's start ---- */

static void app_damaged(void)
{
    toolchain_error(0, 2, "T0014",
                    str_from_cstr("This executable's program part is damaged or cut short: build or copy it again."), 0);
    vo_exit(EXIT_NOINPUT);
}

/* The payload of this executable, checked (T0014), or 0 when it is a toolchain. */
static char *app_payload(long *len_out)
{
    long fd = sys_open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    long size = sys_file_size((int)fd);
    char t[TRAILER];
    if (size < TRAILER || sys_pread_all((int)fd, t, TRAILER, size - TRAILER) || !mem_eq(t, (char *)magic, 16)) {
        sys_close((int)fd);
        return 0;
    }
    unsigned long off = get_u64(t + 16), len = get_u64(t + 24);
    if (off > (unsigned long)size || len > (unsigned long)size || off + len + TRAILER != (unsigned long)size)
        app_damaged();
    char *p = alloc((long)len + 1);
    if (sys_pread_all((int)fd, p, (long)len, (long)off)) app_damaged();
    sys_close((int)fd);
    char hash[32];
    sha256(p, (long)len, hash);
    if (!mem_eq(hash, t + 32, 32)) app_damaged();
    *len_out = (long)len;
    return p;
}

/* Registers the payload's files with the loader: the entry first (file 0, as under `vo run`). */
static void app_register(char *p, long len)
{
    Vec *paths = vec_new(), *texts = vec_new();
    Str *pending = 0, *manifest = 0;
    for (long at = 0; at < len;) {
        if (len - at < 16) app_damaged();
        unsigned long kind = get_u64(p + at), n = get_u64(p + at + 8);
        at += 16;
        if (n > (unsigned long)(len - at)) app_damaged();
        Str *s = str_new(p + at, (long)n);
        at += (long)n;
        if (kind == K_VERSION) {
            if (!str_eq_cstr(s, (char *)vo_version_text)) app_damaged(); /* (a base of another version) */
        } else if (kind == K_ENTRY) {
            app_entry = s;
        } else if (kind == K_PATH || kind == K_MANIFEST) {
            pending = s;
            if (kind == K_MANIFEST) manifest = s;
        } else if (kind == K_BYTES && pending) {
            vec_push(paths, pending);
            vec_push(texts, s);
            pending = 0;
        } else {
            app_damaged();
        }
    }
    if (!app_entry || pending) app_damaged();
    long entry = -1;
    for (long i = 0; i < paths->n; i++)
        if (str_eq(vec_get(paths, i), app_entry)) entry = i;
    if (entry < 0) app_damaged();
    for (long i = -1; i < paths->n; i++) {
        long k = i < 0 ? entry : i;
        if (i == entry) continue;
        Str *path = vec_get(paths, k), *text = vec_get(texts, k);
        int id = file_add(path, text->data, text->len);
        if (path == manifest) app_manifest = id;
    }
    ld_app = 1;
}

long app_options; /* 1 in a built executable */

/* The options of `vo run` a built executable takes as `--vo-NAME`; 1: the option has a value. */
static const struct {
    char *name;
    int value;
} app_opts[] = {{"seed", 1},  {"clock-ms", 1},    {"timeout", 1}, {"max-memory", 1}, {"mem-stats", 0}, {"trace", 1},
                {"trace-limit", 1}, {"record", 1}, {"ca-file", 1}, {"workers", 1},    {"json", 0},      {"log-level", 1}, {"drain", 1}, {0, 0}};

static void app_usage(char *tmpl, char *arg)
{
    report_fd = 2;
    usage_error_arg(tmpl, arg);
}

/* Called first by vo_main. In a toolchain: returns argv unchanged. In a built executable: registers the
 * embedded program and returns the toolchain command line it stands for (research R5):
 *   vo run --timeout 0 OPTIONS ENTRY -- ARGS | vo replay --timeout 0 OPTIONS FILE | vo effects ENTRY |
 *   vo version
 * Arguments that do not start with `--vo-` are the program's, in order. */
char **app_start(long *argc, char **argv)
{
    long len;
    char *payload = app_payload(&len);
    if (!payload) return argv;
    app_register(payload, len);
    app_options = 1; /* usage errors name the options as this executable takes them (`--vo-seed`) */
    long n = *argc;
    char **opts = alloc(n * 16 + 64), **prog = alloc(n * 8 + 8);
    long no = 0, np = 0;
    char *mode = "run", *replay = 0;
    for (long i = 1; i < n; i++) /* (before any usage error: where it is reported) */
        if (cstr_eq(argv[i], "--vo-json")) opt_json = 1;
    for (long i = 1; i < n; i++) {
        char *a = argv[i];
        if (!(a[0] == '-' && a[1] == '-' && a[2] == 'v' && a[3] == 'o' && a[4] == '-')) {
            prog[np++] = a;
            continue;
        }
        char *name = a + 5;
        if (cstr_eq(name, "effects") || cstr_eq(name, "version")) {
            mode = name;
            continue;
        }
        if (cstr_eq(name, "replay")) {
            if (i + 1 >= n) app_usage("`$1` needs a recording: `--vo-replay FILE`.", a);
            mode = name;
            replay = argv[++i];
            continue;
        }
        long k = 0;
        while (app_opts[k].name && !cstr_eq(name, app_opts[k].name)) k++;
        if (!app_opts[k].name)
            app_usage("Unknown option `$1`. Options: --vo-effects, --vo-version, --vo-replay FILE, --vo-record FILE, "
                      "--vo-seed N, --vo-clock-ms N, --vo-timeout SEC, --vo-max-memory MiB, --vo-mem-stats, "
                      "--vo-trace FILE, --vo-trace-limit N, --vo-ca-file FILE, --vo-workers N, --vo-log-level LEVEL, --vo-drain SEC, --vo-json. Every "
                      "other argument goes to the program.",
                      a);
        SB *sb = sb_new(); /* "--NAME" */
        sb_cstr(sb, "--");
        sb_cstr(sb, name);
        opts[no++] = sb_finish(sb)->data;
        if (app_opts[k].value) {
            if (i + 1 >= n) app_usage("`$1` needs a value.", a);
            opts[no++] = argv[++i];
        }
    }
    char **out = alloc((no + np + 8) * 8);
    long c = 0;
    out[c++] = argv[0];
    out[c++] = mode;
    if (cstr_eq(mode, "effects")) {
        opt_app_effects = 1;
        if (opt_json) out[c++] = "--json";
        out[c++] = app_entry->data;
    } else if (cstr_eq(mode, "version")) {
        if (opt_json) out[c++] = "--json";
    } else {
        out[c++] = "--timeout"; /* no time limit unless --vo-timeout sets one (later options win) */
        out[c++] = "0";
        for (long i = 0; i < no; i++) out[c++] = opts[i];
        out[c++] = replay ? replay : app_entry->data;
        if (!replay || np) out[c++] = "--"; /* (arguments with a replay: the usual T0001) */
        for (long i = 0; i < np; i++) out[c++] = prog[i];
    }
    out[c] = 0;
    *argc = c;
    return out;
}

/* ---- vo build ---- */

static void build_refuse(char *tmpl, char *arg)
{
    usage_error_arg(tmpl, arg);
}

/* 1 when hay holds needle. */
static int holds(char *hay, long n, const char *needle, long m)
{
    for (long i = 0; i + m <= n; i++)
        if (hay[i] == needle[0] && mem_eq(hay + i, (char *)needle, m)) return 1;
    return 0;
}

/* The path as the executable keeps it: files under the package manifest's directory (packages are
 * loaded by absolute path) move under PKG_ROOT, so the output does not depend on where the build ran. */
static Str *kept_path(Str *path)
{
    if (!pkg_entries || pkg_manifest < 0 || !pkg_dir || !path_under(path, pkg_dir)) return path;
    SB *sb = sb_new();
    sb_cstr(sb, PKG_ROOT);
    sb_bytes(sb, path->data + pkg_dir->len, path->len - pkg_dir->len);
    return sb_finish(sb);
}

static void build_put_file(SB *sb, long kind, FileRec *f)
{
    Str *p = kept_path(f->path);
    put_record(sb, kind, p->data, p->len);
    put_record(sb, K_BYTES, f->text, f->len);
}

/* `vo build [-o OUTPUT] [--base TOOLCHAIN] FILE` (contracts/cli.md). */
int cmd_build(char *output, char *base)
{
    need_paths();
    if (pos_n != 1) usage_error(str_from_cstr("`build` takes exactly one file."));
    Str *entry = str_from_cstr(((char **)pos_args)[0]);
    long file = load_source(entry);
    check_program((int)file, 1);
    if (!diag_errors) rec_review((int)file, ld_files()); /* what was reviewed is what ships (E0504) */
    if (diag_errors) return finish_check(1);

    char *base_text; /* the toolchain the executable is made from */
    long base_len = read_whole_file(base ? base : "/proc/self/exe", &base_text);
    if (base_len < 0) { /* spec 056: an unreadable input is T0002, as every one is (exit 66) */
        Str *bp = str_from_cstr(base ? base : "/proc/self/exe");
        toolchain_error((int)opt_json, (int)report_fd, "T0002", msg("Cannot read the base toolchain `$1`.", bp, 0, 0), bp);
        diag_errors++;
        if (opt_json) summary_emit(1, (int)report_fd, cmd_name(), file_count, 0);
        vo_exit(EXIT_NOINPUT);
    }
    if (base) {
        if (base_len < TRAILER || !mem_eq(base_text, "\177ELF", 4))
            build_refuse("`$1` is not a vo toolchain: `--base` takes the `vo` binary of another architecture.", base);
        if (mem_eq(base_text + base_len - TRAILER, (char *)magic, 16))
            build_refuse("`$1` is a built program, not a toolchain: `--base` takes a `vo` binary.", base);
        if (!holds(base_text, base_len, vo_version_text, (long)sizeof vo_version_text - 1))
            build_refuse("`$1` is not this toolchain's version (`vo version`): build with the same version on both "
                         "sides.",
                         base);
    }

    Str *out;
    if (output) {
        out = str_from_cstr(output);
    } else { /* the entry's name without `.vo`, in the current directory */
        long a = entry->len, b = entry->len;
        while (a && entry->data[a - 1] != '/') a--;
        if (b - a > 3 && mem_eq(entry->data + b - 3, ".vo", 3)) b -= 3;
        out = str_new(entry->data + a, b - a);
    }
    Str *out_real = rt_resolve(out); /* 0 when there is no such file yet */
    Vec *files = ld_files();
    SB *pay = sb_new();
    put_record(pay, K_VERSION, (char *)vo_version_text, (long)sizeof vo_version_text - 1);
    Str *kept_entry = kept_path(entry);
    put_record(pay, K_ENTRY, kept_entry->data, kept_entry->len);
    long count = 0;
    for (long i = 0; i < files->n; i++) {
        FileRec *f = file_get((int)(long)vec_get(files, i));
        if (f->std) continue; /* the standard library is in the base */
        Str *real = out_real ? rt_resolve(f->path) : 0;
        if (real && str_eq(real, out_real))
            build_refuse("The output `$1` is one of the program's source files: choose another name with `-o`.",
                         out->data);
        build_put_file(pay, K_PATH, f);
        count++;
    }
    if (pkg_entries && pkg_manifest >= 0) build_put_file(pay, K_MANIFEST, file_get((int)pkg_manifest));
    Str *self = rt_resolve(str_from_cstr("/proc/self/exe"));
    if (out_real && self && str_eq(out_real, self))
        build_refuse("The output `$1` is the toolchain itself: choose another name with `-o`.", out->data);

    char trailer[TRAILER];
    SB *t = sb_new();
    sb_bytes(t, (char *)magic, 16);
    put_u64(t, (unsigned long)base_len);
    put_u64(t, (unsigned long)pay->len);
    for (long i = 0; i < 32; i++) sb_byte(t, 0);
    for (long i = 0; i < TRAILER; i++) trailer[i] = t->ptr[i];
    sha256(pay->ptr, pay->len, trailer + 32);

    /* written beside the output and renamed over it, so a failed build leaves nothing */
    Str *tmp = tmp_path(out, "build");
    long fd = open_tmp(tmp);
    long failed = fd < 0;
    if (!failed) {
        failed = write_checked((int)fd, base_text, base_len) || write_checked((int)fd, pay->ptr, pay->len) ||
                 write_checked((int)fd, trailer, TRAILER) || sys_fchmod((int)fd, 0755) < 0;
        failed |= sys_close((int)fd) < 0;
        if (!failed) failed = sys_rename(tmp->data, out->data) < 0;
        if (failed) sys_unlink(tmp->data);
    }
    if (failed) {
        toolchain_error((int)opt_json, (int)report_fd, "T0009", msg("Cannot write `$1`.", out, 0, 0), out);
        return EXIT_OUTPUT;
    }
    long total = base_len + pay->len + TRAILER;
    SB *sb = sb_new();
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"build\",\"output\":");
        json_str_into(sb, out);
        sb_cstr(sb, ",\"files\":");
        int_to_dec(sb, count);
        sb_cstr(sb, ",\"bytes\":");
        int_to_dec(sb, total);
        sb_cstr(sb, "}\n");
    } else {
        sb_cstr(sb, "built ");
        sb_str(sb, out);
        sb_cstr(sb, ": ");
        int_to_dec(sb, count);
        sb_cstr(sb, count == 1 ? " file, " : " files, ");
        int_to_dec(sb, total);
        sb_cstr(sb, " bytes\n");
    }
    out_bytes(1, sb->ptr, sb->len);
    return 0;
}
