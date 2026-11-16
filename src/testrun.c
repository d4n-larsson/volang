/* Test runner: runs each `test` block with its own recovery point and deadline, and reports it
 * (ported from src/testrun.s, spec 008). */
#include "vo.h"

enum { ST_PASS, ST_FAIL, ST_ERROR, ST_TIMEOUT };

long t_passed;
long t_failed;
long t_errored;
long t_timed_out;

static const char *const status_words[] = {"pass", "fail", "error", "timeout"};

/* ---- spec 012: requirement IDs (@spec) ----
 * A fixed table, filled before a file's tests run: tests run on a fresh heap each, so nothing here
 * allocates while they run. The IDs are the parser's strings, which outlive every test's heap.
 * ponytail: at most SP_MAX distinct IDs and SP_PER IDs per test in one run; more are left out of the
 * summary, raise the limits if a program needs them. */
#define SP_MAX 4096
#define SP_PER 64
static Str *sp_ids[SP_MAX]; /* sorted by bytes */
static long sp_tests[SP_MAX], sp_passed[SP_MAX], sp_n;
static Vec *sp_items;  /* the items of the file whose tests run */
static long sp_index;  /* the index of the test being reported */

static long sp_find(Str *id)
{
    for (long i = 0; i < sp_n; i++)
        if (str_eq(sp_ids[i], id)) return i;
    return -1;
}

/* Adds every @spec ID of the file's items to the table (sorted insertion). */
static void sp_collect(Vec *items)
{
    for (long i = 0; i < items->n; i++) {
        Node *a = vec_get(items, i);
        if (a->kind != N_ANNOTATION || !str_eq_cstr(a->a.n->a.s, "spec")) continue;
        for (long k = 0; k < a->b.v->n; k++) {
            Str *id = ((Node *)vec_get(a->b.v, k))->a.s;
            if (sp_find(id) >= 0 || sp_n == SP_MAX) continue;
            long j = sp_n++;
            while (j > 0 && str_cmp(sp_ids[j - 1], id) > 0) {
                sp_ids[j] = sp_ids[j - 1];
                sp_tests[j] = sp_tests[j - 1];
                sp_passed[j] = sp_passed[j - 1];
                j--;
            }
            sp_ids[j] = id;
            sp_tests[j] = sp_passed[j] = 0;
        }
    }
}

/* The table indexes of the IDs of the @spec right above items[index], ascending (= sorted) and once
 * each, into out; their number. */
static long sp_of(Vec *items, long index, long *out)
{
    long n = 0;
    for (long i = index - 1; i >= 0 && ((Node *)vec_get(items, i))->kind == N_ANNOTATION; i--) {
        Node *a = vec_get(items, i);
        if (!str_eq_cstr(a->a.n->a.s, "spec")) continue;
        for (long k = 0; k < a->b.v->n; k++) {
            long t = sp_find(((Node *)vec_get(a->b.v, k))->a.s);
            long j = 0;
            while (j < n && out[j] < t) j++;
            if (t < 0 || (j < n && out[j] == t) || n == SP_PER) continue;
            for (long m = n; m > j; m--) out[m] = out[m - 1];
            out[j] = t;
            n++;
        }
    }
    return n;
}

/* `,"spec":["A","B"]` (JSON) or ` [A, B]` (text) for the test being reported, and its tally. */
static void sp_report(SB *sb, int status)
{
    if (!sp_items) return;
    long ids[SP_PER];
    long n = sp_of(sp_items, sp_index, ids);
    if (!n) return;
    for (long i = 0; i < n; i++) {
        sp_tests[ids[i]]++;
        if (!status) sp_passed[ids[i]]++;
    }
    if (opt_json) {
        sb_cstr(sb, ",\"spec\":[");
        for (long i = 0; i < n; i++) {
            if (i) sb_byte(sb, ',');
            json_str_into(sb, sp_ids[ids[i]]);
        }
        sb_byte(sb, ']');
    } else if (status) {
        sb_cstr(sb, " [");
        for (long i = 0; i < n; i++) {
            if (i) sb_cstr(sb, ", ");
            sb_str(sb, sp_ids[ids[i]]);
        }
        sb_byte(sb, ']');
    }
}

/* One line (or `spec_result` record) per ID that a test ran for, before the summary. */
static void sp_summary(SB *sb)
{
    for (long i = 0; i < sp_n; i++) {
        if (!sp_tests[i]) continue; /* (its tests were filtered out) */
        if (opt_json) {
            sb_cstr(sb, "{\"v\":1,\"kind\":\"spec_result\",\"id\":");
            json_str_into(sb, sp_ids[i]);
            sb_cstr(sb, ",\"tests\":");
            int_to_dec(sb, sp_tests[i]);
            sb_cstr(sb, ",\"passed\":");
            int_to_dec(sb, sp_passed[i]);
            sb_cstr(sb, "}\n");
        } else {
            sb_cstr(sb, "spec ");
            sb_str(sb, sp_ids[i]);
            sb_cstr(sb, ": ");
            int_to_dec(sb, sp_tests[i]);
            sb_cstr(sb, sp_tests[i] == 1 ? " test, " : " tests, ");
            if (sp_passed[i] == sp_tests[i]) {
                sb_cstr(sb, "all passing\n");
            } else {
                int_to_dec(sb, sp_tests[i] - sp_passed[i]);
                sb_cstr(sb, " not passing\n");
            }
        }
    }
}
static const char *const status_upper[] = {"PASS ", "FAIL ", "ERROR ", "TIMEOUT "};

void report_test(Node *test, int status, long ms)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    /* the failure location: expect call, or the runtime error node */
    Node *at = (Node *)(status == ST_FAIL ? tf_node : rt_err_node);
    if (opt_json) { /* JSON record */
        sb_cstr(sb, "{\"v\":1,\"kind\":\"test\",\"name\":");
        json_str_into(sb, test->a.s);
        sb_cstr(sb, ",\"status\":\"");
        sb_cstr(sb, (char *)status_words[status]);
        sb_cstr(sb, "\",\"file\":");
        json_str_into(sb, file_get((int)test->file)->path);
        sb_cstr(sb, ",\"start\":");
        json_pos(sb, (int)test->file, test->start);
        Fx *fx = test->d.fx; /* the test's grant */
        if (fx && fx->declared) {
            Str *uses = fx_render(fx->declared);
            GC_ROOT(&uses);
            sb_cstr(sb, ",\"uses\":");
            json_str_into(sb, uses);
            GC_UNROOT(1);
        }
        sb_cstr(sb, ",\"ms\":");
        int_to_dec(sb, ms);
        if (test->flags & NF_PROP) prop_json(sb, test, status);
        sp_report(sb, status); /* spec 012 */
        if (status && at) {
            sb_cstr(sb, ",\"failure\":{\"file\":");
            json_str_into(sb, file_get((int)at->file)->path);
            sb_cstr(sb, ",\"start\":");
            json_pos(sb, (int)at->file, at->start);
            sb_cstr(sb, ",\"end\":");
            json_pos(sb, (int)at->file, at->end);
            sb_cstr(sb, ",\"message\":");
            if (status == ST_FAIL) {
                json_str_into(sb, (Str *)tf_msg);
                if (tf_expected) {
                    sb_cstr(sb, ",\"expected\":");
                    json_str_into(sb, (Str *)tf_expected);
                    sb_cstr(sb, ",\"actual\":");
                    json_str_into(sb, (Str *)tf_actual);
                }
            } else {
                json_str_into(sb, (Str *)rt_err_msg);
                sb_cstr(sb, ",\"code\":\"");
                sb_cstr(sb, (char *)rt_err_code);
                sb_byte(sb, '"');
                if (rt_err_trace_json) { /* spec 006: the frames and their values */
                    sb_cstr(sb, ",\"trace\":");
                    sb_str(sb, (Str *)rt_err_trace_json);
                }
            }
            sb_byte(sb, '}');
        }
        sb_cstr(sb, "}\n");
    } else { /* human line: STATUS name (Nms) + failure details */
        sb_cstr(sb, (char *)status_upper[status]);
        sb_str(sb, test->a.s);
        sb_cstr(sb, " (");
        if (test->flags & NF_PROP) prop_cases_text(sb);
        int_to_dec(sb, ms);
        sb_cstr(sb, "ms)");
        sp_report(sb, status); /* spec 012: the IDs of a test that does not pass */
        sb_byte(sb, '\n');
        if (status && at) {
            sb_cstr(sb, "  ");
            human_loc(sb, (int)at->file, at->start);
            if (status == ST_FAIL) {
                sb_byte(sb, ':');
                sb_byte(sb, ' ');
                sb_str(sb, (Str *)tf_msg);
                sb_byte(sb, '\n');
                if (tf_expected) {
                    sb_cstr(sb, "    expected: ");
                    sb_str(sb, (Str *)tf_expected);
                    sb_byte(sb, '\n');
                    sb_cstr(sb, "    actual:   ");
                    sb_str(sb, (Str *)tf_actual);
                    sb_byte(sb, '\n');
                }
            } else {
                sb_cstr(sb, ": runtime error[");
                sb_cstr(sb, (char *)rt_err_code);
                sb_cstr(sb, "]: ");
                sb_str(sb, (Str *)rt_err_msg);
                sb_byte(sb, '\n');
                if (rt_err_trace_txt) sb_str(sb, (Str *)rt_err_trace_txt); /* spec 006: frames and values */
            }
        }
        if (test->flags & NF_PROP) prop_text(sb, test, status);
    }
    out_bytes(1, sb->ptr, sb->len);
    GC_UNROOT(1);
}

/* Runs the file's tests in source order (only those whose name contains --filter). */
void run_file_tests(int file)
{
    long jb[JB_SIZE / 8]; /* the recovery buffer */
    Vec *volatile items = file_get(file)->ast->a.v;
    sp_collect(items);
    sp_items = items;
    for (volatile long i = 0; i < items->n; i++) {
        Node *volatile test = vec_get(items, i);
        if (test->kind != N_TEST) continue;
        if (opt_filter && str_find_from(test->a.s, (void *)opt_filter, 0) < 0) continue;
        gc_start(); /* every test starts with an empty heap (spec 003 FR-009) */
        tf_node = 0;
        rt_err_code = 0;
        rt_err_node = 0;
        volatile long start = sys_clock_ms();
        deadline_ms = 0; /* a deadline per test */
        if (opt_timeout) deadline_ms = opt_timeout * 1000 + start;
        long status; /* 0 pass, 1 expect failed, 2 runtime error */
        if (test->flags & NF_PROP) { /* a property runs its own cases (prop.s) */
            status = prop_run(test, i);
        } else {
            recover_point = (long)jb;
            status = jmp_save(jb);
            if (!status) {
                rt_effects_start(1); /* every test starts from seed and clock defaults */
                run_test_body(test);
                status = ST_PASS;
            }
        }
        recover_point = 0;
        deadline_ms = 0;
        interp_reset();
        if (status == ST_ERROR && rt_err_code == (long)r0009) status = ST_TIMEOUT; /* R0009 is a timeout */
        if (status == ST_PASS)
            t_passed++;
        else if (status == ST_FAIL)
            t_failed++;
        else if (status == ST_ERROR)
            t_errored++;
        else
            t_timed_out++;
        sp_index = i;
        report_test(test, (int)status, sys_clock_ms() - start);
        gc_report(1, "test", test->a.s); /* --mem-stats: right after the test's record */
        /* the test's leftovers point into a heap about to be reset; they must not keep anything
           alive in the next test (spec 003) */
        tf_msg = 0;
        tf_expected = 0;
        tf_actual = 0;
        rt_err_msg = 0;
        rt_err_extra = 0;
        ret_val = 0;
        pr_args = 0;
        pr_best = 0;
        pr_cand = 0;
    }
}

void test_summary(long files)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sp_summary(sb); /* spec 012: a line per requirement ID, before the summary */
    if (sb->len) {
        out_bytes(1, sb->ptr, sb->len);
        sb->len = 0;
    }
    if (opt_json) { /* JSON: the common summary plus the test counts */
        sb_cstr(sb, ",\"passed\":");
        int_to_dec(sb, t_passed);
        sb_cstr(sb, ",\"failed\":");
        int_to_dec(sb, t_failed);
        sb_cstr(sb, ",\"errored\":");
        int_to_dec(sb, t_errored);
        sb_cstr(sb, ",\"timed_out\":");
        int_to_dec(sb, t_timed_out);
        GC_UNROOT(1);
        summary_emit(1, 1, "test", files, sb_finish(sb));
        return;
    }
    int_to_dec(sb, t_passed); /* human: "2 passed, 1 failed[, 1 errored][, 1 timed out]" */
    sb_cstr(sb, " passed, ");
    int_to_dec(sb, t_failed);
    sb_cstr(sb, " failed");
    if (t_errored) {
        sb_cstr(sb, ", ");
        int_to_dec(sb, t_errored);
        sb_cstr(sb, " errored");
    }
    if (t_timed_out) {
        sb_cstr(sb, ", ");
        int_to_dec(sb, t_timed_out);
        sb_cstr(sb, " timed out");
    }
    sb_byte(sb, '\n');
    out_bytes(1, sb->ptr, sb->len);
    GC_UNROOT(1);
}
