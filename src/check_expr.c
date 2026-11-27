/* Checker, expressions: typing, operator selection, calls and builtins (contracts/grammar.md;
 * ported from src/check_expr.s, spec 008). */
#include "vo.h"

static int is_ok_or_err(Node *n); /* spec 056 */

#define B_EXIT 9
#define B_STR 11
#define B_INT 12
#define B_FLOAT 13
#define B_LEN 16
#define B_SLICE 17
#define B_PUSH 23
#define B_POP 24
#define B_SORT 25
#define B_HAS 26
#define B_REMOVE 27
#define B_KEYS 28
#define B_GET_OR 55 /* spec 009 */
#define B_EXPECT 29
#define B_EXPECT_EQ 30
#define B_OK 31
#define B_ERR 32
#define B_ABS 33
#define B_MIN 34
#define B_MAX 35
#define B_SERVE_CALL 59 /* spec 011 */
#define B_COUNT 119 /* names up to here (50, 51 and 83 are internal and unnamed) */

static const char *const builtin_names[B_COUNT + 1] = {
    0,          "print",       "write",     "eprint",  "read_line",    "read_all",     "read_file",
    "write_file", "args",      "exit",      "now_ms",  "str",          "int",          "float",
    "parse_int", "parse_float", "len",      "slice",   "find",         "split",        "join",
    "trim",     "chars",       "push",      "pop",     "sort",         "has",          "remove",
    "keys",     "expect",      "expect_eq", "ok",      "err",          "abs",          "min",
    "max",      "upper",       "lower",     "ord",     "chr",          "env",          "random_int",
    "sort_by",  "map",         "filter",    "fold",    "any",          "all",          "todo",
    "assume",   0,             0,           "__float_text", "__http_exchange", "__text_float", "get_or",
    "__serve_listen", "__serve_next", "__serve_reply", "__serve_call", "__http_part", "__coll",
    "concurrent_map", "__fs", "__run", "__https_exchange", "__math", "__math_int",
    "__bytes_of_text", "__text_of_bytes", "__bytes_of_ints", "__ints_of_bytes", "__bytes_hex", "__bytes_unhex",
    "__bytes_b64", "__bytes_unb64", "__stdin_bytes", "__stdout_bytes", "__fsb", "__run_bytes", "__raw_text", /* spec 021 */
    "__sleep", /* spec 022 */
    "format", 0, /* spec 024: format; 83 the internal piece */
    "__re_compile", "__re", "__re_repl", /* spec 025 */
    "__sql_open", "__sql_close", "__sql_exec", "__sql_query", "__sql_tx", "__sql_prepare", /* spec 026 */
    "__serve_workers", /* spec 028 */
    "__to_json", "__from_json", "__json_fields", "__json_text", /* spec 029 */
    "__json_parse", "__json_write",
    "byte_at", "__http_timeout", "map_err",
    "__sha256", "__hmac_sha256", "__pbkdf2_sha256", "__bytes_same", "__entropy", /* spec 035 */
    "__tcp", "__tcp_io", /* spec 039 */
    "__fs_block", "__fs_lines", "__proc_start", "__proc", /* spec 040 */
    "__bits", /* spec 043 */
    "__worker", /* spec 047 */
    "__tz", "__local_tz", /* spec 048 */
    "__log", "__metric", /* spec 055 */
};

/* Fixed signatures: argument codes, '>', result code. i int, f float, b bool, s str, v void,
 * S list[str], r result[str], u result[void], n result[int], x result[float]. */
static const char *const builtin_sigs[B_COUNT + 1] = {
    [1] = "s>v",   [2] = "s>v",   [3] = "s>v",   [4] = ">r",    [5] = ">s",     [6] = "s>r",
    [7] = "ss>u",  [8] = ">S",    [9] = "i>v",   [10] = ">i",   [12] = "f>i",   [13] = "i>f",
    [14] = "s>n",  [15] = "s>x",  [18] = "ss>i", [19] = "ss>S", [20] = "Ss>s",  [21] = "s>s",
    [22] = "s>S",  [29] = "b>v",  [36] = "s>s",  [37] = "s>s",  [38] = "s>i",   [39] = "i>s",
    [40] = "s>r",  [41] = "ii>i", [49] = "b>v",  [52] = "f>s",  [53] = "sis>r", [54] = "s>f",
    [56] = "s>n",  [57] = "i>r",  [58] = "s>v",  [60] = "si>s",  [63] = "iss>r", [64] = "SsS>r", [65] = "sis>r",
    [66] = "iff>f", [67] = "if>i", /* spec 020: __math, __math_int */
    [68] = "s>y",   [69] = "y>r",  [70] = "I>y",  [71] = "y>I",  [72] = "y>s",  [73] = "s>Y",
    [74] = "y>s",   [75] = "s>Y",  [76] = ">y",   [77] = "y>v",  [78] = "isy>Y", [79] = "SyS>Y", [80] = "y>s", [81] = "i>v", /* spec 021: y bytes, Y result[bytes], I list[int] */
    [84] = "s>r", [85] = "iss>I", [86] = "ss>I", /* spec 025: __re_compile, __re, __re_repl */
    [87] = "si>n", [88] = "i>v", [91] = "ii>u", [92] = "s>r", /* spec 026; 89, 90 typed loosely */
    [100] = "si>i", /* byte_at */
    [101] = "i>v",  /* __http_timeout */
    [103] = "y>y", [104] = "yy>y", [105] = "yyi>y", [106] = "yy>b", [107] = "i>y", /* spec 035 */
    [108] = "iis>n", [109] = "iiy>Y", /* spec 039 */
    [110] = "sii>Y", [112] = "SsS>n", [113] = "ii>r", /* spec 040; 111 typed loosely */
    [114] = "iii>i", /* spec 043 */
    [116] = "is>Y", [117] = ">y", /* spec 048 */
    [118] = "iss>s", [119] = "isf>s", /* spec 055 */
};

#define M_NO_FIELD "`$1` has no field `$2`."
#define M_NO_CASE "`$1` is not a case of `$2`."

static int ok_from_value; /* spec 029: the next ok(v) takes its type from v (compared with a generic call) */

Node *ck_stmt_call; /* the call of the expression statement being checked (hole statements) */

/* spec 023: a builtin's name by id, or 0 (unnamed, out of range). */
const char *builtin_name_of(long id)
{
    return id >= 1 && id <= B_COUNT ? builtin_names[id] : 0;
}

int builtin_id(Str *name)
{
    for (int id = 1; id <= B_COUNT; id++)
        if (builtin_names[id] && str_eq_cstr(name, (char *)builtin_names[id])) return id;
    return 0;
}

/* The type for a builtin signature code. */
Type *code_type(int c)
{
    switch (c) {
    case 'i': return t_int;
    case 'f': return t_float;
    case 'b': return t_bool;
    case 's': return t_str;
    case 'v': return t_void;
    case 'S': return ty_list(t_str);
    case 'r': return ty_result(t_str);
    case 'u': return ty_result(t_void);
    case 'n': return ty_result(t_int);
    case 'y': return t_bytes; /* spec 021 */
    case 'Y': return ty_result(t_bytes);
    case 'I': return ty_list(t_int);
    default: return ty_result(t_float);
    }
}

/* The source text covered by a node. */
Str *src_text(Node *node)
{
    FileRec *f = file_get((int)node->file);
    return str_new(f->text + node->start, (long)node->end - node->start);
}

/* Adds a fix wrapping the expression in str(...), float(...) or int(...) when that converts it. */
void suggest_conversion(Diag *d, Type *expected, Type *found, Node *node)
{
    long e = expected->kind, f = found->kind;
    char *fn;
    if (e == TY_STR && (f == TY_INT || f == TY_FLOAT || f == TY_BOOL))
        fn = "str(";
    else if (e == TY_FLOAT && f == TY_INT)
        fn = "float(";
    else if (e == TY_INT && f == TY_FLOAT)
        fn = "int(";
    else
        return;
    SB *sb = sb_new();
    sb_cstr(sb, fn);
    sb_str(sb, src_text(node));
    sb_byte(sb, ')');
    diag_add_fix(d, (int)node->file, node->start, node->end, sb_finish(sb));
}

Type *bad_type(void)
{
    return t_error;
}

/* E0301 "Expected <one of>, found `T`." Returns the error type. */
Type *err_one_of(Node *node, char *expected_text, Type *found)
{
    if (!ty_bad(found)) {
        Str *fname = ty_name(found);
        Str *etext = str_from_cstr(expected_text);
        ck_err("E0301", node, msg("Expected $1, found `$2`.", etext, fname, 0));
    }
    return t_error;
}

/* Checks argument i against an expected type (0 = none); returns its type. */
Type *check_arg(Node *call, long i, Type *expected)
{
    Node *arg = vec_get(call->b.v, i);
    Type *t = check_expr(arg, expected);
    if (!expected) return t;
    expect_type(arg, expected, t);
    return expected;
}

/* Checks every argument without an expected type. */
static void check_args_untyped(Node *call)
{
    long n = call->b.v->n;
    for (long i = 0; i < n; i++) check_arg(call, i, 0);
}

/* E0302 when the call has a different number of arguments (then checks them all, untyped). 1 = ok. */
int check_argc(Node *call, long want)
{
    long got = call->b.v->n;
    if (got == want) return 1;
    Str *name = call_name(call);
    Str *w = int_to_str(want);
    Str *g = int_to_str(got);
    ck_err("E0302", call, msg("`$1` takes $2 argument(s), found $3.", name, w, g));
    check_args_untyped(call);
    return 0;
}

/* A builtin with a fixed signature string. */
Type *check_sig_builtin(Node *call, char *sig)
{
    long n = 0; /* the argument codes */
    while (sig[n] != '>') n++;
    if (check_argc(call, n))
        for (long i = 0; i < n; i++) check_arg(call, i, code_type(sig[i]));
    return code_type(sig[n + 1]);
}

/* todo(desc) has the expected type (research R1-R4): W0601 with that type and the locals in scope,
 * or E0601 when there is no expected type. A statement hole needs the function's return type. */
static Type *check_todo(Node *call, Type *expected)
{
    if (!check_argc(call, 1)) return t_error;
    check_arg(call, 0, t_str);
    Fx *fx = (Fx *)ck_fx;
    if (fx) fx->holes = 1;
    if (ck_stmt_call == call) expected = ck_ret && ck_ret != t_collect ? ck_ret : t_void; /* `todo(...);` */
    if (ty_bad(expected)) {
        ck_err("E0601", call,
               str_from_cstr("Cannot tell what type this hole must have; annotate the binding: `let x: T = todo(...)`."));
        return t_error;
    }
    Str *desc = src_text(vec_get(call->b.v, 0)); /* W0601: Hole: `<desc>` needs a value of type `T`. */
    Str *tname = ty_name(expected);
    Diag *d = ck_warn("W0601", call, msg("Hole: `$1` needs a value of type `$2`.", desc, tname, 0));
    diag_set_ef(d, tname, 0);
    Vec *locals = vec_new(); /* the locals in scope, innermost first: {name, type name} */
    diag_set_scope(d, locals);
    for (Sym *s = ck_scope; s; s = s->next) {
        if (s->kind != SYM_LOCAL) continue;
        W *pair = alloc(16);
        pair[0].s = s->name;
        pair[1].s = ty_name(s->type);
        vec_push(locals, pair);
    }
    return expected;
}

/* The first argument node of a call. */
static Node *arg0(Node *call)
{
    return vec_get(call->b.v, 0);
}

/* A concrete ordered type: int, float, str, or a record of ordered fields (spec 014). Records without
 * an order report E0313 naming the field (and count as handled). */
int is_ordered_type(Type *t, Node *at)
{
    if (t->kind == TY_INT || t->kind == TY_FLOAT || t->kind == TY_STR || t->kind == TY_BYTES || t->kind == TY_BOOL) return 1;
    if (t->kind != TY_RECORD && t->kind != TY_VARIANT) return 0; /* spec 056: variants of ordered payloads */
    if (!ty_ordered(t)) order_error(t, at, 0);
    return 1;
}

/* ok/err without a known result type: E0307. */
static Type *infer_error(Node *call, Type *expected)
{
    check_args_untyped(call);
    if (expected && ty_bad(expected)) return t_error;
    ck_err1("E0307", call, "Cannot infer the type of `$1(...)` here. Add a type annotation.", call->a.n->a.s);
    return t_error;
}

/* W0603: a literal or (spec 031) a constant: a value that cannot depend on the program. */
static int fixed_operand(Node *n)
{
    if (node_is_literal(n) || (n->flags & NF_CONST)) return 1;
    Sym *s = n->kind == N_IDENT ? scope_find(n->a.s) : 0;
    return s && s->kind == SYM_CONST;
}

/* Spec 042 (W0102): a raw string whose text has no backslash, no quote and no line break says
 * nothing a quoted string does not: one spelling per text, with the quoted form as the fix. */
static void raw_string_check(Node *n)
{
    if (n->flags & NF_CONST) return;
    FileRec *f = file_get((int)n->file);
    if (n->end <= n->start || f->text[n->start] != '`' || f->text[n->end - 1] != '`') return;
    Str *v = n->a.s;
    for (long i = 0; i < v->len; i++)
        if (v->data[i] == '\\' || v->data[i] == '"' || v->data[i] == '\n') return;
    Diag *d = ck_warn("W0102", n, str_from_cstr("This raw string needs no backticks: its text has no backslash, quote or line break. Write it in quotes."));
    SB *sb = sb_new();
    render_str_lit(sb, v);
    diag_add_fix(d, (int)n->file, n->start, n->end, sb_finish(sb));
}

Type *check_builtin(Node *call, long id, Type *expected)
{
    call->d.i = id;
    fx_builtin(id);
    if (id == B_FS) fx_builtin_fs(call); /* spec 017: the effect of __fs depends on its operation */
    if (id == B_FSB) fx_builtin_fsb(call); /* spec 021: the same for __fsb */
    if (id == B_SQL_OPEN) fx_builtin_sql_open(call); /* spec 026: by mode */
    if (id == B_TCP) fx_builtin_tcp(call); /* spec 039: by operation */
    if (id == B_RUN || id == B_RUN_BYTES || id == B_PROC_START) fx_builtin_run(call); /* spec 057: by directory */
    fx_builtin_literal(call, id); /* spec 013 (L1) */
    if (id == B_MAP_ERR) return check_map_err(call); /* spec 032 */
    if ((id >= B_SORT_BY && id <= B_ALL) || id == B_CMAP) return check_hof(call, id); /* sort_by, map, filter, fold, any, all, concurrent_map */
    if (id >= B_EXPECT && id <= B_EXPECT_EQ && !ck_in_test)          /* expect / expect_eq only in tests */
        ck_err1("E0410", call, "`$1` can only be used inside a `test` block.", call->a.n->a.s);
    if (id >= B_EXPECT && id <= B_EXPECT_EQ && ck_in_test) { /* spec 018: W0602 counts checks; W0603 */
        ck_test_checks++;
        Vec *args = call->b.v;
        Node *c = args->n ? vec_get(args, 0) : 0;
        int fixed = 0;
        if (c && id == B_EXPECT) {
            if (c->kind == N_UNARY && c->a.n->kind == N_BOOL) c = c->a.n; /* not true */
            fixed = fixed_operand(c) || (c->kind == N_BINARY && fixed_operand(c->a.n) && fixed_operand(c->b.n));
        } else if (c && args->n == 2) {
            fixed = fixed_operand(c) && fixed_operand(vec_get(args, 1));
        }
        if (fixed) ck_warn("W0603", call, str_from_cstr("This check always has the same outcome: it does not depend on the program."));
    }
    if (id == B_TODO && ck_in_test) ck_test_holes++;
    if (id == B_ASSUME && ck_in_test != 2) /* assume only in properties */
        ck_err1("E0604", call, "`$1` can only be used inside a `prop` block.", call->a.n->a.s);
    if ((id == B_INT || id == B_FLOAT) && call->b.v->n == 1) { /* of a str: point to parse_int / parse_float */
        Node *arg = arg0(call);
        Type *t = check_expr(arg, 0);
        if (ty_is(t, TY_STR))
            ck_err("E0301", call,
                   msg("`$1` converts numbers; to read a number from text use `parse_$1` (it returns a `result`).",
                       call->a.n->a.s, 0, 0));
        else /* the usual argument check, without checking it twice */
            expect_type(arg, code_type(id == B_INT ? 'f' : 'i'), t);
        return id == B_INT ? t_int : t_float;
    }
    if (id == B_FORMAT) return check_format_call(call); /* spec 024: src/check_format.c */
    if (builtin_sigs[id]) return check_sig_builtin(call, (char *)builtin_sigs[id]);
    if (id == B_COLL || id == B_WORKER) { /* spec 047: __worker likewise (std.workers only); spec 014: __coll(OP, ...): OP an int literal; the result takes the expected type.
                           ponytail: typed loosely (std.collections only, whose wrappers declare the
                           public types); typed primitives if the library grows */
        Vec *args = call->b.v;
        if (!args->n || ((Node *)vec_get(args, 0))->kind != N_INT) return infer_error(call, expected);
        for (long i = 0; i < args->n; i++) {
            Type *at = check_arg(call, i, 0);
            /* spec 047: __worker(1, body) runs body (in a process of its own): its effects are the caller's */
            if (id == B_WORKER && i == 1 && ((Node *)vec_get(args, 0))->a.i == 1 && at && at->kind == TY_FUNC)
                fx_value_call(at->decl.p);
        }
        return expected ? expected : t_void;
    }
    if (id == B_SQL_EXEC || id == B_SQL_QUERY) { /* spec 026: (id, text, params: list[Value]); the result
                                                     takes the expected type (std.sqlite's wrappers annotate it).
                                                     ponytail: typed loosely, as __coll */
        if (!check_argc(call, 3)) return t_error;
        check_arg(call, 0, t_int);
        check_arg(call, 1, t_str);
        check_arg(call, 2, 0);
        return expected ? expected : infer_error(call, expected);
    }
    if (id == B_FS_LINES) { /* spec 040: (path, offset) -> result[list[str]]; std.fs annotates it */
        if (!check_argc(call, 2)) return t_error;
        check_arg(call, 0, t_str);
        check_arg(call, 1, t_int);
        return expected ? expected : infer_error(call, expected);
    }
    if (id == B_JSON_TEXT) { /* spec 029: __json_text(v) -> str (std.web's answers) */
        if (!check_argc(call, 1)) return t_error;
        check_arg(call, 0, 0);
        return t_str;
    }
    if (id == B_JSON_PARSE) { /* __json_parse(s) -> result[Json]: std.json's wrapper declares the type */
        if (!check_argc(call, 1)) return t_error;
        check_arg(call, 0, t_str);
        return expected ? expected : infer_error(call, expected);
    }
    if (id == B_JSON_WRITE) { /* __json_write(j, pretty) -> result[str] */
        if (!check_argc(call, 2)) return t_error;
        check_arg(call, 0, 0);
        check_arg(call, 1, t_int);
        return ty_result(t_str);
    }
    if (id == B_JSON_FIELDS) { /* spec 029: __json_fields(xs: list[T]) -> list[str] (std.web's settings) */
        if (!check_argc(call, 1)) return t_error;
        check_arg(call, 0, 0);
        return ty_list(t_str);
    }
    if (id == B_TO_JSON || id == B_FROM_JSON) { /* spec 029: std.json's to_json / from_json (typed loosely: the
                                                   wrappers declare the types; the value's type drives it) */
        if (!check_argc(call, id == B_TO_JSON ? 1 : 2)) return t_error;
        check_arg(call, 0, 0);
        if (id == B_FROM_JSON) check_arg(call, 1, t_int);
        return expected ? expected : infer_error(call, expected);
    }
    if (id == B_SERVE_WORKERS) { /* spec 028: __serve_workers(n, setup, answer): both functions' effects become
                                    the caller's. ponytail: typed loosely (std.http's wrappers declare the types) */
        if (!check_argc(call, 3)) return t_error;
        check_arg(call, 0, t_int);
        for (int k = 1; k < 3; k++) {
            Type *ft = check_arg(call, k, 0);
            if (ty_bad(ft) || ft->kind != TY_FUNC) return t_error;
            fx_value_call(ft->decl.p);
        }
        return ty_result(t_void);
    }
    if (id == B_SERVE_CALL) { /* spec 011: __serve_call(f, x): result of f(x); f's effects become the caller's */
        if (!check_argc(call, 2)) return t_error;
        Type *ft = check_arg(call, 0, 0);
        if (ty_bad(ft) || ft->kind != TY_FUNC || ft->a.v->n != 1) return t_error;
        check_arg(call, 1, vec_get(ft->a.v, 0));
        fx_value_call(ft->decl.p);
        fx_mark_param(call, ft->decl.p, 0);
        return ty_result(ft->b.t);
    }
    switch (id) {
    case B_TODO: return check_todo(call, expected);
    case B_STR: /* str(int|float|bool) */
        if (check_argc(call, 1)) {
            Type *t = check_arg(call, 0, 0);
            if (t->kind != TY_INT && t->kind != TY_FLOAT && t->kind != TY_BOOL)
                err_one_of(arg0(call), "`int`, `float` or `bool`", t);
        }
        return t_str;
    case B_LEN: /* len(str|list|map|collection) */
        if (check_argc(call, 1)) {
            Type *t = check_arg(call, 0, 0);
            if (t->kind != TY_STR && t->kind != TY_BYTES && t->kind != TY_LIST && t->kind != TY_MAP &&
                !TY_IS_COLL(t->kind))
                err_one_of(arg0(call), "a `str`, `bytes`, list or map", t);
        }
        return t_int;
    case B_SLICE: { /* slice(str|list, int, int) */
        if (!check_argc(call, 3)) return t_error;
        Type *t = check_arg(call, 0, 0);
        check_arg(call, 1, t_int);
        check_arg(call, 2, t_int);
        if (t->kind == TY_STR || t->kind == TY_BYTES || t->kind == TY_LIST) return t;
        return err_one_of(arg0(call), "a `str`, `bytes` or list", t);
    }
    case B_PUSH: /* push(list[T], T); spec 014: OrderedSet, PriorityQueue, Deque */
        if (check_argc(call, 2)) {
            Type *l = kind_arg(call, 1 << TY_LIST | 1 << TY_OSET | 1 << TY_PQUEUE | 1 << TY_DEQUE, "a list");
            check_arg(call, 1, l ? l->a.t : 0);
        }
        return t_void;
    case B_POP: { /* pop(list[T]) -> result[T] */
        if (!check_argc(call, 1)) return t_error;
        Type *l = kind_arg(call, 1 << TY_LIST | 1 << TY_PQUEUE | 1 << TY_DEQUE, "a list");
        return l ? ty_result(l->a.t) : t_error;
    }
    case B_SORT: /* sort(list[int|float|str]) */
        if (check_argc(call, 1)) {
            Type *t = check_arg(call, 0, 0);
            if (!ty_bad(t)) {
                int ok = t->kind == TY_LIST && /* a list of an `ordered` type parameter (E0311 otherwise) */
                         (is_ordered_type(t->a.t, arg0(call)) || ck_param_order(t->a.t, arg0(call)));
                if (!ok) err_one_of(arg0(call), "a list of an ordered type (`int`, `float`, `str`, `bool`, ...)", t);
            }
        }
        return t_void;
    case B_HAS: /* has(map, key) -> bool / remove(map, key) */
    case B_REMOVE:
        if (check_argc(call, 2)) {
            Type *m = kind_arg(call, 1 << TY_MAP | 1 << TY_OMAP | 1 << TY_OSET, "a map");
            check_arg(call, 1, m ? m->a.t : 0);
        }
        return id == B_HAS ? t_bool : t_void;
    case B_GET_OR: { /* get_or(map[K, V], K, V) -> V (spec 009) */
        if (!check_argc(call, 3)) return t_error;
        Type *m = kind_arg(call, 1 << TY_MAP | 1 << TY_OMAP, "a map");
        check_arg(call, 1, m ? m->a.t : 0);
        check_arg(call, 2, m ? m->b.t : 0);
        return m ? m->b.t : t_error;
    }
    case B_KEYS: { /* keys(map[K, V]) -> list[K] */
        if (!check_argc(call, 1)) return t_error;
        Type *m = kind_arg(call, 1 << TY_MAP | 1 << TY_OMAP, "a map");
        return m ? ty_list(m->a.t) : t_error;
    }
    case B_EXPECT_EQ: /* expect_eq(T, T): the first argument's type is the expected type of the second */
        if (check_argc(call, 2)) {
            Node *other = vec_get(call->b.v, 1); /* spec 056: `expect_eq(first(s), ok(1))` reads first(s) as a result */
            Node *saved = ck_result_ctx;
            if (is_ok_or_err(other)) ck_result_ctx = vec_get(call->b.v, 0);
            Type *t = check_arg(call, 0, 0);
            ck_result_ctx = saved;
            check_arg(call, 1, ty_bad(t) ? 0 : t);
            if (!ty_bad(t) && ty_holds_func(t)) /* compared as == compares */
                ck_err1("E0308", call, "`expect_eq` cannot compare `$1`: it holds a function, and functions cannot be compared.", ty_name(t));
        }
        return t_void;
    case B_OK: { /* ok(v) / ok() */
        if (!expected && ok_from_value && call->b.v->n == 1) { /* spec 029: `from_json(j) == ok(3)` */
            ok_from_value = 0;
            Type *vt = check_arg(call, 0, 0);
            return ty_bad(vt) ? t_error : ty_result(vt);
        }
        if (!ty_is(expected, TY_RESULT)) return infer_error(call, expected);
        Type *inner = expected->a.t;
        if (ty_is(inner, TY_VOID)) {
            check_argc(call, 0);
            return expected;
        }
        if (check_argc(call, 1)) check_arg(call, 0, inner);
        return expected;
    }
    case B_ABS: { /* abs(int|float) -> same type */
        if (!check_argc(call, 1)) return t_error;
        Type *t = check_arg(call, 0, 0);
        if (ty_bad(t)) return t_error;
        if (t->kind == TY_INT || t->kind == TY_FLOAT) return t;
        return err_one_of(arg0(call), "`int` or `float`", t);
    }
    case B_MIN: /* min/max(T, T) for int, float or str -> T */
    case B_MAX: {
        if (!check_argc(call, 2)) return t_error;
        Type *t = check_arg(call, 0, 0);
        if (ty_bad(t)) return t_error;
        if (!is_ordered_type(t, arg0(call)) && !ck_param_order(t, arg0(call))) /* an `ordered` type parameter */
            return err_one_of(arg0(call), "an ordered type (`int`, `float`, `str`, `bool`, ...)", t);
        check_arg(call, 1, t);
        return t;
    }
    default: /* err(msg) */
        if (!ty_is(expected, TY_RESULT)) return infer_error(call, expected);
        if (check_argc(call, 1)) check_arg(call, 0, err_type(expected)); /* spec 032: the error type */
        return expected;
    }
}

/* Checks argument 0 as a list; returns its type, or 0 (after E0301) when it is not one. */
Type *list_arg(Node *call)
{
    Type *t = check_arg(call, 0, 0);
    if (ty_is(t, TY_LIST)) return t;
    err_one_of(arg0(call), "a list", t);
    return 0;
}

/* Checks argument 0 as one of the type kinds in mask (bit per kind); returns its type, or 0 after
 * E0301 naming what (the message of the builtin's original kind). */
Type *kind_arg(Node *call, long mask, char *what)
{
    Type *t = check_arg(call, 0, 0);
    if (!ty_bad(t) && (mask >> t->kind & 1)) return t;
    if (ty_bad(t)) return 0;
    err_one_of(arg0(call), what, t);
    return 0;
}

Type *map_arg(Node *call)
{
    Type *t = check_arg(call, 0, 0);
    if (ty_is(t, TY_MAP)) return t;
    err_one_of(arg0(call), "a map", t);
    return 0;
}

static Type *check_call_inner(Node *call, Type *expected);

/* spec 056: a lookup that now gives an Option, read as a result: E0214 (libchange.c). */
Type *check_call(Node *call, Type *expected)
{
    return libchange_option_type(call, check_call_inner(call, expected), expected);
}

static Type *check_call_inner(Node *call, Type *expected)
{
    Str *name = call->a.n->a.s;
    Sym *s = scope_find(name);
    if (s && (s->flags & SYF_SHADOW)) s = 0; /* spec 056: `len(xs)` beside a local `len` (E0203) is the builtin */
    if (s) {
        if (is_placeholder(s)) { /* already reported as undefined */
            check_args_untyped(call);
            return t_error;
        }
        if (s->kind == SYM_FN) { /* user function */
            ck_use_call = call;
            rename_use(s, call->a.n);
            ck_use_call = 0;
            q_note_sym(s, call->a.n, RR_CALL); /* spec 006 reference log */
            call->c.n = s->decl;
            libchange_arg_call(call); /* spec 057: an argument added since (E0214, inserted) */
            fx_call(call, s->decl);
            if (ck_in_test) { /* spec 018: a function with `ensures` checks its results (W0602) */
                Fx *cfx = item_fx(s->decl);
                if (cfx && cfx->ensures) ck_test_checks++;
            }
            if (ty_tparams_of(call->c.n)) { /* inference */
                Type *gt = check_generic_call(call, call->c.n, expected);
                if (file_get((int)call->c.n->file)->std) {
                    check_sqlite_call(call); /* spec 026: exec, query */
                    check_web_call(call);    /* spec 029: on_get, ... */
                }
                return gt;
            }
            if (libchange_swap_call(call)) { /* spec 056: the text first (E0214 for the old order) */
                check_regex_call(call);
            } else if (check_argc(call, call->c.n->b.v->n)) {
                for (long i = 0; i < call->c.n->b.v->n; i++) {
                    Node *p = vec_get(call->c.n->b.v, i);
                    ck_exp_tnode = p->b.n; /* the parameter's type as written (E0502 fixes) */
                    check_arg(call, i, p->type);
                    ck_exp_tnode = 0;
                }
                if (file_get((int)call->c.n->file)->std) {
                    check_regex_call(call);  /* spec 025 */
                    check_sqlite_call(call); /* spec 026 */
                    check_ops_call(call);    /* spec 055: log_*, counter, gauge, histogram */
                }
            }
            return call->c.n->type;
        }
        if (s->kind == SYM_LOCAL && s->type && (s->type->kind == TY_FUNC || s->type->kind == TY_ERROR)) {
            Node *callee = call->a.n; /* a local holding a function value: f(args) calls the identifier f */
            Node *id = node_new(N_IDENT, callee->start, callee->end);
            id->a.s = callee->a.s;
            id->file = callee->file;
            call->a.n = id;
            call->kind = N_CALL_VALUE;
            return check_call_value(call, expected);
        }
        s->flags |= SYF_USED; /* calling a local still uses it; a local, record or variant is not callable */
        if (s->kind == SYM_MODULE) { /* spec 058: `regex(...)` after `import std.regex;` means `regex.regex` */
            s->decl->e.i++;
            Str *q = msg("$1.$1", name, 0, 0);
            Sym *item = scope_find(q);
            Diag *d = item ? ck_err("E0201", call->a.n, msg("`$1` is a module: write `$2`.", name, q, 0))
                           : ck_err1("E0201", call->a.n, "`$1` is a module: use one of its items, `$1.NAME`.", name);
            if (item) diag_add_fix(d, (int)call->a.n->file, call->a.n->start, call->a.n->end, q);
            check_args_untyped(call);
            return t_error;
        }
        ck_err1("E0303", call->a.n, "`$1` is not a function.", name);
        check_args_untyped(call);
        return t_error;
    }
    long id = builtin_id(name);
    if (id && std_only_ok(name, call->a.n)) { /* spec 007: `__` primitives only inside the standard library */
        q_note(0, id, call->a.n, RR_CALL);   /* spec 006 reference log: (0, builtin id) */
        return check_builtin(call, id, expected);
    }
    q_note(0, (long)name, call->a.n, RR_CALL); /* (0, name) for vo impact's removed functions */
    report_undefined(name, call->a.n);
    check_args_untyped(call);
    return t_error;
}

/* E0201 for a name that resolves to nothing, with a typo suggestion; then binds a silent
 * placeholder (type error) so later uses of the name are not reported again. */
void report_undefined(Str *name, Node *node)
{
    if (module_undefined(name, node)) return; /* spec 058 */
    long dot = str_dot(name); /* spec 058: `m.x` or `m` followed by `.`, with m a module not imported: E0204 */
    FileRec *nf = file_get((int)node->file);
    Str *mod = dot >= 0 ? str_new(name->data, dot) : node->end < nf->len && nf->text[node->end] == '.' ? name : 0;
    if (mod && missing_module_import(mod, node)) {
        Sym *s = sym_new(name, SYM_LOCAL, 0, t_error, 0);
        s->flags = SYF_USED;
        s->slot = -1;
        scope_add(s);
        return;
    }
    Diag *d = ck_err1("E0201", node, "`$1` is not defined.", name);
    if (str_eq_cstr(name, "ok")) { /* `return ok;`: ok and err are called like functions */
        d->msg = str_from_cstr("`ok` is called like a function: `ok()` or `ok(value)`.");
        diag_add_fix(d, (int)node->file, node->start, node->end, str_from_cstr("ok()"));
    } else if (!suggest_name(d, name, node)) { /* nothing similar: maybe another file defines it (E0204) */
        suggest_import(d, name, node);
    }
    Sym *s = sym_new(name, SYM_LOCAL, 0, t_error, 0); /* no name node: marks the placeholder */
    s->flags = SYF_USED;
    s->slot = -1;
    scope_add(s);
}

int is_placeholder(Sym *sym)
{
    return !sym->node;
}

/* A replacement fix with the closest visible name or builtin (edit distance <= 2 and shorter
 * than the name), plus a note where that name is defined. */
int suggest_name(Diag *d, Str *name, Node *node)
{
    Sym *best = 0;
    long dist = 3;
    Str *best_name = 0;
    for (Sym *s = ck_scope; s; s = s->next) {
        if (is_placeholder(s) || !sym_fits(s, node)) continue;
        long k = name_distance(name, s->name);
        if (k >= dist) continue;
        dist = k;
        best = s;
        best_name = s->name;
    }
    if (node->kind == N_NAME) { /* builtins are suggested only where a call names them */
        for (int id = 1; id <= B_COUNT; id++) {
            const char *b = builtin_names[id];
            if (!b || b[0] == '_') continue; /* unnamed, or a std-only primitive: never suggested */
            Str *bs = str_from_cstr((char *)b);
            long k = name_distance(name, bs);
            if (k >= dist) continue;
            dist = k;
            best = 0;
            best_name = bs;
        }
    }
    if (!best_name) return 0;
    if (name->len <= 4 && dist > 1) return 0; /* short names only at distance 1 */
    if (dist >= name->len) return 0;
    diag_add_fix(d, (int)node->file, node->start, node->end, best_name);
    if (!best) return 1;
    best->flags |= SYF_USED; /* the suggested name counts as used */
    Str *m = msg("`$1` is defined here.", best_name, 0, 0);
    Node *bn = best->node;
    diag_add_related(d, (int)bn->file, bn->start, bn->end, m);
    return 1;
}

/* A generic function named as a value where a function type is expected: its type parameters bound from
 * that type (an effect parameter from a `Database[uses ...]` parameter, say), the bindings kept on the
 * name (N_E) for the closure the runtime makes. */
static Type *fn_value_instance(Node *node, Node *decl, Type *ft, Type *expected)
{
    Vec *tparams = ty_tparams_of(decl);
    if (!tparams || !tparams->n) return ft;
    Type **b = alloc(tparams->n * 8);
    if (expected && expected->kind == TY_FUNC && expected->a.v->n == ft->a.v->n) {
        /* the parameters bind (the expected type's own parameters may still be open in its result) */
        for (long i = 0; i < ft->a.v->n; i++) {
            Type *ep = vec_get(expected->a.v, i);
            if (ck_infer_owner && ty_mentions_owner(ep, ck_infer_owner)) continue; /* the outer call's open part */
            if (!ty_unify(vec_get(ft->a.v, i), ep, decl, b)) return ft; /* reported where used */
        }
        ty_unify(ft->b.t, expected->b.t, decl, b); /* what the parameters left open, from the result */
    }
    bind_rest(node, tparams, b, node->a.s); /* E0310 for what stays open */
    for (long i = 0; i < tparams->n; i++) {
        Node *tp = vec_get(tparams, i);
        if (tp->b.i != TPC_EFFECTS) check_targ(b[i], tp, node, node->a.s);
    }
    node->e.v = bindings_vec(b, tparams->n);
    return ty_subst(ft, decl, node->e.v);
}

Type *check_ident(Node *node)
{
    return check_ident_as(node, 0);
}

Type *check_ident_as(Node *node, Type *expected)
{
    Sym *s = scope_find(node->a.s);
    if (s) {
        node->b.sym = s;
        rename_use(s, node);
        q_note_sym(s, node, s->kind == SYM_FN ? RR_VALUE : RR_USE); /* spec 006 reference log */
        if (s->kind == SYM_LOCAL) {
            s->flags |= SYF_USED;
            node->c.i = ck_capture(s, node); /* its slot here (a capture inside anonymous functions) */
            return s->type ? s->type : t_error;
        }
        if (s->kind == SYM_CONST) { /* spec 031: the use becomes the literal of its value */
            Type *t = const_eval(s->decl, node);
            if (ty_bad(t)) return t_error;
            const_use(node, s->decl);
            return t;
        }
        if (s->kind == SYM_FN) { /* a named function as a value */
            node->flags |= NF_FNVAL;
            node->d.n = s->decl;
            return fn_value_instance(node, s->decl, ty_func_of(s->decl), expected);
        }
        if (s->kind == SYM_MODULE) { /* spec 058 */
            s->decl->e.i++; /* a mistaken use is still a use (no W0203) */
            ck_err1("E0201", node, "`$1` is a module: use one of its items, `$1.NAME`.", node->a.s);
            return t_error;
        }
        ck_err1("E0301", node, "`$1` is not a value.", node->a.s);
        return t_error;
    }
    long id = builtin_id(node->a.s); /* a builtin used as a value (`ok`/`err` have their own hint) */
    if (id && id != B_OK && id != B_ERR) {
        ck_err1("E0201", node, "`$1` is a builtin and cannot be used as a value: wrap it in an anonymous function.",
                node->a.s);
        return t_error;
    }
    report_undefined(node->a.s, node);
    return t_error;
}

/* Type of an assignment target (variable, field or element). */
Type *check_place(Node *node, void *unused)
{
    (void)unused;
    return check_expr(node, 0);
}

Type *check_field(Node *node)
{
    Type *t = check_expr(node->a.n, 0);
    if (ty_bad(t)) return t_error;
    Node *fname = node->b.n; /* field name node */
    if (!fname) return t_error;
    if (ty_is(t, TY_RECORD)) {
        long idx = member_index(t->decl.n->b.v, fname->a.s);
        if (idx < 0) /* spec 056: a renamed std field (E0214) */
            idx = libchange_field(t->decl.n, fname, node, node == ck_set_place ? ck_set_value : 0);
        if (idx >= 0) {
            node->c.i = idx;
            q_note((long)t->decl.p, idx, fname, RR_USE); /* spec 006 reference log: (record, field) */
            Node *m = member_at(t->decl.n->b.v, idx);
            return field_type_in(t, m->b.n->type);
        }
    }
    Str *tn = ty_name(t);
    Diag *d = ck_err("E0304", fname, msg(M_NO_FIELD, tn, fname->a.s, 0));
    suggest_field(d, t, fname);
    return t_error;
}

/* A replacement fix with the closest field name (edit distance <= 2 and shorter than the name). */
void suggest_field(Diag *d, Type *record_type, Node *name_node)
{
    if (!ty_is(record_type, TY_RECORD)) return;
    long dist = 3;
    Str *best = 0;
    for (long i = 0;; i++) {
        Node *m = member_at(record_type->decl.n->b.v, i);
        if (!m) break;
        Str *fname = m->a.n->a.s;
        long k = name_distance(name_node->a.s, fname);
        if (k >= dist) continue;
        dist = k;
        best = fname;
    }
    if (!best || dist >= name_node->a.s->len) return;
    diag_add_fix(d, (int)name_node->file, name_node->start, name_node->end, best);
}

Type *check_index(Node *node)
{
    Type *t = check_expr(node->a.n, 0);
    if (!ty_bad(t)) {
        if (t->kind == TY_LIST) {
            Type *i = check_expr(node->b.n, t_int);
            expect_type(node->b.n, t_int, i);
            return t->a.t;
        }
        if (t->kind == TY_BYTES) { /* spec 021: a byte, 0 to 255 */
            Type *i = check_expr(node->b.n, t_int);
            expect_type(node->b.n, t_int, i);
            return t_int;
        }
        if (t->kind == TY_MAP) {
            node->flags |= NF_MAP;
            Type *key = t->a.t;
            Type *k = check_expr(node->b.n, key);
            expect_type(node->b.n, key, k);
            return t->b.t;
        }
        if (t->kind == TY_OMAP || t->kind == TY_DEQUE) { /* spec 014: a map's value by key, a deque's element */
            Type *key = t->kind == TY_OMAP ? t->a.t : t_int;
            Type *k = check_expr(node->b.n, key);
            expect_type(node->b.n, key, k);
            return t->kind == TY_OMAP ? t->b.t : t->a.t;
        }
        if (t->kind == TY_STR)
            ck_err("E0308", node, str_from_cstr("Strings cannot be indexed; use `slice` or `chars`."));
        else
            ck_err1("E0308", node, "`$1` cannot be indexed.", ty_name(t));
    }
    check_expr(node->b.n, 0);
    return t_error;
}

/* E0308 for an operator its operand types do not support; right = 0 for unary operators. */
Type *op_error(Node *node, Type *left, Type *right)
{
    Str *op = tok_raw((int)node->c.i); /* operator token kind */
    Str *lname = ty_name(left);
    Str *m;
    if (right) {
        Str *rname = ty_name(right);
        m = msg("Operator `$1` cannot be applied to `$2` and `$3`.", op, lname, rname);
    } else {
        m = msg("Operator `$1` cannot be applied to `$2`.", op, lname, 0);
    }
    ck_err("E0308", node, m);
    return t_error;
}

/* Spec 029: whether t mentions type parameter idx of owner. */
static int ty_has_tparam(Type *t, Node *owner, long idx)
{
    if (!t) return 0;
    switch (t->kind) {
    case TY_PARAM: return t->decl.n == owner && t->a.i == idx;
    case TY_LIST:
    case TY_RESULT: return ty_has_tparam(t->a.t, owner, idx) || (t->b.t && ty_has_tparam(t->b.t, owner, idx));
    case TY_MAP: return ty_has_tparam(t->a.t, owner, idx) || ty_has_tparam(t->b.t, owner, idx);
    case TY_FUNC:
        if (ty_has_tparam(t->b.t, owner, idx)) return 1;
        for (long i = 0; i < t->a.v->n; i++)
            if (ty_has_tparam(vec_get(t->a.v, i), owner, idx)) return 1;
        return 0;
    case TY_RECORD:
    case TY_VARIANT:
        for (long i = 0; t->b.v && i < t->b.v->n; i++)
            if (ty_has_tparam(vec_get(t->b.v, i), owner, idx)) return 1;
        return 0;
    default: return 0;
    }
}

/* Spec 029: a call of a generic function with a type parameter only its result names (`from_json(j)`):
 * its type must come from where it is used. */
static int needs_expected(Node *e)
{
    if (e->kind != N_CALL || !e->a.n || !e->a.n->a.s) return 0;
    Sym *s = scope_find(e->a.n->a.s);
    if (!s || s->kind != SYM_FN) return 0;
    Vec *tps = ty_tparams_of(s->decl);
    for (long k = 0; tps && k < tps->n; k++) {
        if (((Node *)vec_get(tps, k))->b.i == TPC_EFFECTS) continue;
        int used = 0;
        for (long i = 0; i < s->decl->b.v->n && !used; i++)
            used = ty_has_tparam(((Node *)vec_get(s->decl->b.v, i))->type, s->decl, k);
        if (!used) return 1;
    }
    return 0;
}

/* spec 056: `ok(...)` or `err(...)` written (a value a lookup's old result was compared with). */
static int is_ok_or_err(Node *n)
{
    return n && n->kind == N_CALL && n->a.n && (str_eq_cstr(n->a.n->a.s, "ok") || str_eq_cstr(n->a.n->a.s, "err"));
}

Type *check_binary(Node *node)
{
    long op = node->c.i; /* operator token */
    if (op == T_AND || op == T_OR) {
        check_cond(node->a.n);
        check_cond(node->b.n);
        node->d.i = op == T_AND ? OP_AND : OP_OR;
        return t_bool;
    }
    Type *l, *r;
    if ((op == T_EQ || op == T_NE) && needs_expected(node->a.n) && !needs_expected(node->b.n)) {
        ok_from_value = 1; /* spec 029: `from_json(j) == ok(3)`: the right side gives the type */
        r = check_expr(node->b.n, 0);
        ok_from_value = 0;
        l = check_expr(node->a.n, r);
    } else {
        Node *saved = ck_result_ctx; /* spec 056: `first(s) == ok(1)` reads first(s) as a result */
        if ((op == T_EQ || op == T_NE) && is_ok_or_err(node->b.n)) ck_result_ctx = node->a.n;
        l = check_expr(node->a.n, 0);
        ck_result_ctx = saved;
        r = check_expr(node->b.n, l);
    }
    if (ty_bad(l) || ty_bad(r)) return t_error;
    long k = l->kind;
    if (l != r) { /* operand types differ: E0301 when a conversion helps, else E0308 */
        long rk = r->kind;
        if ((op == T_PLUS && (k == TY_STR || k == TY_BYTES)) || op == T_EQ || op == T_NE) { /* "a" + 1: str(1) */
            expect_type(node->b.n, l, r);
            return t_error;
        }
        if ((k == TY_INT || k == TY_FLOAT) && (rk == TY_INT || rk == TY_FLOAT)) {
            if (k == TY_INT && rk == TY_FLOAT) /* int (left) with float (right): convert the left side */
                expect_type(node->a.n, r, l);
            else
                expect_type(node->b.n, l, r);
            return t_error;
        }
        return op_error(node, l, r);
    }
    if (op == T_EQ || op == T_NE) {
        node->d.i = op == T_EQ ? OP_EQ : OP_NE;
        if (k == TY_VOID || k == TY_FUNC) return op_error(node, l, r); /* functions cannot be compared */
        if (ty_holds_func(l)) { /* nor what holds one */
            ck_err("E0308", node, msg("`$1` cannot compare `$2`: it holds a function, and functions cannot be compared.", tok_raw((int)node->c.i), ty_name(l), 0));
            return t_error;
        }
        return t_bool;
    }
    if (op >= T_LT && op <= T_GE) { /* ordering: int, float, str, bytes, bool, records, variants */
        long base;
        if (k == TY_INT) {
            base = OP_LT_INT;
        } else if (k == TY_FLOAT) {
            base = OP_LT_FLOAT;
        } else if (k == TY_STR || k == TY_BYTES) { /* spec 021: bytes compare byte by byte, as str */
            base = OP_LT_STR;
        } else if (k == TY_PARAM) { /* an `ordered` type parameter (E0311 otherwise) */
            ck_param_order(l, node);
            base = OP_LT_PARAM;
        } else if (k == TY_RECORD || k == TY_VARIANT || k == TY_BOOL) { /* spec 014: field by field (E0313 naming
                                                                           the field otherwise); spec 056: bool, variants */
            if (!ty_ordered(l)) order_error(l, node, 0);
            base = OP_LT_PARAM; /* the generic comparison at run time */
        } else {
            return op_error(node, l, r);
        }
        node->d.i = base + (op - T_LT);
        return t_bool;
    }
    long code; /* arithmetic */
    if (k == TY_INT) {
        code = op == T_PLUS    ? OP_ADD_INT
               : op == T_MINUS ? OP_SUB_INT
               : op == T_STAR  ? OP_MUL_INT
               : op == T_SLASH ? OP_DIV_INT
                               : OP_REM_INT;
    } else if (k == TY_FLOAT) {
        if (op == T_PLUS)
            code = OP_ADD_FLOAT;
        else if (op == T_MINUS)
            code = OP_SUB_FLOAT;
        else if (op == T_STAR)
            code = OP_MUL_FLOAT;
        else if (op == T_SLASH)
            code = OP_DIV_FLOAT;
        else
            return op_error(node, l, r); /* % on floats */
    } else if ((k == TY_STR || k == TY_BYTES) && op == T_PLUS) {
        code = OP_CONCAT;
    } else {
        return op_error(node, l, r);
    }
    node->d.i = code;
    return l;
}

Type *check_unary(Node *node)
{
    if (node->c.i == T_NOT) {
        check_cond(node->a.n);
        node->d.i = OP_NOT;
        return t_bool;
    }
    Type *t = check_expr(node->a.n, 0);
    if (ty_bad(t)) return t_error;
    if (t->kind == TY_INT) {
        node->d.i = OP_NEG_INT;
        Node *lit = node->a.n;
        if (lit->kind == N_INT && lit->a.i == (-9223372036854775807L - 1) && !(lit->flags & NF_CONST)) /* spec 056 */
            ck_err("E0105", node, msg("`$1` is already the smallest int (-9223372036854775808), and its negation "
                                      "overflows: write `$1`.", src_text(lit), 0, 0));
        return t;
    }
    if (t->kind == TY_FLOAT) {
        node->d.i = OP_NEG_FLOAT;
        return t;
    }
    return op_error(node, t, 0);
}

/* Spec 059: a call with an untyped anonymous function argument, which takes its types from the call: under
 * `try`, the call takes its result type from where the `try` is (`let n: int = try transaction(db, fn() ...)`). */
static int lambda_arg(Node *e)
{
    for (long i = 0; e->kind == N_CALL && e->b.v && i < e->b.v->n; i++) {
        Node *a = vec_get(e->b.v, i);
        if (a->kind == N_LAMBDA && !lambda_written(a)) return 1;
    }
    return 0;
}

Type *check_try(Node *node, Type *expected)
{
    if (!ty_is(ck_ret, TY_RESULT))
        ck_err("E0406", node, str_from_cstr("`try` can only be used in a function that returns a `result`."));
    /* spec 029: `let s: Settings = try load_settings(...)`: the call's result type comes from where it is used */
    Type *want = expected && !ty_bad(expected) && (needs_expected(node->a.n) || lambda_arg(node->a.n)) ? ty_result(expected) : 0;
    Node *saved = ck_result_ctx; /* spec 056: `try first_key(m)` reads the call as a result */
    ck_result_ctx = node->a.n;
    Type *t = check_expr(node->a.n, want);
    ck_result_ctx = saved;
    if (ty_bad(t)) return t_error;
    if (ty_is(t, TY_RESULT)) {
        if (ty_is(ck_ret, TY_RESULT) && err_type(t) != err_type(ck_ret) && !ty_bad(err_type(t)) &&
            !ty_bad(err_type(ck_ret))) { /* spec 032: errors pass up unchanged, never converted */
            Str *fname = ck_fn && ck_fn->kind == N_FN && ck_fn->a.n ? ck_fn->a.n->a.s : str_from_cstr("this function");
            ck_err("E0412", node,
                   msg("`try` passes the error on, but this `result` fails with `$1` and `$2` fails with `$3`: convert it "
                       "with `map_err`.",
                       ty_name(err_type(t)), fname, ty_name(err_type(ck_ret))));
        }
        return t->a.t;
    }
    ck_err1("E0301", node->a.n, "`try` needs a `result`, found `$1`.", ty_name(t));
    return t_error;
}

Type *check_list(Node *node, Type *expected)
{
    Type *elem = ty_is(expected, TY_LIST) ? expected->a.t : 0; /* expected element type */
    if (!node->a.v->n) { /* spec 056: [] takes its type from the context, as ok(1) does; without one, E0306 */
        if (elem) return expected;
        ck_err("E0306", node, str_from_cstr("An empty list needs a type: write `new list[T]`, or give it one "
                                            "from the context (`let xs: list[int] = [];`)."));
        return t_error;
    }
    Type *et = check_expr(vec_get(node->a.v, 0), elem); /* element type */
    if (elem && !ty_bad(elem) && ty_is(elem, TY_FUNC)) { /* spec 059: each function element against the expected type */
        for (long i = 0; i < node->a.v->n; i++) {
            Node *item = vec_get(node->a.v, i);
            expect_type(item, elem, i ? check_expr(item, elem) : et);
        }
        check_web_routes(node);
        return expected;
    }
    for (long i = 1; i < node->a.v->n; i++) {
        Node *item = vec_get(node->a.v, i);
        Type *t = check_expr(item, et);
        expect_type(item, et, t);
    }
    check_web_routes(node); /* spec 029: routes of one table that collide */
    return ty_bad(et) ? t_error : ty_list(et);
}

/* The declaration a type name refers to (record or variant), or 0 after an error. */
Node *lookup_decl(Node *name_node, long kind, char *not_kind_msg)
{
    if (!name_node) return 0;
    Sym *s = scope_find(name_node->a.s);
    if (!s) {
        report_undefined(name_node->a.s, name_node);
        return 0;
    }
    if (is_placeholder(s)) return 0;
    rename_use(s, name_node);
    q_note_sym(s, name_node, RR_USE); /* spec 006 reference log */
    if (s->kind == kind) return s->decl;
    ck_err1("E0301", name_node, not_kind_msg, name_node->a.s);
    return 0;
}

/* Spec 014: `new ordered_map[str, int]` creates an empty collection ordered by its key or element
 * type, which must then be ordered (another order: `ordered_map_by` with a comparison function). */
static Type *check_new_coll(Node *node)
{
    Type *t = resolve_type(node->a.n);
    if (ty_bad(t) || t->kind == TY_DEQUE || ty_ordered(t->a.t)) return t;
    if (t->a.t->kind == TY_RECORD) {
        order_error(t->a.t, node, 0);
        return t;
    }
    static char *const by[] = {"ordered_map_by", "ordered_set_by", "priority_queue_by"};
    SB *sb = sb_new();
    sb_cstr(sb, "`$1` is not ordered, so `new $2` cannot order it: use `");
    sb_cstr(sb, by[t->kind - TY_OMAP]);
    sb_cstr(sb, "` with a comparison function.");
    ck_err("E0313", node, msg(sb_finish(sb)->data, ty_name(t->a.t), ty_name(t), 0));
    return t;
}

Type *check_new_record(Node *node, Type *expected)
{
    Node *decl = lookup_decl(node->a.n, SYM_RECORD, "`$1` is not a record.");
    node->c.n = decl;
    Vec *inits = node->b.v;
    if (!decl) { /* unknown record: check the values anyway */
        for (long i = 0; i < inits->n; i++) check_expr(((Node *)vec_get(inits, i))->b.n, 0);
        return t_error;
    }
    q_note((long)decl, -2, node, RR_USE); /* (record, -2) marks a literal (vo edit add-field) */
    if (case_record(decl) && !node->e.i) /* spec 060: `new V.case { ... }` */
        ck_err1("E0201", node->a.n, "`$1` is a case's fields: build it with `$1 { ... }`, without `new`.",
                decl->a.n->a.s);
    Vec *tparams = ty_tparams_of(decl);
    Type **vts = alloc((inits->n + 1) * 8); /* field values already checked (inference), else 0 */
    Type *rt;
    if (tparams && tparams->n && (!node->d.v || !node->d.v->n)) { /* spec 014 (R8): no type arguments written */
        Type **b = alloc(tparams->n * 8);
        if (expected && expected->kind == TY_RECORD && expected->decl.n == decl && expected->b.v)
            for (long i = 0; i < tparams->n; i++) b[i] = expected->b.v->data[i].t;
        for (long pass = 0; pass < 2; pass++) /* values that need their context (`Tree.leaf`) last */
            for (long i = 0; i < inits->n; i++) {
                Node *init = vec_get(inits, i);
                long idx = member_index(decl->b.v, init->a.n->a.s);
                if (idx < 0 || needs_context(init->b.n) != pass) continue;
                Type *ft = member_at(decl->b.v, idx)->b.n->type;
                Type *known = ty_subst_bindings(ft, decl, b);
                vts[i] = check_expr(init->b.n, ty_mentions_owner(known, decl) ? 0 : known);
                ty_unify(ft, vts[i], decl, b); /* a mismatch is reported below, against the instance */
            }
        if (bind_rest(node, tparams, b, decl->a.n->a.s))
            for (long i = 0; i < tparams->n; i++) check_targ(b[i], vec_get(tparams, i), node, decl->a.n->a.s);
        rt = ty_instance(TY_RECORD, decl, bindings_vec(b, tparams->n));
    } else {
        rt = resolve_instance(node, decl, TY_RECORD, node->d.p); /* an instance when generic */
    }
    long *seen = alloc((count_members(decl->b.v) + 1) * 8); /* seen fields */
    for (long i = 0; i < inits->n; i++) {
        Node *init = vec_get(inits, i); /* N_FINIT */
        long idx = member_index(decl->b.v, init->a.n->a.s);
        if (idx < 0) idx = libchange_field(decl, init->a.n, init, init->b.n); /* spec 056: a renamed std field */
        if (idx < 0) { /* unknown field: check the value anyway */
            check_expr(init->b.n, 0);
            init->c.i = -1;
            ck_err("E0304", init->a.n, msg(M_NO_FIELD, decl->a.n->a.s, init->a.n->a.s, 0));
            continue;
        }
        init->c.i = idx;
        q_note((long)decl, idx, init->a.n, RR_USE); /* spec 006 reference log: (record, field) */
        if (seen[idx]) ck_err1("E0202", init->a.n, "Field `$1` is given twice: give every field once.", init->a.n->a.s);
        seen[idx] = 1;
        Node *m = member_at(decl->b.v, idx);
        Type *ft = field_type_in(rt, m->b.n->type);
        Type *vt = vts[i] ? vts[i] : check_expr(init->b.n, ft);
        expect_type(init->b.n, ft, vt);
    }
    SB *missing = sb_new();
    int any = 0;
    for (long i = 0;; i++) {
        Node *m = member_at(decl->b.v, i);
        if (!m) break;
        if (seen[i]) continue;
        if (any) sb_cstr(missing, ", ");
        any = 1;
        sb_byte(missing, '`');
        sb_str(missing, m->a.n->a.s);
        sb_byte(missing, '`');
    }
    if (any) ck_err("E0305", node, msg("Missing fields in `$1`: $2.", decl->a.n->a.s, sb_finish(missing), 0));
    return rt;
}

Str *str_new_copy(Str *str)
{
    return str_new(str->data, str->len);
}

static Type *vcase_bad(Node *node)
{
    if (node->c.n) check_expr(node->c.n, 0);
    return t_error;
}

Type *check_vcase(Node *node, Type *expected)
{
    Node *decl = lookup_decl(node->a.n, SYM_VARIANT, "`$1` is not a variant.");
    node->d.n = decl;
    if (!decl) return vcase_bad(node);
    Node *cn = node->b.n; /* case name node */
    if (!cn) return vcase_bad(node);
    long idx = member_index(decl->b.v, cn->a.s);
    if (idx < 0) idx = libchange_case(decl, cn); /* spec 056: a renamed std case (E0214) */
    node->e.i = idx;
    if (idx < 0) {
        ck_err("E0201", cn, msg(M_NO_CASE, cn->a.s, decl->a.n->a.s, 0));
        return vcase_bad(node);
    }
    q_note((long)decl, idx, cn, RR_USE); /* spec 006 reference log: (variant, case) */
    Node *pt = member_at(decl->b.v, idx)->b.n; /* payload type node, or 0 */
    if ((node->flags & NF_EMPTY_ARM) && !pt) { /* spec 056: `Shape.empty()` */
        Diag *d = ck_err("E0411", node, msg("`$1.$2` has no value: write `$1.$2`, without `()`.", decl->a.n->a.s,
                                            cn->a.s, 0));
        diag_add_fix(d, (int)node->file, cn->end, node->end, str_from_cstr(""));
    }
    /* a generic variant (a missing or extra payload is reported below) */
    if (ty_tparams_of(decl) && !node->c.n == !pt) return vcase_generic(node, decl, pt, expected);
    if (pt) {
        if (node->c.n) { /* payload given and expected */
            Type *t = check_expr(node->c.n, pt->type);
            expect_type(node->c.n, pt->type, t);
        } else { /* payload missing */
            Str *tn = ty_name(pt->type);
            ck_err("E0302", node, msg("`$1.$2` needs a payload of type `$3`.", decl->a.n->a.s, cn->a.s, tn));
        }
    } else if (node->c.n) { /* payload given but the case has none */
        check_expr(node->c.n, 0);
        ck_err("E0302", node, msg("`$1.$2` has no payload.", decl->a.n->a.s, cn->a.s, 0));
    }
    return decl_type(decl);
}

/* Types an expression (expected guides ok/err and empty lists); stores and returns the type. */
Type *check_expr(Node *node, Type *expected)
{
    if (!node) return t_error;
    Type *t;
    /* deeply nested expressions: give up quietly (the parser reported it) */
    if ((unsigned long)__builtin_frame_address(0) < (unsigned long)stack_floor + 0x20000) {
        t = t_error;
    } else {
        switch (node->kind) {
        case N_INT: t = t_int; break;
        case N_FLOAT: t = t_float; break;
        case N_STR:
            t = t_str;
            raw_string_check(node);
            break;
        case N_BOOL: t = t_bool; break;
        case N_IDENT: t = check_ident_as(node, expected); break;
        case N_CALL: t = check_call(node, expected); break;
        case N_FIELD: t = check_field(node); break;
        case N_INDEX: t = check_index(node); break;
        case N_BINARY: t = check_binary(node); break;
        case N_UNARY: t = check_unary(node); break;
        case N_TRY: t = check_try(node, expected); break;
        case N_LIST: t = check_list(node, expected); break;
        case N_NEW_RECORD: t = check_new_record(node, expected); break;
        case N_NEW_LIST: t = ty_list(resolve_type(node->a.n)); break;
        case N_NEW_MAP: t = new_map_type(TY_MAP, node); break; /* reusing the map rules of resolve_type */
        case N_NEW_COLL: t = check_new_coll(node); break; /* spec 014 */
        case N_VCASE: t = check_vcase(node, expected); break;
        case N_LAMBDA: t = check_lambda(node, expected); break;
        case N_CALL_VALUE: t = check_call_value(node, expected); break;
        default: t = t_error; break;
        }
    }
    node->type = t;
    if (q_collect) q_note_expr(node); /* spec 006: vo query type-at */
    return t;
}

/* The type of `new map[K, V]` (E0309 for an invalid key type). */
Type *new_map_type(int unused, Node *node)
{
    (void)unused;
    Node *tn = node_new(N_TYPE_NODE, node->start, node->end); /* a temporary map type node, resolved */
    tn->a.i = TY_MAP;
    tn->b = node->a;
    tn->c = node->b;
    tn->file = node->file;
    return resolve_type(tn);
}

/* When a .vo file under this file's directory defines `name`, turns the E0201 into E0204 with a
 * fix inserting the import line. */
void suggest_import(Diag *d, Str *name, Node *node)
{
    Str *path = find_import((int)node->file, name); /* "a.b.name" */
    if (!path) return;
    d->code = "E0204";
    d->msg = msg("`$1` is defined in another file but not imported: add `import $2;` and write `$2.$1`.", name, path, 0);
    SB *sb = sb_new();
    sb_cstr(sb, "import ");
    sb_str(sb, path);
    sb_cstr(sb, ";\n");
    Str *text = sb_finish(sb);
    long at = import_insert_at((int)node->file);
    diag_add_fix(d, (int)node->file, at, at, text);
}

/* Whether a symbol is a sensible suggestion for this use: a record or variant for a capitalized
 * name, a function (not main) at a call site, otherwise a local value. */
int sym_fits(Sym *sym, Node *node)
{
    Str *name = node->a.s; /* N_NAME and N_IDENT both keep the name in A */
    long dot = str_dot(name);
    int c = (unsigned char)name->data[dot + 1];
    if (dot >= 0) { /* spec 058: `m.name`: the items of that module (functions and constants as values) */
        if (str_dot(sym->name) != dot || memcmp(sym->name->data, name->data, (size_t)dot + 1)) return 0;
        if (c >= 'A' && c <= 'Z') return sym->kind == SYM_RECORD || sym->kind == SYM_VARIANT;
        return sym->kind == SYM_FN || (node->kind != N_NAME && sym->kind == SYM_CONST);
    }
    if (c >= 'A' && c <= 'Z') return sym->kind == SYM_RECORD || sym->kind == SYM_VARIANT;
    if (node->kind == N_NAME) return sym->kind == SYM_FN && !str_eq_cstr(sym->name, "main");
    return sym->kind == SYM_LOCAL;
}

/* Spec 058: the module symbol of this name (a local of the same name, E0203, does not hide it), or 0. */
Sym *module_find(Str *name)
{
    for (Sym *s = ck_scope; s; s = s->next)
        if (s->kind == SYM_MODULE && str_eq(s->name, name)) return s;
    return 0;
}

/* Spec 058: the offset of the `.` in a qualified name `m.name`, or -1. */
long str_dot(Str *s)
{
    for (long i = 0; i < s->len; i++)
        if (s->data[i] == '.') return i;
    return -1;
}

/* Spec 058: "std.json", "@pkg/a/b.vo" or "notes.vo": how a module's file is named in messages. */
Str *module_label(long target)
{
    FileRec *f = file_get((int)target);
    if (f->std) return msg("std.$1", str_new(f->path->data + 4, f->path->len - 7), 0, 0);
    Str *pkg = pkg_relpath(f->path);
    if (pkg) return pkg;
    long cut = f->path->len;
    while (cut > 0 && f->path->data[cut - 1] != '/') cut--;
    return str_new(f->path->data + cut, f->path->len - cut);
}

/* Spec 058 (research R5): the E0201 cases of module names, reported here and returning 1, or 0 for an
 * ordinary undefined name:
 * - `m.name` with `m` a module: a builtin written qualified, a private item (E0209), or a missing item;
 * - a bare name that exactly one imported module exports: the qualify fix, and later uses resolve to the
 *   item through a symbol that reports again with the same fix (SYF_QUALIFY, see rename_use). */
int module_undefined(Str *name, Node *node)
{
    long dot = str_dot(name);
    if (dot >= 0) {
        Sym *ms = module_find(str_new(name->data, dot));
        Sym *pre = ms ? 0 : scope_find(str_new(name->data, dot));
        if (!ms && pre && is_placeholder(pre)) { /* the module's import failed (reported there): silent */
            Sym *s = sym_new(name, SYM_LOCAL, 0, t_error, 0);
            s->flags = SYF_USED;
            s->slot = -1;
            scope_add(s);
            return 1;
        }
        if (!ms) return 0;
        Str *item = str_new(name->data + dot + 1, name->len - dot - 1);
        Diag *d;
        FileRec *mf = file_get((int)ms->slot);
        const LibChange *c = mf->std ? libchange_find(str_new(mf->path->data + 4, mf->path->len - 7), item) : 0;
        if (c) { /* a name changed in vo 0.2 (E0214): reported at each use, nothing bound */
            libchange_qualified(node, ms->name, c);
            return 1;
        }
        if (builtin_id(item)) {
            d = ck_err1("E0201", node, "`$1` is a builtin: write `$1`.", item);
            diag_add_fix(d, (int)node->file, node->start, node->end, item);
        } else {
            Node *decl = module_private_item(ms->slot, item);
            if (decl) {
                private_item_used(node, item, decl, ms->slot);
            } else {
                d = ck_err("E0201", node, msg("`$1` has no item `$2`.", module_label(ms->slot), item, 0));
                suggest_name(d, name, node);
            }
        }
        Sym *s = sym_new(name, SYM_LOCAL, 0, t_error, 0); /* a silent placeholder, as below */
        s->flags = SYF_USED;
        s->slot = -1;
        scope_add(s);
        return 1;
    }
    Sym *found = 0;
    long n = 0;
    for (Sym *s = ck_scope; s; s = s->next) {
        if (s->kind == SYM_MODULE || s->kind == SYM_LOCAL || !s->decl) continue;
        long sd = str_dot(s->name);
        if (sd < 0 || s->name->len - sd - 1 != name->len || memcmp(s->name->data + sd + 1, name->data, (size_t)name->len))
            continue;
        if (found && found->decl == s->decl) continue;
        found = s;
        n++;
    }
    if (n != 1) return 0;
    long fd = str_dot(found->name);
    Str *mod = str_new(found->name->data, fd);
    Sym *ms = module_find(mod);
    if (ms) ms->decl->e.i++; /* the use is meant for the module (no W0203) */
    Diag *d = ck_err("E0201", node, msg("`$1` is in `$2`: write `$3`.", name, ms ? module_label(ms->slot) : mod, found->name));
    diag_add_fix(d, (int)node->file, node->start, node->end, found->name);
    Sym *alias = sym_new(name, found->kind, found->decl, found->type, node);
    alias->flags |= SYF_QUALIFY;
    alias->rename = found->name;
    scope_add(alias);
    return 1;
}
