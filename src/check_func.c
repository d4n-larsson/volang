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

/* Checks an anonymous function; returns its function type. */
Type *check_lambda(Node *node)
{
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
        p->type = resolve_type(p->b.n);
        if (!p->a.n) continue;
        Sym *s = new_local(p->a.n, p->type, 0);
        p->e.i = s->slot;
        s->flags = SYF_USED;
    }
    node->type = node->c.n ? resolve_type(node->c.n) : t_void; /* the return type (none: void) */
    ck_ret = node->type;
    if (node->d.n) {
        check_block(node->d.n);
        long k = node->type->kind; /* E0403, as for named functions */
        if (k != TY_VOID && k != TY_ERROR && !terminates(node->d.n))
            ck_err1("E0403", node->a.n, "`$1` can reach its end without returning a value.", node->a.n->a.s);
    }
    ck_cur_stmt = cx->stmt; /* the statement holding this function (E0505 fixes go before it) */
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
    return ty_func_of(node);
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

/* sort_by, map, filter, fold, any, all: a list, (fold: a start value,) and a function of its
 * elements. The function's effects become the caller's. */
Type *check_hof(Node *call, long id)
{
    Type *bool_or_bad = (id == B_ANY || id == B_ALL) ? t_bool : t_error; /* when the call is wrong */
    if (!check_argc(call, id == B_FOLD ? 3 : 2)) return bool_or_bad;
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
    Type *ft = check_arg(call, fi, 0); /* the function */
    if (ty_bad(ft)) return bool_or_bad;
    Vec *params = vec_new(); /* the parameters it must take: (T) or (A, T) */
    if (acc) vec_push(params, acc);
    vec_push(params, elem);
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
        else if (key->kind != TY_INT && key->kind != TY_FLOAT && key->kind != TY_STR && key->kind != TY_RECORD)
            ck_err1("E0313", call->b.v->data[1].n, "`$1` is not ordered: a sort key must be `int`, `float` or `str`.",
                    ty_name(key));
        return t_void;
    }
    case B_MAP: return ty_list(ft->b.t);
    case B_FILTER: return lt;
    case B_FOLD: return ft->b.t;
    default: return t_bool; /* any / all */
    }
}
