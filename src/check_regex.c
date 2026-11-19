/* Checker, spec 025: calls of std.regex with literal patterns and replacements (research R5).
 * `regex(LITERAL)`: the pattern is compiled by the engine's parser (src/regex.c) and every error is
 * E0319 or E0320 at the source bytes it names; any other argument is E0321. `replace_all(re, s,
 * LITERAL)`: the replacement's `$` references are E0322, checked against the pattern's groups when
 * `re` is `regex(LITERAL)` or a `let` bound to one; Python's `\1` and `\g<name>` get fixes. */
#include "vo.h"

/* Whether fn declaration decl is the standard library's std.regex function name. */
static int is_regex_fn(Node *decl, char *name)
{
    if (!decl || decl->kind != N_FN) return 0;
    FileRec *f = file_get((int)decl->file);
    Str *p = f->path;
    if (!f->std || p->len < 8 || !mem_eq(p->data + p->len - 8, "regex.vo", 8)) return 0;
    return str_eq_cstr(decl->a.n->a.s, name);
}

/* Whether an E0103 (a lexer error) lies inside lit: its value is not what was meant. */
static int has_lex_error(Node *lit)
{
    for (long i = 0; i < diag_n; i++) {
        Diag *d = diag_arr[i];
        if (d->file == lit->file && d->start >= lit->start && d->end <= lit->end && d->code[0] == 'E' &&
            d->code[1] == '0' && d->code[2] == '1' && d->code[3] == '0' && d->code[4] == '3')
            return 1;
    }
    return 0;
}

/* The pattern literal of a regex(...) call, or of the `let` initializer that ident names; 0 if unknown. */
static Node *pattern_literal(Node *e)
{
    if (e->kind == N_IDENT) {
        Sym *s = scope_find(e->a.s);
        if (!s || s->kind != SYM_LOCAL || (s->flags & SYF_MUT) || !s->decl || s->decl->kind != N_LET) return 0;
        e = s->decl->c.n;
        if (!e) return 0;
    }
    if (e->kind != N_CALL || !is_regex_fn(e->c.n, "regex") || e->b.v->n != 1) return 0;
    Node *lit = vec_get(e->b.v, 0);
    return lit->kind == N_STR && !has_lex_error(lit) ? lit : 0;
}

static void check_pattern(Node *call)
{
    Node *lit = vec_get(call->b.v, 0);
    if (lit->kind != N_STR) {
        ck_err("E0321", lit, str_from_cstr("`regex` needs a string literal, so the pattern can be checked; use "
                                           "`parse_regex(s)` for a pattern built at run time."));
        return;
    }
    if (has_lex_error(lit)) return;
    char *buf;
    long *offs;
    long n = lit_decode(lit, &buf, &offs);
    if (n < 0) return;
    ReErr e;
    if (re_check(buf, n, &e, 0) >= 0) return;
    Diag *d = diag_add(e.code, SEV_ERROR, (int)lit->file, offs[e.start], offs[e.end], re_err_text(buf, &e));
    if (e.fix) diag_add_fix(d, (int)lit->file, offs[e.start], offs[e.end], str_from_cstr(e.fix));
}

static void check_replacement(Node *call)
{
    Node *lit = vec_get(call->b.v, 2);
    if (lit->kind != N_STR || has_lex_error(lit)) return;
    char *r;
    long *ro;
    long rn = lit_decode(lit, &r, &ro);
    if (rn < 0) return;
    int file = (int)lit->file, habit = 0;
    for (long i = 0; i + 1 < rn; i++) { /* Python's \1 and \g<name> */
        if (r[i] != '\\') continue;
        long end = 0;
        SB *fix = sb_new();
        if (r[i + 1] >= '1' && r[i + 1] <= '9') {
            end = i + 2;
            sb_byte(fix, '$');
            sb_byte(fix, r[i + 1]);
        } else if (r[i + 1] == 'g' && i + 2 < rn && r[i + 2] == '<') {
            long j = i + 3;
            while (j < rn && r[j] != '>') j++;
            if (j >= rn) continue;
            end = j + 1;
            sb_cstr(fix, "${");
            sb_bytes(fix, r + i + 3, j - i - 3);
            sb_byte(fix, '}');
        } else {
            continue;
        }
        Diag *d = diag_add("E0322", SEV_ERROR, file, ro[i], ro[end], str_from_cstr("Group references are written `$1` or `${name}`."));
        diag_add_fix(d, file, ro[i], ro[end], sb_finish(fix));
        habit = 1;
        i = end - 1;
    }
    if (habit) return;
    long ngroups = -1;
    Str *names = 0;
    Node *plit = pattern_literal(vec_get(call->b.v, 0));
    if (plit) {
        char *p;
        long *po;
        long pn = lit_decode(plit, &p, &po);
        ReErr pe;
        long g = pn >= 0 ? re_check(p, pn, &pe, &names) : -1;
        if (g >= 0) ngroups = g + 1;
    }
    ReErr e;
    if (!re_repl_parse(r, rn, ngroups, names ? names->data : 0, names ? names->len : 0, &e, 0))
        diag_add("E0322", SEV_ERROR, file, ro[e.start], ro[e.end], e.msg);
}

void check_regex_call(Node *call)
{
    Node *decl = call->c.n;
    if (is_regex_fn(decl, "regex") && call->b.v->n == 1)
        check_pattern(call);
    else if (is_regex_fn(decl, "replace_all") && call->b.v->n == 3)
        check_replacement(call);
}
