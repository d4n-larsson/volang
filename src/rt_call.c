/* Builtin dispatch: runs a builtin call on its argument values (ids: check_expr.c; ported from
 * src/rt_call.s, spec 008). The assembly's jump table `btab` is a switch over the same ids, bounded
 * by B_LAST.
 * ponytail: the bytecode engine reaches most builtins through builtin_call (cur_node, the effect
 * check, the switch), so builtin-heavy code gains little from spec 004; len, push and has have
 * their own instructions. Upgrade: more dedicated instructions where measurements show they pay. */
#include "vo.h"

enum {
    BU_PRINT = 1, BU_WRITE, BU_EPRINT, BU_READ_LINE, BU_READ_ALL, BU_READ_FILE, BU_WRITE_FILE, BU_ARGS,
    BU_EXIT, BU_NOW, BU_STR, BU_INT, BU_FLOAT, BU_PARSE_INT, BU_PARSE_FLOAT, BU_LEN,
    BU_SLICE, BU_FIND, BU_SPLIT, BU_JOIN, BU_TRIM, BU_CHARS, BU_PUSH, BU_POP,
    BU_SORT, BU_HAS, BU_REMOVE, BU_KEYS, BU_EXPECT, BU_EXPECT_EQ, BU_OK, BU_ERR,
    BU_ABS, BU_MIN, BU_MAX, BU_UPPER, BU_LOWER, BU_ORD, BU_CHR, BU_ENV, BU_RANDOM_INT,
    BU_SORT_BY, BU_MAP, BU_FILTER, BU_FOLD, BU_ANY, BU_ALL, BU_TODO, BU_ASSUME, BU_REQ_FAIL,
    BU_ENS_FAIL, BU_FLOAT_TEXT, BU_HTTP_EXCHANGE, BU_TEXT_FLOAT, BU_GET_OR, BU_SERVE_LISTEN, BU_SERVE_NEXT,
    BU_SERVE_REPLY, BU_SERVE_CALL, BU_HTTP_PART, BU_COLL, BU_CONCURRENT_MAP, BU_FS, BU_RUN, BU_HTTPS
};
_Static_assert(BU_HTTPS == B_LAST && BU_HTTPS == B_HTTPS && BU_RUN == B_RUN && BU_FS == B_FS && BU_COLL == B_COLL, "the builtin ids end at B_LAST");
_Static_assert(BU_TODO == B_TODO && BU_REQ_FAIL == B_REQ_FAIL && BU_SORT_BY == B_SORT_BY, "builtin ids");

/* The static type of the call's first argument. */
static Type *arg0_type(Node *call)
{
    return ((Node *)call->b.v->data[0].p)->type;
}

/* "<kind> `<condition, whitespace runs as one space>` of `<function>` does not hold." for a
 * compiled contract failure node (N_A the condition, N_C the function). */
static Str *contract_msg(Node *node, char *kind)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, kind);
    sb_byte(sb, '`');
    Str *text = src_text(node->a.n);
    GC_ROOT(&text);
    int space = 0; /* 1 after a whitespace character */
    for (long i = 0; i < text->len; i++) {
        int c = (unsigned char)text->data[i];
        if (c == ' ' || c == 10 || c == 9) {
            if (!space) sb_byte(sb, ' ');
            space = 1;
        } else {
            space = 0;
            sb_byte(sb, c);
        }
    }
    sb_cstr(sb, "` of `");
    sb_str(sb, node->c.n->a.n->a.s);
    sb_cstr(sb, "` does not hold.");
    GC_UNROOT(2);
    return sb_finish(sb);
}

/* Runs builtin call `call` on argument values (spec 004 research R8): sets cur_node, checks the
 * effect, then the body. */
long builtin_call(Node *call, long a0, long a1, long a2)
{
    cur_node = (long)call;
    long id = call->d.i;
    /* the effect must be declared (pure builtins: nothing to check); rt_check_bit allocates only
       when it raises, so the arguments need no roots across it */
    if (builtin_effects[id]) rt_check_bit(id);
    if ((unsigned long)(id - 1) >= B_LAST) return 0;
    switch (id) {
    case BU_TODO: { /* R0011: "Hole reached: <desc>." */
        SB *sb = sb_new();
        GC_ROOT(&sb); /* (rt_raise does not return: no unroot) */
        sb_cstr(sb, "Hole reached: ");
        sb_str(sb, (Str *)a0);
        sb_byte(sb, '.');
        rt_raise("R0011", sb_finish(sb), call);
        return 0;
    }
    case BU_ASSUME: /* false: discard this property case (prop.s) */
        if (!a0) rt_raise((char *)assume_code, 0, call);
        return 0;
    case BU_REQ_FAIL: { /* R0012 at the caller's call, the trace from the caller */
        Str *m = contract_msg(call, "Precondition ");
        Frame *f = (Frame *)cur_frame;
        Node *at = f->call ? f->call : call; /* no caller node (main): the condition itself */
        cur_frame = (long)f->parent;
        rt_raise("R0012", m, at);
        return 0;
    }
    case BU_ENS_FAIL: /* R0013 at the exit (a0: its node), the trace from the function */
        rt_raise("R0013", contract_msg(call, "Postcondition "), (void *)a0);
        return 0;
    case BU_PRINT: rt_print(1, (Str *)a0, 1); return 0;
    case BU_WRITE: rt_print(1, (Str *)a0, 0); return 0;
    case BU_EPRINT: rt_print(2, (Str *)a0, 1); return 0;
    case BU_READ_LINE: return (long)io_read_line(call); /* spec 006: io_* record or replay inputs */
    case BU_READ_ALL: return (long)io_read_all(call);
    case BU_READ_FILE: return (long)io_read_file((Str *)a0);
    case BU_WRITE_FILE: return (long)io_write_file((Str *)a0, (Str *)a1);
    case BU_ARGS: return (long)rt_args();
    case BU_EXIT: rt_exit(a0, call); return 0;
    case BU_NOW: return io_now_ms();
    case BU_ENV: return (long)io_env((Str *)a0);
    case BU_RANDOM_INT: return rt_random_int(a0, a1);
    case BU_SORT_BY: hof_sort_by((List *)a0, (void *)a1, call); return 0;
    case BU_MAP: return (long)hof_map((List *)a0, (void *)a1, call);
    case BU_CONCURRENT_MAP: return (long)task_map((List *)a0, a1, (void *)a2, call); /* spec 016 */
    case BU_FILTER: return (long)hof_filter((List *)a0, (void *)a1, call);
    case BU_FOLD: return hof_fold((List *)a0, a1, (void *)a2, call);
    case BU_ANY: return hof_any((List *)a0, (void *)a1, call, 0);
    case BU_ALL: return hof_any((List *)a0, (void *)a1, call, 1);
    case BU_STR: { /* by the argument's type */
        long kind = arg0_type(call)->kind;
        if (kind == TY_FLOAT) return (long)float_to_str(a0);
        if (kind == TY_BOOL) return (long)bool_str(a0);
        return (long)int_to_str(a0);
    }
    case BU_INT: return rt_int_of_float(a0, call);
    case BU_FLOAT: return rt_float_of_int(a0);
    case BU_FLOAT_TEXT: return (long)std_float_text(a0); /* spec 007: round-trip float text (std.json) */
    case BU_HTTP_EXCHANGE: return (long)io_http_exchange((void *)a0, a1, (void *)a2); /* std.http */
    case BU_SERVE_LISTEN: return (long)io_serve_listen((void *)a0); /* spec 011: std.http.serve */
    case BU_SERVE_NEXT: return (long)io_serve_next(a0);
    case BU_SERVE_REPLY: io_serve_reply((void *)a0); return 0;
    case BU_SERVE_CALL: return (long)rt_serve_call((void *)a0, a1, call);
    case BU_HTTP_PART: return (long)rt_http_part((Str *)a0, a1); /* spec 013 */
    case BU_COLL: return coll_op(call, a0, a1, a2); /* spec 014: std.collections */
    case BU_FS: return (long)io_fs(a0, (Str *)a1, (Str *)a2); /* spec 017: std.fs */
    case BU_RUN: return (long)io_run((List *)a0, (Str *)a1, (List *)a2); /* spec 017: std.process */
    case BU_HTTPS: return (long)io_https_exchange((void *)a0, a1, (void *)a2); /* spec 019: std.http */
    case BU_TEXT_FLOAT: return std_text_float((Str *)a0); /* spec 007: up to 19 digits, exactly */
    case BU_PARSE_INT: return (long)rt_parse_int((Str *)a0);
    case BU_PARSE_FLOAT: return (long)rt_parse_float((Str *)a0);
    case BU_LEN: {
        long kind = arg0_type(call)->kind;
        if (kind == TY_STR) return ((Str *)a0)->len;
        if (kind == TY_MAP) return ((Map *)a0)->count;
        return ((List *)a0)->len;
    }
    case BU_SLICE:
        if (arg0_type(call)->kind == TY_STR) return (long)rt_slice_str((Str *)a0, a1, a2, call);
        return (long)list_slice((List *)a0, a1, a2, call);
    case BU_FIND: return rt_find((Str *)a0, (Str *)a1);
    case BU_SPLIT: return (long)rt_split((Str *)a0, (Str *)a1, call);
    case BU_JOIN: return (long)rt_join((List *)a0, (Str *)a1);
    case BU_TRIM: return (long)rt_trim((Str *)a0);
    case BU_CHARS: return (long)rt_chars((Str *)a0);
    case BU_PUSH:
        if (TY_IS_COLL(arg0_type(call)->kind)) coll_push((Coll *)a0, a1); /* spec 014 */
        else list_push((List *)a0, a1);
        return 0;
    case BU_POP: return TY_IS_COLL(arg0_type(call)->kind) ? (long)coll_pop((Coll *)a0) : (long)list_pop((List *)a0);
    case BU_SORT: { /* by the element type */
        Type *elem = rt_concrete(arg0_type(call)->a.t);
        elem_record_type = elem; /* spec 014: records field by field */
        list_sort((List *)a0, (int)elem->kind);
        return 0;
    }
    case BU_HAS:
        if (TY_IS_COLL(arg0_type(call)->kind)) return coll_has((Coll *)a0, a1); /* spec 014 */
        return map_find((Map *)a0, a1, map_key_type()) >= 0;
    case BU_GET_OR: { /* spec 009: the stored value, or the default; never inserts */
        if (TY_IS_COLL(arg0_type(call)->kind)) return coll_get_or((Coll *)a0, a1, a2);
        long e = map_find((Map *)a0, a1, map_key_type());
        return e >= 0 ? ((Map *)a0)->entries[e].val.i : a2;
    }
    case BU_REMOVE:
        if (TY_IS_COLL(arg0_type(call)->kind)) coll_remove((Coll *)a0, a1);
        else map_remove((Map *)a0, a1, map_key_type());
        return 0;
    case BU_KEYS: return TY_IS_COLL(arg0_type(call)->kind) ? (long)coll_snapshot((Coll *)a0) : (long)map_keys((Map *)a0);
    case BU_EXPECT:
        if (call->flags & NF_EXPECT_OPS) /* spec 018: with the comparison's operands */
            rt_expect_cmp(a0, a1, a2, call);
        else
            rt_expect(a0, call);
        return 0;
    case BU_EXPECT_EQ: rt_expect_eq(a0, a1, call); return 0;
    case BU_OK: return (long)make_result(0, a0);
    case BU_ERR: return (long)make_result(1, a0);
    case BU_ABS: return rt_abs(a0, (int)arg0_kind(), call);
    case BU_MIN:
    case BU_MAX:
        elem_record_type = rt_concrete(arg0_type(call)); /* spec 014: records field by field */
        return rt_minmax(a0, a1, (int)arg0_kind(), id == BU_MAX);
    case BU_UPPER: return (long)rt_case((Str *)a0, 1);
    case BU_LOWER: return (long)rt_case((Str *)a0, 0);
    case BU_ORD: return rt_ord((Str *)a0, call);
    case BU_CHR: return (long)rt_chr(a0, call);
    default: return 0;
    }
}

/* Key type of the map passed as the call's first argument (read from cur_node). */
Type *map_key_type(void)
{
    return arg0_type((Node *)cur_node)->a.t;
}

/* The type kind of the current builtin call's first argument. */
long arg0_kind(void)
{
    return rt_concrete(arg0_type((Node *)cur_node))->kind;
}
