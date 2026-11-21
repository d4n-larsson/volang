/* Spec 036: `vo mcp`, the toolchain as a Model Context Protocol server (contracts/mcp.md).
 *
 * One JSON-RPC 2.0 message per line on stdin, one per line on stdout. The protocol's own methods are
 * answered here. A tool call is the ordinary command: the server forks, the copy puts three pipes on
 * 0/1/2 and calls vo_main with the words built from the call's arguments, so the result is the
 * command's by construction and whatever happens in it (exit, timeout, out of memory, a crash) ends
 * with the copy (research R3). The tools are one table: it prints the tool list, checks a call's
 * arguments and builds the command's words (R4).
 *
 * The server keeps no memory per message: fixed buffers, JSON read in place (R5). One call at a time;
 * lines that arrive during a call wait in the input buffer, except a cancellation of that call. */
#include "vo.h"

#define IN_CAP (4L << 20)  /* an incoming message */
#define OUT_CAP (1L << 20) /* a call's stdout, and its stderr */
#define MAX_WORDS 256
#define MAX_DEPTH 64
#define SIGPIPE 13
#define SIGCHLD 17
#define SIG_BLOCK 0
#define SIG_SETMASK 2
#define F_SETFL 4
#define POLLIN 1
#define POLLOUT 4
#define POLLERR 8
#define POLLHUP 16
#define WNOHANG 1
#define E_INTR 4
#define E_AGAIN 11

typedef struct PollFd {
    int fd;
    short events;
    short revents;
} PollFd;

static char in[IN_CAP + 1];    /* unread input; a line is NUL-terminated where its newline was */
static long in_len, in_pos, in_scan, in_eof;
static char words[IN_CAP + 1]; /* a call's argument words and stdin text, unescaped */
static long words_len;
static char cap[2][OUT_CAP];   /* a call's stdout and stderr */
static long got_len[2], got_over[2];
static char wbuf[65536];       /* the reply being written */
static long wlen;
static int host_gone;          /* a reply could not be written: nobody is listening */
static char id[256];           /* the request's id as its JSON text; empty for a notification */
static long id_len;
static int initialized;
static int sig_fd = -1;
static unsigned long old_mask;

/* ---- writing ---- */

static void w_flush(void)
{
    long done = 0;
    while (done < wlen && !host_gone) {
        long n = sys_write(1, wbuf + done, wlen - done);
        if (n == -E_INTR) continue;
        if (n == -E_AGAIN) {
            sys_wait_fd(1, POLLOUT, -1);
            continue;
        }
        if (n <= 0) host_gone = 1;
        else done += n;
    }
    wlen = 0;
}

static void w_raw(char *p, long n)
{
    for (long i = 0; i < n; i++) {
        if (wlen == (long)sizeof wbuf) w_flush();
        wbuf[wlen++] = p[i];
    }
}

static void w(char *s)
{
    w_raw(s, cstr_len(s));
}

/* v in decimal at buf; the length. */
static long fmt_int(char *buf, long v)
{
    char t[24];
    long n = 0, k = 0;
    unsigned long u = v < 0 ? 0UL - (unsigned long)v : (unsigned long)v;
    do {
        t[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    if (v < 0) buf[k++] = '-';
    while (n) buf[k++] = t[--n];
    return k;
}

static void w_int(long v)
{
    char b[24];
    w_raw(b, fmt_int(b, v));
}

/* The length of the well-formed UTF-8 sequence at p (4 bytes may be read; a NUL ends any sequence),
 * or 0: no overlong forms, no surrogates, nothing above U+10FFFF. */
static int utf8_seq(unsigned char *p, long n)
{
    unsigned c = p[0];
    if (c < 0x80) return 1;
    if (c < 0xC2) return 0;
    if (c < 0xE0) return n >= 2 && (p[1] & 0xC0) == 0x80 ? 2 : 0;
    if (c < 0xF0) {
        if (n < 3 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return 0;
        if ((c == 0xE0 && p[1] < 0xA0) || (c == 0xED && p[1] >= 0xA0)) return 0;
        return 3;
    }
    if (c < 0xF5) {
        if (n < 4 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) return 0;
        if ((c == 0xF0 && p[1] < 0x90) || (c == 0xF4 && p[1] >= 0x90)) return 0;
        return 4;
    }
    return 0;
}

/* The n bytes at p as a JSON string. A byte that is not UTF-8 is written as U+FFFD and counted. */
static void w_json_string(char *p, long n, long *replaced)
{
    static const char hex[] = "0123456789abcdef";
    w("\"");
    for (long i = 0; i < n;) {
        unsigned char c = (unsigned char)p[i];
        if (c == '"' || c == '\\') {
            char e[2] = {'\\', (char)c};
            w_raw(e, 2);
        } else if (c == '\n') {
            w("\\n");
        } else if (c < 0x20) {
            char e[6] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
            w_raw(e, 6);
        } else if (c < 0x80) {
            w_raw(p + i, 1);
        } else {
            int k = utf8_seq((unsigned char *)p + i, n - i);
            if (k) {
                w_raw(p + i, k);
                i += k;
            } else {
                w("\xEF\xBF\xBD");
                if (replaced) ++*replaced;
                i++;
            }
            continue;
        }
        i++;
    }
    w("\"");
}

static void reply_begin(void)
{
    w("{\"jsonrpc\":\"2.0\",\"id\":");
    if (id_len) w_raw(id, id_len);
    else w("null");
    w(",\"result\":");
}

static void reply_end(void)
{
    w("}\n");
    w_flush();
}

/* An error answer; `$1` in tmpl is the n bytes at arg (shortened). A notification gets none, except
 * for the errors that have no id to begin with (to_null). */
static void reply_error(long code, char *tmpl, char *arg, long n, int to_null)
{
    if (!id_len && !to_null) return;
    if (to_null) id_len = 0;
    char m[512];
    long k = 0;
    if (n > 120) n = 120;
    for (char *t = tmpl; *t && k < 300; t++) {
        if (t[0] == '$' && t[1] == '1') {
            for (long i = 0; i < n; i++) m[k++] = arg[i];
            t++;
        } else {
            m[k++] = *t;
        }
    }
    w("{\"jsonrpc\":\"2.0\",\"id\":");
    if (id_len) w_raw(id, id_len);
    else w("null");
    w(",\"error\":{\"code\":");
    w_int(code);
    w(",\"message\":");
    w_json_string(m, k, 0);
    w("}}\n");
    w_flush();
}

/* ---- reading JSON in place ---- */

static char *js_ws(char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static int is_hex(char c)
{
    return is_digit(c) || ((c | 32) >= 'a' && (c | 32) <= 'f');
}

static int starts(char *p, char *lit)
{
    while (*lit)
        if (*p++ != *lit++) return 0;
    return 1;
}

/* The end of the JSON value at p, or 0 when it is not one. */
static char *js_skip(char *p, int depth)
{
    if (depth > MAX_DEPTH) return 0;
    if (*p == '"') {
        for (p++;;) {
            unsigned char c = (unsigned char)*p;
            if (c == '"') return p + 1;
            if (c < 0x20) return 0; /* includes the end of the line */
            if (c == '\\') {
                char e = p[1];
                if (e == 'u') {
                    if (!is_hex(p[2]) || !is_hex(p[3]) || !is_hex(p[4]) || !is_hex(p[5])) return 0;
                    p += 6;
                } else if (e == '"' || e == '\\' || e == '/' || e == 'b' || e == 'f' || e == 'n' || e == 'r' ||
                           e == 't') {
                    p += 2;
                } else {
                    return 0;
                }
            } else {
                int k = utf8_seq((unsigned char *)p, 4);
                if (!k) return 0;
                p += k;
            }
        }
    }
    if (*p == '{' || *p == '[') {
        char close = *p == '{' ? '}' : ']';
        int object = *p == '{';
        p = js_ws(p + 1);
        if (*p == close) return p + 1;
        for (;;) {
            if (object) {
                if (*p != '"') return 0;
                p = js_skip(p, depth);
                if (!p) return 0;
                p = js_ws(p);
                if (*p != ':') return 0;
                p = js_ws(p + 1);
            }
            p = js_skip(p, depth + 1);
            if (!p) return 0;
            p = js_ws(p);
            if (*p == close) return p + 1;
            if (*p != ',') return 0;
            p = js_ws(p + 1);
        }
    }
    if (starts(p, "true") || starts(p, "null")) return p + 4;
    if (starts(p, "false")) return p + 5;
    if (*p == '-') p++;
    if (*p == '0') {
        p++;
    } else {
        if (!is_digit(*p)) return 0;
        while (is_digit(*p)) p++;
    }
    if (*p == '.') {
        if (!is_digit(*++p)) return 0;
        while (is_digit(*p)) p++;
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!is_digit(*p)) return 0;
        while (is_digit(*p)) p++;
    }
    return p;
}

/* Steps through the members of a checked object: *p starts at js_ws(object + 1); 0 at the end, else
 * the key (at its quote) and the value. */
static int js_member(char **p, char **key, char **val)
{
    if (**p == '}') return 0;
    *key = *p;
    char *q = js_ws(js_skip(*p, 0));
    *val = js_ws(q + 1);
    q = js_ws(js_skip(*val, 0));
    *p = *q == ',' ? js_ws(q + 1) : q;
    return 1;
}

/* 1 when the checked JSON string at p is exactly lit (n bytes), compared as written (no escapes). */
static int js_is(char *p, char *lit, long n)
{
    return *p == '"' && mem_eq(p + 1, lit, n) && p[1 + n] == '"';
}

/* The value of key in the checked object, or 0. */
static char *js_get(char *object, char *key)
{
    if (!object || *object != '{') return 0;
    char *p = js_ws(object + 1), *k, *v;
    long n = cstr_len(key);
    while (js_member(&p, &k, &v))
        if (js_is(k, key, n)) return v;
    return 0;
}

static int hex_val(char c)
{
    return is_digit(c) ? c - '0' : (c | 32) - 'a' + 10;
}

static unsigned hex4(char *p)
{
    return (unsigned)(hex_val(p[0]) << 12 | hex_val(p[1]) << 8 | hex_val(p[2]) << 4 | hex_val(p[3]));
}

/* The checked JSON string at p, unescaped, appended to `words` with a NUL after it; its start, the
 * length in *len. A lone surrogate becomes U+FFFD. Unescaping never grows a string, so it fits. */
static char *js_text(char *p, long *len)
{
    char *start = words + words_len, *d = start;
    for (p++; *p != '"';) {
        if (*p != '\\') {
            *d++ = *p++;
            continue;
        }
        char e = p[1];
        p += 2;
        if (e != 'u') {
            *d++ = e == 'b' ? '\b' : e == 'f' ? '\f' : e == 'n' ? '\n' : e == 'r' ? '\r' : e == 't' ? '\t' : e;
            continue;
        }
        unsigned c = hex4(p);
        p += 4;
        if (c >= 0xD800 && c < 0xDC00 && p[0] == '\\' && p[1] == 'u' && is_hex(p[2]) && is_hex(p[3]) &&
            is_hex(p[4]) && is_hex(p[5]) && hex4(p + 2) >= 0xDC00 && hex4(p + 2) < 0xE000) {
            c = 0x10000 + ((c - 0xD800) << 10) + (hex4(p + 2) - 0xDC00);
            p += 6;
        } else if (c >= 0xD800 && c < 0xE000) {
            c = 0xFFFD;
        }
        if (c < 0x80) {
            *d++ = (char)c;
        } else if (c < 0x800) {
            *d++ = (char)(0xC0 | c >> 6);
            *d++ = (char)(0x80 | (c & 63));
        } else if (c < 0x10000) {
            *d++ = (char)(0xE0 | c >> 12);
            *d++ = (char)(0x80 | (c >> 6 & 63));
            *d++ = (char)(0x80 | (c & 63));
        } else {
            *d++ = (char)(0xF0 | c >> 18);
            *d++ = (char)(0x80 | (c >> 12 & 63));
            *d++ = (char)(0x80 | (c >> 6 & 63));
            *d++ = (char)(0x80 | (c & 63));
        }
    }
    *len = d - start;
    *d = 0;
    words_len += *len + 1;
    return start;
}

/* ---- the tools (contracts/mcp.md) ---- */

enum { /* how an argument becomes command words */
    K_OPERAND = 1, /* the value (each element of a list) */
    K_CHOICE,      /* an operand out of `word`'s `a|b|c` */
    K_FLAG,        /* `word` when true */
    K_OPTION,      /* `word` VALUE (once per element of a list) */
    K_MODE,        /* `--VALUE`, out of `word`'s `a|b|c` */
    K_PROGRAM,     /* `--` and the elements, last */
    K_STDIN,       /* the command's standard input */
    K_DIFF         /* `--diff -` and the text as standard input */
};

typedef struct Arg {
    char *name;
    char type; /* s: string, n: whole number, b: boolean, l: list of strings */
    char kind;
    char required;
    char *word;
    char *desc;
} Arg;

typedef struct Tool {
    char *name;
    char hint; /* r: reads, w: changes files, x: runs the user's program */
    char *desc;
    Arg args[14];
} Tool;

#define RUN_OPTIONS                                                                                                    \
    {"timeout", 'n', K_OPTION, 0, "--timeout", "Time limit in whole seconds (default 10; 0: none)."},                  \
        {"seed", 'n', K_OPTION, 0, "--seed", "Seed of the `random` effect, for a reproducible run."},                               \
        {"clock_ms", 'n', K_OPTION, 0, "--clock-ms", "Fixes the clock at this many milliseconds since 1970."},          \
        {"max_memory", 'n', K_OPTION, 0, "--max-memory", "Limit on live data in MiB (default 512)."},                   \
        {"mem_stats", 'b', K_FLAG, 0, "--mem-stats", "Also report memory use."},                                        \
        {"ca_files", 'l', K_OPTION, 0, "--ca-file", "Extra certificate files to trust for HTTPS."}

static const Tool tools[] = {
    {"check", 'r', "Check vo files: diagnostics with stable codes and machine-applicable fixes. Runs nothing.",
     {{"paths", 'l', K_OPERAND, 1, 0, "Files or directories (every *.vo under a directory)."}}},
    {"run", 'x', "Check one vo file and run its `main` with the effects it declares.",
     {{"file", 's', K_OPERAND, 1, 0, "The entry file."},
      {"args", 'l', K_PROGRAM, 0, 0, "The program's arguments (`args()`)."},
      {"stdin", 's', K_STDIN, 0, 0, "The program's standard input (default: empty)."},
      RUN_OPTIONS,
      {"workers", 'n', K_OPTION, 0, "--workers", "Server workers, 1 to 64."}}},
    {"test", 'x', "Run the tests, properties and specs of vo files; each result is one record.",
     {{"paths", 'l', K_OPERAND, 1, 0, "Files or directories."},
      {"filter", 's', K_OPTION, 0, "--filter", "Run only tests whose name contains this text."},
      {"tests", 'l', K_OPTION, 0, "--test", "Run only the tests with these identifiers."},
      {"specs", 'l', K_OPTION, 0, "--spec", "Run only the tests of these @spec identifiers."},
      {"diff", 's', K_DIFF, 0, 0, "A unified diff (its text): run only the tests it can affect."},
      {"mode", 's', K_MODE, 0, "list|coverage|mutate",
       "list: name the tests without running; coverage: report lines not run; mutate: mutation testing."},
      {"max_mutants", 'n', K_OPTION, 0, "--max-mutants", "With mode mutate: how many mutants at most."},
      RUN_OPTIONS}},
    {"fmt", 'w', "Rewrite vo files in canonical form (or only list the ones that are not, with `check`).",
     {{"paths", 'l', K_OPERAND, 1, 0, "Files or directories."},
      {"check", 'b', K_FLAG, 0, "--check", "Change nothing; list the files that are not canonical."}}},
    {"effects", 'w', "Show what a program may touch: the effects of `main` and of every function; `update` "
                     "rewrites the reviewed record `vo.effects`.",
     {{"file", 's', K_OPERAND, 1, 0, "The entry file."},
      {"update", 'b', K_FLAG, 0, "--update", "Write `vo.effects` from the program as it is now."}}},
    {"explain", 'r', "The catalog entry of a diagnostic code, with an example and its fix.",
     {{"code", 's', K_OPERAND, 1, 0, "A code such as E0301, W0201, R0002 or T0001."}}},
    {"doc", 'r', "Documentation with runnable examples: of the standard library (no arguments: the modules; "
                 "`name`: a module or item such as std.json or std.json.parse_json) or of a program (`entry`).",
     {{"entry", 's', K_OPERAND, 0, 0, "A program's entry file, to document its own items."},
      {"name", 's', K_OPERAND, 0, 0, "A module or an item."}}},
    {"query", 'r', "Ask about a checked program: the callers of a function, the references to an item, the type "
                   "at a position, or its @spec identifiers.",
     {{"question", 's', K_CHOICE, 1, "callers|refs|type-at|specs", "What to ask."},
      {"entry", 's', K_OPERAND, 1, 0, "The program's entry file."},
      {"target", 's', K_OPERAND, 0, 0, "An item's name, or FILE:LINE:COL for type-at; none for specs."}}},
    {"impact", 'r', "What a change touches: the items a diff changes, their callers and the tests to run.",
     {{"entry", 's', K_OPERAND, 1, 0, "The program's entry file."},
      {"diff", 's', K_DIFF, 1, 0, "A unified diff (its text)."}}},
    {"edit", 'w', "A checked refactoring across the whole program: rename an item, or add a field to a record "
                  "and to every place that builds one.",
     {{"edit", 's', K_CHOICE, 1, "rename|add-field", "The edit."},
      {"entry", 's', K_OPERAND, 1, 0, "The program's entry file."},
      {"target", 's', K_OPERAND, 1, 0, "rename: the item; add-field: the record."},
      {"change", 's', K_OPERAND, 1, 0, "rename: the new name; add-field: `name: type = default`."},
      {"dry_run", 'b', K_FLAG, 0, "--dry-run", "Report the changes without writing them."}}},
};
#define N_TOOLS ((long)(sizeof tools / sizeof tools[0]))

/* 1 when the n bytes at v are one of the words in `a|b|c`. */
static int in_choices(const char *choices, char *v, long n)
{
    for (const char *c = choices; *c;) {
        const char *e = c;
        while (*e && *e != '|') e++;
        if (e - c == n && mem_eq((char *)c, v, n)) return 1;
        c = *e ? e + 1 : e;
    }
    return 0;
}

static void tools_list(void)
{
    reply_begin();
    w("{\"tools\":[");
    for (long t = 0; t < N_TOOLS; t++) {
        const Tool *tool = &tools[t];
        w(t ? ",{\"name\":" : "{\"name\":");
        w_json_string(tool->name, cstr_len(tool->name), 0);
        w(",\"description\":");
        w_json_string(tool->desc, cstr_len(tool->desc), 0);
        w(",\"inputSchema\":{\"type\":\"object\",\"properties\":{");
        for (const Arg *a = tool->args; a->name; a++) {
            if (a != tool->args) w(",");
            w_json_string(a->name, cstr_len(a->name), 0);
            w(a->type == 'n'   ? ":{\"type\":\"integer\""
              : a->type == 'b' ? ":{\"type\":\"boolean\""
              : a->type == 'l' ? ":{\"type\":\"array\",\"items\":{\"type\":\"string\"}"
                               : ":{\"type\":\"string\"");
            if (a->type == 'l' && a->required) w(",\"minItems\":1");
            if (a->kind == K_CHOICE || a->kind == K_MODE) {
                w(",\"enum\":[");
                for (char *c = a->word; *c;) {
                    char *e = c;
                    while (*e && *e != '|') e++;
                    if (c != a->word) w(",");
                    w_json_string(c, e - c, 0);
                    c = *e ? e + 1 : e;
                }
                w("]");
            }
            w(",\"description\":");
            w_json_string(a->desc, cstr_len(a->desc), 0);
            w("}");
        }
        w("},\"required\":[");
        int first = 1;
        for (const Arg *a = tool->args; a->name; a++) {
            if (!a->required) continue;
            if (!first) w(",");
            first = 0;
            w_json_string(a->name, cstr_len(a->name), 0);
        }
        w("],\"additionalProperties\":false},\"annotations\":");
        w(tool->hint == 'r'   ? "{\"readOnlyHint\":true,\"openWorldHint\":false}}"
          : tool->hint == 'w' ? "{\"readOnlyHint\":false,\"destructiveHint\":true,\"openWorldHint\":false}}"
                              : "{\"readOnlyHint\":false,\"destructiveHint\":true,\"openWorldHint\":true}}");
    }
    w("]}");
    reply_end();
}

/* ---- a call ---- */

static void bad(char *tmpl, const char *name)
{
    reply_error(-32602, tmpl, (char *)name, cstr_len((char *)name), 0);
}

/* 1 when v has a's type. */
static int has_type(const Arg *a, char *v)
{
    if (a->type == 's') return *v == '"';
    if (a->type == 'b') return starts(v, "true") || starts(v, "false");
    if (a->type == 'n') {
        char *p = v + (*v == '-');
        if (!is_digit(*p)) return 0;
        while (is_digit(*p)) p++;
        return *p != '.' && *p != 'e' && *p != 'E';
    }
    if (*v != '[') return 0;
    for (char *p = js_ws(v + 1); *p != ']';) {
        if (*p != '"') return 0;
        p = js_ws(js_skip(p, 0));
        if (*p == ',') p = js_ws(p + 1);
    }
    return 1;
}

static char *av[MAX_WORDS + 2];
static long ac;
static const char *call_bad; /* why the words could not be built: a template for `bad` */

static void push(char *word)
{
    if (ac >= MAX_WORDS) call_bad = "Too many words in `$1`: a call takes at most 256.";
    else av[ac++] = word;
}

/* The string at v as a word. */
static char *word_of(char *v, int operand)
{
    long n;
    char *t = js_text(v, &n);
    if (cstr_len(t) != n) call_bad = "Argument `$1` holds a NUL character.";
    if (operand && t[0] == '-' && t[1] == '-')
        call_bad = "Argument `$1` has a value that begins with `--`; write a file of that name as `./--name`.";
    return t;
}

/* Appends the words of argument a with value v. pass 0: options, 1: operands, 2: program arguments. */
static void arg_words(const Arg *a, char *v, int pass, char **input, long *input_len)
{
    long n;
    int kind = a->kind;
    if (pass != (kind == K_OPERAND || kind == K_CHOICE ? 1 : kind == K_PROGRAM ? 2 : 0)) return;
    if (kind == K_FLAG) {
        if (*v == 't') push(a->word);
    } else if (kind == K_STDIN || kind == K_DIFF) {
        *input = js_text(v, input_len);
        if (kind == K_DIFF) push("--diff"), push("-");
    } else if (kind == K_MODE || kind == K_CHOICE) {
        char *t = js_text(v, &n);
        if (!in_choices(a->word, t, n)) {
            call_bad = "Argument `$1` is not one of its allowed values.";
        } else if (kind == K_CHOICE) {
            push(t);
        } else { /* `--list`: the two dashes go in front, in `words` */
            char *flag = words + words_len;
            flag[0] = flag[1] = '-';
            mem_copy(flag + 2, t, n + 1);
            words_len += n + 3;
            push(flag);
        }
    } else if (a->type == 'n') { /* the digits as written */
        char *e = js_skip(v, 0), *t = words + words_len;
        mem_copy(t, v, e - v);
        t[e - v] = 0;
        words_len += e - v + 1;
        push(a->word), push(t);
    } else if (a->type == 's') {
        if (kind == K_OPTION) push(a->word);
        push(word_of(v, kind == K_OPERAND));
    } else { /* a list */
        if (kind == K_PROGRAM && *js_ws(v + 1) != ']') push("--");
        for (char *p = js_ws(v + 1); *p != ']';) {
            if (kind == K_OPTION) push(a->word);
            push(word_of(p, kind == K_OPERAND));
            p = js_ws(js_skip(p, 0));
            if (*p == ',') p = js_ws(p + 1);
        }
    }
}

static void set_nonblock(int fd)
{
    vo_syscall3(SYS_fcntl, fd, F_SETFL, O_NONBLOCK);
}

static void close_fd(int *fd)
{
    if (*fd >= 0) sys_close(*fd);
    *fd = -1;
}

/* Takes what fd has now: the first OUT_CAP bytes are kept, the rest counted. */
static void drain(int *fd, int k)
{
    static char buf[65536];
    for (;;) {
        long n = sys_read(*fd, buf, sizeof buf);
        if (n == -E_INTR) continue;
        if (n == -E_AGAIN) return;
        if (n <= 0) {
            close_fd(fd);
            return;
        }
        long keep = OUT_CAP - got_len[k];
        if (keep > n) keep = n;
        mem_copy(cap[k] + got_len[k], buf, keep);
        got_len[k] += keep;
        got_over[k] += n - keep;
    }
}

/* Reads more input after moving what is unread to the front. */
static void in_more(int wait)
{
    if (in_pos) {
        for (long i = in_pos; i < in_len; i++) in[i - in_pos] = in[i];
        in_len -= in_pos;
        in_scan = in_scan > in_pos ? in_scan - in_pos : 0;
        in_pos = 0;
    }
    for (;;) {
        long n = sys_read(0, in + in_len, IN_CAP - in_len);
        if (n == -E_INTR) continue;
        if (n == -E_AGAIN) {
            if (!wait) return;
            sys_wait_fd(0, POLLIN, -1);
            continue;
        }
        if (n <= 0) in_eof = 1;
        else in_len += n;
        return;
    }
}

/* During a call: among the complete lines not looked at yet, a cancellation of the running call is
 * taken out (blanked); every other line waits. 1 when the call is cancelled. */
static int cancel_scan(void)
{
    int cancelled = 0;
    for (;;) {
        long nl = in_scan;
        while (nl < in_len && in[nl] != '\n') nl++;
        if (nl >= in_len) return cancelled;
        char *line = js_ws(in + in_scan), *end;
        in[nl] = 0;
        if (*line == '{' && (end = js_skip(line, 0)) && !*js_ws(end) && !js_get(line, "id")) {
            char *m = js_get(line, "method"), *r = js_get(js_get(line, "params"), "requestId");
            if (m && js_is(m, "notifications/cancelled", 23) && r && js_skip(r, 0) - r == id_len &&
                mem_eq(r, id, id_len)) {
                cancelled = 1;
                for (long i = in_scan; i < nl; i++) in[i] = ' ';
            }
        }
        in[nl] = '\n';
        in_scan = nl + 1;
    }
}

/* Runs `vo av...` in a copy of this process and answers with what it printed and its status. */
static void run_call(char *input, long input_len)
{
    int pin[2] = {-1, -1}, pout[2] = {-1, -1}, perr[2] = {-1, -1};
    long pid = -1;
    if (sys_pipe(pin) >= 0 && sys_pipe(pout) >= 0 && sys_pipe(perr) >= 0) {
        long parent = sys_getpid();
        pid = sys_fork();
        if (pid == 0) {
            vo_syscall2(SYS_setpgid, 0, 0);
            sys_die_with_parent(parent);
            vo_syscall4(SYS_rt_sigprocmask, SIG_SETMASK, (long)&old_mask, 0, 8);
            sys_dup_to(pin[0], 0), sys_dup_to(pout[1], 1), sys_dup_to(perr[1], 2);
            if (vo_syscall3(SYS_close_range, 3, ~0UL, 0) < 0)
                for (int fd = 3; fd < 4096; fd++) sys_close(fd);
            vo_exit(vo_main(ac, av, (char **)envp));
        }
    }
    close_fd(&pin[0]), close_fd(&pout[1]), close_fd(&perr[1]);
    if (pid < 0) {
        close_fd(&pin[1]), close_fd(&pout[0]), close_fd(&perr[0]);
        reply_error(-32603, "The command could not be started.", 0, 0, 0);
        return;
    }
    set_nonblock(pin[1]), set_nonblock(pout[0]), set_nonblock(perr[0]);
    got_len[0] = got_len[1] = got_over[0] = got_over[1] = 0;
    if (!input_len) close_fd(&pin[1]);
    in_scan = in_pos;
    int status = 0, reaped = 0, cancelled = cancel_scan(), killed = 0;
    long sent = 0;
    while (!reaped) {
        if ((cancelled || host_gone) && !killed) {
            vo_syscall2(SYS_kill, -pid, 9); /* its group, and itself in case it has no group yet */
            sys_kill(pid);
            killed = 1;
        }
        PollFd fds[6];
        int nf = 0, at_in = -1, at_out;
        fds[nf++] = (PollFd){sig_fd, POLLIN, 0};
        fds[at_out = nf++] = (PollFd){1, 0, 0}; /* only to learn that nobody reads the answers any more */
        if (pout[0] >= 0) fds[nf++] = (PollFd){pout[0], POLLIN, 0};
        if (perr[0] >= 0) fds[nf++] = (PollFd){perr[0], POLLIN, 0};
        if (pin[1] >= 0) fds[nf++] = (PollFd){pin[1], POLLOUT, 0};
        if (!in_eof && in_len - in_pos < IN_CAP) fds[at_in = nf++] = (PollFd){0, POLLIN, 0};
        long r = vo_syscall5(SYS_ppoll, (long)fds, nf, 0, 0, 8);
        if (r < 0 && r != -E_INTR) break;
        if (pout[0] >= 0) drain(&pout[0], 0);
        if (perr[0] >= 0) drain(&perr[0], 1);
        if (pin[1] >= 0) {
            long n = sys_write(pin[1], input + sent, input_len - sent);
            if (n > 0) sent += n;
            if ((n < 0 && n != -E_AGAIN && n != -E_INTR) || sent >= input_len) close_fd(&pin[1]);
        }
        if (r > 0 && (fds[at_out].revents & (POLLERR | POLLHUP))) host_gone = 1;
        if (r > 0 && at_in >= 0 && fds[at_in].revents) {
            in_more(0);
            if (cancel_scan()) cancelled = 1;
        }
        char info[128];
        while (sys_read(sig_fd, info, sizeof info) > 0) {}
        reaped = vo_syscall4(SYS_wait4, pid, (long)&status, WNOHANG, 0) == pid;
    }
    vo_syscall2(SYS_kill, -pid, 9); /* whatever the command left running */
    if (!reaped) sys_wait(pid, &status);
    if (pout[0] >= 0) drain(&pout[0], 0);
    if (perr[0] >= 0) drain(&perr[0], 1);
    close_fd(&pin[1]), close_fd(&pout[0]), close_fd(&perr[0]);
    if (cancelled || host_gone) return; /* a cancelled call gets no answer */
    long code = status >> 8 & 255;
    if (status & 127) { /* ended by a signal: the toolchain failed, not the user's program */
        char m[64] = "vo: the command was ended by signal ";
        long k = cstr_len(m);
        k += fmt_int(m + k, status & 127);
        m[k++] = '\n';
        if (got_len[1] + k <= OUT_CAP) {
            mem_copy(cap[1] + got_len[1], m, k);
            got_len[1] += k;
        }
        code = 125;
    }
    long replaced = 0;
    reply_begin();
    w("{\"content\":[");
    for (int k = 0; k < 2; k++) {
        if (!got_len[k]) continue;
        w("{\"type\":\"text\",\"text\":");
        w_json_string(cap[k], got_len[k], &replaced);
        w("},");
    }
    char e[160] = "{\"v\":1,\"kind\":\"exit\",\"status\":";
    long n = cstr_len(e);
    n += fmt_int(e + n, code);
    static char *const extra[3] = {",\"stdout_omitted\":", ",\"stderr_omitted\":", ",\"replaced\":"};
    long counts[3] = {got_over[0], got_over[1], replaced};
    for (int k = 0; k < 3; k++) {
        if (!counts[k]) continue;
        mem_copy(e + n, extra[k], cstr_len(extra[k]));
        n += cstr_len(extra[k]);
        n += fmt_int(e + n, counts[k]);
    }
    e[n++] = '}';
    w("{\"type\":\"text\",\"text\":");
    w_json_string(e, n, 0);
    w(code == 64 || code == 125 ? "}],\"isError\":true}" : "}],\"isError\":false}");
    reply_end();
}

static void tools_call(char *params)
{
    char *name = js_get(params, "name"), *args = js_get(params, "arguments");
    const Tool *tool = 0;
    if (!name || *name != '"') {
        bad("`tools/call` needs the tool's `$1`.", "name");
        return;
    }
    for (long t = 0; t < N_TOOLS; t++)
        if (js_is(name, tools[t].name, cstr_len(tools[t].name))) tool = &tools[t];
    if (!tool) {
        reply_error(-32602, "Unknown tool `$1`. Tools: check, run, test, fmt, effects, explain, doc, query, impact, "
                            "edit.",
                    name + 1, js_skip(name, 0) - name - 2, 0);
        return;
    }
    if (!args) args = "{}";
    if (*args != '{') {
        bad("`$1` must be an object.", "arguments");
        return;
    }
    char *p = js_ws(args + 1), *k, *v;
    while (js_member(&p, &k, &v)) { /* every argument is known and of its type */
        const Arg *a = tool->args;
        while (a->name && !js_is(k, a->name, cstr_len(a->name))) a++;
        if (!a->name) {
            reply_error(-32602, "Unknown argument `$1` for this tool.", k + 1, js_skip(k, 0) - k - 2, 0);
            return;
        }
        if (!has_type(a, v)) {
            bad(a->type == 's'   ? "Argument `$1` must be a string."
                : a->type == 'n' ? "Argument `$1` must be a whole number."
                : a->type == 'b' ? "Argument `$1` must be true or false."
                                 : "Argument `$1` must be a list of strings.",
                a->name);
            return;
        }
    }
    static char w_vo[] = "vo", w_json[] = "--json";
    char *input = 0;
    long input_len = 0;
    ac = words_len = 0;
    call_bad = 0;
    push(w_vo), push(tool->name), push(w_json);
    for (int pass = 0; pass < 3; pass++) {
        for (const Arg *a = tool->args; a->name; a++) {
            v = js_get(args, a->name);
            if (!v || (a->type == 'l' && a->required && *js_ws(v + 1) == ']')) {
                if (!a->required) continue;
                bad("Missing argument `$1`.", a->name);
                return;
            }
            arg_words(a, v, pass, &input, &input_len);
            if (call_bad) {
                bad((char *)call_bad, a->name);
                return;
            }
        }
    }
    av[ac] = 0;
    run_call(input, input_len);
}

/* ---- the protocol's own methods ---- */

static void initialize(char *params)
{
    static char *const known[] = {"2025-06-18", "2025-03-26", "2024-11-05"};
    char *asked = js_get(params, "protocolVersion"), *version = known[0];
    for (int k = 0; k < 3; k++)
        if (asked && js_is(asked, known[k], 10)) version = known[k];
    long vn = 0;
    while (vo_version_text[3 + vn] != ' ') vn++; /* "vo X.Y.Z schema 1" */
    reply_begin();
    w("{\"protocolVersion\":\"");
    w(version);
    w("\",\"capabilities\":{\"tools\":{},\"resources\":{}},\"serverInfo\":{\"name\":\"vo\",\"version\":\"");
    w_raw((char *)vo_version_text + 3, vn);
    w("\"},\"instructions\":\"The vo toolchain. Read the resource vo://reference before writing vo: it is the "
      "whole language. Each tool is the `vo` command of the same name; a result holds the command's JSON lines "
      "(diagnostics carry a stable code and fixes), then its stderr if any, then one line with its exit "
      "status.\"}");
    reply_end();
    initialized = 1;
}

#define REF_URI "vo://reference"

static void resources_read(char *params)
{
    char *uri = js_get(params, "uri");
    if (!uri || *uri != '"') {
        bad("`resources/read` needs a `$1`.", "uri");
        return;
    }
    if (!js_is(uri, REF_URI, sizeof REF_URI - 1)) {
        reply_error(-32002, "Unknown resource `$1`. The only resource is " REF_URI ".", uri + 1,
                    js_skip(uri, 0) - uri - 2, 0);
        return;
    }
    reply_begin();
    w("{\"contents\":[{\"uri\":\"" REF_URI "\",\"mimeType\":\"text/markdown\",\"text\":");
    w_json_string((char *)ref_md, ref_md_len, 0);
    w("}]}");
    reply_end();
}

static void handle(char *line)
{
    char *p = js_ws(line), *end;
    id_len = 0;
    if (!*p) return; /* an empty line */
    if (!(end = js_skip(p, 0)) || *js_ws(end)) {
        reply_error(-32700, "The line is not one JSON value.", 0, 0, 1);
        return;
    }
    char *idv = js_get(p, "id"), *m = js_get(p, "method");
    if (*p != '{') {
        reply_error(-32600, "A message must be one JSON object (batches are not supported).", 0, 0, 1);
        return;
    }
    if (!m && (js_get(p, "result") || js_get(p, "error"))) return; /* an answer: the server asks nothing */
    long idn = idv ? js_skip(idv, 0) - idv : 0;
    if (!m || *m != '"' || (idv && ((*idv != '"' && *idv != '-' && !is_digit(*idv)) || idn > (long)sizeof id))) {
        reply_error(-32600, "A request needs a string `method` and a string or number `id` (at most 256 bytes).", 0,
                    0, 1);
        return;
    }
    if (!idv) return; /* a notification; a cancellation matters only during its call */
    mem_copy(id, idv, idn);
    id_len = idn;
    char *params = js_get(p, "params");
#define IS(name) js_is(m, name, sizeof name - 1)
    if (IS("ping")) {
        reply_begin();
        w("{}");
        reply_end();
    } else if (IS("initialize")) {
        initialize(params);
    } else if (!initialized) {
        reply_error(-32600, "The session has not been initialized: send `initialize` first.", 0, 0, 0);
    } else if (IS("tools/list")) {
        tools_list();
    } else if (IS("tools/call")) {
        tools_call(params);
    } else if (IS("resources/list")) {
        reply_begin();
        w("{\"resources\":[{\"uri\":\"" REF_URI "\",\"name\":\"reference\",\"title\":\"vo language reference\","
          "\"description\":\"The whole language and toolchain in one document; read it before writing vo.\","
          "\"mimeType\":\"text/markdown\"}]}");
        reply_end();
    } else if (IS("resources/read")) {
        resources_read(params);
    } else if (IS("resources/templates/list")) {
        reply_begin();
        w("{\"resourceTemplates\":[]}");
        reply_end();
    } else {
        reply_error(-32601, "Unknown method `$1`.", m + 1, js_skip(m, 0) - m - 2, 0);
    }
#undef IS
}

int cmd_mcp(void)
{
    /* SIGCHLD wakes the wait for a call through a descriptor; a write to a host that is gone is an
       error, not a signal. Calls get the mask the server was started with. */
    unsigned long mask = (1UL << (SIGCHLD - 1)) | (1UL << (SIGPIPE - 1)), chld = 1UL << (SIGCHLD - 1);
    vo_syscall4(SYS_rt_sigprocmask, SIG_BLOCK, (long)&mask, (long)&old_mask, 8);
    sig_fd = (int)vo_syscall4(SYS_signalfd4, -1, (long)&chld, 8, O_NONBLOCK | O_CLOEXEC);
    int skipping = 0; /* inside a line that is too long */
    while (!host_gone) {
        long nl = in_pos;
        while (nl < in_len && in[nl] != '\n') nl++;
        if (nl < in_len || (in_eof && in_len > in_pos)) { /* a line (the last may have no newline) */
            char *line = in + in_pos;
            in[nl] = 0;
            in_pos = nl < in_len ? nl + 1 : in_len;
            if (skipping) reply_error(-32600, "The message is larger than 4 MiB.", 0, 0, 1);
            else handle(line);
            skipping = 0;
            continue;
        }
        if (in_eof) break;
        if (in_len - in_pos == IN_CAP) { /* no newline in a full buffer: drop the line as it comes */
            skipping = 1;
            in_pos = in_len = 0;
        }
        in_more(1);
    }
    return 0;
}
