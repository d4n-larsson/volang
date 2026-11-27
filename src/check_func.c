/* Functions as values (research §4-§6, contracts/grammar-v2.md; ported from src/check_func.s,
 * spec 008).
 *
 * Anonymous functions are checked in place with their own frame: parameters first, then one slot
 * per captured `let` or parameter of an enclosing function (copied when the anonymous function is
 * created), then their own locals. A capture through several nested anonymous functions is
 * threaded through each of them. Capturing a `var` is E0505. Their effects are inferred: builtins
 * and called functions' declarations collect in the anonymous function's FX_DIRECT. */
#include "vo.h"

/* The checker state of the code enclosing an anonymous function, saved while it is checked. */
typedef struct Ctx {
    Node *node;
    Type *ret;
    long loops;
    long locals; /* the enclosing frame's local count */
    long broke;
    Node *fn;
    void *fx;
    Sym *scope;
    Node *stmt;
} Ctx;
CHECK_SIZE(Ctx, 72);

long ck_depth;      /* anonymous-function nesting of the code being checked */
static Vec *ck_ctx; /* contexts, one per enclosing anonymous function */
Node *ck_cur_stmt;  /* the statement being checked (E0505 fixes insert before it) */

/* The context of the anonymous function at nesting level (1 = outermost). */
void *ck_ctx_at(long level)
{
    return ck_ctx->data[level - 1].p;
}

/* Spec 059: whether an anonymous function writes a parameter or return type (the 0.4 form, E0342). */
int lambda_written(Node *node)
{
    if (node->c.n) return 1;
    Vec *params = node->b.v;
    for (long i = 0; i < params->n; i++)
        if (((Node *)vec_get(params, i))->b.n) return 1;
    return 0;
}

/* Spec 059: the expected function type an untyped anonymous function takes its types from, or 0. Another
 * number of parameters is E0301 naming both, and *bad is set (the function is then an error). */
static Type *lambda_expected(Node *node, Type *expected, int *bad)
{
    if (!expected || expected->kind != TY_FUNC) return 0;
    long want = expected->a.v->n, got = node->b.v->n;
    if (want == got) return expected;
    Str *w = msg(want == 1 ? "1 parameter" : "$1 parameters", int_to_str(want), 0, 0);
    Diag *d = ck_err("E0301", node->a.n, msg("Expected a function of $1 (`$2`), found one of $3.", w,
                                             ty_name(expected), int_to_str(got)));
    diag_set_ef(d, ty_name(expected), msg(got == 1 ? "a function of 1 parameter" : "a function of $1 parameters",
                                          int_to_str(got), 0, 0));
    *bad = 1;
    return 0;
}

/* Spec 059: E0343, an anonymous function with parameters and no expected type to take their types from.
 * As the whole initializer of an untyped `let`/`var`, the message names the binding. */
static void lambda_unknown(Node *node)
{
    Node *st = ck_cur_stmt;
    Node *name = st && st->kind == N_LET && st->c.n == node && !st->b.n ? st->a.n : 0;
    if (name)
        ck_err1("E0343", node->a.n, "Cannot tell the parameter types of this anonymous function: give `$1` a "
                "function type, `let $1: fn(...) -> ... = fn(...)`.", name->a.s);
    else
        ck_err("E0343", node->a.n, str_from_cstr("Cannot tell the parameter types of this anonymous function: "
                                                 "pass it where a function type is expected."));
}

/* Spec 059: E0413, an untyped anonymous function whose block body only returns a value (no comment in it,
 * which is a statement of the block): its one form is `=> expr`, and the fix writes it. 0.4 lambdas get
 * E0342 first; once their types are gone this applies in the next round of fixes. */
static void lambda_single_return(Node *node)
{
    Node *block = node->d.n;
    if ((node->flags & NF_ARROW) || block->a.v->n != 1) return;
    Node *ret = vec_get(block->a.v, 0);
    if (ret->kind != N_RETURN || !ret->a.n) return;
    Diag *d = diag_add("E0413", SEV_ERROR, (int)block->file, block->start, block->end,
                       str_from_cstr("A body that only returns a value is written `=> expr`."));
    Str *e = src_text(ret->a.n);
    SB *sb = sb_new();
    sb_cstr(sb, "=> ");
    sb_str(sb, e);
    diag_add_fix(d, (int)block->file, block->start, block->end, sb_finish(sb));
}

/* Spec 059: E0342 on an anonymous function that writes types (0.4 code), after its body is checked. With an
 * expected function type equal to the written types, the fix removes them; one that differs is today's
 * mismatch, reported by the caller, and no E0342 this round (FR-005). Without one, as the whole initializer
 * of an untyped `let`/`var`, the fix moves the types onto the binding; otherwise there is no fix. */
static void lambda_types_written(Node *node, Type *expected)
{
    Vec *params = node->b.v;
    Node *at = node->c.n;
    for (long i = params->n - 1; i >= 0; i--)
        if (((Node *)vec_get(params, i))->b.n) at = ((Node *)vec_get(params, i))->b.n;
    int fits = 0;
    if (expected) {
        if (expected->kind != TY_FUNC || expected->a.v->n != params->n) return;
        for (long i = 0; i < params->n; i++)
            if (((Node *)vec_get(params, i))->type != vec_get(expected->a.v, i)) return;
        Type *r = node->type; /* an open return (t_collect) comes from the body: only a type no body needs context for */
        if (expected->b.t == t_collect)
            fits = r->kind == TY_INT || r->kind == TY_FLOAT || r->kind == TY_BOOL || r->kind == TY_STR ||
                   r->kind == TY_BYTES || r->kind == TY_VOID ||
                   ((r->kind == TY_RECORD || r->kind == TY_VARIANT) && !(r->b.v && r->b.v->n));
        else if (expected->b.t != r)
            return;
        else
            fits = 1;
    }
    SB *h = sb_new(); /* the untyped header: (a, b) */
    sb_byte(h, '(');
    for (long i = 0; i < params->n; i++) {
        if (i) sb_cstr(h, ", ");
        sb_str(h, ((Node *)vec_get(params, i))->a.n->a.s);
    }
    sb_byte(h, ')');
    Str *untyped = sb_finish(h);
    char *text = file_get((int)node->file)->text;
    long he = node->c.n ? node->c.n->end : node->a.n->end; /* the header: to the return type, or the `)` */
    if (!node->c.n) /* (a `uses` clause after it is E0507's, not this fix's) */
        for (long depth = 0; he < node->d.n->start; he++) {
            depth += text[he] == '(';
            if (text[he] == ')' && --depth == 0) {
                he++;
                break;
            }
        }
    Diag *d = ck_err("E0342", at, msg("An anonymous function takes its types from where it is used: write `fn$1`.",
                                      untyped, 0, 0));
    if (fits) {
        diag_add_fix(d, (int)node->file, node->a.n->end, he, untyped);
        return;
    }
    Node *st = ck_cur_stmt;
    if (!st || st->kind != N_LET || st->c.n != node || st->b.n || !st->a.n) return;
    for (long i = 0; i < params->n; i++) /* a type with an error in it is not one to write down */
        if (ty_bad(((Node *)vec_get(params, i))->type)) return;
    if (ty_bad(node->type)) return;
    SB *r = sb_new(); /* `f: fn(int) -> int = fn(x)` */
    sb_cstr(r, ": ");
    sb_str(r, ty_name(ty_func_of(node)));
    sb_cstr(r, " = fn");
    sb_str(r, untyped);
    diag_add_fix(d, (int)node->file, st->a.n->end, he, sb_finish(r));
}

/* Checks an anonymous function; returns its function type. Without written types (spec 059) its
 * parameter and return types come from `expected`; an expected return of t_collect (a call that leaves
 * it open, research R3) or no expected type: the return type comes from the body (R4). */
Type *check_lambda(Node *node, Type *expected)
{
    int written = lambda_written(node), bad = 0;
    Type *exp = written ? 0 : lambda_expected(node, expected, &bad);
    int collect = !written && (!exp || exp->b.t == t_collect);
    if (!written && !exp && !bad && node->b.v->n && !(expected && ty_bad(expected))) lambda_unknown(node);
    Ctx *cx = alloc(sizeof(Ctx)); /* save the enclosing context */
    cx->node = node;
    cx->ret = ck_ret;
    cx->loops = ck_loops;
    cx->locals = ck_locals;
    cx->broke = ck_broke;
    cx->fn = ck_fn;
    cx->scope = ck_scope;
    cx->stmt = ck_cur_stmt;
    if (!ck_ctx) ck_ctx = vec_new();
    vec_push(ck_ctx, cx);
    ck_depth++;
    Fx *fx = item_fx(node); /* a fresh effect record and capture list */
    fx->direct = es_new(0);
    fx->caps = vec_new();
    fx->badcaps = vec_new();
    cx->fx = fx_swap(fx);
    ck_locals = 0;
    ck_loops = 0;
    ck_broke = 0;
    ck_fn = node;
    Vec *params = node->b.v; /* parameters are the first locals */
    for (long i = 0; i < params->n; i++) {
        Node *p = vec_get(params, i);
        p->type = p->b.n ? resolve_type(p->b.n) : exp ? vec_get(exp->a.v, i) : t_error;
        if (!p->a.n) continue;
        Sym *s = new_local(p->a.n, p->type, 0);
        p->e.i = s->slot;
        s->flags |= SYF_USED;
    }
    if (collect)
        node->type = t_collect;
    else /* written (none: void), or the expected one */
        node->type = written ? (node->c.n ? resolve_type(node->c.n) : t_void) : exp->b.t;
    ck_ret = node->type;
    if (node->d.n) {
        if (!written) lambda_single_return(node);
        check_block(node->d.n);
        if (collect) node->type = ck_ret == t_collect ? t_void : ck_ret; /* no value returned: void */
        long k = node->type->kind; /* E0403, as for named functions */
        if (k != TY_VOID && k != TY_ERROR && !terminates(node->d.n))
            ck_err1("E0403", node->a.n, "`$1` can reach its end without returning a value.", node->a.n->a.s);
    }
    ck_cur_stmt = cx->stmt; /* the statement holding this function (E0505 fixes go before it) */
    if (written) lambda_types_written(node, expected && !ty_bad(expected) ? expected : 0);
    report_bad_captures(node);
    node->e.i = ck_locals; /* this frame's size; then restore the enclosing context */
    scope_leave(cx->scope);
    ck_ret = cx->ret;
    ck_loops = cx->loops;
    ck_locals = cx->locals;
    ck_broke = cx->broke;
    ck_fn = cx->fn;
    ck_cur_stmt = cx->stmt;
    fx_swap(cx->fx);
    ck_depth--;
    ck_ctx->n--;
    return bad ? t_error : ty_func_of(node);
}

/* The slot of a local in the code being checked: its own slot, or a capture slot when it was
 * defined outside the innermost anonymous function. A captured `var` is recorded for E0505. */
long ck_capture(Sym *sym, Node *ident)
{
    long depth = ck_depth;
    if (sym->effects.i >= depth) return sym->slot; /* SY_DEPTH */
    if (sym->flags & SYF_MUT) { /* a var: remember the use in the innermost anonymous function */
        Ctx *cx = ck_ctx_at(depth);
        vec_push(((Fx *)item_fx(cx->node))->badcaps, ident);
        return sym->slot;
    }
    return ck_capture_at(sym, depth);
}

/* The capture slot of sym in the anonymous function at level, created (with its enclosing
 * levels') on first use. */
long ck_capture_at(Sym *sym, long level)
{
    Ctx *cx = ck_ctx_at(level);
    Vec *caps = ((Fx *)item_fx(cx->node))->caps;
    for (long i = 0; i < caps->n; i++) {
        Capture *c = vec_get(caps, i);
        if (c->sym == sym) return c->inner;
    }
    long outer = sym->slot; /* the slot one level out */
    if (sym->effects.i != level - 1) outer = ck_capture_at(sym, level - 1);
    /* this level's next local: live, or saved by the next level */
    long *next = level == ck_depth ? &ck_locals : &((Ctx *)ck_ctx_at(level + 1))->locals;
    long inner = (*next)++;
    Capture *c = alloc(CP_SIZE);
    c->sym = sym;
    c->outer = outer;
    c->inner = inner;
    vec_push(caps, c);
    return inner;
}

/* E0505 once per captured `var`, with a fix that copies it into a `let` before the statement and
 * renames its uses inside the anonymous function. */
void report_bad_captures(Node *lambda)
{
    Vec *bad = ((Fx *)item_fx(lambda))->badcaps;
    for (long i = 0; i < bad->n; i++) {
        Node *use = vec_get(bad, i); /* an identifier using a var */
        Sym *sym = use->b.sym;
        int reported = 0; /* an earlier use of the same var */
        for (long j = 0; j < i && !reported; j++) reported = bad->data[j].n->b.sym == sym;
        if (reported) continue;
        Diag *d = ck_err1("E0505", use,
                          "`$1` is a `var`: anonymous functions cannot capture variables that change. Copy it "
                          "into a `let` first.",
                          use->a.s);
        Node *st = ck_cur_stmt; /* the fix: `let x_now = x;` + the text up to the function's end */
        if (!st) continue;
        char *text = file_get((int)st->file)->text;
        SB *sb = sb_new();
        sb_cstr(sb, "let ");
        sb_str(sb, use->a.s);
        sb_cstr(sb, "_now = ");
        sb_str(sb, use->a.s);
        sb_cstr(sb, ";\n");
        long start = st->start, line = start; /* the statement's indentation */
        while (line && text[line - 1] != '\n') line--;
        sb_bytes(sb, text + line, start - line);
        long at = st->start; /* copy up to the function's end, renaming the var's uses */
        for (long k = 0; k < bad->n; k++) {
            Node *u = vec_get(bad, k);
            if (u->b.sym != sym || u->start < at) continue;
            sb_bytes(sb, text + at, u->start - at);
            at = u->end;
            sb_str(sb, u->a.s);
            sb_cstr(sb, "_now");
        }
        sb_bytes(sb, text + at, lambda->end - at);
        diag_add_fix(d, (int)st->file, st->start, lambda->end, sb_finish(sb));
    }
}

/* How a call names its callee in messages: the name, or the callee's source text. */
Str *call_name(Node *call)
{
    Node *callee = call->a.n;
    if (call->kind == N_CALL || callee->kind == N_IDENT) return callee->a.s;
    return src_text(callee);
}

/* A call of a function value: `f(x)`, `make()(x)`, `fs[0](x)`, `r.handler(x)`. */
Type *check_call_value(Node *node, Type *expected)
{
    if (node->flags & NF_METHOD) return check_method_form(node, expected);
    Type *ct = check_expr(node->a.n, 0); /* the callee's type */
    if (!ty_bad(ct)) {
        if (ct->kind == TY_FUNC) {
            node->c.i = CV_CLOSURE;
            if (check_argc(node, ct->a.v->n))
                for (long i = 0; i < ct->a.v->n; i++) check_arg(node, i, vec_get(ct->a.v, i));
            fx_value_call(ct->decl.p);
            return ct->b.t;
        }
        ck_err1("E0303", node->a.n, "`$1` is not a function.", src_text(node->a.n));
    }
    long n = node->b.v->n; /* still check the arguments */
    for (long i = 0; i < n; i++) check_arg(node, i, 0);
    return t_error;
}

/* `obj.name(args)`: a call of a record field of function type, or else v1's E0110 ("vo has no
 * methods", fixed as `name(obj, args)`) and the rewritten call is checked instead. */
Type *check_method_form(Node *node, Type *expected)
{
    Node *field = node->a.n; /* the N_FIELD callee */
    Node *obj = field->a.n;
    if (obj->kind == N_IDENT && !scope_find(obj->a.s) && missing_module_import(obj->a.s, obj)) {
        for (long i = 0; i < node->b.v->n; i++) check_arg(node, i, 0); /* spec 058: `json.parse_json(...)`
                                                                           without the import: E0204 */
        return t_error;
    }
    diag_mute = 1;           /* a silent look at the object's type */
    Type *ot = check_expr(field->a.n, 0);
    diag_mute = 0;
    if (ot->kind == TY_RECORD) { /* a field with this name and a function type? */
        Vec *members = ot->decl.n->b.v;
        long idx = member_index(members, field->b.n->a.s);
        if (idx >= 0) {
            Node *tn = member_at(members, idx)->b.n;
            if (tn && tn->type && tn->type->kind == TY_FUNC) { /* a field call */
                node->flags &= (uint16_t)~NF_METHOD;
                return check_call_value(node, expected);
            }
        }
    }
    W *e = node->e.p; /* the parser's E0110 and its fix: msg, start, end, fix text, file */
    Diag *d = diag_add("E0110", SEV_ERROR, (int)e[4].i, e[1].i, e[2].i, e[0].s);
    diag_add_fix(d, (int)e[4].i, e[1].i, e[2].i, e[3].s);
    return check_call(node->d.n, expected);
}

/* Spec 016 (E0314): where t holds a function type ("field `on_done` of `Job`"), or 0. Each record and
 * variant is entered once (seen), so recursive types end. */
static Str *task_func_path(Type *t, Vec *seen)
{
    if (!t) return 0;
    switch (t->kind) {
    case TY_FUNC: return str_from_cstr("the result itself");
    case TY_LIST:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE:
        if (task_func_path(t->a.t, seen)) return str_from_cstr(t->kind == TY_LIST ? "list element" : "collection element");
        return 0;
    case TY_MAP:
    case TY_OMAP:
        if (task_func_path(t->a.t, seen) || task_func_path(t->b.t, seen)) return str_from_cstr("map value");
        return 0;
    case TY_RESULT: return task_func_path(t->a.t, seen) ? str_from_cstr("`ok` value") : 0;
    case TY_RECORD:
    case TY_VARIANT:
        for (long i = 0; i < seen->n; i++)
            if (vec_get(seen, i) == t) return 0;
        if (seen->n >= 64) return 0; /* `more(N[list[T]])`: every level is a new instance, without end */
        vec_push(seen, t);
        for (long i = 0;; i++) {
            Node *m = member_at(t->decl.n->b.v, i);
            if (!m) return 0;
            Type *mt = t->kind == TY_RECORD ? field_type_in(t, m->b.n->type) : case_payload(t, i);
            if (!mt) continue;
            Str *inner = task_func_path(mt, seen);
            if (!inner) continue;
            if (mt->kind != TY_FUNC) return inner; /* the innermost place names it */
            return msg(t->kind == TY_RECORD ? "field `$1` of `$2`" : "case `$1` of `$2`", m->a.n->a.s, ty_name(t), 0);
        }
    default: return 0;
    }
}

/* Spec 059: checks function argument i of a builtin and returns its own type. An anonymous function gets
 * fn(params) -> ret as its expected type (ret t_collect: its return type comes from its body, research
 * R3); any other argument is checked as before, without one. The caller compares the type it needs. */
static Type *check_fn_arg(Node *call, long i, Vec *params, Type *ret)
{
    Node *arg = call->b.v->data[i].n;
    int known = ret && !ty_bad(ret);
    for (long k = 0; k < params->n; k++)
        if (ty_bad(vec_get(params, k))) known = 0;
    if (arg->kind != N_LAMBDA || !known) return check_arg(call, i, 0);
    return check_expr(arg, ty_func(params, ret, es_new(0)));
}

/* sort_by, map, filter, fold, any, all: a list, (fold: a start value,) and a function of its
 * elements; concurrent_map (spec 016): a list, the limit and a function. The function's effects
 * become the caller's. */
/* Spec 032: map_err(r: result[T, E1], f: fn(E1) -> E2) -> result[T, E2]; f's effects are the caller's. */
Type *check_map_err(Node *call)
{
    if (!check_argc(call, 2)) return t_error;
    Type *rt = check_arg(call, 0, 0);
    if (ty_bad(rt)) return t_error;
    if (rt->kind != TY_RESULT) {
        ck_err1("E0301", call->b.v->data[0].n, "Expected a `result`, found `$1`.", ty_name(rt));
        return t_error;
    }
    Vec *params = vec_new();
    vec_push(params, err_type(rt));
    Type *ft = check_fn_arg(call, 1, params, t_collect); /* spec 059: fn(E1) -> (from the body) */
    if (ty_bad(ft)) return t_error;
    Type *ret = ft->kind == TY_FUNC ? ft->b.t : t_error;
    EffSet *eff = ft->kind == TY_FUNC ? ft->decl.es : 0;
    if (!eff) eff = es_new(0);
    Type *want = ty_func(params, ret, eff);
    if (want != ft) {
        ck_mismatch(vec_get(call->b.v, 1), want, ft);
        return t_error;
    }
    fx_value_call(ft->decl.p);
    if (ret->kind == TY_VOID || ty_has_func(ret)) {
        ck_err1("E0334", call->b.v->data[1].n,
                "The error of a `result` is a value the caller can read and print: `$1` cannot be one.", ty_name(ret));
        return t_error;
    }
    return ty_result_e(rt->a.t, ret);
}

Type *check_hof(Node *call, long id)
{
    Type *bool_or_bad = (id == B_ANY || id == B_ALL) ? t_bool : t_error; /* when the call is wrong */
    if (!check_argc(call, id == B_FOLD || id == B_CMAP ? 3 : 2)) return bool_or_bad;
    Type *lt = check_arg(call, 0, 0); /* the list */
    if (ty_bad(lt)) return bool_or_bad;
    if (lt->kind != TY_LIST) {
        ck_err1("E0301", call->b.v->data[0].n, "Expected a list, found `$1`.", ty_name(lt));
        return bool_or_bad;
    }
    Type *elem = lt->a.t; /* T */
    Type *acc = 0;        /* fold: A */
    long fi = 1;          /* the function's argument index */
    if (id == B_FOLD) {
        acc = check_arg(call, 1, 0);
        fi = 2;
    }
    if (id == B_CMAP) {
        check_arg(call, 1, t_int); /* the limit */
        fi = 2;
    }
    Vec *params = vec_new(); /* the parameters it must take: (T) or (A, T) */
    if (acc) vec_push(params, acc);
    vec_push(params, elem);
    Type *fret = id == B_FOLD ? acc : id == B_FILTER || id == B_ANY || id == B_ALL ? t_bool : t_collect;
    Type *ft = check_fn_arg(call, fi, params, fret); /* the function (spec 059: R3) */
    if (ty_bad(ft)) return bool_or_bad;
    Type *ret = ft->kind == TY_FUNC ? ft->b.t : t_error; /* the return type it must have */
    if (id == B_FOLD) ret = acc;
    if (id == B_FILTER || id == B_ANY || id == B_ALL) ret = t_bool;
    EffSet *eff = ft->kind == TY_FUNC ? ft->decl.es : 0; /* with the argument's own effects */
    if (!eff) eff = es_new(0);
    Type *want = ty_func(params, ret, eff);
    if (want != ft) {
        ck_mismatch(vec_get(call->b.v, fi), want, ft);
        return bool_or_bad;
    }
    fx_value_call(ft->decl.p);
    switch (id) { /* the result */
    case B_SORT_BY: {
        Type *key = ft->b.t; /* the key must be ordered (spec 014: or a record of ordered fields) */
        if (key->kind == TY_RECORD && !ty_ordered(key))
            order_error(key, call->b.v->data[1].n, 0);
        else if (key->kind != TY_INT && key->kind != TY_FLOAT && key->kind != TY_STR && key->kind != TY_BYTES &&
                 key->kind != TY_RECORD &&
                 !(key->kind == TY_PARAM && key->b.i == TPC_ORDERED)) /* spec 046: an `ordered` type parameter */
            ck_err1("E0313", call->b.v->data[1].n, "`$1` is not ordered: a sort key must be `int`, `float` or `str`.",
                    ty_name(key));
        return t_void;
    }
    case B_MAP: return ty_list(ft->b.t);
    case B_CMAP: { /* spec 016: what cannot cross to the parent, and stdin */
        Node *fn_arg = call->b.v->data[2].n;
        Str *where = task_func_path(ft->b.t, vec_new());
        if (where) ck_err1("E0314", fn_arg, "A task's result cannot hold a function ($1): return data instead.", where);
        if (eff->mask & EF_INPUT)
            ck_err("E0511", fn_arg,
                   str_from_cstr("Tasks cannot read stdin: read it before `concurrent_map` and pass the text in."));
        return ty_list(ft->b.t);
    }
    case B_FILTER: return lt;
    case B_FOLD: return ft->b.t;
    default: return t_bool; /* any / all */
    }
}
