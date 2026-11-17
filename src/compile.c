/* The bytecode compiler (spec 004, research R1, R6, R15; ported from src/compile.s, spec 008): after
 * checking, every function, lambda and test body becomes a compiled function (data-model.md) that
 * the VM runs. One recursive pass per body; instructions follow the tree walker's evaluation order
 * exactly (contracts/equivalence.md).
 *
 * Slots are byte offsets (slot * 8) from FR_SLOTS. Locals keep the checker's slot numbers;
 * temporaries are allocated above them in stack order. cexpr(node, dst) returns the slot holding
 * the value: an identifier's own slot, or dst / a fresh temporary written by the expression's
 * last instruction. Locals cannot change while an expression is evaluated (no references to
 * locals, closures capture copies), so reading them in place is safe.
 * ponytail: recursive over the tree (the parser bounds nesting at 5,000 levels); temporaries are
 * never cleared, so a dead one can keep garbage alive until it is overwritten. Upgrade: an
 * explicit work stack; clearing temporaries at statement ends or precise frame maps. */
#include "vo.h"

void *c_ops = vm_ops; /* the handler table instructions are filled from (spec 006: vm_ops_trace) */

static char *c_buf;       /* instruction buffer of the function being compiled */
static long c_len;        /* bytes used */
static long c_cap;
static long c_temp;       /* next free slot (bytes) */
static long c_max;        /* frame size so far (bytes) */
static Vec *c_fix;        /* instruction offsets whose I_C is a code offset */
static long c_fail;       /* 1: a construct this compiler does not handle (T0003 if it runs) */
static Vec *c_brk;        /* `break` jumps of the innermost loop */
static Vec *c_cont;       /* `continue` jumps of the innermost loop */
static long c_done;       /* compile_program ran */
static char *c_code_ptr;  /* next free byte of the code region (0: not mapped yet) */
static Vec *c_lams;       /* lambdas reached, compiled after their enclosing body */
static Vec *c_ens;        /* spec 005: jumps to the `ensures` epilogue, or 0 (none) */
static Vec *c_reqj;       /* `requires` jumps taken when a condition is false */
static long c_retslot;    /* the `ret` slot (bytes), or -1 (no return type) */
static long c_exitslot;   /* the slot holding the exit node for R0013 (bytes) */

#define INSTR(off) ((Instr *)(c_buf + (off)))

static void c_prologue(Node *decl);

/* ---- spec 018: coverage points (research R8) ----
 * ponytail: at most COV_MAX points per run (T0003 beyond); allocate the table on demand if programs
 * grow past it. */
Node *cov_node[COV_MAX];
char cov_kind[COV_MAX];
Node *cov_owner[COV_MAX];
long cov_n;
long cov_on;
char *cov_hit;
char *cov_reach;
long cov_nfn;
long cov_test;
Node **cov_fns;
static Node *c_owner;  /* the function being compiled when its points count, else 0 */
static long c_owner_fn; /* its index among the functions with points */
static long c_in_contract; /* compiling `requires`/`ensures`: no mutants there (research R10) */

/* spec 018: a mutation candidate for --mutate (only in the program's own functions, not contracts). */
static void c_mut(Node *n)
{
    if (opt_mutate && c_owner && !c_in_contract) mut_note(n, c_owner, c_owner_fn);
}

/* spec 018: mutant children compile the whole program again (research R11), without points. */
void compile_reset(void)
{
    c_done = 0;
    c_owner = 0;
    cov_on = 0;
    cov_n = 0;
    cov_nfn = 0;
}

/* Emits a coverage point for node (nothing unless points are on for this function). */
static void c_cover(Node *node, int kind)
{
    if (!c_owner) return;
    if (cov_n == COV_MAX) {
        out_cstr(2, "error[T0003]: the program has more than 65536 coverage points\n");
        vo_exit(EXIT_INTERNAL);
    }
    cov_node[cov_n] = node;
    cov_kind[cov_n] = (char)kind;
    cov_owner[cov_n] = c_owner;
    c_emit(OPC_COVER, cov_n++, kind == COV_FUNCTION || kind == COV_LAMBDA ? c_owner_fn : -1, 0, node);
}
static void c_fn_end(Node *decl);

/* Compiles every function and test of every loaded file (once). */
void compile_program(void)
{
    if (c_done) return;
    c_done = 1;
    vm_init(); /* the handlers (vm_ops, vm_ops_trace) instructions are filled from */
    std_runtime_grants(); /* spec 007: library functions run with their caller's grant */
    c_buf = c_code_alloc(65536);
    c_cap = 65536;
    c_fix = vec_new();
    c_lams = vec_new();
    for (long f = 0; f < file_count; f++) {
        Node *ast = file_get((int)f)->ast;
        if (!ast) continue; /* not a source file (vo.packages, vo.effects) */
        Vec *items = ast->a.v;
        for (long i = 0; i < items->n; i++) {
            Node *it = vec_get(items, i);
            if (it->kind != N_FN && it->kind != N_TEST) continue;
            c_owner = 0; /* spec 018: the program's own functions (and their lambdas) get points */
            if (cov_on && it->kind == N_FN && !file_is_library((int)f)) {
                if (!cov_fns) cov_fns = perm_alloc(COV_MAX * 8);
                c_owner = it;
                c_owner_fn = cov_nfn;
                cov_fns[cov_nfn++] = it;
            }
            compile_fn(it);
            while (c_lams->n) compile_fn(c_lams->data[--c_lams->n].n); /* then the lambdas it reached */
            c_owner = 0;
        }
    }
}

/* Memory in the code region: compiled functions, their scratch buffer and generic call-site
 * records. Instructions pack two 32-bit slot offsets into one word, which can look like a heap
 * address; the collector never scans this region, so they pin nothing (nothing here points into
 * the heap: constants and nodes are permanent).
 * ponytail: one 256 MiB reservation, never freed; upgrade: map more on demand if a program needs it. */
void *c_code_alloc(long size)
{
    size = (size + 7) & -8L;
    if (!c_code_ptr) {
        map_fixed(ADDR_CODE, CODE_SIZE, 0x4000); /* MAP_NORESERVE */
        c_code_ptr = (char *)ADDR_CODE;
    }
    char *p = c_code_ptr;
    if ((unsigned long)(p + size) > (unsigned long)(ADDR_CODE + CODE_SIZE)) {
        vm_not_compiled(); /* (T0003: the program's code does not fit in 256 MiB) */
        return 0;
    }
    c_code_ptr = p + size;
    return p;
}

/* One N_FN, N_LAMBDA or N_TEST into a compiled function stored in FX_CODE of its FX record;
 * FX_CODE stays 0 when the body uses something not compiled yet (the tree walker runs it). */
void compile_fn(Node *decl)
{
    Node *body;
    long nparams;
    if (decl->kind == N_TEST) { /* test: body N_B, no parameters (a property: FX_PARAMS) */
        body = decl->b.n;
        nparams = (decl->flags & NF_PROP) ? decl->d.fx->params->n : 0;
    } else { /* fn / lambda: body N_D, parameters N_B */
        body = decl->d.n;
        nparams = decl->b.v->n;
    }
    c_len = 0;
    c_temp = decl->e.i * 8; /* locals */
    c_max = c_temp;
    c_fix->n = 0;
    c_fail = 0;
    c_brk = 0;
    c_cont = 0;
    if (tr_on) c_emit(OPC_TRACE_ENTER, 0, 0, 0, decl); /* spec 006: with --trace, the call record first */
    c_prologue(decl);                                  /* contracts (spec 005): `requires` checks first */
    if (decl->kind != N_TEST) c_cover(decl, decl->kind == N_FN ? COV_FUNCTION : COV_LAMBDA); /* spec 018 */
    cblock(body);
    c_fn_end(decl); /* falling off the end returns 0 (or goes to the `ensures` checks) */
    if (c_fail) return;
    CompiledFn *cf = c_code_alloc(c_len + CF_HDR); /* copy out: header + code */
    Instr *code = (Instr *)((char *)cf + CF_HDR);
    mem_copy8((long *)code, (long *)c_buf, c_len);
    cf->code = code;
    cf->n = c_len >> 5;
    cf->slots = c_max >> 3;
    cf->nparams = nparams;
    cf->decl = decl;
    for (long i = 0; i < c_fix->n; i++) { /* code offsets in I_C become addresses */
        Instr *ins = (Instr *)((char *)code + c_fix->data[i].i);
        ins->c.i += (long)code;
    }
    item_fx(decl)->code = cf;
}

/* ---- contracts (spec 005 research R12, R13) ---- */

/* The FX record of a named function, or 0 (lambdas and tests have no contracts). */
static Fx *c_fx_of(Node *decl)
{
    if (decl->kind != N_FN || !decl->a.n) return 0;
    return decl->a.n->b.fx;
}

/* One jump per `requires`, taken when it is false (to a stub after the code); the epilogue state
 * when the function has `ensures`. A function without contracts gets nothing. */
static void c_prologue(Node *decl)
{
    c_ens = 0;
    c_reqj = 0;
    Fx *fx = c_fx_of(decl);
    if (!fx || (!fx->requires_ && !fx->ensures)) return;
    c_exitslot = c_new_temp();
    c_reqj = vec_new();
    c_in_contract = 1;
    for (long i = 0; fx->requires_ && i < fx->requires_->n; i++) {
        long j = ccond(vec_get(fx->requires_, i));
        vec_push(c_reqj, (void *)j);
    }
    c_in_contract = 0;
    if (!fx->ensures) return;
    c_ens = vec_new();
    c_retslot = decl->type->kind == TY_VOID ? -1 : fx->retslot * 8;
}

/* An exit of a function with `ensures`: ret = value, the exit node for R0013, then the epilogue. */
static void c_ens_exit(long value_slot, Node *exit_node)
{
    if (c_retslot >= 0 && value_slot >= 0) c_mov(c_retslot, value_slot, exit_node);
    c_emit(OPC_LOADK, c_exitslot, 0, (long)exit_node, exit_node);
    long j = c_jump(OPC_JMP, 0, 0, exit_node);
    vec_push(c_ens, (void *)j);
}

/* The node of a compiled contract failure: a builtin call (id B_REQ_FAIL / B_ENS_FAIL) with the
 * condition's span, the condition in N_A and the function in N_C. */
static Node *c_fail_node(long id, Node *cond, Node *decl)
{
    Node *n = alloc(NODE_SIZE);
    n->kind = N_CALL;
    n->file = cond->file;
    n->start = cond->start;
    n->end = cond->end;
    n->a.n = cond;
    n->c.n = decl;
    n->d.i = id;
    return n;
}

/* For each condition: its false-jump lands on a CALL_BUILTIN id (argument: the exit-node slot). */
static void c_stubs(Vec *conds, Vec *jumps, long id, Node *decl)
{
    for (long i = 0; i < jumps->n; i++) {
        long j = (long)vec_get(jumps, i);
        c_target(j, c_here());
        Node *n = c_fail_node(id, vec_get(conds, i), decl);
        c_emit(OPC_CALL_BUILTIN, c_exitslot, c_exitslot, 1, n);
    }
}

/* The end of a function: RET 0 (as before), or with contracts: a void function's fall-through
 * goes to the epilogue, which checks each `ensures` and returns `ret`; the failure stubs follow. */
static void c_fn_end(Node *decl)
{
    Fx *fx = c_fx_of(decl);
    if (c_ens && c_retslot < 0) { /* a void function's closing `}` is an exit */
        Node *body = decl->d.n;
        Node *n = alloc(NODE_SIZE);
        n->kind = body->kind;
        n->file = body->file;
        n->end = body->end;
        n->start = body->end - 1;
        c_ens_exit(-1, n);
    }
    c_emit(OPC_RET, 0, -1, 0, decl);
    if (c_ens) {
        c_patch_all(c_ens, c_here()); /* the epilogue */
        Vec *jumps = vec_new();
        c_in_contract = 1;
        for (long i = 0; i < fx->ensures->n; i++) {
            long j = ccond(vec_get(fx->ensures, i));
            vec_push(jumps, (void *)j);
        }
        c_in_contract = 0;
        c_emit(OPC_RET, 0, c_retslot, 0, decl);
        c_stubs(fx->ensures, jumps, B_ENS_FAIL, decl);
    }
    if (c_reqj && fx->requires_) c_stubs(fx->requires_, c_reqj, B_REQ_FAIL, decl);
}

/* Appends one instruction; returns its offset in the buffer. */
long c_emit(long opc, long a, long b, long c, Node *node)
{
    if (c_len + I_SIZE > c_cap) { /* grow: double into a new buffer */
        long cap = c_cap * 2;
        char *grown = c_code_alloc(cap);
        mem_copy8((long *)grown, (long *)c_buf, c_len);
        c_buf = grown;
        c_cap = cap;
    }
    long at = c_len;
    Instr *ins = INSTR(at);
    ins->handler = ((void **)c_ops)[opc]; /* vm_ops, or vm_ops_trace with --trace (spec 006) */
    ins->a = (int32_t)a;
    ins->b = (int32_t)b;
    ins->c.i = c;
    ins->node = node;
    c_len = at + I_SIZE;
    return at;
}

/* Emits a jump (JMP, JT, JF, FOR_RANGE, FOR_LIST: a = 0) whose I_C is a code offset; returns it. */
long c_jump(long opc, long b, long target, Node *node)
{
    long at = c_emit(opc, 0, b, target, node);
    vec_push(c_fix, (void *)at);
    return at;
}

/* Sets the jump target (a code offset) of the instruction at offset at. */
void c_target(long at, long target)
{
    INSTR(at)->c.i = target;
}

long c_here(void)
{
    return c_len;
}

/* Points every jump in vec at target. */
void c_patch_all(Vec *v, long target)
{
    if (!v) return;
    for (long i = 0; i < v->n; i++) INSTR(v->data[i].i)->c.i = target;
}

/* A fresh temporary slot (bytes); the frame grows to fit it. */
long c_new_temp(void)
{
    long slot = c_temp;
    c_temp += 8;
    if ((unsigned long)c_temp > (unsigned long)c_max) c_max = c_temp;
    return slot;
}

/* The slot an expression writes its value to: dst, or a fresh temporary when dst < 0. */
long c_dst(long dst)
{
    return dst < 0 ? c_new_temp() : dst;
}

void c_unsupported(void)
{
    c_fail = 1;
}

/* dst = src unless they are the same slot. */
void c_mov(long dst, long src, Node *node)
{
    if (dst != src) c_emit(OPC_MOV, dst, src, 0, node);
}

/* ---- statements ---- */

void cblock(Node *block)
{
    Vec *stmts = block->a.v;
    for (long i = 0; i < stmts->n; i++) cstmt(vec_get(stmts, i));
}

/* match: the scrutinee once, then the arms in source order. */
static void cmatch(Node *st)
{
    long scrut = cexpr(st->a.n, -1);
    Vec *ends = vec_new(); /* jumps to the end */
    Vec *arms = st->b.v;
    for (long i = 0; i < arms->n; i++) {
        Node *arm = vec_get(arms, i);
        if (arm->kind != N_ARM) continue;
        long next = c_emit(OPC_JTAG_NE, arm->d.i, scrut, 0, arm); /* not this case: the next arm */
        vec_push(c_fix, (void *)next);
        if (arm->e.i >= 0) c_emit(OPC_PAYLOAD, arm->e.i * 8, scrut, 0, arm); /* the binding */
        c_cover(arm, COV_CASE); /* spec 018 */
        cblock(arm->c.n);
        long j = c_jump(OPC_JMP, 0, 0, arm);
        vec_push(ends, (void *)j);
        c_target(next, c_here());
    }
    c_patch_all(ends, c_here());
}

static void cset(Node *st)
{
    Node *place = st->a.n;
    if (place->kind == N_IDENT) {
        long slot = place->c.i * 8;
        c_mov(slot, cexpr(st->b.n, slot), st);
    } else if (place->kind == N_FIELD) { /* field: object, value, store */
        long obj = cexpr(place->a.n, -1);
        long val = cexpr(st->b.n, -1);
        c_emit(OPC_SET_FIELD, obj, val, place->c.i * 8 + R_FIELDS, st);
    } else { /* index: object, key/index, value, then the check */
        long obj = cexpr(place->a.n, -1);
        long key = cexpr(place->b.n, -1);
        long val = cexpr(st->b.n, -1);
        if (TY_IS_COLL(place->a.n->type->kind))
            c_emit(OPC_SET_INDEX_COLL, obj, key, val, place); /* spec 014 */
        else if (place->flags & NF_MAP)
            c_emit(OPC_SET_INDEX_MAP, obj, key, val, st); /* cur_node: the statement */
        else
            c_emit(OPC_SET_INDEX_LIST, obj, key, val, place); /* R0003 at the place */
    }
}

/* for: hidden counter/list and end/index in two temporaries. */
static void cfor(Node *st)
{
    Vec *outer_brk = c_brk, *outer_cont = c_cont; /* restored by c_loop_leave */
    c_loop_enter();
    long it = c_new_temp(); /* counter | list */
    c_new_temp();           /* end | index (it + 8) */
    long op;
    if (st->c.n) { /* range: start, then end, each once */
        c_mov(it, cexpr(st->b.n, it), st);
        c_mov(it + 8, cexpr(st->c.n, it + 8), st);
        op = OPC_FOR_RANGE;
    } else { /* list: evaluated once; the index starts at 0 */
        c_mov(it, cexpr(st->b.n, it), st);
        if (TY_IS_COLL(st->b.n->type->kind)) c_emit(OPC_SNAPSHOT, it, it, 0, st); /* spec 014 */
        c_emit(OPC_LOADK, it + 8, 0, 0, st);
        op = OPC_FOR_LIST;
    }
    long head = c_emit(op, st->e.i * 8, it, 0, st); /* the first test (its exit target comes later) */
    vec_push(c_fix, (void *)head);
    long body = c_here();
    c_cover(st->d.n, COV_LOOP); /* spec 018 */
    cblock(st->d.n);
    long cont = c_here(); /* continue: the back-edge (step, test, next value, jump) */
    long step = c_emit(st->c.n ? OPC_FOR_STEP : OPC_FOR_LIST_STEP, st->e.i * 8, it, body, st);
    vec_push(c_fix, (void *)step);
    long exit = c_here();
    c_target(head, exit);
    c_loop_leave(cont, exit, outer_brk, outer_cont);
}

/* Temporaries are free again after every statement. */
void cstmt(Node *st)
{
    long mark = c_temp; /* temporaries mark */
    switch (st->kind) {
    case N_MATCH: cmatch(st); break;
    case N_EXPR_STMT: cexpr(st->a.n, -1); break;
    case N_LET: /* let / var: into the local's slot (E < 0: `_`) */
        if (st->e.i < 0) {
            cexpr(st->c.n, -1);
        } else {
            long slot = st->e.i * 8;
            c_mov(slot, cexpr(st->c.n, slot), st);
        }
        break;
    case N_SET: cset(st); break;
    case N_IF: { /* if c { } else ... */
        long to_else = ccond(st->a.n);
        c_temp = mark;
        c_cover(st->b.n, COV_THEN); /* spec 018 */
        cblock(st->b.n);
        Node *e = st->c.n;
        if (!e && c_owner) { /* spec 018: a landing for "the condition was false" */
            long skip = c_jump(OPC_JMP, 0, 0, st);
            c_target(to_else, c_here());
            c_cover(st, COV_IF_FALSE);
            c_target(skip, c_here());
            break;
        }
        if (!e) {
            c_target(to_else, c_here());
            break;
        }
        long over = c_jump(OPC_JMP, 0, 0, st); /* over the else part */
        c_target(to_else, c_here());
        if (e->kind == N_IF) {
            cstmt(e);
        } else {
            c_cover(e, COV_ELSE);
            cblock(e);
        }
        c_target(over, c_here());
        break;
    }
    case N_WHILE: { /* while: a step, the condition, the body */
        Vec *outer_brk = c_brk, *outer_cont = c_cont;
        c_loop_enter();
        long top = c_here();
        c_emit(OPC_TICK, 0, 0, 0, st);
        long exit_jump = ccond(st->a.n);
        c_temp = mark;
        c_cover(st->b.n, COV_LOOP); /* spec 018 */
        cblock(st->b.n);
        c_jump(OPC_JMP, 0, top, st);
        long exit = c_here();
        c_target(exit_jump, exit);
        c_loop_leave(top, exit, outer_brk, outer_cont); /* continue: back to the step */
        break;
    }
    case N_FOR: cfor(st); break;
    case N_RETURN: { /* return [e] */
        long v = st->a.n ? cexpr(st->a.n, -1) : -1;
        if (c_ens) /* with `ensures`: to the epilogue */
            c_ens_exit(v, st);
        else
            c_emit(OPC_RET, 0, v, 0, st);
        break;
    }
    case N_BREAK:
    case N_CONTINUE: {
        Vec *list = st->kind == N_BREAK ? c_brk : c_cont;
        long j = c_jump(OPC_JMP, 0, 0, st);
        vec_push(list, (void *)j);
        break;
    }
    default: break; /* anything else does nothing (as before spec 004) */
    }
    c_temp = mark;
}

/* New break/continue lists for a loop (the caller keeps the outer ones). */
void c_loop_enter(void)
{
    c_brk = vec_new();
    c_cont = vec_new();
}

/* Points the loop's continue jumps at cont and break jumps at exit; restores the outer lists. */
void c_loop_leave(long cont, long exit, Vec *outer_brk, Vec *outer_cont)
{
    c_patch_all(c_cont, cont);
    c_patch_all(c_brk, exit);
    c_brk = outer_brk;
    c_cont = outer_cont;
}

/* ---- expressions ---- */

/* 1 when v survives a round trip through 32 bits (a _K instruction's constant). */
int c_fits32(long v)
{
    return (long)(int32_t)v == v;
}

/* The _K opcode for `x op c` (op + - * / % on int, c an int literal that fits in 32 bits; / not
 * by 0 or -1, % not by 0, so the checks of the slot form stay where they can fire), else -1. */
long c_const_op(Node *binary)
{
    long op = binary->d.i;
    Node *rhs = binary->b.n;
    if (rhs->kind != N_INT || op > OP_REM_INT || op < OP_ADD_INT) return -1;
    long v = rhs->a.i;
    if (!v && op >= OP_DIV_INT) return -1; /* 0: only + - * may use it */
    if (v == -1 && op == OP_DIV_INT) return -1; /* -1: not / */
    if (!c_fits32(v)) return -1;
    return op + (OPC_ADD_INT_K - OP_ADD_INT);
}

/* 1 when `==` / `!=` compares int or bool operands (EQ_INT: no call needed). */
int c_int_eq(Node *binary)
{
    Type *t = binary->a.n->type;
    return t && (t->kind == TY_INT || t->kind == TY_BOOL);
}

/* For < <= > >= on int: the jump taken when the comparison is false. */
static const unsigned char c_inverse[] = {OPC_JGE, OPC_JGT, OPC_JLE, OPC_JLT};

/* Compiles a condition into a jump taken when it is false; returns the jump's offset (its
 * target comes later). Int comparisons fuse into one compare-and-branch (US1). */
long ccond(Node *cond)
{
    if (cond->kind == N_BINARY) {
        long op = cond->d.i, jump = -1;
        if (op >= OP_LT_INT && op <= OP_GE_INT)
            jump = c_inverse[op - OP_LT_INT];
        else if (op == OP_EQ && c_int_eq(cond))
            jump = OPC_JNE;
        else if (op == OP_NE && c_int_eq(cond))
            jump = OPC_JEQ;
        if (jump >= 0) {
            c_mut(cond); /* spec 018 */
            long left = cexpr(cond->a.n, -1);
            long right;
            Node *rhs = cond->b.n;
            if (rhs->kind == N_INT && c_fits32(rhs->a.i)) { /* a constant right side */
                c_mut(rhs);
                right = rhs->a.i;
                jump += OPC_JLT_K - OPC_JLT;
            } else {
                right = cexpr(rhs, -1);
            }
            long at = c_emit(jump, left, right, 0, cond);
            vec_push(c_fix, (void *)at);
            return at;
        }
    }
    long v = cexpr(cond, -1); /* any other condition: compute it, jump when false */
    return c_jump(OPC_JF, v, 0, cond);
}

/* Calls of user functions and function values: the arguments in consecutive temporaries. */
static long ccall(Node *node, long dst, long opc, long callee, long mark)
{
    Vec *args = node->b.v;
    long n = args->n;
    if (opc == OPC_CALL_BUILTIN && n > 3) n = 3; /* builtins take at most three */
    long first = c_temp; /* the first argument's slot */
    for (long i = 0; i < n; i++) c_new_temp();
    for (long i = 0; i < n; i++) {
        long slot = first + i * 8;
        c_mov(slot, cexpr(vec_get(args, i), slot), node);
    }
    c_temp = mark; /* the arguments are read by the call itself */
    dst = c_dst(dst);
    long c = n; /* CALL_BUILTIN: the argument count */
    if (opc == OPC_CALL) {
        c = (long)node->c.n; /* CALL: the declaration */
        opc = OPC_CALL_FAST;
        if (node->e.p) { /* generic: a call-site record holds the callee */
            GenericSite *gs = c_code_alloc(GS_SIZE);
            gs->decl = node->c.n;
            gs->valid = 0;
            c = (long)gs;
            opc = OPC_CALL_GENERIC;
        }
    }
    if (opc == OPC_CALL_VALUE) c = callee; /* CALL_VALUE: the callee's slot */
    c_emit(opc, dst, first, c, node);
    return dst;
}

/* Any other builtin: up to three arguments, each read in place. */
static long cbuiltin(Node *node, long dst, long mark)
{
    long slots[3] = {0, 0, 0};
    Vec *args = node->b.v;
    long skip = node->d.i == B_COLL; /* spec 014: __coll's OP literal is read from the node at run time */
    long n = args->n - skip > 3 ? 3 : args->n - skip; /* builtins take at most three */
    for (long i = 0; i < n; i++) slots[i] = cexpr(vec_get(args, i + skip), -1);
    c_temp = mark; /* the arguments are read by the call itself */
    dst = c_dst(dst);
    long c = slots[1] << 16 | slots[2] << 40 | n; /* count | second slot << 16 | third slot << 40 */
    c_emit(OPC_CALL_BUILTIN, dst, slots[0], c, node);
    return dst;
}

/* spec 018 (research R5): `expect(cond)` by the shape of cond. A comparison passes its operands to
 * the check (NF_EXPECT_OPS on the call), `not` of a comparison inverts it (NF_EXPECT_NEG), and
 * `expect(a and b)` becomes `expect(a)` then `expect(b)`: the same short-circuit order. */
static int c_is_cmp(Node *n)
{
    return n->kind == N_BINARY && n->d.i >= OP_EQ && n->d.i <= OP_GE_STR;
}

/* A copy of the expect call checking only part, located at part. */
static Node *c_expect_part(Node *call, Node *part)
{
    Node *p = alloc(NODE_SIZE);
    *p = *call;
    p->b.v = vec_new();
    vec_push(p->b.v, part);
    p->start = part->start;
    p->end = part->end;
    return p;
}

static long cbuiltin(Node *node, long dst, long mark);

static long cexpect(Node *call, long dst, long mark)
{
    Node *cond = call->b.v->data[0].n;
    if (cond->kind == N_BINARY && cond->d.i == OP_AND) {
        cexpect(c_expect_part(call, cond->a.n), -1, mark);
        c_temp = mark;
        cexpect(c_expect_part(call, cond->b.n), -1, mark);
        c_temp = mark;
        return c_dst(dst); /* (a void value: any slot) */
    }
    Node *cmp = cond;
    if (cond->kind == N_UNARY && cond->d.i == OP_NOT && c_is_cmp(cond->a.n)) {
        cmp = cond->a.n;
        call->flags |= NF_EXPECT_NEG;
    }
    if (!c_is_cmp(cmp)) return cbuiltin(call, dst, mark);
    call->flags |= NF_EXPECT_OPS;
    long left = c_new_temp(), right = c_new_temp(), c = c_new_temp();
    c_mov(left, cexpr(cmp->a.n, left), cmp);
    c_mov(right, cexpr(cmp->b.n, right), cmp);
    long op = cmp->d.i, opc = op + (OPC_ADD_INT - OP_ADD_INT); /* as cbinary */
    int int_eq = c_int_eq(cmp);
    if (op == OP_EQ && int_eq) opc = OPC_EQ_INT;
    if (op == OP_NE && int_eq) opc = OPC_NE_INT;
    c_emit(opc, c, left, right, cmp);
    if (call->flags & NF_EXPECT_NEG) c_emit(OPC_NOT, c, c, 0, cond);
    c_temp = mark;
    dst = c_dst(dst);
    c_emit(OPC_CALL_BUILTIN, dst, c, left << 16 | right << 40 | 3, call);
    return dst;
}

/* A binary operator (not and/or). */
static long cbinary(Node *node, long dst, long mark)
{
    long op = node->d.i;
    long kop = c_const_op(node);
    if (kop >= 0) { /* x op constant: one instruction (US1) */
        c_mut(node->b.n); /* spec 018: the constant, read in place */
        long left = cexpr(node->a.n, -1);
        c_temp = mark;
        dst = c_dst(dst);
        c_emit(kop, dst, left, node->b.n->a.i, node);
        return dst;
    }
    long left = cexpr(node->a.n, -1);
    long right = cexpr(node->b.n, -1);
    c_temp = mark; /* operands read, then the result written: temporaries reusable */
    dst = c_dst(dst);
    int int_eq = c_int_eq(node);
    long opc = op + (OPC_ADD_INT - OP_ADD_INT);
    if (op >= OP_LT_PARAM) opc = OPC_CMP_PARAM;
    if (op == OP_EQ && int_eq) opc = OPC_EQ_INT;
    if (op == OP_NE && int_eq) opc = OPC_NE_INT;
    c_emit(opc, dst, left, right, node);
    return dst;
}

/* and / or: a fresh temporary, a jump over the right side. */
static long cand_or(Node *node)
{
    long v = c_new_temp();
    c_mov(v, cexpr(node->a.n, v), node);
    long over = c_jump(node->d.i == OP_AND ? OPC_JF : OPC_JT, v, 0, node);
    c_mov(v, cexpr(node->b.n, v), node);
    c_target(over, c_here());
    c_temp = v + 8;
    return v;
}

/* try e. */
static long ctry(Node *node, long dst, long mark)
{
    long r = cexpr(node->a.n, -1);
    c_temp = mark;
    dst = c_dst(dst);
    if (!c_ens) {
        c_emit(OPC_TRY, dst, r, 0, node);
        return dst;
    }
    long ok = c_emit(OPC_JTAG_NE, 1, r, 0, node); /* with `ensures`: not err takes the payload */
    vec_push(c_fix, (void *)ok);
    c_ens_exit(r, node); /* an err goes to the epilogue */
    c_target(ok, c_here());
    c_emit(OPC_PAYLOAD, dst, r, 0, node);
    return dst;
}

/* A one-operand instruction (a = op b): field access, len, unary operators. */
static long cunary_op(Node *node, long dst, long mark, long opc, Node *operand, long c)
{
    long v = cexpr(operand, -1);
    c_temp = mark;
    dst = c_dst(dst);
    c_emit(opc, dst, v, c, node);
    return dst;
}

/* A two-operand instruction (a = b op c): indexing, has. */
static long cpair_op(Node *node, long dst, long mark, long opc, Node *left, Node *right)
{
    long l = cexpr(left, -1);
    long r = cexpr(right, -1);
    c_temp = mark;
    dst = c_dst(dst);
    c_emit(opc, dst, l, r, node);
    return dst;
}

/* A new list, record or variant in a fresh temporary, then its parts stored into it. */
static long cconstruct(Node *node)
{
    long v = c_new_temp();
    if (node->kind == N_LIST) { /* [a, b, c]: the list first, then each element pushed */
        c_emit(OPC_NEW_LIST, v, 0, node->a.v->n, node);
        for (long i = 0; i < node->a.v->n; i++) {
            c_emit(OPC_LIST_PUSH, v, cexpr(vec_get(node->a.v, i), -1), 0, node);
            c_temp = v + 8;
        }
    } else if (node->kind == N_NEW_RECORD) { /* new R { ... }: the record, then the initialisers */
        c_emit(OPC_NEW_RECORD, v, 0, count_members(node->c.n->b.v), node);
        for (long i = 0; i < node->b.v->n; i++) {
            Node *init = vec_get(node->b.v, i);
            c_emit(OPC_SET_FIELD, v, cexpr(init->b.n, -1), init->c.i * 8 + R_FIELDS, init);
            c_temp = v + 8;
        }
    } else { /* V.case(payload): the variant first, then the payload */
        c_emit(OPC_NEW_VARIANT, v, 0, node->e.i, node);
        if (node->c.n) {
            c_emit(OPC_SET_PAYLOAD, v, cexpr(node->c.n, -1), 0, node);
            c_temp = v + 8;
        }
    }
    return v;
}

/* Compiles an expression; returns the slot holding its value (dst < 0: any). */
long cexpr(Node *node, long dst)
{
    c_mut(node); /* spec 018: every expression the program's own functions evaluate */
    long mark = c_temp; /* temporaries mark */
    switch (node->kind) {
    case N_INT:
    case N_FLOAT:
    case N_STR:
    case N_BOOL: /* constant */
        dst = c_dst(dst);
        c_emit(OPC_LOADK, dst, 0, node->a.i, node);
        return dst;
    case N_IDENT:
        if (node->flags & NF_FNVAL) { /* a named function as a value */
            dst = c_dst(dst);
            c_emit(OPC_FNVAL, dst, 0, node->d.i, node);
            return dst;
        }
        return node->c.i * 8; /* a local: its own slot */
    case N_BINARY:
        if (node->d.i == OP_AND || node->d.i == OP_OR) return cand_or(node);
        return cbinary(node, dst, mark);
    case N_UNARY: {
        long op = node->d.i;
        if (node->flags & NF_MUT_SKIP) return cexpr(node->a.n, dst); /* spec 018: a mutant without this `not` */
        long opc = op == OP_NOT ? OPC_NOT : op == OP_NEG_FLOAT ? OPC_NEG_FLOAT : OPC_NEG_INT;
        return cunary_op(node, dst, mark, opc, node->a.n, 0);
    }
    case N_CALL: {
        long id = node->d.i;
        if (!id) return ccall(node, dst, OPC_CALL, 0, mark);
        Vec *args = node->b.v;
        if (id == BI_EXPECT) return cexpect(node, dst, mark); /* spec 018 */
        if (id == BI_LEN) return cunary_op(node, dst, mark, OPC_LEN, args->data[0].n, 0); /* len(x) */
        if (id == BI_HAS && args->data[0].n->type->kind == TY_MAP) /* has(m, k) (spec 014: maps only) */
            return cpair_op(node, dst, mark, OPC_HAS, args->data[0].n, args->data[1].n);
        if (id == BI_PUSH && args->data[0].n->type->kind == TY_LIST) { /* push(xs, v): no value */
            long xs = cexpr(args->data[0].n, -1);
            long v = cexpr(args->data[1].n, -1);
            c_emit(OPC_PUSH, xs, v, 0, node);
            c_temp = mark;
            return c_dst(dst); /* (a void value: any slot) */
        }
        return cbuiltin(node, dst, mark);
    }
    case N_FIELD: /* r.f */
        return cunary_op(node, dst, mark, OPC_GET_FIELD, node->a.n, node->c.i * 8 + R_FIELDS);
    case N_INDEX: /* xs[i] / m[k] */
        if (TY_IS_COLL(node->a.n->type->kind)) /* spec 014 */
            return cpair_op(node, dst, mark, OPC_INDEX_COLL, node->a.n, node->b.n);
        return cpair_op(node, dst, mark, (node->flags & NF_MAP) ? OPC_INDEX_MAP : OPC_INDEX_LIST, node->a.n,
                        node->b.n);
    case N_LIST:
    case N_NEW_RECORD:
    case N_VCASE: return cconstruct(node);
    case N_NEW_LIST: /* new list[T] */
        dst = c_dst(dst);
        c_emit(OPC_NEW_LIST, dst, 0, 4, node);
        return dst;
    case N_NEW_COLL: /* spec 014: new ordered_map[K, V] */
        dst = c_dst(dst);
        c_emit(OPC_NEW_COLL, dst, 0, 0, node);
        return dst;
    case N_NEW_MAP: /* new map[K, V] */
        dst = c_dst(dst);
        c_emit(OPC_NEW_MAP, dst, 0, 0, node);
        return dst;
    case N_TRY: return ctry(node, dst, mark);
    case N_LAMBDA: /* fn(...) { ... }: a closure; the body is compiled later */
        vec_push(c_lams, node);
        dst = c_dst(dst);
        c_emit(OPC_CLOSURE, dst, 0, 0, node);
        return dst;
    case N_CALL_VALUE: { /* callee(args): the callee, then the arguments, then the call */
        long callee = cexpr(node->a.n, -1);
        return ccall(node, dst, OPC_CALL_VALUE, callee, mark);
    }
    default: c_unsupported(); return 0;
    }
}
