/* Checker, spec 026: calls of std.sqlite with literal SQL (research R9). `sql(LITERAL)`: the SQL is
 * scanned (one statement, `?` parameters, no 'now') and prepared against an empty in-memory database
 * (src/sqlite_rt.c, sql_check_text); errors about the text itself are E0323 at the bytes SQLite
 * names, named or numbered parameters E0325 with the fix to `?`; schema errors are left to run time
 * (the database is not there yet). Any other argument is E0324. `exec`/`query` with a parameter list
 * written in the call: its length against the statement's `?` count (E0325). `open(":memory:")`:
 * E0326 with the fix `ok(open_memory())`. */
#include "vo.h"

/* Whether fn declaration decl is the standard library's std.sqlite function name. */
static int is_sqlite_fn(Node *decl, char *name)
{
    if (!decl || decl->kind != N_FN) return 0;
    FileRec *f = file_get((int)decl->file);
    Str *p = f->path;
    if (!f->std || p->len < 9 || !mem_eq(p->data + p->len - 9, "sqlite.vo", 9)) return 0;
    return str_eq_cstr(decl->a.n->a.s, name);
}

/* Whether an E0103 (a lexer error) lies inside lit. */
static int has_lex_error(Node *lit)
{
    for (long i = 0; i < diag_n; i++) {
        Diag *d = diag_arr[i];
        const char *c = d->code;
        if (d->file == lit->file && d->start >= lit->start && d->end <= lit->end && c[2] == '1' && c[3] == '0' &&
            c[4] == '3')
            return 1;
    }
    return 0;
}

/* The end of the token starting at decoded byte at (a word, or one character). */
static long token_end(char *s, long n, long at)
{
    long e = at;
    while (e < n && (((s[e] | 0x20) >= 'a' && (s[e] | 0x20) <= 'z') || (s[e] >= '0' && s[e] <= '9') || s[e] == '_' ||
                     (unsigned char)s[e] >= 0x80))
        e++;
    return e > at ? e : (at < n ? at + 1 : at);
}

static void check_sql(Node *call)
{
    Node *lit = vec_get(call->b.v, 0);
    if (lit->kind != N_STR) {
        ck_err("E0324", lit, str_from_cstr("Statements are written as literals, so values can only be parameters (no SQL "
                                           "injection); use `?` for values, or `parse_sql(s)` for SQL that must be built "
                                           "at run time."));
        return;
    }
    if (has_lex_error(lit)) return;
    char *buf;
    long *offs;
    long n = lit_decode(lit, &buf, &offs);
    if (n < 0) return;
    int file = (int)lit->file;
    SqlScan sc;
    sql_scan(buf, n, &sc);
    if (sc.named_at >= 0 && sc.second_at < 0) { /* named or numbered parameters: E0325, fixed when each is used once */
        Diag *d = diag_add("E0325", SEV_ERROR, file, offs[sc.named_at], offs[sc.named_at + sc.named_len],
                           msg("Parameters are `?`, taken from the list in order; `$1` is a second spelling.",
                               str_new(buf + sc.named_at, sc.named_len), 0, 0));
        int unique = sc.named_count <= 32;
        for (long i = 0; unique && i < sc.named_count && i < 32; i++) {
            char *a = buf + sc.named_starts[i];
            long la = sc.named_lens[i];
            for (long j = 0; j < i; j++)
                if (sc.named_lens[j] == la && mem_eq(buf + sc.named_starts[j], a, la)) unique = 0;
            if (a[0] == '?') { /* numbered: in order 1, 2, ... */
                long v = 0;
                for (long k = 1; k < la; k++) v = v * 10 + (a[k] - '0');
                if (v != i + 1) unique = 0;
            }
        }
        if (unique && !sc.qmarks)
            for (long i = 0; i < sc.named_count; i++)
                diag_add_fix(d, file, offs[sc.named_starts[i]], offs[sc.named_starts[i] + sc.named_lens[i]],
                             str_from_cstr("?"));
        return;
    }
    long at;
    char *m = sql_check_text(buf, n, &at);
    if (!m) return;
    long s = at >= 0 && at <= n ? at : 0;
    long e = at >= 0 && at <= n ? token_end(buf, n, s) : n;
    if (sc.second_at >= 0 && at == sc.second_at) e = n; /* the second statement, to the end */
    if (sc.now_at >= 0 && at == sc.now_at) e = at + 5; /* 'now' */
    diag_add("E0323", SEV_ERROR, file, offs[s], offs[e], str_from_cstr(m));
}

/* The `?` count of a statement argument written as sql(LITERAL), or -1. */
static long literal_qmarks(Node *arg)
{
    if (arg->kind != N_CALL || !is_sqlite_fn(arg->c.n, "sql") || arg->b.v->n != 1) return -1;
    Node *lit = vec_get(arg->b.v, 0);
    if (lit->kind != N_STR || has_lex_error(lit)) return -1;
    char *buf;
    long *offs;
    long n = lit_decode(lit, &buf, &offs);
    if (n < 0) return -1;
    SqlScan sc;
    sql_scan(buf, n, &sc);
    return sc.named_at >= 0 || sc.second_at >= 0 ? -1 : sc.qmarks;
}

static void check_params(Node *call)
{
    long want = literal_qmarks(vec_get(call->b.v, 1));
    Node *list = vec_get(call->b.v, 2);
    long got = list->kind == N_LIST ? list->a.v->n : list->kind == N_NEW_LIST ? 0 : -1;
    if (want < 0 || got < 0 || want == got) return;
    ck_err("E0325", list, msg("The statement has $1 parameter(s) and the list has $2.", int_to_str(want), int_to_str(got), 0));
}

static void check_open(Node *call)
{
    Node *arg = vec_get(call->b.v, 0);
    if (arg->kind != N_STR || arg->a.s->len != 8 || !mem_eq(arg->a.s->data, ":memory:", 8)) return;
    Diag *d = ck_err("E0326", arg, str_from_cstr("An in-memory database needs no file and no effect: use `open_memory()`."));
    diag_add_fix(d, (int)call->file, call->start, call->end, str_from_cstr("ok(open_memory())"));
}

void check_sqlite_call(Node *call)
{
    Node *decl = call->c.n;
    long n = call->b.v->n;
    if (is_sqlite_fn(decl, "sql") && n == 1)
        check_sql(call);
    else if ((is_sqlite_fn(decl, "exec") || is_sqlite_fn(decl, "query")) && n == 3)
        check_params(call);
    else if ((is_sqlite_fn(decl, "open") || is_sqlite_fn(decl, "open_readonly")) && n == 1)
        check_open(call);
}
