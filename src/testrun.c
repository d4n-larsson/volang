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

/* ---- spec 018: identifiers and selection ---- */

/* A test's identifier, FILE::NAME, with FILE as reports print it (research R1). */
Str *test_id(Node *test)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_str(sb, file_get((int)test->file)->path);
    sb_cstr(sb, "::");
    sb_str(sb, test->a.s);
    GC_UNROOT(1);
    return sb_finish(sb);
}

/* The identifier as a JSON string, written in place: a test's report allocates as little as before,
 * so its memory figures (--mem-stats) do not change. */
void json_test_id_into(SB *sb, Node *test)
{
    Str *path = file_get((int)test->file)->path;
    sb_byte(sb, '"');
    json_escape_into(sb, path->data, path->len);
    sb_cstr(sb, "::");
    json_escape_into(sb, test->a.s->data, test->a.s->len);
    sb_byte(sb, '"');
}

/* 1 when id is the identifier of test (compared in place, nothing allocated). */
int test_id_eq(Node *test, Str *id)
{
    Str *path = file_get((int)test->file)->path;
    Str *name = test->a.s;
    long p = path->len;
    if (id->len != p + 2 + name->len) return 0;
    return mem_eq(id->data, path->data, p) && id->data[p] == ':' && id->data[p + 1] == ':' &&
           mem_eq(id->data + p + 2, name->data, name->len);
}

static Vec *sel_diff;  /* --diff: the tests and properties the diff reaches (0: no --diff) */
Vec *sel_diff_fns;     /* --diff: the functions it changes or adds (mutation, spec 018 US4) */
static long t_selected; /* tests and properties selected so far */

/* 1 when one of the @spec IDs right above items[index] is x or starts with x + "/". */
static int spec_matches(Vec *items, long index, Str *x)
{
    for (long i = index - 1; i >= 0 && ((Node *)vec_get(items, i))->kind == N_ANNOTATION; i--) {
        Node *a = vec_get(items, i);
        if (!str_eq_cstr(a->a.n->a.s, "spec")) continue;
        for (long k = 0; k < a->b.v->n; k++) {
            Str *id = ((Node *)vec_get(a->b.v, k))->a.s;
            if (str_eq(id, x)) return 1;
            if (id->len > x->len && id->data[x->len] == '/' && mem_eq(id->data, x->data, x->len)) return 1;
        }
    }
    return 0;
}

/* 1 when test (items[index]) is selected by --filter, --test, --spec and --diff (data-model.md):
 * the kinds combine as an intersection, repeated --test or --spec as a union. */
int test_selected(Node *test, Vec *items, long index)
{
    if (opt_filter && str_find_from(test->a.s, (void *)opt_filter, 0) < 0) return 0;
    if (opt_tests) {
        long hit = 0;
        for (long i = 0; i < opt_tests->n && !hit; i++) hit = test_id_eq(test, vec_get(opt_tests, i));
        if (!hit) return 0;
    }
    if (opt_specs) {
        long hit = 0;
        for (long i = 0; i < opt_specs->n && !hit; i++) hit = spec_matches(items, index, vec_get(opt_specs, i));
        if (!hit) return 0;
    }
    if (sel_diff) {
        long hit = 0;
        for (long i = 0; i < sel_diff->n && !hit; i++) hit = vec_get(sel_diff, i) == test;
        if (!hit) return 0;
    }
    return 1;
}

/* Up to three candidates closest to want (edit distance, ties in order), as " Closest: `A`, `B`." */
static Str *closest_note(Vec *cands, Str *want)
{
    long best[3], dist[3], n = 0;
    for (long i = 0; i < cands->n; i++) {
        Str *c = vec_get(cands, i);
        long d = levenshtein(c->data, c->len, want->data, want->len, c->len + want->len);
        long j = n < 3 ? n++ : 3;
        while (j > 0 && dist[j - 1] > d) { /* insertion, stable: equal distances keep their order */
            if (j < 3) {
                best[j] = best[j - 1];
                dist[j] = dist[j - 1];
            }
            j--;
        }
        if (j < 3) {
            best[j] = i;
            dist[j] = d;
        }
    }
    SB *sb = sb_new();
    for (long i = 0; i < n; i++) {
        sb_cstr(sb, i ? ", `" : " Closest: `");
        sb_str(sb, vec_get(cands, best[i]));
        sb_byte(sb, '`');
    }
    if (n) sb_byte(sb, '.');
    return sb_finish(sb);
}

/* Before anything runs: the --diff selection, and T0011 for a --test or --spec that matches no
 * test of the entry files (research R3). */
void test_select_start(long *ids, long n)
{
    if (opt_diff) {
        sel_diff = vec_new();
        sel_diff_fns = vec_new();
        impact_for_test(sel_diff, sel_diff_fns);
    }
    Vec *all_ids = vec_new(); /* identifiers, in run order */
    Vec *all_specs = vec_new(); /* @spec IDs, first appearance */
    for (long f = 0; f < n; f++) {
        Vec *items = file_get((int)ids[f])->ast->a.v;
        for (long i = 0; i < items->n; i++) {
            Node *it = vec_get(items, i);
            if (it->kind == N_TEST) vec_push(all_ids, test_id(it));
            if (it->kind != N_ANNOTATION || !str_eq_cstr(it->a.n->a.s, "spec")) continue;
            for (long k = 0; k < it->b.v->n; k++) {
                Str *id = ((Node *)vec_get(it->b.v, k))->a.s;
                long seen = 0;
                for (long j = 0; j < all_specs->n && !seen; j++) seen = str_eq(vec_get(all_specs, j), id);
                if (!seen) vec_push(all_specs, id);
            }
        }
    }
    for (long i = 0; opt_tests && i < opt_tests->n; i++) {
        Str *want = vec_get(opt_tests, i);
        long hit = 0;
        for (long j = 0; j < all_ids->n && !hit; j++) hit = str_eq(vec_get(all_ids, j), want);
        if (hit) continue;
        Str *m = msg("No test has the identifier `$1`.$2", want, closest_note(all_ids, want), 0);
        toolchain_error((int)opt_json, (int)report_fd, "T0011", m, 0);
        vo_exit(EXIT_USAGE);
    }
    for (long i = 0; opt_specs && i < opt_specs->n; i++) {
        Str *want = vec_get(opt_specs, i);
        long hit = 0;
        for (long j = 0; j < all_specs->n && !hit; j++) {
            Str *id = vec_get(all_specs, j);
            hit = str_eq(id, want) ||
                  (id->len > want->len && id->data[want->len] == '/' && mem_eq(id->data, want->data, want->len));
        }
        if (hit) continue;
        Str *m = msg("No test names `$1` in `@spec`.$2", want, closest_note(all_specs, want), 0);
        toolchain_error((int)opt_json, (int)report_fd, "T0011", m, 0);
        vo_exit(EXIT_USAGE);
    }
}

/* ---- spec 018: --list ---- */

static long t_listed;

/* `ID (test|property, FILE:LINE:COL) spec A, B uses ...`, then the documentation lines; or a
 * test_entry record. Nothing runs. */
static void list_entry(SB *sb, Node *test, Vec *items, long index)
{
    long file = test->file;
    Str *doc = doc_block(file, items, index);
    GC_ROOT(&doc);
    Str *uses = 0;
    Fx *fx = test->d.fx;
    if (fx && fx->declared) uses = fx_render(fx->declared);
    if (uses && !uses->len) uses = 0;
    GC_ROOT(&uses);
    long ids[SP_PER];
    long nspec = sp_of(items, index, ids);
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"test_entry\",\"id\":");
        json_test_id_into(sb, test);
        sb_cstr(sb, ",\"name\":");
        json_str_into(sb, test->a.s);
        sb_cstr(sb, (test->flags & NF_PROP) ? ",\"test_kind\":\"property\"" : ",\"test_kind\":\"test\"");
        sb_cstr(sb, ",\"file\":");
        json_str_into(sb, file_get((int)file)->path);
        sb_cstr(sb, ",\"start\":");
        json_pos(sb, (int)file, test->start);
        sb_cstr(sb, ",\"end\":");
        json_pos(sb, (int)file, test->end);
        sb_cstr(sb, ",\"doc\":");
        json_str_into(sb, doc);
        sb_cstr(sb, ",\"uses\":");
        if (uses)
            json_str_into(sb, uses);
        else
            sb_cstr(sb, "\"\"");
        sb_cstr(sb, ",\"spec\":[");
        for (long i = 0; i < nspec; i++) {
            if (i) sb_byte(sb, ',');
            json_str_into(sb, sp_ids[ids[i]]);
        }
        sb_cstr(sb, "]}\n");
    } else {
        sb_str(sb, file_get((int)file)->path);
        sb_cstr(sb, "::");
        sb_str(sb, test->a.s);
        sb_cstr(sb, (test->flags & NF_PROP) ? " (property, " : " (test, ");
        human_loc(sb, (int)file, test->start);
        sb_byte(sb, ')');
        for (long i = 0; i < nspec; i++) {
            sb_cstr(sb, i ? ", " : " spec ");
            sb_str(sb, sp_ids[ids[i]]);
        }
        if (uses) {
            sb_cstr(sb, " uses ");
            sb_str(sb, uses);
        }
        sb_byte(sb, '\n');
        for (long i = 0; i < doc->len;) { /* each line, indented by two spaces */
            long j = i;
            while (j < doc->len && doc->data[j] != '\n') j++;
            sb_cstr(sb, "  ");
            sb_bytes(sb, doc->data + i, j - i);
            sb_byte(sb, '\n');
            i = j + 1;
        }
    }
    GC_UNROOT(2);
}

/* Lists the selected tests of one entry file. */
void list_file_tests(int file)
{
    Vec *items = file_get(file)->ast->a.v;
    sp_collect(items);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    for (long i = 0; i < items->n; i++) {
        Node *test = vec_get(items, i);
        if (test->kind != N_TEST || !test_selected(test, items, i)) continue;
        t_listed++;
        list_entry(sb, test, items, i);
        out_bytes(1, sb->ptr, sb->len);
        sb->len = 0;
    }
    GC_UNROOT(1);
}

void list_summary(long files)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    if (opt_json) {
        sb_cstr(sb, ",\"listed\":");
        int_to_dec(sb, t_listed);
        GC_UNROOT(1);
        summary_emit(1, 1, "test", files, sb_finish(sb));
        return;
    }
    int_to_dec(sb, t_listed);
    sb_cstr(sb, t_listed == 1 ? " test\n" : " tests\n");
    out_bytes(1, sb->ptr, sb->len);
    GC_UNROOT(1);
}

long rt_err_in_test; /* spec 018: the runtime error happened in a test's own statements */

/* The cause of a report that is not a pass (research R7). */
static char *test_cause(int status)
{
    if (status == ST_FAIL) return pr_starved ? "too_few_inputs" : "check_failed";
    if (status == ST_TIMEOUT) return "timeout";
    char *c = (char *)rt_err_code;
    if (c && cstr_eq(c, "R0006")) return "out_of_memory";
    if (c && cstr_eq(c, "R0009")) return "timeout";
    if (c && cstr_eq(c, "R0010")) return "outside_grant";
    if (c && cstr_eq(c, "R0011")) return "hole";
    if (c && cstr_eq(c, "R0013")) return "ensures_broken";
    if (c && cstr_eq(c, "R0012")) return rt_err_in_test ? "requires_broken_by_test" : "requires_broken_by_code";
    return rt_err_in_test ? "error_in_test" : "error_in_code";
}

/* "... N more bytes not shown" after captured output that reached the limit (JSON: after a newline). */
static void cap_note(SB *sb, int json)
{
    if (!cap_dropped) return;
    if (json) sb_cstr(sb, "\\n");
    sb_cstr(sb, "... ");
    int_to_dec(sb, cap_dropped);
    sb_cstr(sb, " more bytes not shown");
}

void report_test(Node *test, int status, long ms)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    /* the failure location: expect call, or the runtime error node */
    Node *at = (Node *)(status == ST_FAIL ? tf_node : rt_err_node);
    if (opt_json) { /* JSON record */
        sb_cstr(sb, "{\"v\":1,\"kind\":\"test\",\"id\":");
        json_test_id_into(sb, test); /* spec 018 */
        sb_cstr(sb, ",\"name\":");
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
        if (rt_mode) { /* spec 054: recorded or replayed (absent: live) */
            sb_cstr(sb, ",\"recording\":\"");
            sb_str(sb, rt_mode);
            sb_byte(sb, '"');
        }
        if (rt_notes && rt_notes->n) {
            sb_cstr(sb, ",\"notes\":[");
            for (long k = 0; k < rt_notes->n; k++) {
                if (k) sb_byte(sb, ',');
                json_str_into(sb, vec_get(rt_notes, k));
            }
            sb_byte(sb, ']');
        }
        if (test->flags & NF_PROP) prop_json(sb, test, status);
        sp_report(sb, status); /* spec 012 */
        if (status) { /* spec 018 */
            sb_cstr(sb, ",\"cause\":\"");
            sb_cstr(sb, test_cause(status));
            sb_byte(sb, '"');
        }
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
                for (long k = 0; k < tf_nops; k++) { /* spec 018: the comparison's operands */
                    sb_cstr(sb, k ? ",{\"text\":" : ",\"operands\":[{\"text\":");
                    json_str_into(sb, (Str *)tf_op_text[k]);
                    sb_cstr(sb, ",\"value\":");
                    json_str_into(sb, (Str *)tf_op_val[k]);
                    sb_cstr(sb, k + 1 == tf_nops ? "}]" : "}");
                }
                if (tf_expected) {
                    sb_cstr(sb, ",\"expected\":");
                    json_str_into(sb, (Str *)tf_expected);
                    sb_cstr(sb, ",\"actual\":");
                    json_str_into(sb, (Str *)tf_actual);
                }
                if (tf_diff_path) { /* spec 018: the first difference */
                    sb_cstr(sb, ",\"difference\":{\"path\":");
                    json_str_into(sb, (Str *)tf_diff_path);
                    sb_cstr(sb, ",\"expected\":");
                    if (tf_diff_exp) json_str_into(sb, (Str *)tf_diff_exp);
                    else sb_cstr(sb, "null");
                    sb_cstr(sb, ",\"actual\":");
                    if (tf_diff_act) json_str_into(sb, (Str *)tf_diff_act);
                    else sb_cstr(sb, "null");
                    if (tf_diff_char) {
                        sb_cstr(sb, ",\"character\":");
                        int_to_dec(sb, tf_diff_char);
                    }
                    sb_byte(sb, '}');
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
        if (cap_len || cap_dropped) { /* spec 018: what the test printed */
            sb_cstr(sb, ",\"output\":\"");
            json_escape_into(sb, cap_buf, cap_len);
            cap_note(sb, 1);
            sb_byte(sb, '"');
        }
        sb_cstr(sb, "}\n");
    } else { /* human line: STATUS name (Nms) + failure details */
        sb_cstr(sb, (char *)status_upper[status]);
        sb_str(sb, test->a.s);
        sb_cstr(sb, " (");
        if (test->flags & NF_PROP) prop_cases_text(sb);
        int_to_dec(sb, ms);
        sb_cstr(sb, "ms");
        if (rt_mode) { /* spec 054: recorded or replayed */
            sb_cstr(sb, ", ");
            sb_str(sb, rt_mode);
        }
        sb_byte(sb, ')');
        sp_report(sb, status); /* spec 012: the IDs of a test that does not pass */
        sb_byte(sb, '\n');
        for (long k = 0; rt_notes && k < rt_notes->n; k++) { /* spec 054 */
            sb_cstr(sb, "  note: ");
            sb_str(sb, vec_get(rt_notes, k));
            sb_byte(sb, '\n');
        }
        if (status && at) {
            sb_cstr(sb, "  ");
            human_loc(sb, (int)at->file, at->start);
            if (status == ST_FAIL) {
                sb_byte(sb, ':');
                sb_byte(sb, ' ');
                sb_str(sb, (Str *)tf_msg);
                sb_byte(sb, '\n');
                long width = 0; /* spec 018: operand lines, values aligned */
                for (long k = 0; k < tf_nops; k++)
                    if (((Str *)tf_op_text[k])->len > width) width = ((Str *)tf_op_text[k])->len;
                for (long k = 0; k < tf_nops; k++) {
                    Str *t = (Str *)tf_op_text[k];
                    sb_cstr(sb, "    ");
                    sb_str(sb, t);
                    sb_byte(sb, ':');
                    for (long p = t->len; p <= width; p++) sb_byte(sb, ' ');
                    sb_str(sb, (Str *)tf_op_val[k]);
                    sb_byte(sb, '\n');
                }
                if (tf_expected) {
                    sb_cstr(sb, "    expected: ");
                    sb_str(sb, (Str *)tf_expected);
                    sb_byte(sb, '\n');
                    sb_cstr(sb, "    actual:   ");
                    sb_str(sb, (Str *)tf_actual);
                    sb_byte(sb, '\n');
                }
                if (tf_diff_path) { /* spec 018: the first difference */
                    sb_cstr(sb, "    first difference at ");
                    if (tf_diff_char) {
                        sb_cstr(sb, "character ");
                        int_to_dec(sb, tf_diff_char);
                    } else {
                        sb_str(sb, (Str *)tf_diff_path);
                    }
                    sb_cstr(sb, ": expected ");
                    if (tf_diff_exp) sb_str(sb, (Str *)tf_diff_exp);
                    else sb_cstr(sb, "(none)");
                    sb_cstr(sb, ", actual ");
                    if (tf_diff_act) sb_str(sb, (Str *)tf_diff_act);
                    else sb_cstr(sb, "(none)");
                    if (tf_diff_len_a >= 0) {
                        sb_cstr(sb, " (lengths: actual ");
                        int_to_dec(sb, tf_diff_len_a);
                        sb_cstr(sb, ", expected ");
                        int_to_dec(sb, tf_diff_len_e);
                        sb_byte(sb, ')');
                    }
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
        if (status && (cap_len || cap_dropped)) { /* spec 018: a failing test's output, indented */
            sb_cstr(sb, "  output:\n");
            for (long i = 0; i < cap_len;) {
                long j = i;
                while (j < cap_len && cap_buf[j] != '\n') j++;
                sb_cstr(sb, "    ");
                sb_bytes(sb, cap_buf + i, j - i);
                sb_byte(sb, '\n');
                i = j + 1;
            }
            if (cap_dropped) {
                sb_cstr(sb, "    ");
                cap_note(sb, 0);
                sb_byte(sb, '\n');
            }
        }
    }
    out_bytes(1, sb->ptr, sb->len);
    GC_UNROOT(1);
}

/* Runs one test or property (items[index] of its file) with its own heap, recovery point and deadline;
 * its status (ST_*). The report reads the tf_* and rt_err_* globals it leaves. */
long run_one_test(Node *test_in, long index_in)
{
    long jb[JB_SIZE / 8]; /* the recovery buffer */
    Node *volatile test = test_in;
    volatile long i = index_in;
    {
        gc_start(); /* every test starts with an empty heap (spec 003 FR-009) */
        cap_len = cap_dropped = 0; /* spec 018: its output is kept for its report */
        cap_on = 1;
        tf_node = 0;
        rt_err_code = 0;
        rt_err_node = 0;
        rt_err_in_test = 0;
        pr_starved = 0; /* (a property sets it again; a test after a starved property must not see it) */
        rt_begin(test); /* spec 054: its recording, started or loaded */
        volatile long start = sys_clock_ms();
        deadline_ms = 0; /* a deadline per test */
        if (opt_timeout) deadline_ms = opt_timeout * 1000 + start;
        long status; /* 0 pass, 1 expect failed, 2 runtime error */
        if (test->flags & NF_PROP) { /* a property runs its own cases (prop.s) */
            status = prop_run(test, i);
        } else {
            volatile long sql_mark = sql_serial(); /* spec 026: the databases this test opens */
            recover_point = (long)jb;
            gc_mark_on_raise = 1;
            status = jmp_save(jb);
            if (!status) {
                rt_effects_start(1); /* every test starts from seed and clock defaults */
                rt_log_reset(); /* spec 055: and with no records, request id or metrics */
                rt_metrics_reset();
                run_test_body(test);
                workers_end(1); /* spec 047: a worker's failure the test did not receive fails it */
                status = ST_PASS;
            }
            sql_close_since(sql_mark); /* closed however it ended: the next test's heap starts empty */
        }
        proc_sweep(); /* spec 040: a child a failed test was following */
        workers_end(0); /* spec 047: no worker outlives its test */
        gc_mark_on_raise = 0;
        recover_point = 0;
        deadline_ms = 0;
        cap_on = 0;
        interp_reset();
        if (status == ST_ERROR && rt_err_code == (long)r0009) status = ST_TIMEOUT; /* R0009 is a timeout */
        rt_end(status != ST_PASS, status == ST_TIMEOUT); /* spec 054 */
        return status;
    }
}

/* Runs the file's selected tests in source order and reports each. */
void run_file_tests(int file)
{
    Vec *items = file_get(file)->ast->a.v;
    sp_collect(items);
    sp_items = items;
    for (long i = 0; i < items->n; i++) {
        Node *test = vec_get(items, i);
        if (test->kind != N_TEST) continue;
        if (!test_selected(test, items, i)) continue;
        cov_test = t_selected++; /* spec 018: its index, for --mutate's reach */
        long start = sys_clock_ms();
        long status = run_one_test(test, i);
        if (opt_mutate) mut_note_test(test, i, sys_clock_ms() - start);
        if (status == ST_PASS)
            t_passed++;
        else if (status == ST_FAIL)
            t_failed++;
        else if (status == ST_ERROR)
            t_errored++;
        else
            t_timed_out++;
        sp_index = i;
        gc_report_mark(); /* the test's memory, not its report's */
        report_test(test, (int)status, sys_clock_ms() - start);
        gc_report(1, "test", test->a.s); /* --mem-stats: right after the test's record */
        /* the test's leftovers point into a heap about to be reset; they must not keep anything
           alive in the next test (spec 003) */
        tf_msg = 0;
        tf_expected = 0;
        tf_actual = 0;
        tf_nops = 0;
        tf_diff_path = tf_diff_exp = tf_diff_act = tf_diff_char = 0;
        tf_op_text[0] = tf_op_text[1] = tf_op_val[0] = tf_op_val[1] = 0;
        rt_err_msg = 0;
        rt_err_extra = 0;
        rt_mode = 0; /* spec 054 */
        rt_notes = 0;
        ret_val = 0;
        pr_args = 0;
        pr_best = 0;
        pr_cand = 0;
    }
}

/* ---- spec 018: the coverage report (research R8) ---- */

static const char *const cov_words[] = {"function", "lambda", "then", "else", "if_false", "case", "loop"};

static int cov_before(long i, long j)
{
    Node *a = cov_node[i], *b = cov_node[j];
    if (a->file != b->file) return a->file < b->file;
    if (a->start != b->start) return a->start < b->start;
    return cov_kind[i] < cov_kind[j];
}

/* The points no selected test reached, in file and source order, then the totals. */
static void cov_report(SB *sb)
{
    long *order = perm_alloc((cov_n ? cov_n : 1) * 8), *tmp = perm_alloc((cov_n ? cov_n : 1) * 8); /* (once per run) */
    for (long i = 0; i < cov_n; i++) order[i] = i;
    for (long w = 1; w < cov_n; w *= 2) /* merge sort: stable and n log n */
        for (long lo = 0; lo < cov_n; lo += 2 * w) {
            long mid = lo + w < cov_n ? lo + w : cov_n, hi = lo + 2 * w < cov_n ? lo + 2 * w : cov_n;
            long i = lo, j = mid, k = lo;
            while (i < mid || j < hi) tmp[k++] = (j >= hi || (i < mid && !cov_before(order[j], order[i]))) ? order[i++] : order[j++];
            for (k = lo; k < hi; k++) order[k] = tmp[k];
        }
    long fns = 0, fns_run = 0, brs = 0, brs_run = 0;
    for (long n = 0; n < cov_n; n++) {
        long k = order[n];
        int fn = cov_kind[k] == COV_FUNCTION || cov_kind[k] == COV_LAMBDA;
        fns += fn;
        brs += !fn;
        if (cov_hit[k]) {
            fns_run += fn;
            brs_run += !fn;
            continue;
        }
        Node *at = cov_kind[k] == COV_IF_FALSE ? cov_node[k] : cov_node[k];
        Str *name = cov_owner[k]->a.n->a.s;
        if (opt_json) {
            sb_cstr(sb, "{\"v\":1,\"kind\":\"coverage\",\"file\":");
            json_str_into(sb, file_get((int)at->file)->path);
            sb_cstr(sb, ",\"start\":");
            json_pos(sb, (int)at->file, at->start);
            sb_cstr(sb, ",\"end\":");
            json_pos(sb, (int)at->file, at->end);
            sb_cstr(sb, ",\"point\":\"");
            sb_cstr(sb, (char *)cov_words[(int)cov_kind[k]]);
            sb_cstr(sb, "\",\"function\":");
            json_str_into(sb, name);
            sb_cstr(sb, "}\n");
        } else {
            sb_cstr(sb, "not run: ");
            human_loc(sb, (int)at->file, at->start);
            sb_byte(sb, ' ');
            sb_cstr(sb, (char *)cov_words[(int)cov_kind[k]]);
            sb_cstr(sb, cov_kind[k] == COV_FUNCTION ? " " : " in ");
            sb_str(sb, name);
            sb_byte(sb, '\n');
        }
    }
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"coverage_summary\",\"functions\":");
        int_to_dec(sb, fns);
        sb_cstr(sb, ",\"functions_run\":");
        int_to_dec(sb, fns_run);
        sb_cstr(sb, ",\"branches\":");
        int_to_dec(sb, brs);
        sb_cstr(sb, ",\"branches_run\":");
        int_to_dec(sb, brs_run);
        sb_cstr(sb, "}\n");
    } else {
        sb_cstr(sb, "coverage: ");
        int_to_dec(sb, fns_run);
        sb_cstr(sb, " of ");
        int_to_dec(sb, fns);
        sb_cstr(sb, fns == 1 ? " function, " : " functions, ");
        int_to_dec(sb, brs_run);
        sb_cstr(sb, " of ");
        int_to_dec(sb, brs);
        sb_cstr(sb, brs == 1 ? " branch\n" : " branches\n");
    }
}

void test_summary(long files)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sp_summary(sb); /* spec 012: a line per requirement ID, before the summary */
    if (opt_coverage) cov_report(sb); /* spec 018 */
    if (opt_mutate && !(t_failed + t_errored + t_timed_out)) mut_report(sb);
    if (sb->len) {
        out_bytes(1, sb->ptr, sb->len);
        sb->len = 0;
    }
    Vec *unused = rt_unused(); /* spec 054: recordings that name no test of the program */
    GC_ROOT(&unused);
    if (opt_json) { /* JSON: the common summary plus the test counts */
        sb_cstr(sb, ",\"passed\":");
        int_to_dec(sb, t_passed);
        sb_cstr(sb, ",\"failed\":");
        int_to_dec(sb, t_failed);
        sb_cstr(sb, ",\"errored\":");
        int_to_dec(sb, t_errored);
        sb_cstr(sb, ",\"timed_out\":");
        int_to_dec(sb, t_timed_out);
        sb_cstr(sb, ",\"selected\":"); /* spec 018 */
        int_to_dec(sb, t_selected);
        if (unused) {
            sb_cstr(sb, ",\"unused_recordings\":[");
            for (long k = 0; k < unused->n; k++) {
                if (k) sb_byte(sb, ',');
                json_str_into(sb, vec_get(unused, k));
            }
            sb_byte(sb, ']');
        }
        GC_UNROOT(2);
        summary_emit(1, 1, "test", files, sb_finish(sb));
        return;
    }
    if (!t_selected && (opt_tests || opt_specs || sel_diff || opt_filter)) { /* spec 018: an empty selection */
        sb_cstr(sb, "0 tests selected\n");
        out_bytes(1, sb->ptr, sb->len);
        GC_UNROOT(2);
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
    if (unused && unused->n) { /* spec 054 */
        sb_cstr(sb, "recordings with no test: ");
        for (long k = 0; k < unused->n; k++) {
            if (k) sb_cstr(sb, ", ");
            sb_str(sb, vec_get(unused, k));
        }
        sb_byte(sb, '\n');
    }
    out_bytes(1, sb->ptr, sb->len);
    GC_UNROOT(2);
}
