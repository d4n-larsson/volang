/* Formatter: AST -> canonical text (4-space indent, imports sorted first, minimal parentheses; ported
 * from src/fmt.s, spec 008). */
#include "vo.h"

static SB *fm_sb;     /* the output builder */
static char *fm_text; /* source text of the file being formatted */
static long fm_level; /* indentation level of the statement being printed */

static void fm_type(Node *t);
static void fm_expr(Node *n, long ctx);
static void fm_block(Node *block, long level);
static void fm_uses(Node *uses);

static void fm_c(const char *s)
{
    sb_cstr(fm_sb, (char *)s);
}

static void fm_s(Str *s)
{
    sb_str(fm_sb, s);
}

static void fm_b(int byte)
{
    sb_byte(fm_sb, byte);
}

static void fm_indent(long level)
{
    for (; level > 0; level--) fm_c("    ");
}

static void fm_name(Node *name)
{
    if (name) fm_s(name->a.s);
}

/* The node's source text (comments). */
static void fm_source(Node *n)
{
    sb_bytes(fm_sb, fm_text + n->start, (long)n->end - (long)n->start);
}

/* `fn(int, str) -> bool uses console`. */
static void fm_fn_type(Node *t)
{
    fm_c("fn(");
    for (long i = 0; i < t->b.v->n; i++) {
        if (i) fm_c(", ");
        fm_type(vec_get(t->b.v, i));
    }
    fm_b(')');
    if (t->c.n) {
        fm_c(" -> ");
        fm_type(t->c.n);
    }
    fm_uses(t->d.n);
}

/* `fn(x: int) -> int {` + the body one level deeper than the statement it is in + `}`. */
static void fm_lambda(Node *f)
{
    long level = fm_level;
    fm_c("fn(");
    for (long i = 0; i < f->b.v->n; i++) {
        if (i) fm_c(", ");
        Node *p = vec_get(f->b.v, i);
        fm_name(p->a.n);
        fm_c(": ");
        fm_type(p->b.n);
    }
    fm_b(')');
    if (f->c.n) {
        fm_c(" -> ");
        fm_type(f->c.n);
    }
    fm_uses(f->a.n->b.fx->uses); /* a `uses` clause (E0507) is kept, so nothing is lost */
    fm_b(' ');
    fm_block(f->d.n, level);
    fm_level = level; /* a later anonymous function in the same statement */
}

/* `[int, str]` (nothing for 0). */
static void fm_type_args(Vec *types)
{
    if (!types) return;
    fm_b('[');
    for (long i = 0; i < types->n; i++) {
        if (i) fm_c(", ");
        fm_type(vec_get(types, i));
    }
    fm_b(']');
}

/* `[T, K: ordered, E: effects]` (nothing for 0). */
static void fm_tparams(Vec *tparams)
{
    if (!tparams) return;
    fm_b('[');
    for (long i = 0; i < tparams->n; i++) {
        if (i) fm_c(", ");
        Node *t = vec_get(tparams, i);
        fm_name(t->a.n);
        if (t->b.i) {
            fm_c(": ");
            fm_c(t->b.i == TPC_ORDERED ? "ordered" : "effects");
        }
    }
    fm_b(']');
}

/* ` uses ...` in canonical order (effects by kind, scopes sorted and normalized). A clause with an
 * unknown name or invalid scope is printed as written, so formatting never loses text. */
static void fm_uses(Node *uses)
{
    if (!uses) return;
    fm_c(" uses ");
    EffSet *set = es_new(0);
    Vec *effects = uses->a.v;
    for (long i = 0; i < effects->n; i++) {
        Node *e = vec_get(effects, i);
        long bit = e->a.i;
        if (!bit) goto as_written; /* unknown name or effect parameter: as written */
        if (!e->b.v) {
            set->mask |= (unsigned long)bit;
            continue;
        }
        for (long j = 0; j < e->b.v->n; j++) {
            Node *scope = vec_get(e->b.v, j);
            if (!es_add_scope(set, bit, scope->a.s)) goto as_written;
        }
    }
    es_render(set, fm_sb);
    return;
as_written:
    for (long i = 0; i < effects->n; i++) {
        if (i) fm_c(", ");
        Node *e = vec_get(effects, i);
        fm_s(e->d.s);
        if (!e->b.v) continue;
        fm_b('(');
        for (long j = 0; j < e->b.v->n; j++) {
            if (j) fm_c(", ");
            render_str_lit(fm_sb, ((Node *)vec_get(e->b.v, j))->a.s);
        }
        fm_b(')');
    }
}

static void fm_type(Node *t)
{
    if (!t) return;
    switch (t->a.i) {
    case TY_INT: fm_c("int"); return;
    case TY_FLOAT: fm_c("float"); return;
    case TY_BOOL: fm_c("bool"); return;
    case TY_STR: fm_c("str"); return;
    case TY_VOID: fm_c("void"); return;
    case TY_NAMED:
        fm_name(t->b.n);
        fm_type_args(t->c.v); /* Name[args] */
        return;
    case TY_LIST:
        fm_c("list[");
        fm_type(t->b.n);
        break;
    case TY_RESULT:
        fm_c("result[");
        fm_type(t->b.n);
        break;
    case TY_FUNC:
        fm_fn_type(t);
        return;
    default: /* map[K, V] */
        fm_c("map[");
        fm_type(t->b.n);
        fm_c(", ");
        fm_type(t->c.n);
        break;
    }
    fm_b(']');
}

/* or 1, and 2, not 3, comparison 4, + - 5, * / % 6, unary - and try 7, postfix 8, primary 9. */
static long expr_prec(Node *n)
{
    switch (n->kind) {
    case N_BINARY:
        switch (n->c.i) {
        case T_OR: return 1;
        case T_AND: return 2;
        case T_PLUS:
        case T_MINUS: return 5;
        case T_STAR:
        case T_SLASH:
        case T_PERCENT: return 6;
        default: return 4;
        }
    case N_UNARY: return n->c.i == T_NOT ? 3 : 7;
    case N_TRY: return 7;
    case N_FIELD:
    case N_INDEX:
    case N_CALL_VALUE: return 8;
    default: return 9;
    }
}

/* Comma-separated expressions. */
static void fm_list(Vec *v)
{
    if (!v) return;
    for (long i = 0; i < v->n; i++) {
        if (i) fm_c(", ");
        fm_expr(vec_get(v, i), 0);
    }
}

/* 1 for an `and` written in parentheses as an operand of `or`: not needed, but kept for the reader. */
static int keep_parens(Node *op, Node *operand)
{
    return op->c.i == T_OR && operand->kind == N_BINARY && operand->c.i == T_AND && (operand->flags & NF_PAREN);
}

/* Prints an expression, in parentheses when its precedence is below ctx. */
static void fm_expr(Node *n, long ctx)
{
    if (!n) return;
    long prec = expr_prec(n);
    int parens = prec < ctx;
    if (parens) fm_b('(');
    switch (n->kind) {
    case N_LAMBDA:
        fm_lambda(n);
        break;
    case N_CALL_VALUE: /* callee(args) */
        fm_expr(n->a.n, 8);
        fm_b('(');
        fm_list(n->b.v);
        fm_b(')');
        break;
    case N_INT:
        int_to_dec(fm_sb, n->a.i);
        break;
    case N_FLOAT:
        float_to_text(fm_sb, n->a.i);
        break;
    case N_STR:
        render_str_lit(fm_sb, n->a.s);
        break;
    case N_BOOL:
        fm_c(n->a.i ? "true" : "false");
        break;
    case N_IDENT:
        fm_s(n->a.s);
        break;
    case N_CALL:
        fm_name(n->a.n);
        fm_b('(');
        fm_list(n->b.v);
        fm_b(')');
        break;
    case N_BINARY: /* left at its own level, right one higher (comparisons: both) */
        fm_expr(n->a.n, keep_parens(n, n->a.n) ? 3 : prec == 4 ? prec + 1 : prec);
        fm_b(' ');
        fm_s(tok_raw((int)n->c.i));
        fm_b(' ');
        fm_expr(n->b.n, keep_parens(n, n->b.n) ? 3 : prec + 1);
        break;
    case N_UNARY:
        if (n->c.i == T_NOT)
            fm_c("not ");
        else
            fm_b('-');
        fm_expr(n->a.n, prec);
        break;
    case N_TRY:
        fm_c("try ");
        fm_expr(n->a.n, 7);
        break;
    case N_FIELD:
        fm_expr(n->a.n, 8);
        fm_b('.');
        fm_name(n->b.n);
        break;
    case N_INDEX:
        fm_expr(n->a.n, 8);
        fm_b('[');
        fm_expr(n->b.n, 0);
        fm_b(']');
        break;
    case N_LIST:
        fm_b('[');
        fm_list(n->a.v);
        fm_b(']');
        break;
    case N_NEW_RECORD: /* new T { f: e, ... } */
        fm_c("new ");
        fm_name(n->a.n);
        fm_type_args(n->d.v);
        fm_c(" { ");
        for (long i = 0; i < n->b.v->n; i++) {
            if (i) fm_c(", ");
            Node *init = vec_get(n->b.v, i);
            fm_name(init->a.n);
            fm_c(": ");
            fm_expr(init->b.n, 0);
        }
        fm_c(" }");
        break;
    case N_NEW_LIST:
        fm_c("new ");
        fm_c("list[");
        fm_type(n->a.n);
        fm_b(']');
        break;
    case N_NEW_MAP:
        fm_c("new ");
        fm_c("map[");
        fm_type(n->a.n);
        fm_c(", ");
        fm_type(n->b.n);
        fm_b(']');
        break;
    case N_VCASE:
        fm_name(n->a.n);
        fm_b('.');
        fm_name(n->b.n);
        if (n->c.n) {
            fm_b('(');
            fm_expr(n->c.n, 0);
            fm_b(')');
        }
        break;
    default:
        break;
    }
    if (parens) fm_b(')');
}

/* "{", the statements one level deeper, then "}" at this level (no newline after it). */
static void fm_stmt(Node *s, long level);
static void fm_block(Node *block, long level)
{
    fm_b('{');
    fm_b('\n');
    for (long i = 0; i < block->a.v->n; i++) fm_stmt(vec_get(block->a.v, i), level + 1);
    fm_indent(level);
    fm_c("}");
}

/* if ... { } [else if ... | else { }] without the leading indent or trailing newline. */
static void fm_if(Node *n, long level)
{
    fm_c("if ");
    fm_expr(n->a.n, 0);
    fm_b(' ');
    fm_block(n->b.n, level);
    if (!n->c.n) return;
    fm_c(" else ");
    if (n->c.n->kind == N_IF)
        fm_if(n->c.n, level);
    else
        fm_block(n->c.n, level);
}

/* One statement (with its indentation and newline). */
static void fm_stmt(Node *s, long level)
{
    fm_level = level;
    fm_indent(level);
    switch (s->kind) {
    case N_COMMENT:
        fm_source(s);
        break;
    case N_LET:
        fm_c(s->flags & NF_MUT ? "var " : "let ");
        if (s->flags & NF_DISCARD)
            fm_c("_");
        else
            fm_name(s->a.n);
        if (s->b.n) {
            fm_c(": ");
            fm_type(s->b.n);
        }
        fm_c(" = ");
        fm_expr(s->c.n, 0);
        fm_b(';');
        break;
    case N_SET:
        fm_c("set ");
        fm_expr(s->a.n, 0);
        fm_c(" = ");
        fm_expr(s->b.n, 0);
        fm_b(';');
        break;
    case N_IF:
        fm_if(s, level);
        break;
    case N_WHILE:
        fm_c("while ");
        fm_expr(s->a.n, 0);
        fm_b(' ');
        fm_block(s->b.n, level);
        break;
    case N_FOR:
        fm_c("for ");
        if (s->a.n)
            fm_name(s->a.n);
        else
            fm_c("_");
        fm_c(" in ");
        fm_expr(s->b.n, 0);
        if (s->c.n) {
            fm_c("..");
            fm_expr(s->c.n, 0);
        }
        fm_b(' ');
        fm_block(s->d.n, level);
        break;
    case N_MATCH: /* match expr { case ... } */
        fm_c("match ");
        fm_expr(s->a.n, 0);
        fm_c(" {\n");
        for (long i = 0; i < s->b.v->n; i++) {
            Node *arm = vec_get(s->b.v, i);
            fm_indent(level + 1);
            if (arm->kind == N_COMMENT) {
                fm_source(arm);
                fm_b('\n');
                continue;
            }
            fm_c("case ");
            fm_name(arm->a.n);
            if (arm->flags & NF_EMPTY_ARM) { /* spec 009 */
                fm_c("()");
            } else if (arm->flags & NF_DISCARD) {
                fm_b('(');
                fm_c("_");
                fm_b(')');
            } else if (arm->b.n) {
                fm_b('(');
                fm_name(arm->b.n);
                fm_b(')');
            }
            fm_b(' ');
            fm_block(arm->c.n, level + 1);
            fm_b('\n');
        }
        fm_indent(level);
        fm_c("}");
        break;
    case N_RETURN:
        fm_c("return");
        if (s->a.n) {
            fm_b(' ');
            fm_expr(s->a.n, 0);
        }
        fm_b(';');
        break;
    case N_BREAK:
        fm_c("break;");
        break;
    case N_CONTINUE:
        fm_c("continue;");
        break;
    case N_EXPR_STMT:
        fm_expr(s->a.n, 0);
        fm_b(';');
        break;
    default:
        break;
    }
    fm_b('\n');
}

/* The body lines of a record (name: type;) or variant (name; / name(type);). */
static void fm_members(Vec *v, int is_record)
{
    for (long i = 0; i < v->n; i++) {
        Node *m = vec_get(v, i);
        fm_c("    ");
        if (m->kind == N_COMMENT) {
            fm_source(m);
        } else {
            fm_name(m->a.n);
            if (is_record) {
                fm_c(": ");
                fm_type(m->b.n);
            } else if (m->b.n) {
                fm_b('(');
                fm_type(m->b.n);
                fm_b(')');
            }
            fm_b(';');
        }
        fm_b('\n');
    }
}

/* Contract clauses in source order, each on its own line indented four spaces. */
static void fm_clauses(Fx *fx)
{
    long r = 0, e = 0; /* next requires, next ensures */
    for (;;) {
        Node *req = fx->requires_ && r < fx->requires_->n ? fx->requires_->data[r].n : 0;
        Node *ens = fx->ensures && e < fx->ensures->n ? fx->ensures->data[e].n : 0;
        if (req && (!ens || req->start <= ens->start)) { /* the requires clause comes first */
            fm_c("\n    requires ");
            r++;
            fm_expr(req, 0);
        } else if (ens) {
            fm_c("\n    ensures ");
            e++;
            fm_expr(ens, 0);
        } else {
            break;
        }
    }
}

/* The dotted path of an import (a.b.name) as a str. */
static Str *import_text(Node *imp)
{
    SB *sb = sb_new();
    for (long i = 0; i < imp->a.v->n; i++) {
        if (i) sb_byte(sb, '.');
        sb_str(sb, vec_get(imp->a.v, i));
    }
    return sb_finish(sb);
}

static void fm_item(Node *item)
{
    switch (item->kind) {
    case N_COMMENT:
        fm_source(item);
        break;
    case N_ANNOTATION: { /* spec 012: @name("A", "B"), IDs sorted by bytes */
        fm_b('@');
        fm_s(item->a.n->a.s);
        fm_b('(');
        Vec *ids = item->b.v;
        Str **sorted = alloc(8 * (ids->n + 1));
        for (long i = 0; i < ids->n; i++) {
            Str *s = ((Node *)vec_get(ids, i))->a.s;
            long j = i;
            while (j > 0 && str_cmp(sorted[j - 1], s) > 0) {
                sorted[j] = sorted[j - 1];
                j--;
            }
            sorted[j] = s;
        }
        for (long i = 0; i < ids->n; i++) {
            if (i) fm_c(", ");
            render_str_lit(fm_sb, sorted[i]);
        }
        fm_b(')');
        break;
    }
    case N_IMPORT:
        fm_c("import ");
        fm_s(import_text(item));
        fm_b(';');
        break;
    case N_FN: {
        if (item->flags & NF_EXPORT) fm_c("export ");
        fm_c("fn ");
        fm_name(item->a.n);
        Fx *fx = item->a.n ? item->a.n->b.fx : 0;
        if (fx) fm_tparams(fx->tparams); /* [T, ...] */
        fm_b('(');
        for (long i = 0; i < item->b.v->n; i++) {
            if (i) fm_c(", ");
            Node *p = vec_get(item->b.v, i);
            fm_name(p->a.n);
            fm_c(": ");
            fm_type(p->b.n);
        }
        fm_b(')');
        if (item->c.n) {
            fm_c(" -> ");
            fm_type(item->c.n);
        }
        if (fx) {
            fm_uses(fx->uses);
            fm_clauses(fx);
        }
        fm_b(' ');
        fm_block(item->d.n, 0);
        break;
    }
    case N_RECORD:
    case N_VARIANT:
        if (item->flags & NF_EXPORT) fm_c("export ");
        fm_c(item->kind == N_RECORD ? "record " : "variant ");
        fm_name(item->a.n);
        fm_tparams(item->c.v); /* [T, ...] */
        fm_c(" {\n");
        fm_members(item->b.v, item->kind == N_RECORD);
        fm_c("}");
        break;
    case N_TEST:
        fm_c(item->flags & NF_PROP ? "prop " : "test ");
        render_str_lit(fm_sb, item->a.s);
        if (item->flags & NF_PROP) { /* prop: " (a: T, b: U)" */
            fm_c(" (");
            Vec *params = item->d.fx->params;
            for (long i = 0; i < params->n; i++) {
                if (i) fm_c(", ");
                Node *p = vec_get(params, i);
                fm_name(p->a.n);
                fm_c(": ");
                fm_type(p->b.n);
            }
            fm_b(')');
        }
        fm_uses(item->c.n);
        fm_b(' ');
        fm_block(item->b.n, 0);
        break;
    default:
        break;
    }
    fm_b('\n');
}

/* Insertion sort of vec[from..] (import nodes) by their dotted text. */
static void sort_imports(Vec *v, long from)
{
    for (long i = from + 1; i < v->n; i++) {
        Node *imp = v->data[i].n;
        Str *text = import_text(imp);
        long j = i - 1;
        while (j >= from && str_cmp(import_text(v->data[j].n), text) > 0) {
            v->data[j + 1] = v->data[j];
            j--;
        }
        v->data[j + 1].n = imp;
    }
}

/* 1 when the line after the comment's line is blank in the source (the comment is not attached to
 * what follows it: a file header or a section comment, kept apart so `vo doc` does not read it). */
static long blank_after(Node *c)
{
    char *p = fm_text + c->end;
    while (*p && *p != '\n')
        p++;
    if (!*p) return 0;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r')
        p++;
    return *p == '\n';
}

/* The canonical text of a parsed file: leading comments, sorted imports, then the other items in
 * order; one blank line between items, none between imports, and none after a comment unless the
 * source had one there. */
Str *fmt_file(int file)
{
    FileRec *f = file_get(file);
    fm_text = f->text;
    Vec *items = f->ast->a.v;
    fm_sb = sb_new();
    Vec *order = vec_new();
    long first = 0; /* leading comments */
    while (first < items->n && ((Node *)vec_get(items, first))->kind == N_COMMENT)
        vec_push(order, vec_get(items, first++));
    long imports_at = order->n;
    for (long i = 0; i < items->n; i++) {
        Node *it = vec_get(items, i);
        if (it->kind == N_IMPORT) vec_push(order, it);
    }
    sort_imports(order, imports_at); /* by their dotted text */
    for (long i = first; i < items->n; i++) { /* everything else, in order */
        Node *it = vec_get(items, i);
        if (it->kind != N_IMPORT) vec_push(order, it);
    }
    long prev = 0; /* kind of the previous element (0 = none) */
    long gap = 0;  /* the previous comment had a blank line after it */
    for (long i = 0; i < order->n; i++) {
        Node *it = vec_get(order, i);
        long kind = it->kind;
        if (prev == N_ANNOTATION) { /* spec 012: an annotation sits right above its item */
        } else if (prev == N_COMMENT ? gap : prev && !(prev == N_IMPORT && kind == N_IMPORT)) {
            fm_b('\n'); /* blank line */
        }
        fm_item(it);
        prev = kind;
        if (kind == N_COMMENT) gap = blank_after(it);
    }
    return sb_finish(fm_sb);
}
