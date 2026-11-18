/* Runtime support for the bytecode engine (src/vm.c): frames entered from native code, runtime
 * errors and their reports, timeouts, structural equality, closures, run_main and test bodies
 * (ported from src/interp.s, spec 008). Programs run as bytecode since spec 004.
 * Re-entry through builtin callbacks (map, filter, fold, any, all, sort_by) is bounded by the value
 * stack (rt_stack_full); results.md "Recursion limits (R7)" pins the depths. */
#include "vo.h"

long cur_frame;     /* Frame * */
long vsp;           /* value stack: next free byte */
long vbase;
long vstack_end;
long ret_val;
long deadline_ms;
long steps;
long rt_budget;     /* work units left before rt_poll checks the clock */
long recover_point; /* set by the test runner: runtime errors jump here */
long rt_err_code;   /* char * */
long rt_err_msg;    /* Str * */
long rt_err_node;   /* Node * */
long rt_err_extra;  /* extra JSON fields for the report (R0010), or 0 */
long cur_node;      /* the expression being evaluated (for out-of-memory reports) */

char r0009[] = "R0009"; /* the test runner tells timeouts from other errors by this address */

#define M_STACK "Stack exhausted: the recursion is too deep."

/* From here on, values live in the collected heap (spec 003). */
void gc_start(void)
{
    steps = 0; /* deadline checks at the same steps whatever ran before (spec 003 FR-009) */
    gc_init();
    gc_on = 1;
}

void interp_init(void)
{
    if (!vsp) { /* its own fixed region, outside the permanent arena (spec 003) */
        vsp = (long)map_fixed(ADDR_VSTACK, VSTACK_SIZE, 0);
        vbase = vsp;
        vstack_end = vbase + VSTACK_SIZE;
    }
    oom_hook = (long)rt_oom;
}

/* Live data passes the --max-memory limit (spec 003 US3): R0006 with in_use and limit. The report
 * itself allocates from the permanent arena (the collector is switched off for it). */
void rt_oom_live(long in_use)
{
    gc_on = 0;
    sample_peak();
    oom_hook = (long)oom_default; /* if even the arena runs out, the plain handler */
    SB *used = sb_new();
    size_into_up(used, in_use);
    SB *limit = sb_new();
    size_into(limit, gc_limit);
    Str *u = sb_finish(used);
    Str *l = sb_finish(limit);
    Str *m = msg("The program holds $1 of live data, over the $2 limit.", u, l, 0);
    SB *sb = sb_new();
    sb_cstr(sb, ",\"in_use\":");
    int_to_dec(sb, in_use);
    sb_cstr(sb, ",\"limit\":");
    int_to_dec(sb, gc_limit);
    rt_err_extra = (long)sb_finish(sb);
    rt_raise("R0006", m, (void *)cur_node);
}

void rt_oom(void)
{
    oom_hook = (long)oom_default; /* reporting allocates: the plain handler if that fails too */
    arena_cap += 0x4000000;       /* and 64 MiB of headroom for the report itself */
    rt_raise("R0006", str_from_cstr("Out of memory: the 512 MiB limit is reached."), (void *)cur_node);
}

/* The function name for a trace entry (tests show as test "name"). */
Str *frame_fn_name(Frame *frame)
{
    Node *fn = frame->fn;
    if (fn->kind != N_TEST) return fn->a.n->a.s;
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, (fn->flags & NF_PROP) ? "prop " : "test ");
    sb_cstr(sb, "\"");
    sb_str(sb, fn->a.s);
    sb_cstr(sb, "\"");
    GC_UNROOT(1);
    return sb_finish(sb);
}

/* The human report: location, message, snippet and up to ten frames with their values. */
static void report_human(SB *sb, char *code, Str *message, Node *node)
{
    human_loc(sb, (int)node->file, node->start);
    sb_cstr(sb, ": runtime error[");
    sb_cstr(sb, code);
    sb_byte(sb, ']');
    sb_byte(sb, ':');
    sb_byte(sb, ' ');
    sb_str(sb, message);
    sb_byte(sb, '\n');
    human_snippet(sb, (int)node->file, node->start, node->end);
    sb_cstr(sb, "trace:\n");
    Node *at = node; /* location in the current frame */
    long written = 0;
    for (Frame *f = (Frame *)cur_frame; f;) {
        if (written >= 10) {
            sb_cstr(sb, "  ... more frames omitted\n");
            break;
        }
        written++;
        sb_cstr(sb, "  at ");
        sb_str(sb, frame_fn_name(f));
        sb_byte(sb, ' ');
        sb_byte(sb, '(');
        human_loc(sb, (int)at->file, at->start);
        sb_byte(sb, ')');
        sb_byte(sb, '\n');
        frame_values(sb, f, at, 0); /* spec 006: the frame's parameters and locals */
        at = f->call;
        f = f->parent;
        if (!at) break;
    }
}

/* The JSON report. */
static void report_json(SB *sb, char *code, Str *message, Node *node)
{
    sb_cstr(sb, "{\"v\":1,\"kind\":\"runtime_error\",\"code\":\"");
    sb_cstr(sb, code);
    sb_cstr(sb, "\",\"name\":\"");
    sb_str(sb, catalog_name(code));
    sb_cstr(sb, "\",\"message\":");
    json_str_into(sb, message);
    sb_cstr(sb, ",\"file\":");
    json_span(sb, (int)node->file, node->start, node->end);
    if (rt_err_extra) sb_str(sb, (Str *)rt_err_extra);
    sb_cstr(sb, ",\"trace\":[");
    Node *at = node;
    long written = 0;
    Frame *f = (Frame *)cur_frame;
    while (f) {
        if (written >= 10) { /* count what was left out */
            long omitted = 0;
            for (; f; f = f->parent) omitted++;
            sb_byte(sb, ']');
            sb_cstr(sb, ",\"trace_omitted\":");
            int_to_dec(sb, omitted);
            sb_byte(sb, '}');
            sb_byte(sb, '\n');
            return;
        }
        if (written) sb_byte(sb, ',');
        written++;
        sb_cstr(sb, "{\"function\":");
        json_str_into(sb, frame_fn_name(f));
        sb_cstr(sb, ",\"file\":");
        json_str_into(sb, file_get((int)at->file)->path);
        sb_cstr(sb, ",\"start\":");
        json_pos(sb, (int)at->file, at->start);
        frame_values(sb, f, at, 1); /* spec 006: "locals" */
        sb_byte(sb, '}');
        at = f->call;
        f = f->parent;
        if (!at) break;
    }
    sb_byte(sb, ']');
    sb_byte(sb, '}');
    sb_byte(sb, '\n');
}

/* Stops the program with a runtime error report (or returns to the test runner). */
void rt_raise(char *code, Str *message, Node *node)
{
    render_depth = 0; /* spec 013: a raise unwinds any rendering in progress */
    coll_reset_active(); /* spec 014: and any collection operation */
    rt_err_code = (long)code;
    rt_err_msg = (long)message;
    rt_err_node = (long)node;
    if (tr_on) tr_error(code, message, node); /* spec 006: the trace's error record */
    if (recover_point && serve_guard) { /* spec 011: a handler failed: report it, the server answers 500 */
        rt_reporting = 1;
        SB *sb = sb_new();
        GC_ROOT(&sb);
        if (opt_json)
            report_json(sb, code, message, node);
        else
            report_human(sb, code, message, node);
        out_bytes(2, sb->ptr, sb->len);
        GC_UNROOT(1);
        rt_reporting = 0;
        rt_recover(2);
    }
    if (recover_point) { /* inside a test: let the runner record it */
        rt_err_extra = 0;
        capture_trace(node); /* spec 006: the frames and their values, for the test's report */
        rt_recover(2);
    }
    rt_reporting = 1; /* spec 006: no timeouts while the report renders values */
    SB *sb = sb_new();
    GC_ROOT(&sb); /* (vo_exit does not return: no unroot) */
    if (opt_json)
        report_json(sb, code, message, node);
    else
        report_human(sb, code, message, node);
    out_bytes(2, sb->ptr, sb->len);
    int status = (code[4] == '9' && code[3] == '0') ? EXIT_TIMEOUT : EXIT_RUNTIME; /* R0009 -> 124 */
    gc_report(2, "run", 0); /* --mem-stats: the run ends here */
    vo_exit(status);
}

/* Back to the recovery point (status 1: an expect failed, 2: a runtime error). In a task whose recovery
 * point is the parent's, the parent goes back to it instead (spec 016, research R6). */
void rt_recover(long status)
{
    if (task_depth && recover_point == task_recover_base) task_jump(status);
    jmp_restore((void *)recover_point, status);
}

void rt_raise_c(char *code, char *message, Node *node)
{
    rt_raise(code, str_from_cstr(message), node);
}

/* R0002 at node (the bytecode engine's division handlers). */
void rt_div_zero(Node *node)
{
    rt_raise_c("R0002", "Division by zero.", node);
}

/* R0005 at node: a new frame does not fit the value stack. */
void rt_stack_full(Node *node)
{
    rt_raise_c("R0005", M_STACK, node);
}

void rt_overflow(Node *node)
{
    rt_raise_c("R0001", "Integer overflow.", node);
}

void rt_index_error(Node *node, long index, long length)
{
    Str *i = int_to_str(index);
    GC_ROOT(&i); /* (rt_raise does not return: no unroot) */
    Str *n = int_to_str(length);
    rt_raise("R0003", msg("Index $1 is out of bounds for length $2.", i, n, 0), node);
}

void rt_key_error(Node *node, long key, Type *key_type)
{
    SB *sb = sb_new();
    GC_ROOT(&sb); /* (rt_raise does not return: no unroot) */
    render_value(sb, key, key_type);
    rt_raise("R0004", msg("Key $1 is not in the map.", sb_finish(sb), 0, 0), node);
}

/* Counts a step; every 65,536 steps compares the clock with the deadline (R0009). */
void tick(Node *node)
{
    if (++steps & 0xFFFF) return;
    if (!deadline_ms) return;
    if (sys_clock_ms() >= deadline_ms) rt_timeout(node);
}

/* The POLL budget ran out: refills it and stops with R0009 when the deadline has passed, but never
 * while the collector is in the middle of a pass (spec 013: a jump out of it left the heap half
 * swept, and a server went on using it). */
void rt_poll(void)
{
    rt_budget = BUDGET_UNITS;
    if (rt_reporting || !deadline_ms) return; /* spec 006: a runtime error report is being built */
    if (sys_clock_ms() < deadline_ms) return;
    if (!cur_node) return; /* no location yet: the next tick reports it */
    if (gc_busy) return;    /* spec 013 (H10): never inside a collection; the next poll after it */
    rt_timeout((void *)cur_node);
}

void rt_timeout(Node *node)
{
    deadline_ms = 0; /* one report: the polls inside it must not raise again */
    rt_raise(r0009, msg("Time limit of $1 s exceeded.", int_to_str(opt_timeout), 0, 0), node);
}

/* R0005 when the native stack is nearly used up. */
void check_stack(Node *node)
{
    if ((unsigned long)__builtin_frame_address(0) < (unsigned long)stack_floor) rt_raise_c("R0005", M_STACK, node);
}

long *local_addr(long slot)
{
    return &((Frame *)cur_frame)->slots[slot].i;
}

/* Calls a user function from native code (run_main, builtin callbacks): a frame for its compiled
 * code, the arguments from vals (args is always 0 since spec 004), a step, then the bytecode
 * engine. A closure adds its captured values and type arguments (vm_push). */
long call_fn(Node *fn, Node *call, void *args, long *vals, void *closure)
{
    (void)args;
    if (call) check_stack(call);
    CompiledFn *code = fn->a.n->b.fx->code;
    if (!code) vm_not_compiled(); /* (every function is compiled before the program runs) */
    long saved = vsp;             /* the value stack goes back here afterwards */
    void *frame = vm_push(code, fn, call, vals, closure);
    if (call) tick(call); /* count the call as a step */
    long r = vm_run(frame);
    vsp = saved;
    return r;
}

typedef union {
    long i;
    double d;
} Bits;

/* Structural equality (floats by IEEE rules). */
int values_equal(long a, long b, Type *type)
{
    POLL(1);
    check_stack((Node *)cur_node); /* spec 013 (H11): deep or cyclic values are R0005, not a crash */
    type = rt_concrete(type); /* type parameters: the running frame's type arguments */
    if (type->kind != TY_FLOAT && a == b) return 1; /* the same object (NaN is never equal to itself) */
    switch (type->kind) {
    case TY_FLOAT: {
        Bits x = {.i = a}, y = {.i = b};
        return x.d == y.d;
    }
    case TY_STR:
    case TY_BYTES: return str_eq((Str *)a, (Str *)b); /* spec 021: the same object */
    case TY_OMAP:
    case TY_OSET:
    case TY_PQUEUE:
    case TY_DEQUE: return coll_equal((Coll *)a, (Coll *)b, type); /* spec 014: contents in order */
    case TY_LIST: {
        List *x = (List *)a, *y = (List *)b;
        if (x->len != y->len) return 0;
        for (long i = 0; i < x->len; i++)
            if (!values_equal(x->data[i].i, y->data[i].i, type->a.t)) return 0;
        return 1;
    }
    case TY_MAP: {
        Map *x = (Map *)a, *y = (Map *)b;
        if (x->count != y->count) return 0;
        for (long i = 0; i < x->used; i++) {
            MapEntry *e = &x->entries[i];
            if (!e->live) continue;
            long j = map_find(y, e->key.i, type->a.t);
            if (j < 0 || !values_equal(e->val.i, y->entries[j].val.i, type->b.t)) return 0;
        }
        return 1;
    }
    case TY_RECORD: {
        Vec *fields = type->decl.n->b.v;
        for (long i = 0;; i++) {
            Node *m = member_at(fields, i);
            if (!m) return 1;
            Type *ft = field_type_in(type, m->b.n->type);
            if (!values_equal(((Record *)a)->fields[i].i, ((Record *)b)->fields[i].i, ft)) return 0;
        }
    }
    case TY_VARIANT: {
        long tag = (long)(uint32_t)((Variant *)a)->tag;
        if (tag != (long)(uint32_t)((Variant *)b)->tag) return 0;
        Node *pt = member_at(type->decl.n->b.v, tag)->b.n; /* payload type node */
        if (!pt) return 1;
        Type *t = field_type_in(type, pt->type);
        return values_equal(((Variant *)a)->pay.i, ((Variant *)b)->pay.i, t);
    }
    case TY_RESULT: {
        long tag = (long)(uint32_t)((Variant *)a)->tag;
        if (tag != (long)(uint32_t)((Variant *)b)->tag) return 0;
        Type *t = case_payload(type, tag);
        if (!t) return 1;
        return values_equal(((Variant *)a)->pay.i, ((Variant *)b)->pay.i, t);
    }
    default: return a == b; /* int, bool, void */
    }
}

/* Runs `main`; returns the process exit status (main returning err prints "error: msg", status 1). */
int run_main(Node *main_fn)
{
    interp_init();
    gc_start();
    if (opt_timeout) {
        long ms = opt_timeout * 1000;
        deadline_ms = sys_clock_ms() + ms;
    }
    long r = call_fn(main_fn, 0, 0, 0, 0);
    gc_roots_check(); /* spec 008: stress builds check the C root stack is balanced */
    if (main_fn->type->kind != TY_RESULT || !(uint32_t)((Variant *)r)->tag) return 0;
    out_cstr(2, "error: ");
    out_str(2, ((Variant *)r)->pay.s);
    out_cstr(2, "\n");
    return 1;
}

/* Back to an empty call stack (after a test ends, normally or through its recovery point). */
void interp_reset(void)
{
    cur_frame = 0;
    vsp = vbase;
}

/* Calls a function value with argument values. */
long call_value(Closure *closure, Node *call, long *vals)
{
    CompiledFn *code = closure->fn->a.n->b.fx->code;
    if (!code) return call_fn(closure->fn, call, 0, vals, closure);
    if (call) check_stack(call); /* (as call_fn: the native stack, then the frame, then a step) */
    long saved = vsp;
    void *frame = vm_push(code, closure->fn, call, vals, closure);
    if (call) tick(call);
    long r = vm_run(frame);
    vsp = saved;
    return r;
}

/* An anonymous function as a value: its node, and copies of the values it captures. */
Closure *make_closure(Node *lambda)
{
    Vec *caps = lambda->a.n->b.fx->caps; /* capture entries */
    long n = caps ? caps->n : 0;
    Closure *c = alloc_obj(OBJ_CLOSURE, n * 8 + CL_CAPS - 8);
    c->fn = lambda;
    c->n = n;
    c->targs = ((Frame *)cur_frame)->targs;
    for (long i = 0; i < n; i++) c->caps[i].i = *local_addr(((Capture *)vec_get(caps, i))->outer);
    return c;
}

/* A named function as a value (made once per function). */
Closure *fn_closure(Node *fn)
{
    Fx *fx = fn->a.n->b.fx;
    if (!fx->closure.p) {
        perm_begin(); /* cached in the function's node: must outlive every test */
        Closure *c = alloc_obj(OBJ_CLOSURE, CL_CAPS - 8);
        c->fn = fn;
        fx->closure.p = c;
        perm_end();
    }
    return fx->closure.p;
}

/* Runs a test block's compiled code in a fresh frame (the test's declared effects are its grant). */
void run_test_body(Node *test)
{
    if (!test->d.fx || !test->d.fx->code) vm_not_compiled(); /* (every test is compiled first) */
    tr_test(test); /* spec 006: the trace's test record */
    vm_run(vm_push(test->d.fx->code, test, 0, 0, 0));
    gc_roots_check(); /* spec 008: stress builds check the C root stack is balanced */
    interp_reset();
}
