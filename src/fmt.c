/* Formatter: AST -> canonical text (4-space indent, imports sorted first, minimal parentheses; ported
 * from src/fmt.s, spec 008). */
#include "vo.h"

static SB *fm_sb;     /* the output builder */
static char *fm_text; /* source text of the file being formatted */
static long fm_level; /* indentation level of the statement being printed */

/* Lines over FM_WIDTH characters: a statement is printed, then printed again with the outermost list
 * literal, argument list or record literal crossing the limit written one item per line (fm_wrap), until
 * every line fits or nothing is left to break. A call's last argument that is a list or record literal
 * keeps its opening bracket on the call's line (`web_app([` ... `])`). Only a group that ends its line (closing
 * brackets, `;`, `,` or ` {` after it) is broken: a long operator chain stays on one line. */
#define FM_WIDTH 100
static Vec *fm_wrap;   /* the groups written one item per line */
static Vec *fm_groups; /* the groups the statement being printed wrote on one line: node, start, end */
static long fm_operand; /* > 0 while printing an operand of a binary operator: its groups are not broken */

static void fm_type(Node *t);
static void fm_expr(Node *n, long ctx);
static void fm_block(Node *block, long level);
static void fm_uses(Node *uses);
static void fm_uses_body(Node *uses);

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

static long fm_line_start(long pos)
{
    while (pos > 0 && fm_sb->ptr[pos - 1] != '\n') pos--;
    return pos;
}

/* The spaces at the start of the line being written. */
static long fm_cur_indent(void)
{
    long s = fm_line_start(fm_sb->len), n = 0;
    while (s + n < fm_sb->len && fm_sb->ptr[s + n] == ' ') n++;
    return n;
}

/* Characters (not bytes) of the output in [from, to). */
static long fm_chars(long from, long to)
{
    long n = 0;
    for (long i = from; i < to; i++) n += ((unsigned char)fm_sb->ptr[i] & 0xC0) != 0x80;
    return n;
}

static int fm_wrapped(Node *n)
{
    for (long i = 0; i < fm_wrap->n; i++)
        if (vec_get(fm_wrap, i) == n) return 1;
    return 0;
}

static void fm_group(Node *n, long start)
{
    if (fm_operand) return; /* (`a + f(x)`: an operator chain is not broken) */
    vec_push(fm_groups, n);
    vec_push(fm_groups, (void *)start);
    vec_push(fm_groups, (void *)fm_sb->len);
}

static void fm_spaces(long n)
{
    for (; n > 0; n--) fm_b(' ');
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

/* A return type that a `uses` clause follows: a function type without its own `uses` would take that
 * clause, so it is written in parentheses, `-> (fn() -> int) uses console`. */
static void fm_ret_type(Node *t, Node *uses)
{
    int open = uses && t && t->kind == N_TYPE_NODE && t->a.i == TY_FUNC && !t->d.n;
    if (open) fm_b('(');
    fm_type(t);
    if (open) fm_b(')');
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
        fm_ret_type(t->c.n, t->d.n);
    }
    fm_uses(t->d.n);
}

/* `(a: T, b: U)`, or (wrap) each parameter on its own line one level deeper than ind, `)` at ind. */
static void fm_params(Vec *params, int wrap, long ind)
{
    fm_b('(');
    for (long i = 0; i < params->n; i++) {
        if (wrap) {
            fm_b('\n');
            fm_spaces(ind + 4);
        } else if (i) {
            fm_c(", ");
        }
        Node *p = vec_get(params, i);
        fm_name(p->a.n);
        fm_c(": ");
        fm_type(p->b.n);
        if (wrap && i + 1 < params->n) fm_b(',');
    }
    if (wrap && params->n) {
        fm_b('\n');
        fm_spaces(ind);
    }
    fm_b(')');
}

/* Spec 030 (research R7): an item's ` uses ...` on its line when that line (with tail more characters
 * after it) fits FM_WIDTH, else on its own line one level deeper. */
static void fm_header_uses(Node *uses, long tail)
{
    if (!uses) return;
    long at = fm_sb->len;
    fm_uses(uses);
    if (fm_chars(fm_line_start(at), fm_sb->len) + tail <= FM_WIDTH) return;
    fm_sb->len = at;
    fm_c("\n    uses ");
    fm_uses_body(uses);
}

/* 1 when the header written from hs has a first line that fits FM_WIDTH (with tail more characters when
 * the header is one line). */
static int fm_header_fits(long hs, long tail)
{
    long nl = hs;
    while (nl < fm_sb->len && fm_sb->ptr[nl] != '\n') nl++;
    return fm_chars(hs, nl) + (nl == fm_sb->len ? tail : 0) <= FM_WIDTH;
}

/* `fn(x: int) -> int {` + the body one level deeper than the statement it is in + `}`. Its parameters go
 * one per line when the statement's line is too long (a group of fm_pick). */
static void fm_lambda(Node *f)
{
    long level = fm_cur_indent() / 4; /* (its body one level deeper than the line it starts on) */
    long st = fm_sb->len;
    int wrap = f->b.v->n && fm_wrapped(f);
    fm_c("fn");
    fm_params(f->b.v, wrap, fm_cur_indent());
    if (f->c.n) {
        fm_c(" -> ");
        fm_ret_type(f->c.n, f->a.n->b.fx->uses);
    }
    fm_uses(f->a.n->b.fx->uses); /* a `uses` clause (E0507) is kept, so nothing is lost */
    if (f->b.v->n && !wrap) fm_group(f, st);
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
            fm_c(t->b.i == TPC_ORDERED ? "ordered" : t->b.i == TPC_JSON ? "json" : t->b.i == TPC_KEY ? "key" : "effects");
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
    fm_uses_body(uses);
}

/* The effects of a `uses` clause or effect argument, canonical (see fm_uses). */
static void fm_uses_body(Node *uses)
{
    EffSet *set = es_new(0);
    Vec *effects = uses->a.v;
    Vec *names = vec_new(); /* effect parameters and sets (spec 030): after the effects, sorted */
    for (long i = 0; i < effects->n; i++) {
        Node *e = vec_get(effects, i);
        long bit = e->a.i;
        if (!bit && e->c.n && !e->b.v) {
            long j = names->n;
            vec_push(names, e->d.s);
            while (j > 0 && str_cmp(names->data[j - 1].p, e->d.s) > 0) {
                names->data[j] = names->data[j - 1];
                j--;
            }
            names->data[j].p = e->d.s;
            continue;
        }
        if (!bit) goto as_written; /* unknown name: as written */
        if (!e->b.v) {
            set->mask |= (unsigned long)bit;
            continue;
        }
        for (long j = 0; j < e->b.v->n; j++) {
            Node *scope = vec_get(e->b.v, j);
            if (scope->kind != N_STR) goto as_written; /* spec 031: a constant: its value is not known here */
            if (!es_add_scope(set, bit, scope->a.s)) goto as_written;
        }
    }
    if (!es_is_empty(set)) es_render(set, fm_sb);
    for (long i = 0; i < names->n; i++) {
        if (i || !es_is_empty(set)) fm_c(", ");
        fm_s(names->data[i].p);
    }
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
            Node *scope = vec_get(e->b.v, j);
            if (scope->kind == N_STR)
                render_str_lit(fm_sb, scope->a.s);
            else
                fm_expr(scope, 0);
        }
        fm_b(')');
    }
}

static void fm_type(Node *t)
{
    if (!t) return;
    if (t->kind == N_USES) { /* spec 026: an effect argument, `Database[uses file.read]` */
        fm_c("uses ");
        fm_uses_body(t);
        return;
    }
    switch (t->a.i) {
    case TY_INT: fm_c("int"); return;
    case TY_FLOAT: fm_c("float"); return;
    case TY_BOOL: fm_c("bool"); return;
    case TY_STR: fm_c("str"); return;
    case TY_BYTES: fm_c("bytes"); return;
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
        if (t->c.n) { /* spec 032: the error type */
            fm_c(", ");
            fm_type(t->c.n);
        }
        break;
    case TY_FUNC:
        fm_fn_type(t);
        return;
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE: { /* spec 014 */
        static const char *const names[] = {"ordered_map[", "ordered_set[", "priority_queue[", "deque["};
        fm_c((char *)names[t->a.i - TY_OMAP]);
        fm_type(t->b.n);
        if (t->c.n) {
            fm_c(", ");
            fm_type(t->c.n);
        }
        break;
    }
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

/* Comma-separated expressions (each its own context: an operand's argument may still be broken). */
static void fm_list(Vec *v)
{
    if (!v) return;
    long saved = fm_operand;
    fm_operand = 0;
    for (long i = 0; i < v->n; i++) {
        if (i) fm_c(", ");
        fm_expr(vec_get(v, i), 0);
    }
    fm_operand = saved;
}

/* Items one per line, one level deeper than the line they start on, then the line's indent (the caller
 * writes the brackets). Record literals: `name: value` items. */
static void fm_items(Vec *v, int fields)
{
    long ind = fm_cur_indent(), saved = fm_operand;
    fm_operand = 0;
    fm_b('\n');
    for (long i = 0; i < v->n; i++) {
        fm_spaces(ind + 4);
        Node *e = vec_get(v, i);
        if (fields) {
            fm_name(e->a.n);
            fm_c(": ");
            e = e->b.n;
        }
        fm_expr(e, 0);
        if (i + 1 < v->n) fm_b(',');
        fm_b('\n');
    }
    fm_spaces(ind);
    fm_operand = saved;
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
    case N_CALL_VALUE: { /* callee(args) */
        long st = fm_sb->len;
        fm_expr(n->a.n, 8);
        fm_b('(');
        if (n->b.v && n->b.v->n && fm_wrapped(n))
            fm_items(n->b.v, 0);
        else
            fm_list(n->b.v);
        fm_b(')');
        if (n->b.v && n->b.v->n && !fm_wrapped(n)) fm_group(n, st);
        break;
    }
    case N_INT: {
        /* spec 043: a 0x or 0b literal keeps its base and digits; prefix lower case, hex letters upper */
        char *src = file_get((int)n->file)->text;
        long i = n->start + 2;
        int based = n->end > i && src[n->start] == '0' && ((src[n->start + 1] | 32) == 'x' || (src[n->start + 1] | 32) == 'b');
        for (; based && i < n->end; i++) based = is_hex_digit(src[i]); /* (a folded expression is not a literal) */
        if (!based) {
            int_to_dec(fm_sb, n->a.i);
            break;
        }
        fm_b('0');
        fm_b(src[n->start + 1] | 32);
        for (i = n->start + 2; i < n->end; i++) fm_b(src[i] >= 'a' ? src[i] - 32 : src[i]);
        break;
    }
    case N_FLOAT: {
        /* spec 042: a literal with an exponent keeps it (lower case, no `+`); others are rewritten */
        char *src = file_get((int)n->file)->text;
        int exp = 0;
        for (long i = n->start; i < n->end && !exp; i++) exp = src[i] == 'e' || src[i] == 'E';
        if (!exp) {
            float_to_text(fm_sb, n->a.i);
            break;
        }
        for (long i = n->start; i < n->end; i++)
            if (src[i] != '+') fm_b(src[i] == 'E' ? 'e' : src[i]);
        break;
    }
    case N_STR: {
        /* spec 042: a raw string stays raw only while its text needs it (a backslash, a quote or a line
           break); its inside is written as it is, never re-indented */
        Str *v = n->a.s;
        int raw = file_get((int)n->file)->text[n->start] == '`', needed = 0;
        for (long i = 0; raw && i < v->len && !needed; i++) needed = v->data[i] == '\\' || v->data[i] == '"' || v->data[i] == '\n';
        if (!needed) {
            render_str_lit(fm_sb, v);
            break;
        }
        fm_b('`');
        fm_s(v);
        fm_b('`');
        break;
    }
    case N_BOOL:
        fm_c(n->a.i ? "true" : "false");
        break;
    case N_IDENT:
        fm_s(n->a.s);
        break;
    case N_CALL: {
        long st = fm_sb->len;
        fm_name(n->a.n);
        fm_b('(');
        if (n->b.v && n->b.v->n && fm_wrapped(n))
            fm_items(n->b.v, 0);
        else
            fm_list(n->b.v);
        fm_b(')');
        if (n->b.v && n->b.v->n && !fm_wrapped(n)) fm_group(n, st);
        break;
    }
    case N_BINARY: /* left at its own level, right one higher (comparisons: both) */
        fm_operand++;
        fm_expr(n->a.n, keep_parens(n, n->a.n) ? 3 : prec == 4 ? prec + 1 : prec);
        fm_b(' ');
        fm_s(tok_raw((int)n->c.i));
        fm_b(' ');
        fm_expr(n->b.n, keep_parens(n, n->b.n) ? 3 : prec + 1);
        fm_operand--;
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
    case N_LIST: {
        long st = fm_sb->len;
        fm_b('[');
        if (n->a.v->n && fm_wrapped(n))
            fm_items(n->a.v, 0);
        else
            fm_list(n->a.v);
        fm_b(']');
        if (n->a.v->n && !fm_wrapped(n)) fm_group(n, st);
        break;
    }
    case N_NEW_RECORD: /* new T { f: e, ... } */
        fm_c("new ");
        fm_name(n->a.n);
        fm_type_args(n->d.v);
        if (!n->b.v->n) { /* spec 029: a record without fields */
            fm_c(" {}");
            break;
        }
        if (fm_wrapped(n)) {
            fm_c(" {");
            fm_items(n->b.v, 1);
            fm_c("}");
            break;
        }
        long st = fm_sb->len, saved_op = fm_operand;
        fm_c(" { ");
        fm_operand = 0;
        for (long i = 0; i < n->b.v->n; i++) {
            if (i) fm_c(", ");
            Node *init = vec_get(n->b.v, i);
            fm_name(init->a.n);
            fm_c(": ");
            fm_expr(init->b.n, 0);
        }
        fm_c(" }");
        fm_operand = saved_op;
        fm_group(n, st);
        break;
    case N_NEW_LIST:
        fm_c("new ");
        fm_c("list[");
        fm_type(n->a.n);
        fm_b(']');
        break;
    case N_NEW_COLL: /* spec 014: new ordered_map[K, V] */
        fm_c("new ");
        fm_type(n->a.n);
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

/* The group to write one item per line next: on the first line over FM_WIDTH, the outermost group crossing
 * the limit (else the widest on that line); a call's last list or record argument instead of the call. */
static Node *fm_pick(long mark)
{
    Node *best = 0;
    long best_line = -1, best_start = 0, best_w = 0;
    int best_cross = 0;
    for (long i = mark; i < fm_groups->n; i += 3) {
        Node *g = vec_get(fm_groups, i);
        long st = (long)vec_get(fm_groups, i + 1), en = (long)vec_get(fm_groups, i + 2);
        long ls = fm_line_start(st), le = st;
        while (le < fm_sb->len && fm_sb->ptr[le] != '\n') le++;
        if (fm_chars(ls, le) <= FM_WIDTH) continue;
        long k = en; /* only closing brackets, `;`, `,` or ` {` may follow it on its line (an operator chain
                        is not broken in the middle) */
        while (k < le && (fm_sb->ptr[k] == ')' || fm_sb->ptr[k] == ']' || fm_sb->ptr[k] == '}' || fm_sb->ptr[k] == ';' ||
                          fm_sb->ptr[k] == ',' || fm_sb->ptr[k] == ' ' || fm_sb->ptr[k] == '{'))
            k++;
        if (k < le) continue;
        int cross = fm_chars(ls, en) > FM_WIDTH;
        long w = fm_chars(st, en);
        int better = !best || ls < best_line ||
                     (ls == best_line && (cross > best_cross || (cross == best_cross && (cross ? st < best_start : w > best_w))));
        if (better) best = g, best_line = ls, best_start = st, best_w = w, best_cross = cross;
    }
    /* `f(a, [` ... `])`, `print(str(total([` ... `])))`: while a call's last argument is a call, list or record
       that starts within the limit, it is broken instead */
    while (best && (best->kind == N_CALL || best->kind == N_CALL_VALUE)) {
        Node *last = vec_get(best->b.v, best->b.v->n - 1);
        long at = -1;
        for (long i = mark; i < fm_groups->n; i += 3)
            if (vec_get(fm_groups, i) == last) at = (long)vec_get(fm_groups, i + 1);
        if (at < 0 || last->kind == N_LAMBDA || fm_wrapped(last) || fm_chars(fm_line_start(at), at) >= FM_WIDTH - 1)
            break;
        best = last;
    }
    return best;
}

static void fm_stmt_once(Node *s, long level);

/* One statement (with its indentation and newline), its long lines broken (FM_WIDTH). */
static void fm_stmt(Node *s, long level)
{
    long start = fm_sb->len, mark = fm_groups->n;
    for (int round = 0; round < 200; round++) {
        fm_sb->len = start;
        fm_groups->n = mark;
        fm_stmt_once(s, level);
        Node *g = fm_pick(mark);
        if (!g) break;
        vec_push(fm_wrap, g);
    }
    fm_groups->n = mark; /* (an enclosing statement's lines are its own) */
}

static void fm_stmt_once(Node *s, long level)
{
    fm_level = level;
    fm_indent(level);
    switch (s->kind) {
    case N_COMMENT:
        fm_source(s);
        break;
    case N_CONST: /* spec 031: an item, laid out as a statement (the 100-column rule) */
        if (s->flags & NF_EXPORT) fm_c("export ");
        fm_c("const ");
        fm_name(s->a.n);
        if (s->b.n) {
            fm_c(": ");
            fm_type(s->b.n);
        }
        fm_c(" = ");
        fm_expr(s->c.n, 0);
        fm_b(';');
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
            for (long j = 0; is_record && m->c.v && j < m->c.v->n; j++) { /* spec 029: field annotations */
                Node *a = vec_get(m->c.v, j);
                fm_b('@');
                fm_name(a->a.n);
                fm_b('(');
                for (long k = 0; k < a->b.v->n; k++) {
                    if (k) fm_c(", ");
                    fm_expr(vec_get(a->b.v, k), 0);
                }
                fm_c(")\n    ");
            }
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
        Fx *fx = item->a.n ? item->a.n->b.fx : 0;
        long hs = fm_sb->len, tail = fx && (fx->requires_ || fx->ensures) ? 0 : 2; /* ` {` */
        for (int wrap = 0; wrap < 2; wrap++) { /* spec 030: long headers (research R7) */
            fm_sb->len = hs;
            if (item->flags & NF_EXPORT) fm_c("export ");
            fm_c("fn ");
            fm_name(item->a.n);
            if (fx) fm_tparams(fx->tparams); /* [T, ...] */
            fm_params(item->b.v, wrap, 0);
            if (item->c.n) {
                fm_c(" -> ");
                fm_ret_type(item->c.n, fx ? fx->uses : 0);
            }
            if (fx) fm_header_uses(fx->uses, tail);
            if (wrap || !item->b.v->n || fm_header_fits(hs, tail)) break;
        }
        if (fx) fm_clauses(fx);
        fm_b(' ');
        fm_block(item->d.n, 0);
        break;
    }
    case N_CONST: /* spec 031: fm_stmt ends the line itself */
        fm_stmt(item, 0);
        return;
    case N_EFFECTS: /* spec 030: effects Name = e, ...; */
        if (item->flags & NF_EXPORT) fm_c("export ");
        fm_c("effects ");
        fm_name(item->a.n);
        fm_c(" = ");
        fm_uses_body(item->b.n);
        fm_b(';');
        break;
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
    {
        long hs = fm_sb->len;
        Vec *params = item->flags & NF_PROP ? item->d.fx->params : 0;
        for (int wrap = 0; wrap < 2; wrap++) { /* spec 030: as a function's header */
            fm_sb->len = hs;
            fm_c(item->flags & NF_PROP ? "prop " : "test ");
            render_str_lit(fm_sb, item->a.s);
            if (params) { /* prop: " (a: T, b: U)" */
                fm_b(' ');
                fm_params(params, wrap, 0);
            }
            fm_header_uses(item->c.n, 2);
            if (wrap || !params || !params->n || fm_header_fits(hs, 2)) break;
        }
    }
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
    fm_wrap = vec_new();
    fm_groups = vec_new();
    fm_operand = 0;
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
        } else if (prev == N_COMMENT ? gap : prev && !(prev == kind && (kind == N_IMPORT || kind == N_EFFECTS || kind == N_CONST))) {
            fm_b('\n'); /* blank line */
        }
        fm_item(it);
        prev = kind;
        if (kind == N_COMMENT) gap = blank_after(it);
    }
    return sb_finish(fm_sb);
}
