/* Mutation testing (spec 018 US4, research R10, R11): small type-preserving changes to the program's
 * own functions, one at a time, each run in a forked child that compiles the changed program again and
 * runs only the tests that reached the changed function. A change no test notices is reported.
 * ponytail: one child at a time, and a full recompile in each; a bounded pool of children (results
 * reordered as tasks do) and recompiling only the changed item if run times need it. */
#include "vo.h"

/* Everything here outlives the tests, whose heap is reset before each one: permanent memory. */

enum { MK_OP, MK_BOOL, MK_NOT, MK_INT };

typedef struct Mutant {
    Node *node;
    Node *fn;      /* the named function it belongs to */
    long fn_index; /* that function's index in the reach matrix */
    long kind;     /* MK_* */
    long value;    /* the new operator code (MK_OP) or integer (MK_INT) */
} Mutant;

static Mutant *muts;
static long mut_n, mut_cap;
static Node **mt_test; /* the baseline's tests in run order, their item index and time */
static long *mt_index, *mt_ms;
static long mt_n, mt_cap;
static long *surv; /* indexes into muts of the changes that survived */
static long surv_n, m_caught, m_not_tried, m_total;

/* The other operator of the R10 table, or 0. */
static long swap_op(long op)
{
    if (op >= OP_LT_INT && op <= OP_GE_STR) return ((op - OP_LT_INT) % 2) ? op - 1 : op + 1; /* < <=, > >= */
    switch (op) {
    case OP_EQ: return OP_NE;
    case OP_NE: return OP_EQ;
    case OP_ADD_INT: return OP_SUB_INT;
    case OP_SUB_INT: return OP_ADD_INT;
    case OP_MUL_INT: return OP_DIV_INT;
    case OP_DIV_INT: return OP_MUL_INT;
    case OP_ADD_FLOAT: return OP_SUB_FLOAT;
    case OP_SUB_FLOAT: return OP_ADD_FLOAT;
    case OP_MUL_FLOAT: return OP_DIV_FLOAT;
    case OP_DIV_FLOAT: return OP_MUL_FLOAT;
    case OP_AND: return OP_OR;
    case OP_OR: return OP_AND;
    default: return 0;
    }
}

static const char *op_text(long op)
{
    if (op >= OP_LT_INT && op <= OP_GE_STR) {
        static const char *const cmp[] = {"<", "<=", ">", ">="};
        return cmp[(op - OP_LT_INT) % 4];
    }
    switch (op) {
    case OP_EQ: return "==";
    case OP_NE: return "!=";
    case OP_ADD_INT:
    case OP_ADD_FLOAT: return "+";
    case OP_SUB_INT:
    case OP_SUB_FLOAT: return "-";
    case OP_MUL_INT:
    case OP_MUL_FLOAT: return "*";
    case OP_DIV_INT:
    case OP_DIV_FLOAT: return "/";
    case OP_AND: return "and";
    default: return "or";
    }
}

static void push_mut(Node *n, Node *fn, long fn_index, long kind, long value)
{
    if (mut_n == mut_cap) {
        long cap = mut_cap ? mut_cap * 2 : 256;
        Mutant *m = perm_alloc(cap * (long)sizeof(Mutant));
        for (long i = 0; i < mut_n; i++) m[i] = muts[i];
        muts = m;
        mut_cap = cap;
    }
    muts[mut_n++] = (Mutant){n, fn, fn_index, kind, value};
}

/* Called by the compiler for each expression of the program's own functions (outside contracts). */
void mut_note(Node *n, Node *fn, long fn_index)
{
    for (long i = mut_n - 1; i >= 0 && i >= mut_n - 8; i--) /* (a node can be visited twice in a row) */
        if (muts[i].node == n) return;
    if (n->kind == N_BINARY) {
        long op = swap_op(n->d.i);
        if (op) push_mut(n, fn, fn_index, MK_OP, op);
    } else if (n->kind == N_BOOL) {
        push_mut(n, fn, fn_index, MK_BOOL, !n->a.i);
    } else if (n->kind == N_UNARY && n->d.i == OP_NOT) {
        push_mut(n, fn, fn_index, MK_NOT, 0);
    } else if (n->kind == N_INT && n->a.i != 0x7fffffffffffffffL) {
        push_mut(n, fn, fn_index, MK_INT, n->a.i + 1);
    }
}

void mut_note_test(Node *test, long index, long ms)
{
    if (mt_n == mt_cap) {
        long cap = mt_cap ? mt_cap * 2 : 64;
        Node **t = perm_alloc(cap * 8);
        long *x = perm_alloc(cap * 8), *y = perm_alloc(cap * 8);
        for (long i = 0; i < mt_n; i++) {
            t[i] = mt_test[i];
            x[i] = mt_index[i];
            y[i] = mt_ms[i];
        }
        mt_test = t;
        mt_index = x;
        mt_ms = y;
        mt_cap = cap;
    }
    mt_test[mt_n] = test;
    mt_index[mt_n] = index;
    mt_ms[mt_n++] = ms;
}

/* The source text of the change: the node's text with the operator token, literal or `not` replaced. */
static void mut_texts(Mutant *m, Str **text, Str **mutated)
{
    Node *n = m->node;
    char *src = file_get((int)n->file)->text;
    Str *t = str_new(src + n->start, (long)n->end - n->start);
    GC_ROOT(&t);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    if (m->kind == MK_OP) {
        const char *old = op_text(n->d.i);
        long ol = cstr_len((char *)old);
        long at = n->a.n->end; /* the operator lies between the operands */
        while (at + ol <= (long)n->b.n->start && !mem_eq(src + at, (char *)old, ol)) at++;
        sb_bytes(sb, src + n->start, at - n->start);
        sb_cstr(sb, (char *)op_text(m->value));
        sb_bytes(sb, src + at + ol, (long)n->end - at - ol);
    } else if (m->kind == MK_BOOL) {
        sb_cstr(sb, m->value ? "true" : "false");
    } else if (m->kind == MK_NOT) {
        sb_bytes(sb, src + n->a.n->start, (long)n->a.n->end - n->a.n->start);
    } else {
        int_to_dec(sb, m->value);
    }
    Str *mut = sb_finish(sb);
    GC_UNROOT(2);
    *text = t;
    *mutated = mut;
}

static int mut_before(Mutant *a, Mutant *b)
{
    if (a->node->file != b->node->file) return a->node->file < b->node->file;
    return a->node->start < b->node->start;
}

/* Runs change m in a child: 1 when a test noticed it (failed, errored or timed out), 0 when not. */
static int mut_try(Mutant *m, long *tests, long nt, long limit_ms)
{
    out_flush(); /* the child must not write our buffered output again */
    int fds[2];
    if (sys_pipe(fds) < 0) return 1;
    long parent = vo_syscall0(SYS_getpid);
    long pid = sys_fork();
    if (pid < 0) {
        sys_close(fds[0]);
        sys_close(fds[1]);
        return 1;
    }
    if (pid == 0) { /* the child: the changed program, the reaching tests, quietly */
        sys_die_with_parent(parent);
        sys_close(fds[0]);
        long nul = sys_open("/dev/null", 1 /* O_WRONLY */);
        if (nul >= 0) {
            sys_dup_to((int)nul, 1);
            sys_dup_to((int)nul, 2);
        }
        Node *n = m->node;
        if (m->kind == MK_OP) n->d.i = m->value;
        else if (m->kind == MK_BOOL) n->a.i = m->value;
        else if (m->kind == MK_NOT) n->flags |= NF_MUT_SKIP;
        else n->a.i = m->value;
        cov_reach = 0; /* research R11: children never write the shared bytes */
        cov_hit = 0;
        gc_on = 0; /* compiled code and its nodes are permanent, as in the parent (each test starts the heap) */
        compile_reset();
        compile_program();
        for (long i = 0; i < nt; i++)
            if (run_one_test(mt_test[tests[i]], mt_index[tests[i]]) != 0) vo_syscall1(SYS_exit_group, 1);
        vo_syscall1(SYS_exit_group, 0);
    }
    sys_close(fds[1]);
    int timed_out = !sys_wait_fd(fds[0], 1 /* POLLIN: readable or closed when the child ends */, limit_ms);
    if (timed_out) sys_kill(pid);
    int status = 0;
    sys_wait(pid, &status);
    sys_close(fds[0]);
    return timed_out || status != 0; /* exit 0 is the only "nothing noticed" */
}

/* After a passing baseline: tries the changes in order (file, position), the first --max-mutants of
 * those whose function a selected test reached. Returns the number of survivors. */
long mutate_run(void)
{
    for (long w = 1; w < mut_n; w *= 2) { /* a stable merge sort of the changes by position */
        Mutant *tmp = perm_alloc(mut_n * (long)sizeof(Mutant));
        for (long lo = 0; lo < mut_n; lo += 2 * w) {
            long mid = lo + w < mut_n ? lo + w : mut_n, hi = lo + 2 * w < mut_n ? lo + 2 * w : mut_n;
            long i = lo, j = mid, k = lo;
            while (i < mid || j < hi) tmp[k++] = (j >= hi || (i < mid && !mut_before(&muts[j], &muts[i]))) ? muts[i++] : muts[j++];
        }
        muts = tmp;
    }
    surv = perm_alloc((mut_n ? mut_n : 1) * 8);
    long *tests = perm_alloc((mt_n ? mt_n : 1) * 8);
    long tried = 0;
    for (long k = 0; k < mut_n; k++) {
        Mutant *m = &muts[k];
        if (sel_diff_fns) { /* --diff: only functions the diff changes or adds */
            long in = 0;
            for (long i = 0; i < sel_diff_fns->n && !in; i++) in = vec_get(sel_diff_fns, i) == m->fn;
            if (!in) continue;
        }
        long nt = 0, ms = 0;
        for (long t = 0; t < mt_n; t++)
            if (cov_reach[t * cov_nfn + m->fn_index]) {
                tests[nt++] = t;
                ms += mt_ms[t];
            }
        if (!nt) continue; /* no test reaches it: coverage reports that */
        m_total++;
        if (tried == opt_max_mutants) {
            m_not_tried++;
            continue;
        }
        tried++;
        if (mut_try(m, tests, nt, 10 * ms + 1000)) {
            m_caught++;
        } else {
            surv[surv_n++] = k;
        }
    }
    return surv_n;
}

/* The tests that reach the function of muts[k] as identifiers, into sb (JSON) or their count. */
static long mut_tests(SB *sb, Mutant *m)
{
    long n = 0;
    for (long t = 0; t < mt_n; t++) {
        if (!cov_reach[t * cov_nfn + m->fn_index]) continue;
        if (sb) {
            if (n) sb_byte(sb, ',');
            json_test_id_into(sb, mt_test[t]);
        }
        n++;
    }
    return n;
}

/* The surviving changes and the summary (contracts/cli.md). */
void mut_report(SB *sb)
{
    for (long i = 0; i < surv_n; i++) {
        Mutant *m = &muts[surv[i]];
        Str *text, *mutated;
        mut_texts(m, &text, &mutated);
        GC_ROOT(&text);
        GC_ROOT(&mutated);
        Node *n = m->node;
        Str *fname = m->fn->a.n->a.s;
        if (opt_json) {
            sb_cstr(sb, "{\"v\":1,\"kind\":\"mutant\",\"file\":");
            json_str_into(sb, file_get((int)n->file)->path);
            sb_cstr(sb, ",\"start\":");
            json_pos(sb, (int)n->file, n->start);
            sb_cstr(sb, ",\"end\":");
            json_pos(sb, (int)n->file, n->end);
            sb_cstr(sb, ",\"function\":");
            json_str_into(sb, fname);
            sb_cstr(sb, ",\"text\":");
            json_str_into(sb, text);
            sb_cstr(sb, ",\"mutated\":");
            json_str_into(sb, mutated);
            sb_cstr(sb, ",\"tests\":[");
            mut_tests(sb, m);
            sb_cstr(sb, "]}\n");
        } else {
            sb_cstr(sb, "SURVIVED ");
            human_loc(sb, (int)n->file, n->start);
            sb_cstr(sb, " in ");
            sb_str(sb, fname);
            sb_cstr(sb, ": `");
            sb_str(sb, text);
            sb_cstr(sb, "` -> `");
            sb_str(sb, mutated);
            long nt = mut_tests(0, m);
            sb_cstr(sb, "` (");
            int_to_dec(sb, nt);
            sb_cstr(sb, nt == 1 ? " test ran)\n" : " tests ran)\n");
        }
        GC_UNROOT(2);
    }
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"mutation_summary\",\"mutants\":");
        int_to_dec(sb, m_total);
        sb_cstr(sb, ",\"caught\":");
        int_to_dec(sb, m_caught);
        sb_cstr(sb, ",\"survived\":");
        int_to_dec(sb, surv_n);
        sb_cstr(sb, ",\"not_tried\":");
        int_to_dec(sb, m_not_tried);
        sb_cstr(sb, "}\n");
        return;
    }
    sb_cstr(sb, "mutation: ");
    int_to_dec(sb, m_total);
    sb_cstr(sb, m_total == 1 ? " change, " : " changes, ");
    int_to_dec(sb, m_caught);
    sb_cstr(sb, " caught, ");
    int_to_dec(sb, surv_n);
    sb_cstr(sb, " survived, ");
    int_to_dec(sb, m_not_tried);
    sb_cstr(sb, " not tried\n");
    if (surv_n) sb_cstr(sb, "survivors are candidates: a change can behave exactly like the original\n");
}
