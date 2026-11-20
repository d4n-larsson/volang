/* Checker, spec 029: std.web routes (research R6). For each `on_get(...)`-style call: the pattern must be a
 * string literal of literal segments and {variables} (E0330 with the position), each variable once, and
 * each a field of the operation's input record with a type a path can give (scalar or enum-like variant).
 * For the routes written in one list literal: two routes of one method whose patterns match the same
 * paths are E0331 (literal segments beat variables, so only patterns of the same shape collide). */
#include "vo.h"

Vec *web_inputs; /* the input records of the program's std.web operations (record decls) */

static char *const route_fns[] = {"on_get", "on_post", "on_put", "on_patch", "on_delete", 0};
static char *const route_methods[] = {"GET", "POST", "PUT", "PATCH", "DELETE", 0};

/* The index of the std.web route function decl is, or -1. */
static int route_fn(Node *decl)
{
    if (!decl || decl->kind != N_FN) return -1;
    FileRec *f = file_get((int)decl->file);
    Str *p = f->path;
    if (!f->std || p->len < 6 || !mem_eq(p->data + p->len - 6, "web.vo", 6)) return -1;
    for (int i = 0; route_fns[i]; i++)
        if (str_eq_cstr(decl->a.n->a.s, route_fns[i])) return i;
    return -1;
}

static int seg_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
           c == '~';
}

static void bad_at(Node *lit, long at, long to, char *m)
{
    long base = lit->start + 1; /* (patterns hold no escapes: offsets map to source bytes) */
    diag_add("E0330", SEV_ERROR, (int)lit->file, base + at, base + (to > at ? to : at + 1), str_from_cstr(m));
}

/* The input record type of the call's operation, or 0. */
static Type *input_type(Node *call)
{
    if (call->b.v->n != 2) return 0;
    Type *ft = ((Node *)vec_get(call->b.v, 1))->type;
    if (!ft || ft->kind != TY_FUNC || ft->a.v->n != 2) return 0;
    Type *it = vec_get(ft->a.v, 1);
    return it && it->kind == TY_RECORD ? it : 0;
}

static int path_type_ok(Type *t)
{
    return t->kind == TY_STR || t->kind == TY_INT || t->kind == TY_FLOAT || t->kind == TY_BOOL || ty_enum_like(t);
}

void check_web_call(Node *call)
{
    if (route_fn(call->c.n) < 0 || call->b.v->n != 2) return;
    Node *lit = vec_get(call->b.v, 0);
    if (lit->kind != N_STR) {
        ck_err("E0330", lit, str_from_cstr("A route pattern must be a string literal, so it can be checked."));
        return;
    }
    Str *p = lit->a.s;
    char *s = p->data;
    long n = p->len;
    if (n != (long)(lit->end - lit->start - 2)) return; /* escapes in the literal: offsets would be off */
    if (!n || s[0] != '/') {
        bad_at(lit, 0, 1, "A route pattern starts with `/`.");
        return;
    }
    if (n > 1 && s[n - 1] == '/') {
        bad_at(lit, n - 1, n, "A route pattern does not end with `/` (except `/` itself).");
        return;
    }
    Type *in = input_type(call);
    if (in && web_inputs) vec_push(web_inputs, in->decl.n);
    Vec *seen = vec_new();
    for (long i = 1; i < n;) {
        long j = i;
        while (j < n && s[j] != '/') j++;
        if (j == i) {
            bad_at(lit, i, i + 1, "An empty segment (`//`) never matches.");
            return;
        }
        if (s[i] == '{') {
            if (s[j - 1] != '}' || j - i < 3) {
                bad_at(lit, i, j, "A variable is written `{name}`, a whole segment.");
                return;
            }
            Str *name = str_new(s + i + 1, j - i - 2);
            for (long k = 0; k < name->len; k++)
                if (!((name->data[k] >= 'a' && name->data[k] <= 'z') || name->data[k] == '_' || (k && name->data[k] >= '0' && name->data[k] <= '9'))) {
                    bad_at(lit, i, j, "A variable's name is lower-case letters, digits and `_`.");
                    return;
                }
            for (long k = 0; k < seen->n; k++)
                if (str_eq(vec_get(seen, k), name)) {
                    bad_at(lit, i, j, "This variable is already in the pattern.");
                    return;
                }
            vec_push(seen, name);
            if (in) {
                long f = member_index(in->decl.n->b.v, name);
                if (f < 0) {
                    diag_add("E0330", SEV_ERROR, (int)lit->file, lit->start + 1 + i, lit->start + 1 + j,
                             msg("The operation's input `$1` has no field `$2` for this variable.", ty_name(in), name, 0));
                } else {
                    Node *fd = member_at(in->decl.n->b.v, f);
                    Type *ft = fd->b.n ? fd->b.n->type : 0;
                    if (ft && !ty_bad(ft) && !path_type_ok(ft))
                        diag_add("E0330", SEV_ERROR, (int)lit->file, lit->start + 1 + i, lit->start + 1 + j,
                                 msg("`$1.$2` is a `$3`; a path variable gives a `str`, `int`, `float`, `bool` or enum-like case.",
                                     ty_name(in), name, ty_name(ft)));
                }
            }
        } else {
            for (long k = i; k < j; k++)
                if (!seg_char(s[k])) {
                    bad_at(lit, k, k + 1, "A literal segment holds letters, digits, `-`, `_`, `.` and `~` only.");
                    return;
                }
        }
        i = j + 1;
    }
}

/* Whether two literal patterns match the same paths: the same segments, variables anywhere the other has
 * a variable. */
static int same_shape(Str *a, Str *b)
{
    long i = 0, j = 0;
    for (;;) {
        long ie = i, je = j;
        while (ie < a->len && a->data[ie] != '/') ie++;
        while (je < b->len && b->data[je] != '/') je++;
        int av = ie > i && a->data[i] == '{', bv = je > j && b->data[j] == '{';
        if (av != bv) return 0;
        if (!av && (ie - i != je - j || !mem_eq(a->data + i, b->data + j, ie - i))) return 0;
        if (ie >= a->len || je >= b->len) return ie >= a->len && je >= b->len;
        i = ie + 1;
        j = je + 1;
    }
}

/* Whether an E0330 lies inside lit (a broken pattern takes part in no conflict: no cascading errors). */
static int pattern_broken(Node *lit)
{
    for (long i = 0; i < diag_n; i++) {
        Diag *d = diag_arr[i];
        if (d->file == lit->file && d->start >= lit->start && d->end <= lit->end && mem_eq((char *)d->code, "E0330", 5)) return 1;
    }
    return 0;
}

void check_web_routes(Node *list)
{
    Vec *items = list->a.v;
    for (long i = 0; i < items->n; i++) {
        Node *a = vec_get(items, i);
        int fa = a->kind == N_CALL ? route_fn(a->c.n) : -1;
        if (fa < 0 || a->b.v->n != 2 || ((Node *)vec_get(a->b.v, 0))->kind != N_STR) continue;
        for (long j = 0; j < i; j++) {
            Node *b = vec_get(items, j);
            int fb = b->kind == N_CALL ? route_fn(b->c.n) : -1;
            if (fb != fa || b->b.v->n != 2 || ((Node *)vec_get(b->b.v, 0))->kind != N_STR) continue;
            if (pattern_broken(vec_get(a->b.v, 0)) || pattern_broken(vec_get(b->b.v, 0))) continue;
            Str *pa = ((Node *)vec_get(a->b.v, 0))->a.s, *pb = ((Node *)vec_get(b->b.v, 0))->a.s;
            if (!same_shape(pa, pb)) continue;
            Diag *d = ck_err("E0331", a,
                             msg("`$1 $2` matches the same paths as `$1 $3`: every request would go to the first; make the "
                                 "patterns differ or remove one.",
                                 str_from_cstr(route_methods[fa]), pa, pb));
            diag_add_related(d, (int)b->file, b->start, b->end, msg("`$1 $2` is here.", str_from_cstr(route_methods[fa]), pb, 0));
            break;
        }
    }
}

/* After the program is checked: `@header` on a field of a record no operation takes is E0329. */
void check_web_headers(Vec *files)
{
    for (long i = 0; i < files->n; i++) {
        FileRec *f = file_get((int)(long)vec_get(files, i));
        if (f->std || !f->ast) continue;
        Vec *items = f->ast->a.v;
        for (long k = 0; k < items->n; k++) {
            Node *it = vec_get(items, k);
            if (it->kind != N_RECORD) continue;
            int used = 0;
            for (long u = 0; web_inputs && u < web_inputs->n; u++) used |= vec_get(web_inputs, u) == it;
            if (used) continue;
            for (long m = 0; m < it->b.v->n; m++) {
                Node *fd = vec_get(it->b.v, m);
                for (long a = 0; fd->kind == N_FIELD_DECL && fd->c.v && a < fd->c.v->n; a++) {
                    Node *an = vec_get(fd->c.v, a);
                    if (an->a.n && str_eq_cstr(an->a.n->a.s, "header"))
                        ck_err("E0329", an, msg("`@header` binds a request header, but no std.web operation takes `$1` as its input.", it->a.n->a.s, 0, 0));
                }
            }
        }
    }
}
