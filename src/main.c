/* CLI: argument parsing, path expansion, command dispatch (contracts/cli.md; ported from
 * src/main.s, spec 008). */
#include "vo.h"

enum {
    CMD_CHECK = 1,
    CMD_RUN,
    CMD_TEST,
    CMD_FMT,
    CMD_EXPLAIN,
    CMD_REFERENCE,
    CMD_VERSION,
    CMD_EFFECTS,
    CMD_QUERY,
    CMD_IMPACT,
    CMD_EDIT,
    CMD_REPLAY,
    CMD_DOC,
    CMD_PACKAGE,
    CMD_BUILD, /* spec 034 */
    CMD_MCP,   /* spec 036 */
    CMD_LSP    /* spec 038 */
};

long opt_json;
long opt_timeout = 10;
long opt_mem_stats;
long opt_workers; /* spec 028: vo run --workers N (0: as declared) */
long opt_remove; /* spec 007: vo package --remove */
long opt_filter; /* Str * */
long opt_check;
long opt_update;
long prog_argc;
long prog_argv; /* char ** */
long opt_cmd;
long report_fd = 1;
long pos_args; /* char ** (positional arguments) */
long pos_n;
/* spec 006 options (contracts/cli.md) */
long opt_diff; /* char *: --diff FILE (impact) */
long opt_dry_run;
long opt_trace;            /* char *: --trace FILE */
long opt_trace_limit;      /* 0: not given (default 100000) */
long opt_record;           /* char *: --record FILE (run) */
/* spec 018 options of `vo test` (contracts/cli.md) */
long opt_list;             /* --list */
long opt_coverage;         /* --coverage */
long opt_mutate;           /* --mutate */
long opt_max_mutants;      /* --max-mutants N; 0: not given (default 500) */
Vec *opt_tests;            /* --test ID (Str *), repeatable; 0: none */
Vec *opt_specs;            /* --spec ID (Str *), repeatable; 0: none */
Vec *opt_ca_files;         /* spec 019: --ca-file FILE (char *), repeatable; 0: none */
static long opt_seed_given; /* 1 when --seed or --clock-ms was given (replay refuses them) */
long path_list;            /* Str ** (expanded .vo paths) */
long path_n;
static long path_cap;

static char *const cmd_names[] = {"check", "run",    "test",   "fmt",  "explain", "reference", "version",
                                  "effects", "query", "impact", "edit", "replay",  "doc",       "package", "build", "mcp", "lsp", 0};

int cstr_eq(char *a, char *b)
{
    for (; *a == *b; a++, b++)
        if (!*a) return 1;
    return 0;
}

/* Emits T0001 and exits 64. */
void usage_error(Str *message)
{
    toolchain_error((int)opt_json, (int)report_fd, "T0001", message, 0);
    vo_exit(EXIT_USAGE);
}

void usage_error_arg(char *tmpl, char *arg)
{
    usage_error(msg(tmpl, str_from_cstr(arg), 0, 0));
}

void path_push(Str *str)
{
    if (path_n >= path_cap) {
        long cap = path_cap ? path_cap * 2 : 64;
        void *grown = alloc(cap * 8);
        void *old = (void *)path_list;
        path_list = (long)grown;
        mem_copy(grown, old, path_n * 8);
        path_cap = cap;
    }
    ((Str **)path_list)[path_n++] = str;
}

/* Insertion sort (byte-wise) of path_list[from..path_n). */
void sort_paths(long from)
{
    for (long i = from + 1; i < path_n; i++) {
        Str **list = (Str **)path_list;
        Str *s = list[i];
        long j = i - 1;
        for (; j >= from && str_cmp(list[j], s) > 0; j--) list[j + 1] = list[j];
        list[j + 1] = s;
    }
}

int ends_with_vo(Str *str)
{
    long n = str->len;
    return n >= 3 && str->data[n - 3] == '.' && str->data[n - 2] == 'v' && str->data[n - 1] == 'o';
}

/* Adds every *.vo file below the open directory fd (recursively) to path_list. */
long walk_strict;   /* spec 013 (H5): a package's walk: links to directories inside walk_root are followed, */
long walk_failed;   /* and a directory that cannot be read or a link leading outside sets walk_failed */
Str *walk_root;     /* the package directory, resolved */
static long walk_depth;

void walk_dir(Str *dirpath, int fd)
{
    char *buf = alloc(32768); /* dirent buffer */
    if (++walk_depth > 64) { /* (a loop of links inside the package) */
        walk_failed = 1;
        walk_depth--;
        return;
    }
    for (;;) {
        long got = sys_getdents64(fd, buf, 32768);
        if (got < 0 && walk_strict) walk_failed = 1;
        if (got <= 0) break;
        for (long off = 0; off < got;) {
            char *ent = buf + off; /* struct linux_dirent64: ino 8, off 8, reclen 2, type 1, name */
            off += *(uint16_t *)(ent + 16);
            char *name = ent + 19;
            if (name[0] == '.') continue; /* skip ".", ".." and hidden entries */
            SB *sb = sb_new();
            sb_str(sb, dirpath);
            sb_byte(sb, '/');
            sb_cstr(sb, name);
            Str *full = sb_finish(sb);
            int type = ent[18];
            long sub = -1;
            if (type == 4 || (walk_strict && (type == 10 || type == 0))) { /* DT_DIR, or a link (DT_LNK, unknown) */
                sub = sys_open(full->data, O_DIRECTORY);
                if (sub < 0 && type == 4 && walk_strict) walk_failed = 1; /* an unreadable directory */
            }
            if (sub >= 0) {
                if (type != 4) { /* a link to a directory: followed only when it stays inside the package */
                    Str *real = rt_resolve(full);
                    if (!real || !path_under(real, walk_root)) walk_failed = 1;
                }
                if (!walk_failed) walk_dir(full, (int)sub);
                sys_close((int)sub);
            } else if (type != 4 && ends_with_vo(full)) {
                path_push(full);
            }
        }
    }
    walk_depth--;
}

/* Turns the positional arguments into path_list: directories become their sorted *.vo files. */
void expand_paths(void)
{
    for (long i = 0; i < pos_n; i++) {
        char *arg = ((char **)pos_args)[i];
        long fd = sys_open(arg, O_DIRECTORY);
        if (fd < 0) {
            path_push(str_from_cstr(arg));
            continue;
        }
        Str *dir = str_from_cstr(arg); /* strip trailing '/' for clean joins */
        while (dir->len > 1 && dir->data[dir->len - 1] == '/') dir->len--;
        long from = path_n;
        walk_dir(dir, (int)fd);
        sys_close((int)fd);
        sort_paths(from);
    }
}

/* Reads a source file and registers it; returns the file id, or exits 66 with T0002. */
long load_source(Str *path)
{
    long id = file_find(path); /* already loaded (e.g. as another file's import) */
    if (id >= 0) return id;
    char *text;
    long len = read_whole_file(path->data, &text);
    if (len >= 0) return file_add(path, text, len);
    Str *m = msg("Cannot read `$1`.", path, 0, 0);
    toolchain_error((int)opt_json, (int)report_fd, "T0002", m, path);
    diag_errors++;
    /* run prints no summary for an unreadable input. The summary is always JSON: main.s passed
       cmd_name's result where the json flag belongs (kept for identical output; results.md
       "Deviations" lists it for a fix after the port). */
    if (opt_cmd != CMD_RUN) summary_emit(1, (int)report_fd, cmd_name(), file_count, 0);
    vo_exit(EXIT_NOINPUT);
    return 0;
}

char *cmd_name(void)
{
    return cmd_names[opt_cmd - 1];
}

void need_paths(void)
{
    if (!pos_n) usage_error_arg("`$1` needs at least one path.", cmd_name());
}

/* A whole number option value; 0 when it is one. */
static int option_number(char *text, long *out)
{
    return dec_to_int(text, cstr_len(text), out);
}

int vo_main(long argc, char **argv, char **env)
{
    envp = (long)env;
    static char *opt_output, *opt_base; /* spec 034: vo build -o OUTPUT --base TOOLCHAIN */
    argv = app_start(&argc, argv);      /* spec 034: a built executable stands for a toolchain command */
    for (int i = 1; i < argc; i++) { /* pre-scan for --json (before any "--") */
        if (cstr_eq(argv[i], "--")) break;
        if (cstr_eq(argv[i], "--json")) opt_json = 1;
    }
    pos_args = (long)alloc(argc * 8L);
    if (argc < 2)
        usage_error(str_from_cstr("Missing command. Commands: check, run, test, build, fmt, effects, query, impact, "
                                  "edit, replay, doc, package, mcp, lsp, explain, reference, version."));
    char *cmd = argv[1];
    long c = 0;
    while (cmd_names[c] && !cstr_eq(cmd, cmd_names[c])) c++;
    if (!cmd_names[c])
        usage_error_arg("Unknown command `$1`. Commands: check, run, test, build, fmt, effects, query, impact, edit, "
                        "replay, doc, package, mcp, lsp, explain, reference, version.",
                        cmd);
    opt_cmd = c + 1;
    if (opt_cmd == CMD_MCP) { /* spec 036: a server, before any option is read; its calls run vo_main again */
        if (argc != 2)
            usage_error(str_from_cstr("`mcp` takes no arguments: it serves the Model Context Protocol on standard "
                                      "input and output."));
        return cmd_mcp();
    }
    if (opt_cmd == CMD_LSP) { /* spec 038: likewise */
        if (argc != 2)
            usage_error(str_from_cstr("`lsp` takes no arguments: it serves the Language Server Protocol on standard "
                                      "input and output."));
        return cmd_lsp();
    }
    if (opt_cmd == CMD_REPLAY || opt_cmd == CMD_RUN) report_fd = 2;
    for (int i = 2; i < argc;) { /* options and positional arguments */
        char *a = argv[i++];
        if (cstr_eq(a, "--")) { /* the program's arguments */
            prog_argc = argc - i;
            prog_argv = (long)(argv + i);
            break;
        }
        if (opt_cmd == CMD_BUILD && (cstr_eq(a, "-o") || cstr_eq(a, "--base"))) { /* spec 034 */
            if (i >= argc) usage_error_arg("`$1` needs a file name.", a);
            if (a[1] == 'o') opt_output = argv[i++];
            else opt_base = argv[i++];
            continue;
        }
        if (a[0] != '-' || a[1] != '-') { /* positional */
            ((char **)pos_args)[pos_n++] = a;
            continue;
        }
        long v;
        if (cstr_eq(a, "--json")) continue;
        if (cstr_eq(a, "--check")) {
            opt_check = 1;
        } else if (cstr_eq(a, "--update")) {
            opt_update = 1;
        } else if (cstr_eq(a, "--timeout")) {
            if (i >= argc || option_number(argv[i++], &v) || v < 0)
                usage_error(str_from_cstr("`--timeout` needs a whole number of seconds."));
            opt_timeout = v;
        } else if (cstr_eq(a, "--filter")) {
            if (i >= argc) usage_error(str_from_cstr("`--filter` needs a text to match."));
            opt_filter = (long)str_from_cstr(argv[i++]);
        } else if (cstr_eq(a, "--seed") || cstr_eq(a, "--clock-ms")) {
            long which = cstr_eq(a, "--seed") ? 1 : 2;
            opt_seed_given = 1;
            if (i >= argc || option_number(argv[i++], &v) || v < 0)
                usage_error(str_from_cstr("`--seed` and `--clock-ms` need a whole number."));
            rt_set_option(which, v);
        } else if (cstr_eq(a, "--list")) { /* spec 018 */
            opt_list = 1;
        } else if (cstr_eq(a, "--coverage")) {
            opt_coverage = 1;
        } else if (cstr_eq(a, "--mutate")) {
            opt_mutate = 1;
        } else if (cstr_eq(a, "--test") || cstr_eq(a, "--spec")) {
            if (i >= argc || !*argv[i]) usage_error_arg("`$1` needs an identifier.", a);
            Vec **list = cstr_eq(a, "--test") ? &opt_tests : &opt_specs;
            if (!*list) *list = vec_new();
            vec_push(*list, str_from_cstr(argv[i++]));
        } else if (cstr_eq(a, "--ca-file")) { /* spec 019 */
            if (i >= argc || !*argv[i]) usage_error(str_from_cstr("`--ca-file` needs a certificate file."));
            if (!opt_ca_files) opt_ca_files = vec_new();
            vec_push(opt_ca_files, argv[i++]);
        } else if (cstr_eq(a, "--max-mutants")) {
            if (i >= argc || option_number(argv[i++], &v) || v < 1 || v > 100000)
                usage_error(str_from_cstr("`--max-mutants` needs a whole number from 1 to 100000."));
            opt_max_mutants = v;
        } else if (cstr_eq(a, "--remove")) { /* spec 007: vo package */
            opt_remove = 1;
        } else if (cstr_eq(a, "--workers")) { /* spec 028 */
            if (i >= argc || option_number(argv[i++], &v) || v < 1 || v > 64)
                usage_error(str_from_cstr("`--workers` needs a whole number from 1 to 64."));
            opt_workers = v;
        } else if (cstr_eq(a, "--mem-stats")) {
            opt_mem_stats = 1;
        } else if (cstr_eq(a, "--max-memory")) {
            if (i >= argc || option_number(argv[i++], &v) || v < 1 || v > 65536)
                usage_error(str_from_cstr("`--max-memory` needs a whole number of MiB from 1 to 65536."));
            gc_limit = v << 20; /* MiB -> bytes */
        } else { /* spec 006 options */
            long used = opt_spec006(a, i < argc ? argv[i] : 0);
            if (used < 0)
                usage_error_arg("Unknown option `$1`. Options: --json, --timeout SEC, --filter TEXT, --check, --update, "
                                "--seed N, --clock-ms N, --max-memory MiB, --mem-stats, --trace FILE, --trace-limit N, "
                                "--record FILE, --diff FILE, --dry-run, --list, --test ID, --spec ID, --coverage, "
                                "--mutate, --max-mutants N, --ca-file FILE, --workers N.",
                                a);
            i += (int)used;
        }
    }
    opt_spec006_check();
    switch (opt_cmd) {
    case CMD_QUERY: return cmd_query();
    case CMD_IMPACT: return cmd_impact();
    case CMD_EDIT: return cmd_edit();
    case CMD_REPLAY: return cmd_replay();
    case CMD_DOC: return cmd_doc();
    case CMD_PACKAGE: return cmd_package();
    case CMD_VERSION:
        out_cstr(1, opt_json ? "{\"v\":1,\"kind\":\"version\",\"version\":\"0.1.0\",\"schema\":1}\n"
                             : (char *)vo_version_text);
        return 0;
    case CMD_BUILD: return cmd_build(opt_output, opt_base);
    case CMD_CHECK: return cmd_check();
    case CMD_EXPLAIN: return cmd_explain();
    case CMD_REFERENCE: return cmd_reference();
    case CMD_RUN: return cmd_run();
    case CMD_TEST: return cmd_test();
    case CMD_EFFECTS: return cmd_effects();
    default: return cmd_fmt();
    }
}

/* The options added by spec 006. Returns -1 when flag is none of them, else how many of the
 * following arguments it used (0 or 1). A missing or bad value is T0001. */
long opt_spec006(char *flag, char *next)
{
    if (cstr_eq(flag, "--dry-run")) {
        opt_dry_run = 1;
        return 0;
    }
    long *file_opt = 0;
    if (cstr_eq(flag, "--diff"))
        file_opt = &opt_diff;
    else if (cstr_eq(flag, "--trace"))
        file_opt = &opt_trace;
    else if (cstr_eq(flag, "--record"))
        file_opt = &opt_record;
    else if (!cstr_eq(flag, "--trace-limit"))
        return -1;
    if (file_opt) { /* FILE: any non-empty word */
        if (!next || !*next) usage_error_arg("`$1` needs a file name.", flag);
        *file_opt = (long)next;
        return 1;
    }
    long v; /* --trace-limit N, 1..100000000 */
    if (!next || option_number(next, &v) || v < 1 || v > 100000000)
        usage_error(str_from_cstr("`--trace-limit` needs a whole number from 1 to 100000000."));
    opt_trace_limit = v;
    return 1;
}

/* T0001: "`flag` only works with `commands`." */
void only_with(char *flag, char *commands)
{
    Str *f = str_from_cstr(flag);
    Str *c = str_from_cstr(commands);
    usage_error(msg("`$1` only works with `$2`.", f, c, 0));
}

/* Which commands the spec 006 options belong to (contracts/cli.md "Rules"). Paths that are
 * program source files are checked after loading (out_paths_check). */
void opt_spec006_check(void)
{
    long cmd = opt_cmd;
    if (cmd == CMD_IMPACT) {
        if (!opt_diff) usage_error(str_from_cstr("`impact` needs `--diff FILE` (or `--diff -` for stdin)."));
    } else if (opt_diff && cmd != CMD_TEST) {
        only_with("--diff", "impact` or `test");
    }
    if (cmd != CMD_TEST) { /* spec 018: the options of `vo test` */
        if (opt_list) only_with("--list", "test");
        if (opt_coverage) only_with("--coverage", "test");
        if (opt_mutate) only_with("--mutate", "test");
        if (opt_tests) only_with("--test", "test");
        if (opt_specs) only_with("--spec", "test");
        if (opt_max_mutants) only_with("--max-mutants", "test");
    }
    if (opt_ca_files && cmd != CMD_RUN && cmd != CMD_TEST) only_with("--ca-file", "run` or `test"); /* spec 019 */
    if (opt_list + opt_coverage + opt_mutate > 1)
        usage_error(str_from_cstr("`--list`, `--coverage` and `--mutate` cannot be combined: use one at a time."));
    if (opt_max_mutants && !opt_mutate) usage_error(str_from_cstr("`--max-mutants` needs `--mutate`."));
    if (!opt_max_mutants) opt_max_mutants = 500;
    if (opt_dry_run && cmd != CMD_EDIT) only_with("--dry-run", "edit");
    if (opt_record && cmd != CMD_RUN) only_with("--record", "run");
    if (opt_workers && cmd != CMD_RUN) only_with("--workers", "run"); /* spec 028 */
    if (opt_trace && cmd != CMD_RUN && cmd != CMD_TEST && cmd != CMD_REPLAY)
        only_with("--trace", "run`, `test` or `replay");
    if (opt_trace_limit && !opt_trace) usage_error(str_from_cstr("`--trace-limit` needs `--trace FILE`."));
    if (cmd == CMD_REPLAY && (opt_seed_given || prog_argv))
        usage_error(str_from_cstr(
            "`replay` takes `--seed`, `--clock-ms` and program arguments from the recording; leave them out."));
    if (opt_trace && opt_record && cstr_eq((char *)opt_trace, (char *)opt_record)) /* the same file for both */
        usage_error_arg("`--trace` and `--record` name the same file `$1`.", (char *)opt_trace);
}

/* T0001 when --trace or --record names one of the loaded program's source files (contracts/cli.md). */
void out_paths_check(void)
{
    if (opt_trace) out_path_is_source((char *)opt_trace);
    if (opt_record) out_path_is_source((char *)opt_record);
}

/* T0001 if path equals the path of a loaded source file. */
void out_path_is_source(char *path)
{
    Str *p = str_from_cstr(path);
    for (long i = 0; i < file_count; i++)
        if (str_eq(file_get((int)i)->path, p))
            usage_error_arg("`$1` is a source file of the program; choose another file for the trace or recording.",
                            path);
}

/* Spec 006: checks that --trace and --record do not name a source file, then starts them. */
void outputs_start(long is_test)
{
    out_paths_check();
    trace_start(is_test);
    rcd_start();
}

int not_implemented(void)
{
    Str *m = msg("`$1` is not implemented yet.", str_from_cstr(cmd_name()), 0, 0);
    toolchain_error((int)opt_json, (int)report_fd, "T0003", m, 0);
    return EXIT_INTERNAL;
}

/* Emits diagnostics and the summary; returns 65 when there were errors. */
int finish_check(long files)
{
    diag_emit_all((int)opt_json, (int)report_fd);
    summary_emit((int)opt_json, (int)report_fd, cmd_name(), files, 0);
    return diag_errors ? EXIT_CHECK : 0;
}

int cmd_check(void)
{
    need_paths();
    expand_paths();
    long i = 0;
    for (; i < path_n; i++) {
        long file = load_source(((Str **)path_list)[i]);
        long errors = diag_errors;
        check_program((int)file, 0); /* mode: check */
        if (diag_errors == errors) rec_review((int)file, ld_files()); /* a clean program: its effects record */
        if (diag_errors == errors) structure_check((int)file, ld_files()); /* spec 027 */
    }
    return finish_check(i);
}

/* `vo effects [--json] [--update] FILE` (contracts/cli-v2.md). */
int cmd_effects(void)
{
    need_paths();
    if (pos_n != 1) usage_error(str_from_cstr("`effects` takes exactly one file."));
    long file = load_source(str_from_cstr(((char **)pos_args)[0]));
    check_program((int)file, 0);
    if (!diag_errors) rec_command((int)file, ld_files(), (int)opt_update, (int)opt_json);
    if (opt_app_effects && !diag_errors) return 0; /* spec 034: `--vo-effects` prints the line only */
    return finish_check(1);
}

/* Prints the catalog section of one code (or an `explain` record under --json). */
int cmd_explain(void)
{
    if (pos_n != 1) usage_error(str_from_cstr("`explain` takes one error code, e.g. `vo explain E0301`."));
    char *code = ((char **)pos_args)[0];
    int ok = cstr_len(code) == 5 && (code[0] == 'E' || code[0] == 'W' || code[0] == 'R' || code[0] == 'T'); /* [EWRT][0-9]{4} */
    for (int i = 1; ok && i < 5; i++) ok = code[i] >= '0' && code[i] <= '9';
    Str *section = ok ? catalog_section(code) : 0;
    if (!section)
        usage_error_arg("Unknown error code `$1`. Codes look like E0301, W0201, R0002 or T0001.", code);
    if (!opt_json) {
        out_str(1, section);
        return 0;
    }
    SB *sb = sb_new(); /* {"v":1,"kind":"explain","code":..,"name":..,"fixable":..,"text":..} */
    sb_cstr(sb, "{\"v\":1,\"kind\":\"explain\",\"code\":\"");
    sb_cstr(sb, code);
    sb_cstr(sb, "\",\"name\":\"");
    sb_str(sb, catalog_name(code));
    sb_cstr(sb, "\",\"fixable\":");
    sb_cstr(sb, catalog_fixable(code) ? "true" : "false");
    sb_cstr(sb, ",\"text\":");
    json_str_into(sb, section);
    sb_cstr(sb, "}\n");
    out_bytes(1, sb->ptr, sb->len);
    return 0;
}

/* Prints the embedded language reference unchanged (or a `reference` record under --json). */
int cmd_reference(void)
{
    if (!opt_json) {
        out_bytes(1, (char *)ref_md, ref_md_len);
        return 0;
    }
    SB *sb = sb_new();
    sb_cstr(sb, "{\"v\":1,\"kind\":\"reference\",\"text\":");
    json_str_into(sb, str_new((char *)ref_md, ref_md_len));
    sb_cstr(sb, "}\n");
    out_bytes(1, sb->ptr, sb->len);
    return 0;
}

/* Checks one file (main required), then runs it. */
/* spec 019: reads every --ca-file into the trust anchors (T0002 unreadable, T0013 no certificate). */
static void ca_files_load(void)
{
    for (long i = 0; opt_ca_files && i < opt_ca_files->n; i++) tls_add_ca_file(vec_get(opt_ca_files, i));
}

int cmd_run(void)
{
    need_paths();
    if (pos_n != 1) usage_error(str_from_cstr("`run` takes exactly one file."));
    long file = load_source(str_from_cstr(((char **)pos_args)[0]));
    check_program((int)file, 1);
    if (diag_errors) return finish_check(1); /* warnings do not stop the run (not shown either) */
    Node *main_fn = find_main(file_get((int)file)->ast->a.v);
    ca_files_load();     /* spec 019: --ca-file, before the program runs */
    outputs_start(0);    /* spec 006: --trace / --record */
    compile_program();   /* bytecode for every function (spec 004) */
    rt_effects_start(0);
    int status = run_main(main_fn);
    gc_report(2, "run", 0);
    return status;
}

/* Checks every file (test mode), then runs their tests; 1 when any test did not pass. */
/* Spec 027 (FR-014): adds file to the test files unless it is there already. */
static void add_test_file(Vec *files, long file)
{
    for (long k = 0; k < files->n; k++)
        if ((long)vec_get(files, k) == file) return;
    vec_push(files, (void *)file);
}

int cmd_test(void)
{
    need_paths();
    expand_paths();
    long n = path_n;
    Vec *files = vec_new(); /* the program files of every entry (spec 027: not only the entries), each once */
    if (opt_diff) q_collect = 1; /* spec 018: --diff needs the reference log, as `vo impact` */
    for (long i = 0; i < n; i++) {
        long entry = load_source(((Str **)path_list)[i]);
        check_program((int)entry, 2); /* mode: test */
        add_test_file(files, entry); /* the entry first, then its imports as loaded */
        Vec *program = ld_files();
        for (long k = 0; k < program->n; k++) {
            FileRec *f = file_get((int)(long)vec_get(program, k));
            if (!f->std && !f->pkg) add_test_file(files, (long)vec_get(program, k));
        }
    }
    long m = files->n;
    long *ids = alloc(m * 8); /* test file ids */
    for (long i = 0; i < m; i++) ids[i] = (long)vec_get(files, i);
    q_collect = 0;
    if (diag_errors) return finish_check(n);
    test_select_start(ids, m); /* spec 018: --diff, and T0011 before anything runs */
    diag_emit_all((int)opt_json, 1); /* warnings first, then the tests */
    ca_files_load();                 /* spec 019: --ca-file, before any test runs */
    if (opt_list) { /* spec 018: nothing is compiled or run */
        for (long i = 0; i < m; i++) list_file_tests((int)ids[i]);
        list_summary(n);
        return 0;
    }
    outputs_start(1);                /* spec 006: --trace */
    if (opt_coverage || opt_mutate) { /* spec 018: coverage points, shared with tasks */
        cov_on = 1;
        cov_hit = map_shared(COV_MAX);
    }
    compile_program();               /* bytecode for every function and test (spec 004) */
    if (opt_mutate) { /* spec 018: which tests reach which functions (a byte each, shared) */
        long tests = 0;
        for (long i = 0; i < m; i++) {
            Vec *items = file_get((int)ids[i])->ast->a.v;
            for (long j = 0; j < items->n; j++) tests += ((Node *)vec_get(items, j))->kind == N_TEST;
        }
        cov_reach = map_shared((tests ? tests : 1) * (cov_nfn ? cov_nfn : 1));
    }
    interp_init();
    gc_start();
    for (long i = 0; i < m; i++) run_file_tests((int)ids[i]);
    long failing = t_failed + t_errored + t_timed_out;
    if (opt_mutate && !failing) mutate_run(); /* spec 018: before the summary, which reports it */
    test_summary(n);
    if (opt_mutate && failing) { /* T0012: changes cannot be judged against failing tests */
        Str *why = msg("Mutation testing needs passing tests: $1 of the selected tests did not pass. Fix them first.",
                       int_to_str(failing), 0, 0);
        toolchain_error((int)opt_json, 1, "T0012", why, 0);
    }
    return failing ? EXIT_FAIL : 0;
}

/* Formats files in place (or lists them with --check). Any syntax error: nothing is written, 65. */
int cmd_fmt(void)
{
    need_paths();
    expand_paths();
    long n = path_n;
    long *ids = alloc(n * 8); /* file ids */
    for (long i = 0; i < n; i++) {
        ids[i] = load_source(((Str **)path_list)[i]);
        lex((int)ids[i]);
        parse_file((int)ids[i]);
    }
    if (diag_errors) return finish_check(n); /* refuse: report the syntax errors, change nothing */
    long changed_files = 0;
    for (long i = 0; i < n; i++) {
        Str *text = fmt_file((int)ids[i]); /* formatted text */
        FileRec *f = file_get((int)ids[i]);
        int changed = !str_eq_bytes(text, f->text, f->len);
        changed_files += changed;
        if (changed && !opt_check) write_file_safely(f->path, text);
        fmt_report(f->path, changed);
    }
    SB *sb = sb_new(); /* summary */
    if (opt_json) {
        sb_cstr(sb, ",\"changed\":");
        int_to_dec(sb, changed_files);
        summary_emit(1, 1, "fmt", n, sb_finish(sb));
    } else {
        int_to_dec(sb, changed_files);
        sb_cstr(sb, " of ");
        int_to_dec(sb, n);
        sb_cstr(sb, opt_check ? " files need formatting\n" : " files changed\n");
        out_bytes(1, sb->ptr, sb->len);
    }
    return opt_check && changed_files ? EXIT_FAIL : 0;
}

/* JSON: one fmt record per file. Human: a line per changed file. */
void fmt_report(Str *path, int changed)
{
    SB *sb = sb_new();
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"fmt\",\"file\":");
        json_str_into(sb, path);
        sb_cstr(sb, changed ? ",\"changed\":true}\n" : ",\"changed\":false}\n");
    } else {
        if (!changed) return;
        sb_cstr(sb, opt_check ? "not formatted: " : "formatted ");
        sb_str(sb, path);
        sb_byte(sb, '\n');
    }
    out_bytes(1, sb->ptr, sb->len);
}

/* Writes <path>.vo-fmt-<random>.tmp (created exclusively), then renames it over path, so a crash never
 * truncates the source. */
Str *write_captured; /* spec 038: the last text a command would have written */
long write_capture;  /* spec 038: 1 in a copy of `vo lsp`: nothing is written, the text is kept */

void write_file_safely(Str *path, Str *text)
{
    if (write_capture) {
        write_captured = text;
        return;
    }
    Str *tmp = tmp_path(path, "fmt"); /* spec 013 (M3): unpredictable, created exclusively */
    long fd = open_tmp(tmp);
    if (fd < 0) return;
    write_all((int)fd, text->data, text->len);
    sys_close((int)fd);
    sys_rename(tmp->data, path->data);
}
