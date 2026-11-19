/* Recursive-descent parser: tokens -> AST (contracts/grammar.md; ported from src/parse.s, spec 008).
 * Every statement kind is chosen from its first token. After an error, p_err is set and every routine
 * returns early; the block and item loops stop (US2 adds recovery). */
#include "vo.h"

static Token *p_toks;
static long p_pos;
static char *p_text;
static long p_err;  /* an error in the current statement: unwind to recovery */
long p_hard;        /* a syntax error left the tree incomplete: skip checking */
static Vec *p_methods; /* N_CALL_VALUE nodes in method form (their E0110 is pending) */
static long p_depth;   /* current expression/block nesting */

/* p_last_diag compares the code pointer with this one: every E0110 of the parser uses it. */
static char e0110[] = "E0110";

/* Token spellings and display classes, indexed by token kind. */
static const char *const tok_spell[] = {
    "end of file", "name", "type name", "number", "number", "string", "comment", "_", "invalid token", "?",
    "fn", "record", "variant", "test", "import", "let", "var", "set", "if", "else",
    "while", "for", "in", "match", "case", "return", "break", "continue", "try", "new",
    "and", "or", "not", "true", "false", "int", "float", "bool", "str", "list",
    "map", "result", "void", "uses", "prop", "requires", "ensures", "export", "bytes", "?",
    "(", ")", "{", "}", "[", "]", ",", ";", ":", ".",
    "..", "->", "=", "==", "!=", "<", "<=", ">", ">=", "+",
    "-", "*", "/", "%", "@",
};
static const char *const tok_display_class[] = {
    "the end of the file", "a name", "a type name", "a number", "a number", "a string", "a comment", "`_`",
    "an invalid token",
};

static Node *parse_type(void);
static Vec *parse_args(void);
static Node *parse_expr(void);
static Node *parse_unary(void);
static Node *parse_block(void);
static Node *parse_stmt(void);
static Node *parse_fn_kind(int kind);
static Vec *parse_tparams(void);
static Vec *parse_type_args(void);
static Node *parse_uses(void);
static Node *parse_effect(void);
static void p_hard_msg(const char *message);
static Node *p_method_call(Node *object, Node *name, long start, Node *field);
static int p_elif(void);
static void p_missing_new(void);
static int p_member_comma(void);
static void p_missing_init(Node *type_node);
static void p_arrow_void(long arrow_start);
static int p_compound_stmt(void);
static void p_compound_assign(Node *place);
static int p_insert_ok(int expected);
static Diag *p_last_diag(void);
static void p_unclosed(long open_start);
static Str *node_text(Node *n);

static Token *p_tok(void);
static int p_kind(void);
static int p_kind_at(long ahead);

/* Spec 014: TY_OMAP.. when the current token is one of the built-in collection names followed by `[`
 * (`ordered_map[K, V]`, `ordered_set[T]`, `priority_queue[T]`, `deque[T]`), else 0. The names are
 * types only there: elsewhere they are ordinary identifiers. */
static long p_coll_kind(void)
{
    static const char *const names[] = {"ordered_map", "ordered_set", "priority_queue", "deque"};
    if (p_kind() != T_IDENT || p_kind_at(1) != T_LBRACKET) return 0;
    Token *t = p_tok();
    for (int i = 0; i < 4; i++)
        if (str_eq_bytes(str_from_cstr((char *)names[i]), p_text + t->start, (long)t->end - (long)t->start))
            return TY_OMAP + i;
    return 0;
}

static Token *p_tok(void)
{
    return &p_toks[p_pos];
}

static int p_kind(void)
{
    return p_toks[p_pos].kind;
}

/* Kind of the token `ahead` positions after the current one (EOF does not advance). */
static int p_kind_at(long ahead)
{
    Token *t = &p_toks[p_pos];
    while (ahead && t->kind) {
        t++;
        ahead--;
    }
    return t->kind;
}

static long p_start(void)
{
    return p_toks[p_pos].start;
}

static long p_prev_end(void)
{
    return p_pos ? p_toks[p_pos - 1].end : 0;
}

/* Consumes the current token (never moves past EOF) and returns it. */
static Token *p_next(void)
{
    Token *t = &p_toks[p_pos];
    if (t->kind) p_pos++;
    return t;
}

static int p_accept(int kind)
{
    if (p_kind() != kind) return 0;
    p_next();
    return 1;
}

/* How a token kind is named in messages: `;`, a name, the end of the file. */
static Str *tok_display(int kind)
{
    if (kind < T_KW_FIRST) return str_from_cstr((char *)tok_display_class[kind]);
    const char *spell = tok_spell[kind];
    SB *sb = sb_new();
    sb_byte(sb, '`');
    sb_cstr(sb, (char *)spell);
    sb_byte(sb, '`');
    return sb_finish(sb);
}

Str *tok_raw(int kind)
{
    return str_from_cstr((char *)tok_spell[kind]);
}

/* Records a syntax error that leaves the tree incomplete. */
static void p_mark_hard(void)
{
    p_err = 1;
    p_hard = 1;
}

/* E0110 at the current token (only the first error of a statement is reported). */
static void p_fail(Str *expected_display, Str *expected_raw)
{
    if (p_err) return;
    int kind = p_kind();
    Str *m = msg("Expected $1, found $2.", expected_display, tok_display(kind), 0);
    Token *t = p_tok();
    Diag *d = diag_add(e0110, SEV_ERROR, (int)cur_file, t->start, t->end, m);
    diag_set_ef(d, expected_raw, tok_raw(kind));
    p_mark_hard();
}

/* A syntax error the parser can continue past (the tree stays complete). */
static Diag *p_soft_err(char *code, long start, long end, const char *message)
{
    return diag_add(code, SEV_ERROR, (int)cur_file, start, end, str_from_cstr((char *)message));
}

static void p_fix(Diag *d, long start, long end, const char *replacement)
{
    diag_add_fix(d, (int)cur_file, start, end, str_from_cstr((char *)replacement));
}

/* Consumes the `;` ending a statement; when it is missing, E0112 with an insertion fix (parsing goes
 * on). */
static void p_end_stmt(void)
{
    if (p_err) return;
    if (p_kind() == T_SEMI) {
        p_next();
        return;
    }
    long at = p_prev_end();
    Diag *d = p_soft_err("E0112", at, at, "Missing `;` at the end of this statement.");
    p_fix(d, at, at, ";");
}

/* After a list element: 1 when a comma and another element follow. A comma right before the closing
 * token is E0113 with a removal fix (parsing goes on). */
static int p_comma_more(int close)
{
    if (!p_accept(T_COMMA)) return 0;
    if (p_kind() != close) return 1;
    long comma_end = p_prev_end(); /* the comma just consumed */
    Diag *d = p_soft_err("E0113", comma_end - 1, comma_end, "A list cannot end with a comma.");
    p_fix(d, comma_end - 1, comma_end, "");
    return 0;
}

static int is_item_kw(int kind)
{
    return kind >= T_FN && kind <= T_IMPORT;
}

/* After an error in a statement: skips to the next `;` (consumed) or the enclosing `}` and clears the
 * error (returns 0); stops at an item keyword or the end of the file (returns 1, error kept). */
static int p_sync_stmt(void)
{
    long depth = 0;
    for (;;) {
        int k = p_kind();
        if (k == 0) return 1;
        if (depth == 0 && is_item_kw(k)) return 1;
        if (k == T_LBRACE) {
            depth++;
        } else if (k == T_RBRACE) {
            if (depth == 0) break;
            depth--;
        } else if (k == T_SEMI && depth == 0) {
            p_next();
            break;
        }
        p_next();
    }
    p_err = 0;
    return 0;
}

/* After an error in an item: skips to the next item keyword outside braces (or the end) and clears
 * it. */
static void p_sync_item(void)
{
    long depth = 0;
    for (;;) {
        int k = p_kind();
        if (k == 0) break;
        if (depth <= 0 && is_item_kw(k)) break;
        if (k == T_LBRACE) depth++;
        if (k == T_RBRACE) depth--;
        p_next();
    }
    p_err = 0;
}

static void p_fail_c(const char *expected_display, const char *expected_raw)
{
    Str *display = str_from_cstr((char *)expected_display);
    p_fail(display, str_from_cstr((char *)expected_raw));
}

/* Consumes a token of the kind, or reports E0110 and returns 0. */
static Token *p_expect(int kind)
{
    if (p_err) return 0;
    if (p_kind() == kind) return p_next();
    Str *display = tok_display(kind);
    p_fail(display, tok_raw(kind));
    /* exactly one token fits: insert it only for ) or ] before a token that can follow them
     * (otherwise the fix would not help) */
    if (!p_insert_ok(kind)) return 0;
    Diag *d = p_last_diag();
    if (!d) return 0;
    long at = p_prev_end();
    diag_add_fix(d, (int)cur_file, at, at, tok_raw(kind));
    return 0;
}

/* 1 (after reporting E0115 once) when the native stack is nearly exhausted (or forced, below). */
static int p_deep_at(unsigned long floor)
{
    if ((unsigned long)__builtin_frame_address(0) >= floor + 0x40000) return 0; /* 256 KiB reserve */
    if (!p_err) {
        Token *t = p_tok();
        diag_add("E0115", SEV_ERROR, (int)cur_file, t->start, t->end, str_from_cstr("This code is nested too deeply."));
        p_mark_hard();
    }
    return 1;
}

static int p_deep(void)
{
    return p_deep_at((unsigned long)stack_floor);
}

/* Enters one nesting level (expressions and blocks); 1 (after E0115) beyond 5,000 levels or when the
 * native stack runs low. Every call is paired with p_leave. */
static int p_enter(void)
{
    p_depth++;
    if (p_depth <= 5000) return p_deep();
    return p_deep_at(~0UL >> 1); /* report as if the stack were exhausted */
}

static void p_leave(void)
{
    p_depth--;
}

static int p_failed(void)
{
    return p_err != 0;
}

/* Sets the node's span from start to the end of the previous token. */
static Node *p_finish(Node *n, long start)
{
    n->start = (uint32_t)start;
    n->end = (uint32_t)p_prev_end();
    return n;
}

/* A node starting at the current token. */
static Node *p_node(int kind)
{
    Token *t = p_tok();
    return node_new(kind, t->start, t->end);
}

/* The token's source text as a str. */
static Str *p_tok_str(Token *t)
{
    return str_new(p_text + t->start, (long)t->end - (long)t->start);
}

/* A name (identifier or type name) as an N_NAME node, or 0 after E0110. */
static Node *p_name(void)
{
    int k = p_kind();
    if (k != T_IDENT && k != T_TYPE_IDENT) {
        p_fail_c("a name", "name");
        return 0;
    }
    Node *n = p_node(N_NAME);
    n->a.s = p_tok_str(p_next());
    return n;
}

/* The value of a string literal token (quotes removed, escapes decoded). */
static Str *decode_string(long start, long end)
{
    char *p = p_text + start + 1; /* skip the opening quote */
    char *e = p_text + end;
    char *last = e - 1; /* drop the closing quote (the same character as the opening one) */
    if (last >= p && *last == p[-1]) e = last;
    SB *sb = sb_new();
    while (p < e) {
        int c = (unsigned char)*p++;
        if (c == '\\' && p < e) {
            c = (unsigned char)*p++;
            if (c == 'n')
                c = '\n';
            else if (c == 't')
                c = '\t';
        }
        sb_byte(sb, c);
    }
    return sb_finish(sb);
}

static Node *parse_type(void)
{
    long start = p_start();
    Node *n = p_node(N_TYPE_NODE);
    int k = p_kind();
    long simple = 0;
    switch (k) {
    case T_KINT: simple = TY_INT; break;
    case T_KFLOAT: simple = TY_FLOAT; break;
    case T_KBOOL: simple = TY_BOOL; break;
    case T_KSTR: simple = TY_STR; break;
    case T_KBYTES: simple = TY_BYTES; break; /* spec 021 */
    default: break;
    }
    if (simple) {
        n->a.i = simple;
        p_next();
    } else if (k == T_TYPE_IDENT) {
        n->a.i = TY_NAMED;
        n->b.n = p_name();
        n->c.v = parse_type_args();
    } else if (k == T_KLIST) {
        n->a.i = TY_LIST;
        p_next();
        p_expect(T_LBRACKET);
        n->b.n = parse_type();
        p_expect(T_RBRACKET);
    } else if (k == T_KMAP) {
        n->a.i = TY_MAP;
        p_next();
        p_expect(T_LBRACKET);
        n->b.n = parse_type();
        p_expect(T_COMMA);
        n->c.n = parse_type();
        p_expect(T_RBRACKET);
    } else if (p_coll_kind()) { /* spec 014: ordered_map[K, V], ordered_set[T], priority_queue[T], deque[T] */
        n->a.i = p_coll_kind();
        p_next();
        p_expect(T_LBRACKET);
        n->b.n = parse_type();
        if (n->a.i == TY_OMAP) {
            p_expect(T_COMMA);
            n->c.n = parse_type();
        }
        p_expect(T_RBRACKET);
    } else if (k == T_KRESULT) {
        n->a.i = TY_RESULT;
        p_next();
        p_expect(T_LBRACKET);
        if (p_kind() == T_KVOID) {
            Node *v = p_node(N_TYPE_NODE);
            v->a.i = TY_VOID;
            n->b.n = v;
            p_next();
        } else {
            n->b.n = parse_type();
        }
        p_expect(T_RBRACKET);
    } else if (k == T_FN) { /* fn(types) -> type uses ... */
        n->a.i = TY_FUNC;
        p_next();
        n->b.v = vec_new();
        p_expect(T_LPAREN);
        if (!p_accept(T_RPAREN)) {
            do {
                if (p_failed()) return n;
                vec_push(n->b.v, parse_type());
            } while (p_accept(T_COMMA));
            p_expect(T_RPAREN);
        }
        if (p_accept(T_ARROW)) n->c.n = parse_type();
        n->d.n = parse_uses();
    } else if (k == T_LPAREN) { /* `(fn() -> T) uses e`: the `uses` that follows is not the type's */
        p_next();
        Node *inner = parse_type();
        p_expect(T_RPAREN);
        return inner;
    } else {
        p_fail_c("a type", "type");
        return n;
    }
    p_finish(n, start);
    return n;
}

/* "(" [expr {"," expr}] ")" -> vec of expressions. */
static Vec *parse_args(void)
{
    Vec *v = vec_new();
    p_expect(T_LPAREN);
    if (p_accept(T_RPAREN)) return v;
    do {
        vec_push(v, parse_expr());
        if (p_failed()) return v;
    } while (p_comma_more(T_RPAREN));
    p_expect(T_RPAREN);
    return v;
}

/* new Name[args] { field: expr, ... } after `new` (or after the missing-`new` error), with n made. */
static void parse_record_literal(Node *n)
{
    n->a.n = p_name();
    n->d.v = parse_type_args();
    n->b.v = vec_new();
    p_expect(T_LBRACE);
    if (p_failed()) return;
    do {
        long start = p_start();
        Node *init = p_node(N_FINIT);
        init->a.n = p_name();
        p_expect(T_COLON);
        init->b.n = parse_expr();
        vec_push(n->b.v, p_finish(init, start));
        if (p_failed()) return;
    } while (p_comma_more(T_RBRACE));
    p_expect(T_RBRACE);
}

static Node *parse_primary(void)
{
    long start = p_start();
    int k = p_kind();
    Node *n;
    switch (k) {
    case T_INT: { /* int literal (range errors were reported by the lexer) */
        n = p_node(N_INT);
        Token *t = p_next();
        dec_to_int(p_text + t->start, (long)t->end - (long)t->start, &n->a.i);
        break;
    }
    case T_FLOAT: {
        n = p_node(N_FLOAT);
        Token *t = p_next();
        n->a.i = text_to_float(p_text + t->start, (long)t->end - (long)t->start);
        break;
    }
    case T_STRING: {
        n = p_node(N_STR);
        Token *t = p_next();
        n->a.s = decode_string(t->start, t->end);
        break;
    }
    case T_TRUE:
    case T_FALSE:
        n = p_node(N_BOOL);
        n->a.i = k == T_TRUE;
        p_next();
        break;
    case T_IDENT:
        if (p_kind_at(1) != T_LPAREN) { /* a name */
            n = p_node(N_IDENT);
            n->a.s = p_tok_str(p_next());
            break;
        }
        goto call;
    case T_KMAP: /* `map(xs, f)`: the builtin (`map[K, V]` is a type) */
        if (p_kind_at(1) != T_LPAREN) goto other;
        goto call;
    case T_KSTR:
    case T_KINT:
    case T_KFLOAT:
    call: { /* call (conversion keywords str/int/float are calls too) */
        n = p_node(N_CALL);
        Node *name = p_node(N_NAME);
        name->a.s = p_tok_str(p_next());
        n->a.n = name;
        if (p_kind() != T_LPAREN)
            p_expect(T_LPAREN); /* a bare `str`/`int`/`float`: report the missing "(" */
        else
            n->b.v = parse_args();
        break;
    }
    case T_TYPE_IDENT: /* Type.case or Type.case(expr) */
        if (p_kind_at(1) == T_LBRACE) {
            p_missing_new(); /* `Point { ... }`: records are built with `new` */
            n = p_node(N_NEW_RECORD);
            parse_record_literal(n);
            break;
        }
        n = p_node(N_VCASE);
        n->a.n = p_name();
        p_expect(T_DOT);
        n->b.n = p_name();
        if (!p_failed() && p_accept(T_LPAREN)) {
            n->c.n = parse_expr();
            p_expect(T_RPAREN);
        }
        break;
    case T_LPAREN: /* ( expr ) */
        p_next();
        n = parse_expr();
        p_expect(T_RPAREN);
        if (n) n->flags |= NF_PAREN;
        return n;
    case T_LBRACKET: /* [a, b, ...] */
        n = p_node(N_LIST);
        n->a.v = vec_new();
        p_next();
        if (p_accept(T_RBRACKET)) break;
        do {
            vec_push(n->a.v, parse_expr());
            if (p_failed()) goto done;
        } while (p_comma_more(T_RBRACKET));
        p_expect(T_RBRACKET);
        break;
    case T_NEW: /* new ... */
        p_next();
        if (p_kind() == T_KLIST) {
            n = p_node(N_NEW_LIST);
            p_next();
            p_expect(T_LBRACKET);
            n->a.n = parse_type();
            p_expect(T_RBRACKET);
        } else if (p_kind() == T_KMAP) {
            n = p_node(N_NEW_MAP);
            p_next();
            p_expect(T_LBRACKET);
            n->a.n = parse_type();
            p_expect(T_COMMA);
            n->b.n = parse_type();
            p_expect(T_RBRACKET);
        } else if (p_coll_kind()) { /* spec 014: new ordered_map[K, V] */
            n = p_node(N_NEW_COLL);
            n->a.n = parse_type();
        } else {
            n = p_node(N_NEW_RECORD);
            if (p_kind() == T_TYPE_IDENT)
                parse_record_literal(n);
            else
                p_expect(T_TYPE_IDENT);
        }
        break;
    case T_FN:
        return parse_fn_kind(N_LAMBDA);
    default:
    other:
        if (k == T_LBRACE)
            p_hard_msg("vo has no map literals: create a map with `new map[str, int]`, then `set m[key] = value;`.");
        else
            p_fail_c("an expression", "expression");
        return p_node(N_ERROR);
    }
done:
    return p_finish(n, start);
}

static Node *parse_postfix(void)
{
    long start = p_start();
    Node *n = parse_primary();
    while (!p_failed()) {
        int k = p_kind();
        if (k == T_LPAREN) { /* a call of whatever the expression evaluates to */
            Node *c = node_new(N_CALL_VALUE, 0, 0);
            c->a.n = n;
            c->b.v = parse_args();
            n = p_finish(c, start);
        } else if (k == T_DOT) {
            p_next();
            Node *f = node_new(N_FIELD, 0, 0);
            f->a.n = n;
            f->b.n = p_name();
            /* `x.f(...)`: a field call, or `xs.len()` (no methods): the checker decides */
            if (f->b.n && p_kind() == T_LPAREN) {
                p_finish(f, start);
                n = p_method_call(n, f->b.n, start, f);
            } else {
                n = p_finish(f, start);
            }
        } else if (k == T_LBRACKET) {
            p_next();
            Node *x = node_new(N_INDEX, 0, 0);
            x->a.n = n;
            x->b.n = parse_expr();
            p_expect(T_RBRACKET);
            n = p_finish(x, start);
        } else {
            break;
        }
    }
    return n;
}

static Node *parse_unary(void)
{
    Node *n;
    if (p_enter()) {
        n = p_node(N_ERROR);
    } else {
        long start = p_start();
        int k = p_kind();
        if (k == T_MINUS) {
            n = p_node(N_UNARY);
            n->c.i = k;
            p_next();
            n->a.n = parse_unary();
            p_finish(n, start);
        } else if (k == T_TRY) {
            n = p_node(N_TRY);
            p_next();
            n->a.n = parse_unary();
            p_finish(n, start);
        } else {
            n = parse_postfix();
        }
    }
    p_leave();
    return n;
}

static Node *p_binary(Node *left, int op, long start, Node *right)
{
    Node *n = node_new(N_BINARY, 0, 0);
    n->a.n = left;
    n->b.n = right;
    n->c.i = op;
    return p_finish(n, start);
}

static Node *parse_prod(void)
{
    long start = p_start();
    Node *n = parse_unary();
    while (!p_failed()) {
        int k = p_kind();
        if (k != T_STAR && k != T_SLASH && k != T_PERCENT) break;
        p_next();
        n = p_binary(n, k, start, parse_unary());
    }
    return n;
}

static Node *parse_sum(void)
{
    long start = p_start();
    Node *n = parse_prod();
    while (!p_failed()) {
        int k = p_kind();
        if (k != T_PLUS && k != T_MINUS) break;
        p_next();
        n = p_binary(n, k, start, parse_prod());
    }
    return n;
}

static int is_cmp(int kind)
{
    return kind >= T_EQ && kind <= T_GE;
}

static Node *parse_cmp(void)
{
    long start = p_start();
    Node *n = parse_sum();
    if (p_failed()) return n;
    int k = p_kind();
    if (!is_cmp(k)) return n;
    p_next();
    n = p_binary(n, k, start, parse_sum());
    if (p_failed() || !is_cmp(p_kind())) return n;
    /* a < b < c */
    Str *m = str_from_cstr("Comparisons cannot be chained; combine them with `and`.");
    Token *t = p_tok();
    Diag *d = diag_add(e0110, SEV_ERROR, (int)cur_file, t->start, t->end, m);
    Str *expected = str_from_cstr("and");
    diag_set_ef(d, expected, tok_raw(p_kind()));
    p_mark_hard();
    return n;
}

static Node *parse_not(void)
{
    if (p_deep()) return p_node(N_ERROR);
    if (p_kind() != T_NOT) return parse_cmp();
    long start = p_start();
    Node *n = p_node(N_UNARY);
    n->c.i = T_NOT;
    p_next();
    n->a.n = parse_not();
    return p_finish(n, start);
}

static Node *parse_and(void)
{
    long start = p_start();
    Node *n = parse_not();
    while (!p_failed() && p_accept(T_AND)) n = p_binary(n, T_AND, start, parse_not());
    return n;
}

static Node *parse_expr(void)
{
    long start = p_start();
    Node *n = parse_and();
    while (!p_failed() && p_accept(T_OR)) n = p_binary(n, T_OR, start, parse_and());
    return n;
}

static Node *parse_block(void)
{
    long start = p_start();
    Node *n = p_node(N_BLOCK);
    n->a.v = vec_new();
    p_expect(T_LBRACE);
    if (!p_enter()) {
        for (;;) {
            if (p_failed()) goto out;
            int k = p_kind();
            if (k == T_RBRACE || k == 0) break;
            vec_push(n->a.v, parse_stmt());
            if (p_failed() && p_sync_stmt()) goto out;
        }
        if (!p_failed() && p_kind() == 0) {
            p_unclosed(start); /* the end of the file inside this block: E0111 */
            goto out;
        }
        p_expect(T_RBRACE);
        p_finish(n, start);
    }
out:
    p_leave(); /* paired with p_enter on every path */
    return n;
}

static Node *parse_if(void)
{
    long start = p_start();
    Node *n = p_node(N_IF);
    p_next();
    n->a.n = parse_expr();
    n->b.n = parse_block();
    if (p_elif()) { /* `elif`: vo writes `else if` */
        n->c.n = parse_if();
    } else if (p_accept(T_ELSE)) {
        if (p_kind() == T_IF)
            n->c.n = parse_if();
        else
            n->c.n = parse_block();
    }
    return p_finish(n, start);
}

static Node *parse_stmt(void)
{
    long start = p_start();
    int k = p_kind();
    Node *n;
    switch (k) {
    case T_COMMENT:
        n = p_node(N_COMMENT);
        p_next();
        return n;
    case T_LET:
    case T_VAR: /* let / var */
        n = p_node(N_LET);
        if (k == T_VAR) n->flags = NF_MUT;
        p_next();
        if (p_kind() == T_UNDERSCORE && k != T_VAR) {
            n->flags |= NF_DISCARD;
            p_next();
        } else {
            n->a.n = p_name();
        }
        if (p_accept(T_COLON)) {
            n->b.n = parse_type();
            if (p_kind() == T_SEMI) p_missing_init(n->b.n); /* `var x: int;`: a starting value */
        }
        p_expect(T_ASSIGN);
        n->c.n = parse_expr();
        goto end_stmt;
    case T_SET: { /* set place = expr; */
        n = p_node(N_SET);
        p_next();
        Node *place = parse_postfix();
        n->a.n = place;
        int pk = place->kind;
        if (pk != N_IDENT && pk != N_FIELD && pk != N_INDEX && pk != N_ERROR)
            p_fail_c("a name", "name"); /* set needs a variable, field or index */
        p_compound_assign(n->a.n); /* `set x += 1;` (no compound assignment in vo) */
        p_expect(T_ASSIGN);
        n->b.n = parse_expr();
        goto end_stmt;
    }
    case T_IF:
        return parse_if();
    case T_WHILE:
        n = p_node(N_WHILE);
        p_next();
        n->a.n = parse_expr();
        n->b.n = parse_block();
        goto finish;
    case T_FOR: /* for x in expr [.. expr] block */
        n = p_node(N_FOR);
        p_next();
        if (p_kind() == T_UNDERSCORE) { /* `for _ in a..b` repeats without naming the counter */
            n->flags |= NF_DISCARD;
            p_next();
        } else {
            n->a.n = p_name();
        }
        p_expect(T_IN);
        n->b.n = parse_expr();
        if (p_accept(T_DOTDOT)) n->c.n = parse_expr();
        n->d.n = parse_block();
        goto finish;
    case T_MATCH: /* match expr { case name[(binding)] block ... } */
        n = p_node(N_MATCH);
        p_next();
        n->a.n = parse_expr();
        n->b.v = vec_new();
        p_expect(T_LBRACE);
        for (;;) {
            if (p_failed()) break;
            int a = p_kind();
            if (a == T_RBRACE || a == 0) break;
            if (a == T_COMMENT) {
                vec_push(n->b.v, p_node(N_COMMENT));
                p_next();
                continue;
            }
            long arm_start = p_start();
            Node *arm = p_node(N_ARM);
            arm->e.i = -1;
            p_expect(T_CASE);
            arm->a.n = p_name();
            if (p_accept(T_LPAREN)) {
                if (p_kind() == T_RPAREN) { /* spec 009: `case ok()` */
                    arm->flags |= NF_EMPTY_ARM;
                } else if (p_kind() == T_UNDERSCORE) {
                    arm->flags |= NF_DISCARD;
                    p_next();
                } else {
                    arm->b.n = p_name();
                }
                p_expect(T_RPAREN);
            }
            arm->c.n = parse_block();
            vec_push(n->b.v, p_finish(arm, arm_start));
        }
        p_expect(T_RBRACE);
        goto finish;
    case T_RETURN: /* return [expr]; */
        n = p_node(N_RETURN);
        p_next();
        if (p_kind() != T_SEMI) n->a.n = parse_expr();
        goto end_stmt;
    case T_BREAK:
    case T_CONTINUE: /* break; continue; */
        n = p_node(k == T_BREAK ? N_BREAK : N_CONTINUE);
        p_next();
        goto end_stmt;
    case T_TRY: /* try call; */
        n = p_node(N_EXPR_STMT);
        n->a.n = parse_unary();
        goto end_stmt;
    case T_IDENT:
    case T_KSTR:
    case T_KINT:
    case T_KFLOAT:
    case T_TYPE_IDENT: { /* call statement */
        if (p_compound_stmt()) { /* `x += 1;` */
            n = 0;
            goto finish;
        }
        n = p_node(N_EXPR_STMT);
        n->a.n = parse_expr();
        if (p_failed()) goto finish;
        int ek = n->a.n->kind;
        if (ek == N_CALL || ek == N_CALL_VALUE) goto end_stmt;
        if ((ek == N_IDENT || ek == N_FIELD || ek == N_INDEX) && p_kind() == T_ASSIGN) {
            /* `x = e;`: E0114 with the fix `set x = e;`, parsed as a set */
            Str *m = msg("Assignments start with `set`: write `set $1 = ...;`.", node_text(n->a.n), 0, 0);
            Diag *d = diag_add("E0114", SEV_ERROR, (int)cur_file, n->a.n->start, n->a.n->end, m);
            p_fix(d, start, start, "set ");
            n->kind = N_SET;
            p_next(); /* the `=` */
            n->b.n = parse_expr();
            goto end_stmt;
        }
        Str *m = str_from_cstr("A statement must start with a keyword or be a call."); /* E0114 */
        diag_add("E0114", SEV_ERROR, (int)cur_file, n->a.n->start, n->a.n->end, m);
        p_mark_hard();
        goto finish;
    }
    case T_FN: /* `fn` inside a block: reported, then parsed and dropped */
        p_hard_msg("Functions are declared at the top level: write `let f = fn(...) { ... };` for a local function.");
        n = p_node(N_ERROR);
        parse_fn_kind(N_FN);
        return n;
    default:
        p_fail_c("a statement", "statement");
        return p_node(N_ERROR);
    }
end_stmt:
    p_end_stmt();
finish:
    /* (the compound-statement path finishes a null node, as the assembly did: s0 was 0) */
    if (n) p_finish(n, start);
    return n;
}

/* `requires COND` and `ensures COND` after `uses` (spec 005), in any order, into the FX record's
 * FX_REQUIRES / FX_ENSURES vecs (source order within each kind; vo fmt merges them by position). */
static void parse_clauses(Fx *fx)
{
    while (!p_failed()) {
        int k = p_kind();
        Vec **where;
        if (k == T_REQUIRES)
            where = &fx->requires_;
        else if (k == T_ENSURES)
            where = &fx->ensures;
        else
            break;
        if (!fx->clause_at) fx->clause_at = p_start(); /* fixes to `uses` insert before it */
        p_next();
        Node *cond = parse_expr();
        if (!*where) *where = vec_new();
        vec_push(*where, cond);
    }
}

/* fn / anonymous fn: an anonymous function has a made-up name node ("anonymous function") on the
 * `fn` keyword and the same layout as N_FN. */
static Node *parse_fn_kind(int kind)
{
    Vec *tparams = 0;
    long start = p_start();
    Node *n = p_node(kind);
    if (kind == N_FN) {
        p_next();
        n->a.n = p_name();
        tparams = parse_tparams();
    } else {
        Node *name = p_node(N_NAME);
        name->a.s = str_from_cstr("anonymous function");
        n->a.n = name;
        p_next();
    }
    n->b.v = vec_new();
    p_expect(T_LPAREN);
    if (!p_accept(T_RPAREN)) {
        do {
            if (p_failed()) goto finish;
            long pstart = p_start();
            Node *param = p_node(N_PARAM);
            param->a.n = p_name();
            p_expect(T_COLON);
            param->b.n = parse_type();
            vec_push(n->b.v, p_finish(param, pstart));
        } while (p_accept(T_COMMA));
        p_expect(T_RPAREN);
    }
    long arrow = p_start(); /* where `->` starts */
    if (p_accept(T_ARROW)) {
        if (p_kind() == T_KVOID) /* `-> void`: functions that return nothing have no `->` */
            p_arrow_void(arrow);
        else
            n->c.n = parse_type();
    }
    Node *name = n->a.n; /* the side record hangs off the name node */
    if (name) {
        Fx *fx = alloc(FX_SIZE);
        name->b.fx = fx;
        fx->tparams = tparams;
        fx->uses = parse_uses();
        if (kind == N_LAMBDA) { /* anonymous functions infer their effects (E0507) */
            if (fx->uses) {
                /* E0507: an anonymous function with a `uses` clause; the fix removes it */
                Node *uses = fx->uses;
                long body = p_start(); /* the body's `{` */
                diag_add("E0507", SEV_ERROR, (int)cur_file, uses->start, uses->end,
                         str_from_cstr("Anonymous functions do not declare effects: they get them from their body. Remove the `uses` clause."));
                p_fix((Diag *)diag_last(), uses->start, body, "");
            }
        } else {
            parse_clauses(fx); /* named functions: contract clauses */
        }
    } else {
        parse_uses();
    }
    n->d.n = parse_block();
finish:
    return p_finish(n, start);
}

/* An optional type-parameter list `[T, K: ordered, E: effects]` (vec of N_TPARAM), or 0. */
static Vec *parse_tparams(void)
{
    if (p_kind() != T_LBRACKET) return 0;
    p_next();
    Vec *v = vec_new();
    do {
        if (p_failed()) return v;
        long start = p_start();
        Node *t = p_node(N_TPARAM);
        t->a.n = p_name();
        if (p_accept(T_COLON)) { /* the constraint: `ordered` or `effects` */
            long c = 0;
            if (p_kind() == T_IDENT) {
                if (str_eq_cstr(p_tok_str(p_tok()), "ordered"))
                    c = TPC_ORDERED;
                else if (str_eq_cstr(p_tok_str(p_tok()), "effects"))
                    c = TPC_EFFECTS;
            }
            if (!c) {
                p_fail_c("`ordered` or `effects`", "constraint");
                return v;
            }
            t->b.i = c;
            p_next();
        }
        vec_push(v, p_finish(t, start));
    } while (p_accept(T_COMMA));
    p_expect(T_RBRACKET);
    return v;
}

/* Optional type arguments `[type, ..., uses effects]` after a type name (vec of type nodes, the
 * effect argument an N_USES node), or 0. Spec 026: effects written without `uses` (`Db[file.read]`)
 * are kept as an N_USES node with B = 1, which the checker reports as E0327 with the fix. */
static Vec *parse_type_args(void)
{
    if (p_kind() != T_LBRACKET) return 0;
    p_next();
    Vec *v = vec_new();
    do {
        if (p_failed()) return v;
        int written = p_kind() == T_USES;
        if (written || p_kind() == T_IDENT) { /* effects; T_IDENT: an effect name, `uses` missing */
            long start = p_start();
            Node *u = p_node(N_USES);
            u->a.v = vec_new();
            u->b.i = !written;
            if (written) p_next();
            while (!p_failed()) {
                Node *e = parse_effect();
                if (!e) break;
                vec_push(u->a.v, e);
                if (p_kind() != T_COMMA || p_kind_at(1) != T_IDENT) break; /* the next is a type: E0327 */
                p_next();
            }
            vec_push(v, p_finish(u, start));
            continue;
        }
        vec_push(v, parse_type());
    } while (p_accept(T_COMMA));
    p_expect(T_RBRACKET);
    return v;
}

/* One effect: a name (`file.read` is two names joined by `.`) with optional scope strings, or an
 * effect parameter (a type name). N_EFFECT: A bit (0 = unknown), B scope N_STR nodes, C the effect
 * parameter's name node, D the name text. */
static Node *parse_effect(void)
{
    long start = p_start();
    Node *n = p_node(N_EFFECT);
    int k = p_kind();
    if (k == T_TYPE_IDENT) { /* effect parameter */
        n->c.n = p_name();
        n->d.s = n->c.n->a.s;
        return p_finish(n, start);
    }
    if (k != T_IDENT) {
        p_expect(T_IDENT);
        return 0;
    }
    Str *name = p_tok_str(p_next()); /* the name text */
    if (p_kind() == T_DOT && p_kind_at(1) == T_IDENT) {
        p_next(); /* `.` */
        Str *second = p_tok_str(p_next());
        SB *sb = sb_new();
        sb_str(sb, name);
        sb_byte(sb, '.');
        sb_str(sb, second);
        name = sb_finish(sb);
        start = n->start;
    }
    n->d.s = name;
    n->a.i = es_bit_by_name(name->data, name->len);
    if (p_accept(T_LPAREN)) {
        n->b.v = vec_new();
        do {
            if (p_kind() != T_STRING) {
                p_expect(T_STRING);
                return p_finish(n, start);
            }
            Node *s = p_node(N_STR);
            Token *t = p_next();
            s->a.s = decode_string(t->start, t->end);
            vec_push(n->b.v, s);
        } while (p_accept(T_COMMA));
        p_expect(T_RPAREN);
    }
    return p_finish(n, start);
}

/* An optional effect clause `uses e, e(...), ...` as an N_USES node, or 0 when there is none. */
static Node *parse_uses(void)
{
    if (p_kind() != T_USES) return 0;
    long start = p_start();
    Node *n = p_node(N_USES);
    p_next();
    n->a.v = vec_new();
    while (!p_failed()) {
        Node *e = parse_effect();
        if (!e) break;
        vec_push(n->a.v, e);
        if (p_kind() != T_COMMA) break;
        if (p_kind_at(2) == T_COLON) break; /* `, name: type`: a function type's clause ended */
        p_next();
    }
    return p_finish(n, start);
}

/* The body of a record (kind N_FIELD_DECL: "name: type;") or variant (N_CASE_DECL: "name[(type)];"). */
static Vec *parse_members(int kind)
{
    Vec *v = vec_new();
    p_expect(T_LBRACE);
    for (;;) {
        if (p_failed()) return v;
        int k = p_kind();
        if (k == T_RBRACE || k == 0) break;
        if (k == T_COMMENT) {
            vec_push(v, p_node(N_COMMENT));
            p_next();
            continue;
        }
        long start = p_start();
        Node *m = p_node(kind);
        m->a.n = p_name();
        if (kind == N_FIELD_DECL) {
            p_expect(T_COLON);
            m->b.n = parse_type();
        } else if (p_accept(T_LPAREN)) {
            m->b.n = parse_type();
            p_expect(T_RPAREN);
        }
        if (!p_member_comma()) p_end_stmt(); /* `x: int,`: members end with `;` */
        vec_push(v, p_finish(m, start));
    }
    p_expect(T_RBRACE);
    return v;
}

static Node *parse_item(void)
{
    long start = p_start();
    int k = p_kind();
    Node *n;
    switch (k) {
    case T_COMMENT:
        n = p_node(N_COMMENT);
        p_next();
        return n;
    case T_FN:
        return parse_fn_kind(N_FN);
    case T_RECORD:
    case T_VARIANT:
        n = p_node(k == T_RECORD ? N_RECORD : N_VARIANT);
        p_next();
        n->a.n = p_name();
        n->c.v = parse_tparams();
        n->b.v = parse_members(k == T_RECORD ? N_FIELD_DECL : N_CASE_DECL);
        break;
    case T_TEST:
        n = p_node(N_TEST);
        p_next();
        if (p_kind() != T_STRING) {
            p_expect(T_STRING);
            break;
        }
        {
            Token *t = p_next();
            n->a.s = decode_string(t->start, t->end);
        }
        n->c.n = parse_uses();
        n->b.n = parse_block();
        break;
    case T_PROP: { /* prop "name" (param, ...) uses? block: N_TEST + NF_PROP */
        n = p_node(N_TEST);
        n->flags |= NF_PROP;
        Fx *fx = alloc(FX_SIZE); /* the parameters live in the side record */
        n->d.fx = fx;
        fx->params = vec_new();
        p_next();
        if (p_kind() != T_STRING) {
            p_expect(T_STRING);
            break;
        }
        {
            Token *t = p_next();
            n->a.s = decode_string(t->start, t->end);
        }
        p_expect(T_LPAREN);
        do {
            if (p_failed()) goto finish;
            long pstart = p_start();
            Node *param = p_node(N_PARAM);
            param->a.n = p_name();
            p_expect(T_COLON);
            param->b.n = parse_type();
            vec_push(fx->params, p_finish(param, pstart));
        } while (p_accept(T_COMMA));
        p_expect(T_RPAREN);
        n->c.n = parse_uses();
        fx->uses = n->c.n;
        n->b.n = parse_block();
        break;
    }
    case T_EXPORT: { /* spec 010: export fn|record|variant */
        p_next();
        int k2 = p_kind();
        if (k2 != T_FN && k2 != T_RECORD && k2 != T_VARIANT) {
            p_fail_c("`fn`, `record` or `variant` after `export`", "item");
            return p_node(N_ERROR);
        }
        n = parse_item();
        n->flags |= NF_EXPORT;
        n->start = (uint32_t)start; /* the item's span starts at `export` */
        return n;
    }
    case T_AT: { /* spec 012: @NAME("ID", ...) on the line before an item */
        n = p_node(N_ANNOTATION);
        p_next();
        n->a.n = p_name();
        n->b.v = vec_new();
        if (!n->a.n || !p_expect(T_LPAREN)) goto finish;
        while (p_kind() != T_RPAREN) {
            if (p_kind() != T_STRING) {
                p_expect(T_STRING);
                goto finish;
            }
            Node *s = p_node(N_STR);
            Token *t = p_next();
            s->a.s = decode_string(t->start, t->end);
            vec_push(n->b.v, s);
            if (!p_accept(T_COMMA)) break;
        }
        p_expect(T_RPAREN);
        p_finish(n, start);
        int next = p_kind();
        if (next == T_EOF || next == T_COMMENT) /* E0117: nothing (or a comment) follows it */
            diag_add("E0117", SEV_ERROR, (int)cur_file, n->start, n->end,
                     str_from_cstr("An annotation goes on the line right above the `test` or `prop` it annotates."));
        return n;
    }
    case T_IMPORT: /* import a.b.name; */
        n = p_node(N_IMPORT);
        p_next();
        n->a.v = vec_new();
        for (;;) {
            /* spec 021: the module std.bytes, whose name is the keyword `bytes`, as a path segment */
            Node *name = p_kind() == T_KBYTES && n->a.v->n ? p_node(N_NAME) : p_name();
            if (!name) goto finish;
            if (name->kind == N_NAME && !name->a.s) name->a.s = p_tok_str(p_next());
            n->b.n = name;
            vec_push(n->a.v, name->a.s);
            if (!p_accept(T_DOT)) break;
        }
        p_end_stmt();
        break;
    default:
        p_fail_c("`fn`, `record`, `variant`, `test` or `import`", "item");
        return p_node(N_ERROR);
    }
finish:
    return p_finish(n, start);
}

/* E0110 for a pending `obj.name(args)` with its rewrite fix (stored by p_method_call). */
static void p_method_diag(Node *call)
{
    W *info = call->e.p; /* message, start, end, replacement, file */
    Diag *d = diag_add(e0110, SEV_ERROR, (int)info[4].i, info[1].i, info[2].i, info[0].s);
    diag_add_fix(d, (int)info[4].i, info[1].i, info[2].i, info[3].s);
}

/* Parses a lexed file; stores and returns its N_FILE_NODE. */
Node *parse_file(int file)
{
    cur_file = file;
    FileRec *f = file_get(file);
    p_toks = f->tokens;
    p_text = f->text;
    p_pos = 0;
    p_err = 0;
    p_hard = 0;
    Node *n = node_new(N_FILE_NODE, 0, f->len);
    f->ast = n;
    n->a.v = vec_new();
    p_methods = vec_new();
    while (p_kind()) {
        vec_push(n->a.v, parse_item());
        if (p_failed()) p_sync_item();
    }
    if (p_hard) /* the checker will not run: report `obj.name(...)` as v1 did */
        for (long i = 0; i < p_methods->n; i++) p_method_diag(vec_get(p_methods, i));
    return n;
}

/* The source text of a node in the file being parsed. */
static Str *node_text(Node *n)
{
    return str_new(p_text + n->start, (long)n->end - (long)n->start);
}

/* E0111 at the end of the file for the block opened at open_start, with a fix inserting "}". */
static void p_unclosed(long open_start)
{
    long at = p_start(); /* the EOF token */
    Diag *d = p_soft_err("E0111", at, at, "Expected `}` before the end of the file.");
    p_fix(d, at, at, "}\n");
    diag_add_related(d, (int)cur_file, open_start, open_start + 1, str_from_cstr("This block is never closed."));
    p_mark_hard();
}

/* The most recent diagnostic when it is an E0110 at the current token, else 0. */
static Diag *p_last_diag(void)
{
    Diag *d = diag_last();
    if (!d) return 0;
    if (d->code != e0110 || d->start != p_start()) return 0;
    return d;
}

/* After `set place`: an operator followed by `=` (`+=`, `-=`, ...) is E0110 with a fix that rewrites
 * it to `= place op`. */
static void p_compound_assign(Node *place)
{
    int op = p_kind();
    if (op < T_PLUS || op > T_PERCENT || p_kind_at(1) != T_ASSIGN) return;
    long start = p_tok()->start; /* span: the operator and the `=` */
    p_next();
    long end = p_tok()->end;
    Str *text = node_text(place);
    Str *m = msg("vo has no compound assignment: write `set $1 = $1 $2 ...;`.", text, tok_raw(op), 0);
    Diag *d = diag_add(e0110, SEV_ERROR, (int)cur_file, start, end, m);
    SB *sb = sb_new(); /* replacement: "= <place> <op>" */
    sb_byte(sb, '=');
    sb_byte(sb, ' ');
    sb_str(sb, text);
    sb_byte(sb, ' ');
    sb_str(sb, tok_raw(op));
    diag_add_fix(d, (int)cur_file, start, end, sb_finish(sb));
    p_mark_hard();
}

/* Whether inserting the expected token before the current one is a plausible repair. */
static int p_insert_ok(int expected)
{
    if (expected != T_RPAREN && expected != T_RBRACKET) return 0;
    Token *t = p_tok();
    if (t->flags & TF_NEWLINE_BEFORE) return 1;
    int k = t->kind;
    return k == T_SEMI || k == T_LBRACE || k == T_RPAREN || k == T_RBRACKET || k == T_COMMA;
}

/* A hard syntax error with a custom message at the current token. */
static void p_hard_msg(const char *message)
{
    if (p_err) return;
    Token *t = p_tok();
    p_soft_err(e0110, t->start, t->end, message);
    p_mark_hard();
}

/* `obj.name(args)`: an N_CALL_VALUE of the field, flagged NF_METHOD, holding v1's rewrite `name(obj,
 * args)` (N_D) and its E0110 message, span and fix (N_E: message, start, end, replacement, file).
 * The checker reports E0110 unless the field has a function type; so does parse_file when the checker
 * will not run. */
static Node *p_method_call(Node *object, Node *name, long start, Node *field)
{
    long args_start = p_tok()->end; /* just after "(" */
    Vec *args = parse_args();
    long close = p_prev_end(); /* the closing ")" */
    Vec *all = vec_new(); /* args: the object first */
    vec_push(all, object);
    for (long i = 0; i < args->n; i++) vec_push(all, vec_get(args, i));
    Node *call = node_new(N_CALL, start, close);
    call->a.n = name;
    call->b.v = all;
    SB *sb = sb_new(); /* replacement text: name(obj[, args]) */
    sb_str(sb, name->a.s);
    sb_byte(sb, '(');
    sb_str(sb, node_text(object));
    if (close - 1 > args_start) { /* the original argument text, if any */
        sb_cstr(sb, ", ");
        sb_bytes(sb, p_text + args_start, close - 1 - args_start);
    }
    sb_byte(sb, ')');
    Str *replacement = sb_finish(sb);
    Str *m = msg("vo has no methods: write `$1`.", replacement, 0, 0);
    W *info = alloc(40); /* E0110 info */
    info[0].s = m;
    info[1].i = start;
    info[2].i = call->end;
    info[3].s = replacement;
    info[4].i = cur_file;
    Node *value = node_new(N_CALL_VALUE, start, call->end); /* the call as written */
    value->a.n = field;
    value->b.v = vec_new(); /* its own arguments: the rewrite's without the object */
    for (long i = 1; i < call->b.v->n; i++) vec_push(value->b.v, vec_get(call->b.v, i));
    value->d.n = call;
    value->e.p = info;
    value->flags = NF_METHOD;
    vec_push(p_methods, value);
    return value;
}

/* When the current token is `elif`: E0110 with a fix to `else if` (1; the caller parses it as if). */
static int p_elif(void)
{
    if (p_kind() != T_IDENT) return 0;
    Token *t = p_tok();
    if (!str_eq_cstr(p_tok_str(t), "elif")) return 0;
    Diag *d = p_soft_err(e0110, t->start, t->end, "vo writes `else if`, not `elif`.");
    p_fix(d, t->start, t->end, "else if");
    return 1;
}

/* At `Type {`: E0110 with a fix inserting `new ` (the caller parses the record literal). */
static void p_missing_new(void)
{
    Token *t = p_tok();
    Str *m = msg("Records are built with `new`: `new $1 { ... }`.", p_tok_str(t), 0, 0);
    Diag *d = diag_add(e0110, SEV_ERROR, (int)cur_file, t->start, t->end, m);
    p_fix(d, t->start, t->start, "new ");
}

/* A `,` where a field or case should end with `;`: E0112 with a fix replacing it (consumed, 1). */
static int p_member_comma(void)
{
    if (p_kind() != T_COMMA) return 0;
    Token *t = p_tok();
    Diag *d = p_soft_err("E0112", t->start, t->end, "Fields and cases end with `;`, not `,`.");
    p_fix(d, t->start, t->end, ";");
    p_next();
    return 1;
}

/* `var x: T;`: a hard E0110 with a fix inserting a starting value for T when there is an obvious
 * one. */
static void p_missing_init(Node *type_node)
{
    long at = p_prev_end();
    Diag *d = p_soft_err(e0110, at, at, "A variable needs a starting value, like `var n: int = 0;`.");
    const char *value = 0;
    switch (type_node->a.i) {
    case TY_INT: value = " = 0"; break;
    case TY_FLOAT: value = " = 0.0"; break;
    case TY_BOOL: value = " = false"; break;
    case TY_STR: value = " = \"\""; break;
    default: break;
    }
    if (value) {
        p_fix(d, at, at, value);
    } else if (type_node->a.i == TY_LIST || type_node->a.i == TY_MAP) {
        SB *sb = sb_new(); /* " = new list[T]" / " = new map[K, V]" */
        sb_cstr(sb, " = new ");
        sb_str(sb, node_text(type_node));
        Str *text = sb_finish(sb);
        long here = p_prev_end();
        diag_add_fix(d, (int)cur_file, here, here, text);
    }
    p_mark_hard();
}

/* `-> void`: E0110 with a fix removing it (soft: the function returns nothing). */
static void p_arrow_void(long arrow_start)
{
    long end = p_tok()->end; /* end of `void` */
    p_next();
    long from = arrow_start - 1; /* include the space before `->` */
    if (p_text[from] != ' ') from = arrow_start;
    Diag *d = p_soft_err(e0110, arrow_start, end, "A function that returns nothing has no `->`: remove `-> void`.");
    p_fix(d, from, end, "");
}

/* At a statement `x += e;`: hard E0114 with a fix to `set x = x + e;`. 1 when it applied. */
static int p_compound_stmt(void)
{
    int op = p_kind_at(1);
    if (op < T_PLUS || op > T_PERCENT || p_kind_at(2) != T_ASSIGN) return 0;
    Token *t = p_tok(); /* the name token */
    Str *name = p_tok_str(t);
    Str *m = msg("Assignments start with `set`, and vo has no compound assignment: write `set $1 = $1 $2 ...;`.",
                 name, tok_raw(op), 0);
    Diag *d = diag_add("E0114", SEV_ERROR, (int)cur_file, t->start, t[2].end, m); /* through the `=` */
    SB *sb = sb_new(); /* "set x = x +" */
    sb_cstr(sb, "set ");
    sb_str(sb, name);
    sb_cstr(sb, " = ");
    sb_str(sb, name);
    sb_byte(sb, ' ');
    sb_str(sb, tok_raw(op));
    diag_add_fix(d, (int)cur_file, t->start, t[2].end, sb_finish(sb));
    p_mark_hard();
    return 1;
}
