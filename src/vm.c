/* The bytecode engine (spec 004) and traces (spec 006 US3, US4): runs compiled functions
 * (compile.c) with a direct-threaded dispatch loop, writes trace records, and lists the
 * parameters and locals shown in runtime errors (ported from src/vm.s and src/trace.s, spec 008
 * R6). Frames live on the value stack (data-model.md "Frame"); vo-to-vo calls stay in the loop,
 * and builtins that call back into vo re-enter it through call_fn.
 *
 * Inside vm_run, pc is the instruction being executed, frame the current frame and slots its
 * slots. Slot operands are byte offsets from slots. Handlers are labels of vm_run, published in
 * vm_ops (and vm_ops_trace) by vm_init before compile_program fills instructions from them.
 * With --trace, compile_program copies its handlers from vm_ops_trace: vm_ops with OPC_RET and
 * OPC_TRY writing a `return` record first, and OPC_TRACE_ENTER (emitted at the start of every
 * function only when tracing) writing the `call` record. Runs without --trace execute exactly the
 * code they did before (FR-016).
 * ponytail: fixed 32-byte instructions with the source node inline (about 4x a packed encoding);
 * upgrade: 8-byte instructions with an opcode table and a side table of nodes, if memory matters.
 * ponytail: 100,000 trace records by default, then only counted; a per-function filter if traces
 * of long runs need more than the first records. */
#include "vo.h"

#define TRACE_DEFAULT_LIMIT 100000
#define VO_MIN ((long)(1UL << 63))

void *vm_ops[OPC_COUNT];       /* the handlers, in OPC order */
void *vm_ops_trace[OPC_COUNT]; /* vm_ops with the tracing handlers */

long tr_on; /* 1 with --trace (the compiler emits OPC_TRACE_ENTER) */
static FBuf *tr_fb; /* the trace file, or 0 when it could not be created */
static long tr_limit;
static long tr_written;
static long tr_omitted;
static long tr_depth;
static long tr_is_test;

long rt_reporting;
long rt_root_frame; /* spec 013: Frame * of the root grant (the frame pushed with no caller) */ /* 1 while a runtime error report is built (no timeouts) */
long rt_err_trace_txt;  /* Str *: "    at f (loc)\n        values\n..." or 0 */
long rt_err_trace_json; /* Str *: [{"function",...,"locals":[...]},...] or 0 */

/* Every function, lambda and test is compiled before a program runs (compile_program): one
 * without code is a toolchain bug (T0003, exit 125). */
void vm_not_compiled(void)
{
    out_cstr(2, "error[T0003]: internal error: a function has no compiled code\n");
    vo_exit(EXIT_INTERNAL);
}

/* The inline step just made steps a multiple of 65,536: undo it and let tick count it and check
 * the clock, at the current instruction's node. */
static void vm_tick_slow(Instr *pc)
{
    steps--;
    tick(pc->node);
}

/* Fills a generic call site's record for the running frame and the call instruction pc: the
 * caller's type arguments and eff0 as the key, rt_targs and rt_es_subst as values. */
static void vm_generic_resolve(GenericSite *gs, EffSet *eff0, Frame *frame, Instr *pc)
{
    Vec *targs = rt_targs(pc->node->e.v, frame);
    gs->targs = targs;
    gs->eff = 0;
    (void)eff0; /* spec 013: the grant is computed per call from the caller's (rt_es_grant) */
    gs->ctargs = frame->targs;
    gs->eff0 = eff0;
    gs->valid = 1;
}

/* A new frame for compiled function cf at the top of the value stack (spec 004 research R3, R4):
 * R0005 at the call when it does not fit, zeroed slots, the arguments copied from vals, then the
 * rules for effects, type arguments and captured values. Sets cur_frame; FR_RPC 0 and FR_RDST -1
 * (the caller fills them in for a call inside the loop). */
void *vm_push(void *cf_, void *decl_, void *call_, long *vals, void *closure_)
{
    CompiledFn *cf = cf_;
    Node *decl = decl_, *call = call_;
    Closure *closure = closure_;
    Frame *f = (Frame *)vsp;
    long *end = (long *)f->slots + cf->slots;
    if ((unsigned long)end >= (unsigned long)vstack_end) {
        rt_stack_full(call); /* (does not return) */
        return 0;
    }
    vsp = (long)end;
    for (long *p = (long *)f->slots; p < end; p++) *p = 0;
    if (vals) { /* a property's values: its FX_PARAMS (a test's N_B is its body, not a vec) */
        long n = decl->kind == N_TEST ? decl->d.fx->params->n : decl->b.v->n;
        for (long i = 0; i < n; i++) f->slots[i].i = vals[i];
    }
    Frame *caller = (Frame *)cur_frame;
    if (!caller) rt_root_frame = (long)f; /* spec 013: main's, a test's or a property case's frame */
    f->parent = caller;
    f->fn = decl;
    f->call = call;
    f->targs = 0;
    f->effects = 0;
    f->code = cf;
    f->rpc = 0;
    f->rdst = -1;
    /* effects: a test's grant; a function's declaration, or for an anonymous function its caller's */
    if (decl->kind == N_TEST) {
        if (decl->d.fx) f->effects = decl->d.fx->declared;
    } else if (decl->a.n && decl->a.n->b.fx) {
        EffSet *es = decl->a.n->b.fx->declared;
        if (es) f->effects = es;
        else if (caller) f->effects = caller->effects;
    }
    GC_ROOT(&closure);
    if (call && call->kind == N_CALL && call->e.v) { /* a generic call: its type arguments */
        f->targs = rt_targs(call->e.v, caller);
        f->effects = rt_es_grant(f->effects, caller ? caller->effects : 0); /* spec 013 (C3) */
    } else if (closure && closure->targs && decl->kind != N_TEST && decl->a.n->b.fx->declared) {
        /* spec 028: a generic function as a value: its effect parameters stand for the caller's grant */
        f->effects = rt_es_grant(f->effects, caller ? caller->effects : 0);
    }
    GC_UNROOT(1);
    if (closure) { /* its type arguments and captured values */
        f->targs = closure->targs;
        Vec *caps = decl->a.n->b.fx->caps;
        if (caps)
            for (long i = 0; i < closure->n; i++)
                f->slots[((Capture *)caps->data[i].p)->inner] = closure->caps[i];
    }
    cur_frame = (long)f;
    return f;
}

#define SL(off) (*(long *)((char *)slots + (off)))
#define SD(off) (*(double *)((char *)slots + (off)))
#define NEXT          \
    do {              \
        pc++;         \
        goto *pc->handler; \
    } while (0)
#define STORE(v)                 \
    do {                         \
        SL(pc->a) = (v);         \
        NEXT;                    \
    } while (0)
#define JUMP                     \
    do {                         \
        pc = (Instr *)pc->c.p;   \
        goto *pc->handler;       \
    } while (0)
#define STEP_FAST                                    \
    do {                                             \
        if (!(++steps & 0xFFFF)) vm_tick_slow(pc);   \
    } while (0)
/* a type parameter bound to int, bool or void compares inline (spec 004 US1) */
#define RAW_EQ_TYPE(t)                                                                        \
    ((t)->kind == TY_PARAM && frame->targs && (unsigned long)(t)->a.i < (unsigned long)frame->targs->n && \
     frame->targs->data[(t)->a.i].t &&                                                       \
     (frame->targs->data[(t)->a.i].t->kind == TY_INT || frame->targs->data[(t)->a.i].t->kind == TY_BOOL || \
      frame->targs->data[(t)->a.i].t->kind == TY_VOID))

/* Runs the compiled function of a frame made by vm_push until that frame returns; the result.
 * vm_run(0) publishes the handlers (vm_init). */
__attribute__((noinline, noclone))
long vm_run(void *start)
{
    static void *const ops[OPC_COUNT] = {
        &&op_mov, &&op_loadk,
        &&op_add_int, &&op_sub_int, &&op_mul_int, &&op_div_int, &&op_rem_int,
        &&op_add_float, &&op_sub_float, &&op_mul_float, &&op_div_float,
        &&op_concat, &&op_eq, &&op_ne,
        &&op_lt_int, &&op_le_int, &&op_gt_int, &&op_ge_int,
        &&op_lt_float, &&op_le_float, &&op_gt_float, &&op_ge_float,
        &&op_lt_str, &&op_le_str, &&op_gt_str, &&op_ge_str,
        &&op_not, &&op_neg_int, &&op_neg_float,
        &&op_jmp, &&op_jt, &&op_jf,
        &&op_for_range, &&op_for_list, &&op_inc, &&op_tick,
        &&op_call, &&op_call_builtin, &&op_ret, &&op_cmp_param,
        &&op_new_list, &&op_list_push, &&op_new_map, &&op_new_record, &&op_set_field, &&op_get_field,
        &&op_index_list, &&op_index_map, &&op_set_index_list, &&op_set_index_map,
        &&op_new_variant, &&op_set_payload, &&op_jtag_ne, &&op_payload, &&op_try,
        &&op_closure, &&op_fnval, &&op_call_value,
        &&op_add_int_k, &&op_sub_int_k, &&op_mul_int_k, &&op_div_int_k, &&op_rem_int_k,
        &&op_eq_int, &&op_ne_int,
        &&op_jlt, &&op_jle, &&op_jgt, &&op_jge, &&op_jeq, &&op_jne,
        &&op_jlt_k, &&op_jle_k, &&op_jgt_k, &&op_jge_k, &&op_jeq_k, &&op_jne_k,
        &&op_for_step, &&op_for_list_step, &&op_call_fast,
        &&op_len, &&op_push, &&op_has, &&op_call_generic,
        &&op_not_compiled, /* OPC_TRACE_ENTER: only emitted with --trace (vm_ops_trace) */
        &&op_index_coll, &&op_set_index_coll, &&op_snapshot, &&op_new_coll, /* spec 014 */
        &&op_cover, /* spec 018 */
        &&op_index_bytes, /* spec 021 */
        &&op_append, &&op_join,
        &&op_for_str, &&op_for_str_step, /* spec 042 */
        &&op_for_value, /* spec 046 */
    };
    if (!start) {
        for (int i = 0; i < OPC_COUNT; i++) vm_ops[i] = vm_ops_trace[i] = ops[i];
        vm_ops_trace[OPC_RET] = &&op_trace_ret;
        vm_ops_trace[OPC_TRY] = &&op_trace_try;
        vm_ops_trace[OPC_TRACE_ENTER] = &&op_trace_enter;
        return 0;
    }
    Frame *frame = start, *nf;
    W *slots = frame->slots;
    Instr *pc = frame->code->code;
    long r, x, y;
    Node *decl;
    Fx *fx;
    CompiledFn *cf;
    GenericSite *gs;
    EffSet *eff0;
    goto *pc->handler;

op_mov:
    STORE(SL(pc->b));
op_loadk:
    STORE(pc->c.i);

    /* ---- integer arithmetic ---- */
op_add_int:
    if (__builtin_add_overflow(SL(pc->b), SL(pc->c.i), &r)) goto overflow;
    STORE(r);
op_sub_int:
    if (__builtin_sub_overflow(SL(pc->b), SL(pc->c.i), &r)) goto overflow;
    STORE(r);
op_mul_int:
    if (__builtin_mul_overflow(SL(pc->b), SL(pc->c.i), &r)) goto overflow;
    STORE(r);
op_div_int:
    x = SL(pc->b);
    y = SL(pc->c.i);
    if (!y) goto div_zero;
    if (y == -1 && x == VO_MIN) goto overflow; /* MIN / -1 */
    STORE(vo_div(x, y));
op_rem_int:
    x = SL(pc->b);
    y = SL(pc->c.i);
    if (!y) goto div_zero;
    STORE(vo_rem(x, y)); /* MIN % -1 is 0 */

    /* ---- floats (the bits stay in the slots) ---- */
op_add_float: {
    double d = SD(pc->b) + SD(pc->c.i);
    SD(pc->a) = d;
    NEXT;
}
op_sub_float: {
    double d = SD(pc->b) - SD(pc->c.i);
    SD(pc->a) = d;
    NEXT;
}
op_mul_float: {
    double d = SD(pc->b) * SD(pc->c.i);
    SD(pc->a) = d;
    NEXT;
}
op_div_float: {
    double d = SD(pc->b) / SD(pc->c.i);
    SD(pc->a) = d;
    NEXT;
}

    /* ---- strings and equality (runtime calls; the operands stay in their slots for the collector) ---- */
op_concat:
    cur_node = (long)pc->node;
    STORE((long)str_concat((Str *)SL(pc->b), (Str *)SL(pc->c.i)));
    /* a = a + c, for `set s = s + c`: slot b holds a's value while nothing has read it since the last
       append (the compiler clears b before every read of s), so that string can grow in place */
op_append:
    cur_node = (long)pc->node;
    r = (long)str_append((Str *)SL(pc->a), (Str *)SL(pc->c.i), SL(pc->b) == SL(pc->a));
    SL(pc->b) = r;
    STORE(r);
op_join: /* the pieces stay in their slots for the collector */
    cur_node = (long)pc->node;
    STORE((long)str_join((W *)((char *)slots + pc->b), pc->c.i));
op_eq: {
    Type *t = pc->node->a.n->type;
    if (RAW_EQ_TYPE(t)) {
        cur_node = (long)pc->node; /* `==` sets cur_node; values_equal: POLL 1, a raw compare */
        POLL(1);
        STORE(SL(pc->b) == SL(pc->c.i));
    }
    cur_node = (long)pc->node; /* a long comparison can time out (R0009 points here) */
    STORE(values_equal(SL(pc->b), SL(pc->c.i), t));
}
op_ne: {
    Type *t = pc->node->a.n->type;
    if (RAW_EQ_TYPE(t)) {
        cur_node = (long)pc->node;
        POLL(1);
        STORE(SL(pc->b) != SL(pc->c.i));
    }
    cur_node = (long)pc->node;
    STORE(values_equal(SL(pc->b), SL(pc->c.i), t) ^ 1);
}

    /* ---- comparisons: 0 or 1 ---- */
op_lt_int:
    STORE(SL(pc->b) < SL(pc->c.i));
op_le_int:
    STORE(SL(pc->b) <= SL(pc->c.i));
op_gt_int:
    STORE(SL(pc->b) > SL(pc->c.i));
op_ge_int:
    STORE(SL(pc->b) >= SL(pc->c.i));
op_lt_float:
    STORE(SD(pc->b) < SD(pc->c.i));
op_le_float:
    STORE(SD(pc->b) <= SD(pc->c.i));
op_gt_float:
    STORE(SD(pc->c.i) < SD(pc->b));
op_ge_float:
    STORE(SD(pc->c.i) <= SD(pc->b));
op_lt_str:
    STORE(str_cmp((Str *)SL(pc->b), (Str *)SL(pc->c.i)) < 0);
op_le_str:
    STORE(str_cmp((Str *)SL(pc->b), (Str *)SL(pc->c.i)) < 1);
op_gt_str:
    STORE(str_cmp((Str *)SL(pc->b), (Str *)SL(pc->c.i)) > 0);
op_ge_str:
    STORE(str_cmp((Str *)SL(pc->b), (Str *)SL(pc->c.i)) >= 0);

    /* a <op> on an `ordered` type parameter: the concrete kind picks the int, float or str
       handler, which then runs on this same instruction */
op_cmp_param: {
    Type *t = pc->node->a.n->type;
    long kind;
    if (t->kind == TY_PARAM && frame->targs && (unsigned long)t->a.i < (unsigned long)frame->targs->n &&
        frame->targs->data[t->a.i].t)
        kind = frame->targs->data[t->a.i].t->kind; /* rt_concrete's bare-parameter case, inline */
    else
        kind = rt_concrete(t)->kind;
    if (kind == TY_RECORD) { /* spec 014: field by field */
        long rc = value_cmp(SL(pc->b), SL(pc->c.i), rt_concrete(t));
        long which = pc->node->d.i - OP_LT_PARAM; /* <, <=, >, >= */
        STORE(which == 0 ? rc < 0 : which == 1 ? rc <= 0 : which == 2 ? rc > 0 : rc >= 0);
    }
    long base = kind == TY_FLOAT ? OP_LT_FLOAT : (kind == TY_STR || kind == TY_BYTES) ? OP_LT_STR : OP_LT_INT;
    goto *ops[pc->node->d.i - OP_LT_PARAM + base + (OPC_ADD_INT - OP_ADD_INT)];
}

    /* ---- unary ---- */
op_not:
    STORE(SL(pc->b) ^ 1);
op_neg_int:
    x = SL(pc->b);
    if (x == VO_MIN) goto overflow; /* -MIN */
    STORE(-x);
op_neg_float:
    STORE((long)((unsigned long)SL(pc->b) ^ (1UL << 63)));

    /* ---- control ---- */
op_jmp:
    JUMP;
op_jt:
    if (SL(pc->b)) JUMP;
    NEXT;
op_jf:
    if (!SL(pc->b)) JUMP;
    NEXT;

    /* for over a range: counter at b, end at b+8 (both hidden temporaries) */
op_for_range:
    if (SL(pc->b) >= SL(pc->b + 8)) JUMP;
    tick(pc->node);
    STORE(SL(pc->b));
    /* for over a list: the list at b, the index at b+8; the length and data are read every time */
    /* spec 046: the second variable of a two-variable loop, first in its body. The first variable
       (slot a-8) is a key of the map in b, or an index into the list in b; a key the map no longer
       has is skipped (c: the back-edge), an index past the end ends the loop (c: the exit). */
op_for_value: {
    long kind = pc->node->b.n->type->kind;
    x = SL(pc->a - 8);
    cur_node = (long)pc->node;
    if (kind == TY_MAP) {
        long e = map_find((Map *)SL(pc->b), x, pc->node->b.n->type->a.t);
        if (e < 0) JUMP;
        STORE(((Map *)SL(pc->b))->entries[e].val.i);
    }
    if (kind == TY_OMAP) {
        if (!coll_has((Coll *)SL(pc->b), x)) JUMP;
        STORE(coll_index((Coll *)SL(pc->b), x, pc->node));
    }
    if (x >= ((List *)SL(pc->b))->len) JUMP;
    STORE(((List *)SL(pc->b))->data[x].i);
}
op_for_list:
    if (SL(pc->b + 8) >= ((List *)SL(pc->b))->len) JUMP;
    tick(pc->node);
    STORE(((List *)SL(pc->b))->data[SL(pc->b + 8)].i);
op_inc:
    SL(pc->a)++;
    NEXT;
    /* spec 042: for over a str: the text at b, the byte offset of the next character at b+8. A str is
       valid UTF-8, so the lead byte gives the length; ASCII characters are the shared strings */
op_for_str:
    x = SL(pc->b + 8);
    if (x >= ((Str *)SL(pc->b))->len) JUMP;
    tick(pc->node);
    goto for_str_take;
op_for_str_step:
    x = SL(pc->b + 8);
    if (x >= ((Str *)SL(pc->b))->len) NEXT;
    STEP_FAST;
for_str_take:
    y = (unsigned char)((Str *)SL(pc->b))->data[x];
    y = y < 0x80 ? 1 : y < 0xe0 ? 2 : y < 0xf0 ? 3 : 4;
    SL(pc->b + 8) = x + y;
    cur_node = (long)pc->node;
    SL(pc->a) = (long)(y == 1 ? str_one(((Str *)SL(pc->b))->data[x]) : str_new(((Str *)SL(pc->b))->data + x, y));
    if (pc->handler == &&op_for_str) NEXT;
    JUMP;
op_tick:
    STEP_FAST;
    NEXT;

op_cover: /* spec 018: a coverage point; a constant store, so tasks writing the same bytes cannot race */
    cov_hit[pc->a] = 1;
    if (pc->b >= 0 && cov_reach) cov_reach[cov_test * cov_nfn + pc->b] = 1;
    else if (!cov_reach) pc->handler = &&op_cover_done; /* --coverage: once is enough (--mutate needs reach per test) */
    NEXT;
op_cover_done: /* a coverage point already reached */
    NEXT;

    /* ---- calls ----
       a = c(arguments in b, b+8, ...): c is the callee's declaration. A compiled callee gets a frame
       and runs in this loop; one not compiled (yet) runs through call_fn. */
op_call:
    decl = pc->c.n;
    cf = decl->a.n->b.fx->code;
    if (!cf) STORE(call_fn(decl, pc->node, 0, &SL(pc->b), 0));
    nf = vm_push(cf, decl, pc->node, &SL(pc->b), 0);
enter: /* nf: the callee's new frame; return here to the next instruction */
    nf->rpc = pc;
    nf->rdst = pc->a;
    frame = nf;
    slots = frame->slots;
    tick(pc->node); /* the call counts as a step (in the callee's frame) */
    pc = frame->code->code;
    goto *pc->handler;

    /* a = builtin(b, ...): c = argument count | second slot << 16 | third slot << 40 */
op_call_builtin: {
    unsigned long c = pc->c.u;
    long n = (long)(c & 0xFF), a0 = 0, a1 = 0, a2 = 0;
    if (n) {
        a0 = SL(pc->b);
        if (n > 1) {
            a1 = SL((long)((c << 24) >> 40));
            if (n > 2) a2 = SL((long)(c >> 40));
        }
    }
    STORE(builtin_call(pc->node, a0, a1, a2));
}

    /* return b (b < 0: 0): pop the frame; back to the caller's next instruction, or out of vm_run */
op_ret:
    r = pc->b < 0 ? 0 : SL(pc->b);
return_r: /* return r from the current frame */
    cur_frame = (long)frame->parent;
    vsp = (long)frame;
    if (!frame->rpc) return r; /* the frame vm_run was entered with */
    pc = frame->rpc;
    x = frame->rdst;
    frame = frame->parent;
    slots = frame->slots;
    SL(x) = r;
    NEXT;

    /* ---- data (the same runtime calls and checks as the tree walker) ---- */
op_new_list:
    STORE((long)list_new(pc->c.i));
op_list_push:
    list_push((List *)SL(pc->a), SL(pc->b));
    NEXT;
op_new_map:
    STORE((long)map_new());
op_new_record:
    STORE((long)alloc_obj(OBJ_RECORD, pc->c.i * 8));
op_set_field:
    *(long *)((char *)SL(pc->a) + pc->c.i) = SL(pc->b);
    NEXT;
op_get_field: {
    char *rec = (char *)SL(pc->b);
#ifdef GC_STRESS_BUILD
    gc_check(rec, OBJ_RECORD); /* stress build: a live record (spec 003 FR-011) */
#endif
    STORE(*(long *)(rec + pc->c.i));
}
op_index_list: {
    List *l = (List *)SL(pc->b);
    long i = SL(pc->c.i);
    if (i < 0 || i >= l->len) {
        rt_index_error(pc->node, i, l->len);
        return 0; /* (not reached) */
    }
    STORE(l->data[i].i);
}
op_index_map: {
    long e = map_find((Map *)SL(pc->b), SL(pc->c.i), pc->node->a.n->type->a.t);
    if (e < 0) {
        rt_key_error(pc->node, SL(pc->c.i), pc->node->a.n->type->a.t);
        return 0; /* (not reached) */
    }
    STORE(((Map *)SL(pc->b))->entries[e].val.i);
}
op_set_index_list: {
    List *l = (List *)SL(pc->a);
    long i = SL(pc->b);
    if (i < 0 || i >= l->len) {
        rt_index_error(pc->node, i, l->len);
        return 0; /* (not reached) */
    }
    l->data[i].i = SL(pc->c.i);
    NEXT;
}
op_index_bytes: { /* spec 021 */
    Str *s = (Str *)SL(pc->b);
    long i = SL(pc->c.i);
    if (i < 0 || i >= s->len) {
        rt_index_error(pc->node, i, s->len);
        return 0; /* (not reached) */
    }
    STORE((long)(unsigned char)s->data[i]);
}
op_index_coll: /* spec 014: R0004 / R0003 from the collection */
    cur_node = (long)pc->node;
    STORE(coll_index((Coll *)SL(pc->b), SL(pc->c.i), pc->node));
op_set_index_coll:
    cur_node = (long)pc->node;
    coll_set_index((Coll *)SL(pc->a), SL(pc->b), SL(pc->c.i), pc->node);
    NEXT;
op_snapshot:
    cur_node = (long)pc->node;
    if (pc->node->kind == N_FOR && pc->node->b.n->type->kind == TY_MAP) /* spec 046: `for k, v in m`: its keys now */
        STORE((long)map_keys((Map *)SL(pc->b)));
    STORE((long)coll_snapshot((Coll *)SL(pc->b)));
op_new_coll: /* the concrete collection type orders it */
    cur_node = (long)pc->node;
    STORE((long)coll_new(rt_concrete(pc->node->type), 0));
op_set_index_map:
    cur_node = (long)pc->node; /* the `set` statement; its place gives the map's key type */
    map_set((Map *)SL(pc->a), SL(pc->b), SL(pc->c.i), pc->node->a.n->a.n->type->a.t);
    NEXT;
op_new_variant: {
    Variant *v = alloc_obj(OBJ_VARIANT, VARIANT_SIZE);
    *(uint32_t *)&v->tag = (uint32_t)pc->c.i;
    STORE((long)v);
}
op_set_payload:
    ((Variant *)SL(pc->a))->pay.i = SL(pc->b);
    NEXT;
op_jtag_ne:
    if ((long)*(uint32_t *)&((Variant *)SL(pc->b))->tag != (long)pc->a) JUMP;
    NEXT;
op_payload:
    STORE(((Variant *)SL(pc->b))->pay.i);
op_try: { /* ok: unwrap; err: the err value is the function's result */
    Variant *v = (Variant *)SL(pc->b);
    if (*(uint32_t *)&v->tag) {
        r = (long)v;
        goto return_r;
    }
    STORE(v->pay.i);
}

    /* ---- function values ---- */
op_closure:
    STORE((long)make_closure(pc->node));
op_fnval:
    STORE((long)(pc->node->e.v ? fn_closure_with(pc->c.n, rt_targs(pc->node->e.v, frame)) : fn_closure(pc->c.n)));
    /* a = closure(c)(arguments in b, ...) */
op_call_value: {
    Closure *cl = (Closure *)SL(pc->c.i);
#ifdef GC_STRESS_BUILD
    gc_check(cl, OBJ_CLOSURE); /* stress build: a live OBJ_CLOSURE (spec 003 FR-011) */
#endif
    cf = cl->fn->a.n->b.fx->code;
    if (!cf) STORE(call_value(cl, pc->node, &SL(pc->b)));
    nf = vm_push(cf, cl->fn, pc->node, &SL(pc->b), cl);
    goto enter;
}

    /* ---- constant operands (spec 004 US1): c is the constant ---- */
op_add_int_k:
    if (__builtin_add_overflow(SL(pc->b), pc->c.i, &r)) goto overflow;
    STORE(r);
op_sub_int_k:
    if (__builtin_sub_overflow(SL(pc->b), pc->c.i, &r)) goto overflow;
    STORE(r);
op_mul_int_k:
    if (__builtin_mul_overflow(SL(pc->b), pc->c.i, &r)) goto overflow;
    STORE(r);
op_div_int_k: /* c not 0 or -1 */
    STORE(vo_div(SL(pc->b), pc->c.i));
op_rem_int_k: /* c not 0 */
    STORE(vo_rem(SL(pc->b), pc->c.i));
op_eq_int:
    cur_node = (long)pc->node; /* `==` sets cur_node (out-of-memory reports) */
    STORE(SL(pc->b) == SL(pc->c.i));
op_ne_int:
    cur_node = (long)pc->node;
    STORE(SL(pc->b) != SL(pc->c.i));

    /* ---- fused compare-and-branch (spec 004 US1): if a <cond> b: pc = c ---- */
op_jlt:
    if (SL(pc->a) < SL(pc->b)) JUMP;
    NEXT;
op_jle:
    if (SL(pc->a) <= SL(pc->b)) JUMP;
    NEXT;
op_jgt:
    if (SL(pc->a) > SL(pc->b)) JUMP;
    NEXT;
op_jge:
    if (SL(pc->a) >= SL(pc->b)) JUMP;
    NEXT;
op_jeq:
    cur_node = (long)pc->node; /* (from `==`: sets cur_node) */
    if (SL(pc->a) == SL(pc->b)) JUMP;
    NEXT;
op_jne:
    cur_node = (long)pc->node;
    if (SL(pc->a) != SL(pc->b)) JUMP;
    NEXT;
op_jlt_k:
    if (SL(pc->a) < (long)pc->b) JUMP;
    NEXT;
op_jle_k:
    if (SL(pc->a) <= (long)pc->b) JUMP;
    NEXT;
op_jgt_k:
    if (SL(pc->a) > (long)pc->b) JUMP;
    NEXT;
op_jge_k:
    if (SL(pc->a) >= (long)pc->b) JUMP;
    NEXT;
op_jeq_k:
    cur_node = (long)pc->node;
    if (SL(pc->a) == (long)pc->b) JUMP;
    NEXT;
op_jne_k:
    cur_node = (long)pc->node;
    if (SL(pc->a) != (long)pc->b) JUMP;
    NEXT;

    /* ---- loop back-edges (spec 004 US1): one instruction per iteration, the step counted inline ----
       range: counter b, end b+8; c is the first instruction of the body */
op_for_step:
    x = ++SL(pc->b);
    if (x >= SL(pc->b + 8)) NEXT;
    STEP_FAST; /* every 65,536th step: tick's clock check */
    SL(pc->a) = SL(pc->b);
    JUMP;
    /* list: list b, index b+8 */
op_for_list_step:
    x = ++SL(pc->b + 8);
    if (x >= ((List *)SL(pc->b))->len) NEXT;
    STEP_FAST;
    SL(pc->a) = ((List *)SL(pc->b))->data[SL(pc->b + 8)].i;
    JUMP;

    /* CALL of a named, non-generic function (spec 004 US1): the frame of vm_push, set up inline. A
       callee without code, or a frame that does not fit (R0005), takes op_call's path. */
op_call_fast: {
    decl = pc->c.n;
    fx = decl->a.n->b.fx;
    cf = fx->code;
    if (!cf) goto op_call;
    nf = (Frame *)vsp;
    long *end = (long *)nf->slots + cf->slots;
    if ((unsigned long)end >= (unsigned long)vstack_end) goto op_call;
    vsp = (long)end;
    long *dst = (long *)nf->slots, *src = &SL(pc->b);
    for (long n = cf->nparams; n; n--) *dst++ = *src++; /* the parameters, then zeros */
    while (dst < end) *dst++ = 0;
    nf->parent = frame;
    nf->fn = decl;
    nf->call = pc->node;
    nf->targs = 0;
    nf->code = cf;
    nf->rpc = pc;
    nf->rdst = pc->a;
    nf->effects = fx->declared ? fx->declared : frame->effects; /* or (none declared) the caller's */
    goto inline_enter;
}

    /* ---- hot pure builtins (spec 004 US1): len and push without builtin_call; cur_node as before ---- */
op_len:
    cur_node = (long)pc->node;
    STORE(((List *)SL(pc->b))->len);
op_push:
    cur_node = (long)pc->node;
    list_push((List *)SL(pc->a), SL(pc->b));
    NEXT;
op_has: /* map_find with the key type of the first argument */
    cur_node = (long)pc->node;
    STORE(map_find((Map *)SL(pc->b), SL(pc->c.i), pc->node->b.v->data[0].n->type->a.t) >= 0);

    /* a = generic call: CALL_FAST's inline frame, with type arguments and effects from the call
       site's record (vm_push's rt_targs / rt_es_subst, recomputed only when the caller's type
       arguments or effect set differ from the last call's). c is the record. */
op_call_generic: {
    gs = (GenericSite *)pc->c.p;
    decl = gs->decl;
    fx = decl->a.n->b.fx;
    cf = fx->code;
    if (!cf) STORE(call_fn(decl, pc->node, 0, &SL(pc->b), 0)); /* no code: through call_fn */
    eff0 = fx->declared ? fx->declared : frame->effects; /* the effect set before substitution */
    if (!gs->valid || gs->ctargs != frame->targs || gs->eff0 != eff0) {
        vm_generic_resolve(gs, eff0, frame, pc); /* (re)fill the record */
        gs = (GenericSite *)pc->c.p;
        cf = gs->decl->a.n->b.fx->code;
    }
    nf = (Frame *)vsp;
    long *end = (long *)nf->slots + cf->slots;
    if ((unsigned long)end >= (unsigned long)vstack_end) goto op_call; /* (R0005 through vm_push) */
    vsp = (long)end;
    long *dst = (long *)nf->slots, *src = &SL(pc->b);
    for (long n = cf->nparams; n; n--) *dst++ = *src++;
    while (dst < end) *dst++ = 0;
    nf->parent = frame;
    nf->fn = gs->decl;
    nf->call = pc->node;
    nf->targs = gs->targs;
    nf->effects = rt_es_grant(eff0, frame->effects); /* spec 013 (C3) */
    nf->code = cf;
    nf->rpc = pc;
    nf->rdst = pc->a;
    goto inline_enter;
}
inline_enter: /* nf set up inline: run it; the call counts as a step */
    cur_frame = (long)nf;
    frame = nf;
    slots = frame->slots;
    STEP_FAST; /* (pc is still the call: its node) */
    pc = frame->code->code;
    goto *pc->handler;

    /* ---- the tracing handlers (vm_ops_trace): each continues like the handler it stands for ---- */
op_trace_enter:
    tr_call(frame);
    NEXT;
op_trace_ret:
    tr_return(frame, pc->b < 0 ? 0 : SL(pc->b));
    goto op_ret;
op_trace_try:
    if (*(uint32_t *)&((Variant *)SL(pc->b))->tag) tr_return(frame, SL(pc->b)); /* an err: the function returns it */
    goto op_try;

op_not_compiled:
    vm_not_compiled();
    return 0;

overflow:
    rt_overflow(pc->node);
div_zero:
    rt_div_zero(pc->node);
    return 0; /* (not reached: runtime errors do not return) */
}

/* Publishes the handlers in vm_ops and vm_ops_trace (before compile_program fills instructions). */
void vm_init(void)
{
    vm_run(0);
}

/* ---- traces (contracts/trace.md) ---- */

/* With --trace: opens the trace file and makes compile_program use the tracing handlers. */
void trace_start(long is_test)
{
    if (!opt_trace) return;
    tr_is_test = is_test;
    tr_on = 1;
    tr_limit = opt_trace_limit ? opt_trace_limit : TRACE_DEFAULT_LIMIT;
    tr_fb = fbuf_open((char *)opt_trace);
    c_ops = vm_ops_trace;
}

/* 1 when another record may be written; otherwise counts it as omitted and returns 0. */
static long tr_room(void)
{
    if (tr_written >= tr_limit) {
        tr_omitted++;
        return 0;
    }
    tr_written++;
    return 1;
}

static void tr_emit(SB *sb)
{
    if (tr_fb) fbuf_write(tr_fb, sb->ptr, sb->len);
}

/* ---- spec 016: traces of tasks (research R11) ---- */
static long tr_omitted_base; /* in a task: tr_omitted when it started */

/* In a task: records go to the parent; the inherited count bounds how many it may write. */
void tr_task_child(void)
{
    if (tr_fb) tr_fb->fd = -4; /* TM_TRACE (task.c) */
    tr_omitted_base = tr_omitted;
}

/* In a task, at its end: the rest of its records; returns how many it left out. */
long tr_task_end(void)
{
    if (tr_fb) fbuf_flush(tr_fb);
    return tr_omitted - tr_omitted_base;
}

/* Before a task starts: nothing buffered is inherited. */
void tr_flush(void)
{
    if (tr_on && tr_fb) fbuf_flush(tr_fb);
}

/* In the parent: the `task` record before the records of the task at index. */
void tr_task_record(long index)
{
    if (!tr_on || !tr_room()) return;
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, "{\"v\":1,\"kind\":\"task\",\"index\":");
    int_to_dec(sb, index);
    sb_cstr(sb, "}\n");
    tr_emit(sb);
    GC_UNROOT(1);
}

/* In the parent: a task's records (whole lines), counted against --trace-limit in merged order. */
void tr_take(char *p, long n, long omitted)
{
    tr_omitted += omitted;
    for (long i = 0; i < n;) {
        long j = i;
        while (j < n && p[j] != '\n') j++;
        if (j < n) j++;
        if (tr_on && tr_room() && tr_fb) fbuf_write(tr_fb, p + i, j - i);
        i = j;
    }
}

/* A value in vo syntax (shortened to 200 bytes) as a JSON string. */
static void tr_value(SB *sb, long value, Type *type)
{
    GC_ROOT(&sb);
    SB *tmp = sb_new();
    GC_ROOT(&tmp);
    render_capped(tmp, value, type);
    sb_byte(sb, '"');
    json_escape_into(sb, tmp->ptr, tmp->len);
    sb_byte(sb, '"');
    GC_UNROOT(2);
}

/* The parameter vec of a function, lambda or property; 0 for a test. */
static Vec *tr_params(Node *decl)
{
    if (decl->kind != N_TEST) return decl->b.v;
    if (decl->flags & NF_PROP) return decl->d.fx->params;
    return 0;
}

/* The `call` record of a frame that starts running. */
void tr_call(void *frame_)
{
    Frame *frame = frame_;
    tr_depth++;
    if (!tr_room()) return;
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, "{\"v\":1,\"kind\":\"call\",\"depth\":");
    int_to_dec(sb, tr_depth);
    sb_cstr(sb, ",\"function\":");
    json_str_into(sb, frame_fn_name(frame));
    Node *site = frame->call;
    if (site) {
        sb_cstr(sb, ",\"file\":");
        json_str_into(sb, file_get((int)site->file)->path);
        sb_cstr(sb, ",\"start\":");
        json_pos(sb, (int)site->file, site->start);
    }
    sb_cstr(sb, ",\"args\":[");
    Vec *params = tr_params(frame->fn);
    for (long i = 0; params && i < params->n; i++) {
        if (i) sb_byte(sb, ',');
        Node *p = vec_get(params, i); /* N_PARAM: name node, slot, type */
        sb_cstr(sb, "{\"name\":");
        json_str_into(sb, p->a.n->a.s);
        sb_cstr(sb, ",\"value\":");
        tr_value(sb, frame->slots[p->e.i].i, p->type);
        sb_byte(sb, '}');
    }
    sb_cstr(sb, "]}\n");
    tr_emit(sb);
    GC_UNROOT(1);
}

/* The return type of a function or lambda; 0 when it returns nothing (and for tests). */
static Type *tr_ret_type(Node *decl)
{
    if (decl->kind == N_TEST) return 0;
    Type *t = decl->type;
    if (decl->kind == N_LAMBDA) {
        if (!t) return 0;
        t = t->b.t; /* a lambda: its function type's result */
    }
    if (!t || t->kind == TY_VOID) return 0;
    return t;
}

/* The `return` record of a frame about to return value. */
void tr_return(void *frame_, long value)
{
    Frame *frame = frame_;
    if (tr_room()) {
        SB *sb = sb_new();
        GC_ROOT(&sb);
        sb_cstr(sb, "{\"v\":1,\"kind\":\"return\",\"depth\":");
        int_to_dec(sb, tr_depth);
        sb_cstr(sb, ",\"function\":");
        json_str_into(sb, frame_fn_name(frame));
        Type *t = tr_ret_type(frame->fn);
        if (t) {
            sb_cstr(sb, ",\"value\":");
            tr_value(sb, value, t);
        }
        sb_cstr(sb, "}\n");
        tr_emit(sb);
        GC_UNROOT(1);
    }
    tr_depth--;
}

/* `vo test`: the record before a test's (or a property case's) calls. */
void tr_test(void *test)
{
    if (!tr_on) return;
    tr_depth = 0;
    if (!tr_room()) return;
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, "{\"v\":1,\"kind\":\"test\",\"name\":");
    json_str_into(sb, fx_item_name(test));
    sb_cstr(sb, "}\n");
    tr_emit(sb);
    GC_UNROOT(1);
}

/* The runtime error that ends the run (or the test). */
void tr_error(char *code, void *message, void *node_)
{
    Node *node = node_;
    if (!tr_room()) return;
    GC_ROOT(&message);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, "{\"v\":1,\"kind\":\"error\",\"code\":\"");
    sb_cstr(sb, code);
    sb_cstr(sb, "\",\"name\":\"");
    sb_str(sb, catalog_name(code));
    sb_byte(sb, '"');
    sb_cstr(sb, ",\"message\":");
    json_str_into(sb, message);
    if (node) {
        sb_cstr(sb, ",\"file\":");
        json_str_into(sb, file_get((int)node->file)->path);
        sb_cstr(sb, ",\"start\":");
        json_pos(sb, (int)node->file, node->start);
    }
    sb_cstr(sb, "}\n");
    tr_emit(sb);
    GC_UNROOT(2);
}

/* ---- the end ---- */

/* Writes a C string to a buffered file (no allocation). */
void fb_cstr(void *fb, char *s)
{
    fbuf_write(fb, s, cstr_len(s));
}

/* Writes a non-negative decimal number (no allocation). */
void fb_dec(void *fb, long v)
{
    char buf[32];
    char *p = buf + sizeof buf;
    do {
        *--p = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    fbuf_write(fb, p, buf + sizeof buf - p);
}

/* Writes the `end` record and flushes the trace; returns the exit status (74 when the trace could
 * not be written and the run had not already failed). No allocation (out-of-memory path). */
static long tr_finish(long status)
{
    if (!tr_on) return status;
    tr_on = 0; /* once */
    FBuf *fb = tr_fb;
    if (fb) {
        fb_cstr(fb, "{\"v\":1,\"kind\":\"end\",\"records\":");
        fb_dec(fb, tr_written);
        fb_cstr(fb, ",\"omitted\":");
        fb_dec(fb, tr_omitted);
        if (!tr_is_test) {
            fb_cstr(fb, ",\"status\":");
            fb_dec(fb, status);
        }
        fb_cstr(fb, "}\n");
        fbuf_flush(fb);
        if (!fb->err) return status;
    }
    out_unwritable((char *)opt_trace);
    return status ? status : EXIT_OUTPUT;
}

/* T0009 for a trace or recording that could not be written (reported on stderr). */
void out_unwritable(char *path)
{
    Str *m = msg("Cannot write the trace or recording `$1`.", str_from_cstr(path), 0, 0);
    toolchain_error((int)opt_json, 2, "T0009", m, 0);
}

/* Called by vo_exit before the output is flushed: completes the trace and the recording, and
 * returns the exit status (74 when one of them could not be written, T0009). Must not allocate on
 * the way of a normal end (it runs on the out-of-memory path too). */
long exit_hooks(long status)
{
    return rcd_finish(tr_finish(status));
}

/* ---- values in runtime errors (spec 006 research R13) ---- */

typedef struct Local {
    Str *name;
    Type *type;
    long slot;
} Local;

/* One visible local. */
static void fl_add(Vec *vec, Str *name, Type *type, long slot)
{
    if (slot < 0 || !name) return;
    Local *l = alloc(sizeof(Local));
    l->name = name;
    l->type = type;
    l->slot = slot;
    vec_push(vec, l);
}

static int fl_contains(Node *node, long off)
{
    return node && node->start <= off && off < node->end;
}

static void fl_stmt(Vec *vec, Node *stmt, long off);

/* The bindings of a block visible at off: the `let`s of the statements that end before it, then
 * whatever the statement containing off declares around it. */
static void fl_block(Vec *vec, Node *block, long off)
{
    if (!block) return;
    Vec *stmts = block->a.v;
    for (long i = 0; i < stmts->n; i++) {
        Node *s = vec_get(stmts, i);
        if (s->end > off) { /* the statement containing off (later ones are not reached yet) */
            if (s->start <= off) fl_stmt(vec, s, off);
            return;
        }
        if (s->kind == N_LET && s->a.n) fl_add(vec, s->a.n->a.s, s->type, s->e.i); /* ended before off */
    }
}

/* Inside a statement that contains off: its block that contains off, with the loop variable or
 * match binding of that block. */
static void fl_stmt(Vec *vec, Node *stmt, long off)
{
    switch (stmt->kind) {
    case N_BLOCK:
        fl_block(vec, stmt, off);
        return;
    case N_IF: { /* then, else, else-if */
        if (fl_contains(stmt->b.n, off)) {
            fl_block(vec, stmt->b.n, off);
            return;
        }
        Node *e = stmt->c.n;
        if (!fl_contains(e, off)) return;
        if (e->kind == N_IF) fl_stmt(vec, e, off);
        else fl_block(vec, e, off);
        return;
    }
    case N_WHILE:
        if (fl_contains(stmt->b.n, off)) fl_block(vec, stmt->b.n, off);
        return;
    case N_FOR: { /* the variable, inside the body */
        if (!fl_contains(stmt->d.n, off)) return;
        Type *t = t_int; /* a range: int */
        if (stmt->flags & NF_PAIR) { /* spec 046: key and value, or index and element */
            Type *ct = stmt->b.n->type;
            int is_map = ct && (ct->kind == TY_MAP || ct->kind == TY_OMAP);
            if (!ct || !(is_map || ct->kind == TY_LIST || ct->kind == TY_DEQUE)) return;
            fl_add(vec, stmt->a.n ? stmt->a.n->a.s : 0, is_map ? ct->a.t : t_int, stmt->e.i);
            fl_add(vec, stmt->c.n ? stmt->c.n->a.s : 0, is_map ? ct->b.t : ct->a.t, stmt->e.i + 1);
            fl_block(vec, stmt->d.n, off);
            return;
        }
        if (!stmt->c.n) { /* a list: its element type */
            Type *lt = stmt->b.n->type;
            t = lt && (lt->kind == TY_LIST || TY_IS_COLL(lt->kind)) ? lt->a.t : 0; /* spec 014: collections too */
            if (lt && lt->kind == TY_STR) t = t_str; /* spec 042: a text's characters */
        }
        if (t) fl_add(vec, stmt->a.n ? stmt->a.n->a.s : 0, t, stmt->e.i);
        fl_block(vec, stmt->d.n, off);
        return;
    }
    case N_MATCH: { /* the arm containing off */
        Vec *arms = stmt->b.v;
        for (long i = 0; i < arms->n; i++) {
            Node *arm = vec_get(arms, i);
            if (!fl_contains(arm->c.n, off)) continue;
            if (arm->b.n && arm->type) fl_add(vec, arm->b.n->a.s, arm->type, arm->e.i);
            fl_block(vec, arm->c.n, off);
            return;
        }
        return;
    }
    default:
        return;
    }
}

/* The parameters (and an anonymous function's captures) of a frame, then its locals visible at
 * the node pos, in declaration order: a vec of Local. */
static Vec *frame_locals(Frame *frame, Node *pos)
{
    Vec *vec = vec_new();
    GC_ROOT(&vec);
    Node *fn = frame->fn;
    Vec *params = tr_params(fn);
    for (long i = 0; params && i < params->n; i++) {
        Node *p = vec_get(params, i);
        fl_add(vec, p->a.n->a.s, p->type, p->e.i);
    }
    if (fn->kind == N_LAMBDA && fn->a.n->b.fx && fn->a.n->b.fx->caps) { /* its captures */
        Vec *caps = fn->a.n->b.fx->caps;
        for (long i = 0; i < caps->n; i++) {
            Capture *c = vec_get(caps, i);
            fl_add(vec, c->sym->name, c->sym->type, c->inner);
        }
    }
    if (pos) fl_block(vec, fn->kind == N_TEST ? fn->b.n : fn->d.n, pos->start); /* the body's bindings */
    GC_UNROOT(1);
    return vec;
}

/* The local's value from the frame's slot in *out; 0 when it cannot be shown (a reference that is
 * still unset). */
static int fl_value(Frame *frame, Local *l, long *out)
{
    long v = frame->slots[l->slot].i;
    *out = v;
    if (v) return 1;
    if (!l->type) return 0; /* 0 is a value only for int, float and bool */
    long k = l->type->kind;
    return k == TY_INT || k == TY_FLOAT || k == TY_BOOL || k == TY_PARAM;
}

/* 1 when the runtime error being reported (rt_err_code) is R0009. */
static int err_is_timeout(void)
{
    char *code = (char *)rt_err_code;
    return code && code[0] == 'R' && code[3] == '0' && code[4] == '9';
}

/* The frame's visible parameters and locals, rendered with the frame's type arguments (none for
 * R0009, whose values depend on the machine's speed, Principle III):
 * `      name: type = value, ...` and a newline (nothing when there are none), or
 * `,"locals":[{"name","type","value"},...]` in JSON. */
void frame_values(void *sb_, void *frame_, void *pos, long json)
{
    if (err_is_timeout()) return;
    SB *sb = sb_;
    Frame *frame = frame_;
    GC_ROOT(&sb);
    Vec *locals = frame_locals(frame, pos);
    GC_ROOT(&locals);
    long saved = cur_frame; /* type parameters resolve through this frame */
    cur_frame = (long)frame;
    if (json) sb_cstr(sb, ",\"locals\":[");
    long shown = 0;
    for (long i = 0; i < locals->n; i++) {
        Local *l = vec_get(locals, i);
        long value;
        if (!fl_value(frame, l, &value)) continue;
        if (shown) {
            if (json) sb_byte(sb, ',');
            else sb_cstr(sb, ", ");
        } else if (!json) {
            sb_cstr(sb, "      ");
        }
        shown++;
        Type *ct = rt_concrete(l->type); /* the concrete type, for its name */
        if (json) {
            sb_cstr(sb, "{\"name\":");
            json_str_into(sb, l->name);
            sb_cstr(sb, ",\"type\":");
            json_str_into(sb, ty_name(ct));
            sb_cstr(sb, ",\"value\":");
            tr_value(sb, value, l->type);
            sb_byte(sb, '}');
        } else {
            sb_str(sb, l->name);
            sb_cstr(sb, ": ");
            sb_str(sb, ty_name(ct));
            sb_cstr(sb, " = ");
            render_capped(sb, value, l->type);
        }
    }
    if (json) sb_byte(sb, ']');
    else if (shown) sb_byte(sb, 10);
    cur_frame = saved;
    GC_UNROOT(2);
}

/* Inserts a C string at byte `at` of the builder. */
static void sb_insert_cstr(SB *sb, char *s, long at)
{
    long n = cstr_len(s);
    sb_reserve(sb, n); /* room at the end, then move the tail up */
    for (long i = sb->len; i > at; i--) sb->ptr[i - 1 + n] = sb->ptr[i - 1];
    for (long i = 0; i < n; i++) sb->ptr[at + i] = s[i];
    sb->len += n;
}

/* Spec 006 FR-017 in vo test: the frames of a runtime error (at most 10), with their values, as
 * text and JSON, kept until the test runner reports the error. */
void capture_trace(void *node)
{
    rt_err_trace_txt = 0;
    rt_err_trace_json = 0;
    /* spec 018: whether the innermost frame is a test's own body (the cause of the report; for
       R0012 this frame is the caller that broke the `requires`) */
    rt_err_in_test = cur_frame && ((Frame *)cur_frame)->fn && ((Frame *)cur_frame)->fn->kind == N_TEST;
    if (err_is_timeout()) return; /* a timeout keeps its report as it was (no frames) */
    rt_reporting = 1;
    SB *txt = sb_new();
    GC_ROOT(&txt);
    SB *js = sb_new();
    GC_ROOT(&js);
    sb_byte(js, '[');
    Node *at = node; /* location in the current frame */
    long n = 0;
    for (Frame *f = (Frame *)cur_frame; f && at; at = f->call, f = f->parent) {
        if (n >= 10) {
            sb_cstr(txt, "    ... more frames omitted\n");
            break;
        }
        if (n) sb_byte(js, ',');
        n++;
        sb_cstr(txt, "    at "); /* text: at f (loc), then the values indented two more */
        Str *name = frame_fn_name(f);
        GC_ROOT(&name);
        sb_str(txt, name);
        sb_cstr(txt, " (");
        human_loc(txt, (int)at->file, at->start);
        sb_cstr(txt, ")\n");
        long mark = txt->len;
        frame_values(txt, f, at, 0);
        if (txt->len != mark) sb_insert_cstr(txt, "  ", mark);
        sb_cstr(js, "{\"function\":"); /* JSON */
        json_str_into(js, name);
        GC_UNROOT(1);
        sb_cstr(js, ",\"file\":");
        json_str_into(js, file_get((int)at->file)->path);
        sb_cstr(js, ",\"start\":");
        json_pos(js, (int)at->file, at->start);
        frame_values(js, f, at, 1);
        sb_byte(js, '}');
    }
    sb_byte(js, ']');
    rt_err_trace_txt = (long)sb_finish(txt);
    rt_err_trace_json = (long)sb_finish(js);
    rt_reporting = 0;
    GC_UNROOT(2);
}

/* spec 011: the trace depth, saved and put back around a guarded handler call (src/serve.c). */
long tr_depth_get(void)
{
    return tr_depth;
}

void tr_depth_set(long depth)
{
    tr_depth = depth;
}
