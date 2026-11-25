/* Checker, spec 055: calls of std.log and std.metrics.
 * - W0506 secret_logged: in a log_debug/info/warn/error call, a `field(NAME, VALUE)` of a list literal whose
 *   VALUE is a variable, parameter or field access with a secret-like last name (src/rt_log.c's rule, without
 *   redact_logs words): the warning at VALUE, and a fix removing that field.
 * - E0341 bad_metric_name: literal metric and label names of counter/gauge/histogram against the text
 *   format's rules; a fix replacing each disallowed character with `_` (none for a `__` label or a repeat). */
#include "vo.h"

static char *const log_fns[] = {"log_debug", "log_info", "log_warn", "log_error", 0};
static char *const metric_fns[] = {"counter", "gauge", "histogram", 0};

/* Whether decl is one of names in std/NAME.vo (file the last part of its path, e.g. "log.vo"). */
static int std_fn(Node *decl, char *file, char *const *names)
{
    if (!decl || decl->kind != N_FN) return 0;
    FileRec *f = file_get((int)decl->file);
    long n = cstr_len(file);
    if (!f->std || f->path->len < n + 1 || !mem_eq(f->path->data + f->path->len - n - 1, "/", 1) ||
        !mem_eq(f->path->data + f->path->len - n, file, n))
        return 0;
    for (int i = 0; names[i]; i++)
        if (str_eq_cstr(decl->a.n->a.s, names[i])) return 1;
    return 0;
}

/* The last name of a variable or field access, or 0. */
static Str *last_name(Node *v)
{
    if (v->kind == N_IDENT) return v->a.s;
    if (v->kind == N_FIELD) return v->b.n->a.s;
    return 0;
}

static void secret_logged(Node *call)
{
    Node *list = vec_get(call->b.v, 1);
    if (list->kind != N_LIST) return;
    Vec *xs = list->a.v;
    for (long i = 0; i < xs->n; i++) {
        Node *f = vec_get(xs, i);
        if (f->kind != N_CALL || f->b.v->n != 2 || !std_fn(f->c.n, "log.vo", (char *const[]){"field", 0})) continue;
        Node *v = vec_get(f->b.v, 1);
        Str *name = last_name(v);
        if (!name || !secret_like(name->data, name->len, 0) || allow_takes("W0506", ck_item)) continue;
        Diag *d = ck_warn("W0506", v, msg("`$1` looks like a secret: it is written as [redacted] at best; leave it out of the log record.", src_text(v), 0, 0));
        if (xs->n > 1) { /* the field and the comma after it (before it, for the last) */
            Node *next = i + 1 < xs->n ? vec_get(xs, i + 1) : 0;
            Node *prev = i ? vec_get(xs, i - 1) : 0;
            diag_add_fix(d, (int)f->file, next ? f->start : prev->end, next ? next->start : f->end, str_from_cstr(""));
        } else { /* the only field: no fields (`[]` takes its type, list[Field], from the parameter) */
            diag_add_fix(d, (int)list->file, list->start, list->end, str_from_cstr("[]"));
        }
    }
}

static int name_char(int c, int first, int colon)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (colon && c == ':') || (!first && c >= '0' && c <= '9');
}

/* E0341 on a literal name; colon: metric names may have `:`. 1 when it was reported. */
static int bad_name(Node *lit, int colon, char *what)
{
    Str *s = lit->a.s;
    long bad = -1;
    for (long i = 0; i < s->len && bad < 0; i++)
        if (!name_char((unsigned char)s->data[i], i == 0, colon)) bad = i;
    if (bad < 0 && s->len) return 0;
    long at = lit->start + 1 + (bad < 0 ? 0 : bad); /* (names hold no escapes: offsets map to source bytes) */
    Diag *d = diag_add("E0341", SEV_ERROR, (int)lit->file, at, bad < 0 ? at : at + utf8_seq_len(s->data + bad, s->len - bad),
                       msg(colon ? "`$1` is not a metric name: use letters, digits, `_` and `:`, not starting with a digit."
                                 : "`$1` is not a label name: use letters, digits and `_`, not starting with a digit.",
                           s, 0, 0));
    (void)what;
    if (!s->len) return 1;
    SB *sb = sb_new(); /* each disallowed character (a whole UTF-8 sequence) becomes `_` */
    GC_ROOT(&sb);
    sb_byte(sb, '"');
    for (long i = 0; i < s->len;) {
        long k = utf8_seq_len(s->data + i, s->len - i);
        if (k < 1) k = 1;
        sb_byte(sb, k == 1 && name_char((unsigned char)s->data[i], i == 0, colon) ? s->data[i] : '_');
        i += k;
    }
    sb_byte(sb, '"');
    diag_add_fix(d, (int)lit->file, lit->start, lit->end, sb_finish(sb));
    GC_UNROOT(1);
    return 1;
}

static void metric_names(Node *call)
{
    Node *name = vec_get(call->b.v, 0);
    if (name->kind == N_STR) bad_name(name, 1, "metric");
    Node *labels = vec_get(call->b.v, 2);
    if (labels->kind != N_LIST) return;
    Vec *xs = labels->a.v;
    for (long i = 0; i < xs->n; i++) {
        Node *l = vec_get(xs, i);
        if (l->kind != N_STR || bad_name(l, 0, "label")) continue;
        if (l->a.s->len >= 2 && l->a.s->data[0] == '_' && l->a.s->data[1] == '_') {
            ck_err1("E0341", l, "`$1` is not a label name: names starting with `__` are reserved.", l->a.s);
            continue;
        }
        for (long j = 0; j < i; j++) {
            Node *m = vec_get(xs, j);
            if (m->kind == N_STR && str_eq(m->a.s, l->a.s)) {
                ck_err1("E0341", l, "The label `$1` is already in this list.", l->a.s);
                break;
            }
        }
    }
}

void check_ops_call(Node *call)
{
    if (call->b.v->n == 2 && std_fn(call->c.n, "log.vo", log_fns)) secret_logged(call);
    if (call->b.v->n >= 3 && std_fn(call->c.n, "metrics.vo", metric_fns)) metric_names(call);
}
