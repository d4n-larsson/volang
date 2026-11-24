/* Spec 055: metrics (std.metrics) in one registry per program, in memory mapped shared, so worker processes
 * forked after it is mapped (serve_workers maps it first) update the same series and any worker's /metrics
 * answer shows the totals (research R7). Updates take a lock word holding the owner's pid; a waiter takes over
 * a lock whose owner is gone (a dead worker is replaced). Nothing is allocated while the lock is held: a scrape
 * copies the registry, then renders the copy.
 * The key std/metrics.vo passes: KIND US NAME US HELP US LABELS US BUCKETS US VALUES (US: byte 31; labels and
 * buckets (float text) joined with byte 30; each value after a byte 30). For op 4, the listed metrics' keys (without VALUES), joined
 * with byte 29. */
#include "vo.h"

#define MX_METRICS 256
#define MX_SERIES 2048 /* ponytail: a fixed table scanned in order; a hash index if services need far more */
#define MX_BUCKETS 32
#define MX_NAME 128
#define MX_HELP 512
#define MX_LABELS 256
#define MX_VALUES 512

typedef struct MxMetric {
    char name[MX_NAME], help[MX_HELP], labels[MX_LABELS];
    long name_len, help_len, labels_len, nlabels, kind; /* kind 1 counter, 2 gauge, 3 histogram */
    long nb;
    double b[MX_BUCKETS];
} MxMetric;

typedef struct MxSeries {
    long metric, hash, values_len;
    char values[MX_VALUES];
    long v; /* counter, gauge */
    long count;
    double sum;
    long counts[MX_BUCKETS + 1]; /* per bucket (not cumulative), the last +Inf */
} MxSeries;

typedef struct Mx {
    long lock, nmetrics, nseries, dropped;
    MxMetric m[MX_METRICS];
    MxSeries s[MX_SERIES];
} Mx;

static Mx *mx;
static Mx *mx_copy; /* private: what a scrape renders */

static char *const kinds[] = {0, "counter", "gauge", "histogram"};

/* The registry, mapped on first use; R0006 when the system refuses. */
static Mx *mx_get(Node *call)
{
    if (!mx) {
        mx = map_shared_try(sizeof(Mx), 1);
        if (mx) mx_copy = map_shared_try(sizeof(Mx), 0);
        if (!mx || !mx_copy) rt_raise("R0006", str_from_cstr("Out of memory: the system refused the shared memory for metrics."), std_user_call(call));
    }
    return mx;
}

void rt_metrics_share(Node *call)
{
    mx_get(call);
}

/* Before each test: no metrics. */
void rt_metrics_reset(void)
{
    if (mx) mx->nmetrics = mx->nseries = mx->dropped = 0;
}

static void mx_lock(void)
{
    if (!sys_pid_cache) sys_pid_cache = sys_getpid();
    for (long n = 1;; n++) {
        long cur = 0;
        if (__atomic_compare_exchange_n(&mx->lock, &cur, sys_pid_cache, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return;
        if (!(n & 4095) && !sys_alive(cur)) /* its owner died holding it */
            __atomic_compare_exchange_n(&mx->lock, &cur, 0, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);
    }
}

static void mx_unlock(void)
{
    __atomic_store_n(&mx->lock, 0, __ATOMIC_RELEASE);
}

/* ---- the key ---- */

typedef struct Part {
    char *p;
    long n;
} Part;

/* Splits [p, p+n) at byte sep into at most max parts; the number of parts. */
static long split_at(char *p, long n, int sep, Part *out, long max)
{
    long k = 0, start = 0;
    for (long i = 0; i <= n && k < max; i++)
        if (i == n || p[i] == sep) {
            out[k].p = p + start;
            out[k++].n = i - start;
            start = i + 1;
        }
    return k;
}

static long count_parts(Part list)
{
    if (!list.n) return 0;
    long k = 1;
    for (long i = 0; i < list.n; i++) k += list.p[i] == 30;
    return k;
}

static int part_eq(char *a, long an, Part b)
{
    return an == b.n && mem_eq(a, b.p, an);
}

static int why; /* set under the lock with why_buf, the message of an R0016; raised after unlocking */
static char why_buf[512];

static void conflict(char *what, Part name)
{
    long n = 0;
    for (char *q = "The metric `"; *q && n < 400; q++) why_buf[n++] = *q;
    for (long i = 0; i < name.n && n < 400; i++) why_buf[n++] = name.p[i];
    for (char *q = "` "; *q; q++) why_buf[n++] = *q;
    for (char *q = what; *q && n < 510; q++) why_buf[n++] = *q;
    why_buf[n] = 0;
    why = 1;
}

/* The buckets of a key (float text joined with byte 30), parsed before the lock (parsing allocates). */
static long buckets_of(Part text, double *out)
{
    Part bs[MX_BUCKETS];
    long nb = text.n ? split_at(text.p, text.n, 30, bs, MX_BUCKETS) : 0;
    for (long i = 0; i < nb; i++) {
        long bits = std_text_float(str_new(bs[i].p, bs[i].n));
        mem_copy((char *)&out[i], (char *)&bits, 8);
    }
    return nb;
}

/* The metric of the key's definition (registered on first use), or -1 with an R0016 reason. */
static long mx_metric(Part *f, double *b, long nb)
{
    long kind = f[0].p[0] - '0';
    for (long i = 0; i < mx->nmetrics; i++) {
        MxMetric *m = &mx->m[i];
        if (!part_eq(m->name, m->name_len, f[1])) continue;
        if (m->kind != kind) conflict("is already used as another kind of metric.", f[1]);
        else if (!part_eq(m->help, m->help_len, f[2])) conflict("is already used with another help text.", f[1]);
        else if (!part_eq(m->labels, m->labels_len, f[3])) conflict("is already used with other labels.", f[1]);
        else return i;
        return -1;
    }
    if (f[1].n > MX_NAME || f[2].n > MX_HELP || f[3].n > MX_LABELS) {
        conflict("has a name, help text or labels too long for the registry.", f[1]);
        return -1;
    }
    if (mx->nmetrics == MX_METRICS) {
        conflict("is one metric too many: the registry holds 256.", f[1]);
        return -1;
    }
    MxMetric *m = &mx->m[mx->nmetrics];
    mem_copy(m->name, f[1].p, m->name_len = f[1].n);
    mem_copy(m->help, f[2].p, m->help_len = f[2].n);
    mem_copy(m->labels, f[3].p, m->labels_len = f[3].n);
    m->nlabels = count_parts(f[3]);
    m->kind = kind;
    m->nb = nb;
    for (long i = 0; i < nb; i++) m->b[i] = b[i];
    return mx->nmetrics++;
}

static long hash_of(long metric, Part v)
{
    unsigned long h = 1469598103934665603UL ^ (unsigned long)metric;
    for (long i = 0; i < v.n; i++) h = (h ^ (unsigned char)v.p[i]) * 1099511628211UL;
    return (long)h;
}

/* The series of metric m with these label values, created when new; 0 when the table is full (dropped). */
static MxSeries *mx_series(long m, Part values)
{
    long h = hash_of(m, values);
    for (long i = 0; i < mx->nseries; i++) {
        MxSeries *s = &mx->s[i];
        if (s->hash == h && s->metric == m && part_eq(s->values, s->values_len, values)) return s;
    }
    if (mx->nseries == MX_SERIES || values.n > MX_VALUES) {
        mx->dropped++;
        return 0;
    }
    MxSeries *s = &mx->s[mx->nseries++];
    mem_copy((char *)s, (char *)&(MxSeries){0}, sizeof *s);
    s->metric = m;
    s->hash = h;
    mem_copy(s->values, values.p, s->values_len = values.n);
    return s;
}

/* Ops 1 add (x: a whole amount), 2 set, 3 observe. */
static void mx_update(long op, Part *f, double x, Node *call)
{
    why = 0;
    double b[MX_BUCKETS];
    long nb = buckets_of(f[4], b);
    mx_lock();
    long mi = mx_metric(f, b, nb);
    if (mi >= 0) {
        MxMetric *m = &mx->m[mi];
        long nv = 0;
        for (long i = 0; i < f[5].n; i++) nv += f[5].p[i] == 30;
        if (nv != m->nlabels)
            conflict("takes one value for each of its labels.", f[1]);
        else if (op == 1 && m->kind == 1 && x < 0)
            conflict("is a counter: it never goes down.", f[1]);
        else {
            MxSeries *s = mx_series(mi, f[5]);
            if (s && op == 3) {
                long k = 0;
                while (k < m->nb && x > m->b[k]) k++;
                s->counts[k]++;
                s->count++;
                s->sum += x;
            } else if (s) {
                s->v = op == 1 ? s->v + (long)x : (long)x;
            }
        }
    }
    mx_unlock();
    if (why) rt_raise("R0016", str_from_cstr(why_buf), std_user_call(call));
}

/* ---- the text exposition format 0.0.4 ---- */

static void escaped(SB *sb, char *p, long n, int quote)
{
    for (long i = 0; i < n; i++) {
        if (p[i] == '\\') sb_cstr(sb, "\\\\");
        else if (p[i] == '\n') sb_cstr(sb, "\\n");
        else if (quote && p[i] == '"') sb_cstr(sb, "\\\"");
        else sb_byte(sb, p[i]);
    }
}

static void number(SB *sb, double x)
{
    long bits;
    mem_copy((char *)&bits, (char *)&x, 8);
    Str *t = float_to_str(bits);
    long n = t->len;
    if (n > 2 && t->data[n - 2] == '.' && t->data[n - 1] == '0') n -= 2; /* `1`, as scrapers write it */
    sb_bytes(sb, t->data, n);
}

/* `{a="x",b="y"` plus le when given, then `}`; nothing for no labels and no le. */
static void labels_into(SB *sb, MxMetric *m, MxSeries *s, double *le)
{
    Part names[64], vals[64];
    long nn = m->labels_len ? split_at(m->labels, m->labels_len, 30, names, 64) : 0;
    long nv = s && s->values_len ? split_at(s->values + 1, s->values_len - 1, 30, vals, 64) : 0;
    if (!nn && !le) return;
    sb_byte(sb, '{');
    for (long i = 0; i < nn; i++) {
        if (i) sb_byte(sb, ',');
        sb_bytes(sb, names[i].p, names[i].n);
        sb_cstr(sb, "=\"");
        if (i < nv) escaped(sb, vals[i].p, vals[i].n, 1);
        sb_byte(sb, '"');
    }
    if (le) {
        if (nn) sb_byte(sb, ',');
        sb_cstr(sb, "le=\"");
        if (*le == 1.0 / 0.0) sb_cstr(sb, "+Inf");
        else number(sb, *le);
        sb_byte(sb, '"');
    }
    sb_byte(sb, '}');
}

static int series_before(Mx *r, long a, long b)
{
    MxSeries *x = &r->s[a], *y = &r->s[b];
    long n = x->values_len < y->values_len ? x->values_len : y->values_len;
    for (long i = 0; i < n; i++)
        if (x->values[i] != y->values[i]) return (unsigned char)x->values[i] < (unsigned char)y->values[i];
    return x->values_len < y->values_len;
}

static int metric_before(Mx *r, long a, long b)
{
    MxMetric *x = &r->m[a], *y = &r->m[b];
    long n = x->name_len < y->name_len ? x->name_len : y->name_len;
    for (long i = 0; i < n; i++)
        if (x->name[i] != y->name[i]) return (unsigned char)x->name[i] < (unsigned char)y->name[i];
    return x->name_len < y->name_len;
}

static void long_into(SB *sb, long v)
{
    char d[24];
    int k = 0;
    unsigned long u = v < 0 ? -(unsigned long)v : (unsigned long)v;
    do d[k++] = (char)('0' + u % 10); while (u /= 10);
    if (v < 0) sb_byte(sb, '-');
    while (k) sb_byte(sb, d[--k]);
}

static Str *render(Mx *r)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    long order[MX_METRICS], idx[MX_SERIES];
    for (long i = 0; i < r->nmetrics; i++) { /* by name, then each metric's series by label values */
        long j = i;
        while (j && metric_before(r, i, order[j - 1])) order[j] = order[j - 1], j--;
        order[j] = i;
    }
    for (long oi = 0; oi < r->nmetrics; oi++) {
        long mi = order[oi];
        MxMetric *m = &r->m[mi];
        sb_cstr(sb, "# HELP ");
        sb_bytes(sb, m->name, m->name_len);
        sb_byte(sb, ' ');
        escaped(sb, m->help, m->help_len, 0);
        sb_cstr(sb, "\n# TYPE ");
        sb_bytes(sb, m->name, m->name_len);
        sb_byte(sb, ' ');
        sb_cstr(sb, kinds[m->kind]);
        sb_byte(sb, '\n');
        long n = 0;
        for (long i = 0; i < r->nseries; i++) {
            if (r->s[i].metric != mi) continue;
            long j = n++;
            while (j && series_before(r, i, idx[j - 1])) idx[j] = idx[j - 1], j--;
            idx[j] = i;
        }
        MxSeries none = {0};
        if (!n && !m->nlabels) idx[n++] = -1; /* listed, no updates yet: 0 */
        for (long k = 0; k < n; k++) {
            MxSeries *s = idx[k] < 0 ? &none : &r->s[idx[k]];
            if (m->kind != 3) {
                sb_bytes(sb, m->name, m->name_len);
                labels_into(sb, m, s, 0);
                sb_byte(sb, ' ');
                long_into(sb, s->v);
                sb_byte(sb, '\n');
                continue;
            }
            long cum = 0;
            for (long b = 0; b <= m->nb; b++) {
                double le = b < m->nb ? m->b[b] : 1.0 / 0.0;
                cum += s->counts[b];
                sb_bytes(sb, m->name, m->name_len);
                sb_cstr(sb, "_bucket");
                labels_into(sb, m, s, &le);
                sb_byte(sb, ' ');
                long_into(sb, cum);
                sb_byte(sb, '\n');
            }
            sb_bytes(sb, m->name, m->name_len);
            sb_cstr(sb, "_sum");
            labels_into(sb, m, s, 0);
            sb_byte(sb, ' ');
            number(sb, s->sum);
            sb_byte(sb, '\n');
            sb_bytes(sb, m->name, m->name_len);
            sb_cstr(sb, "_count");
            labels_into(sb, m, s, 0);
            sb_byte(sb, ' ');
            long_into(sb, s->count);
            sb_byte(sb, '\n');
        }
    }
    if (r->dropped) {
        sb_cstr(sb, "# HELP vo_metrics_dropped_series_total Series not kept: the registry holds 2048.\n"
                    "# TYPE vo_metrics_dropped_series_total counter\nvo_metrics_dropped_series_total ");
        long_into(sb, r->dropped);
        sb_byte(sb, '\n');
    }
    GC_UNROOT(1);
    return sb_finish(sb);
}

/* Op 4: registers the listed metrics, then the registry as text (rendered from a private copy, outside the
 * lock). */
static Str *mx_text(Str *key, Node *call)
{
    Part defs[MX_METRICS];
    long nd = key->len ? split_at(key->data, key->len, 29, defs, MX_METRICS) : 0;
    static double b[MX_METRICS][MX_BUCKETS]; /* (64 KiB) */
    long nb[MX_METRICS];
    for (long i = 0; i < nd; i++) {
        Part f[6] = {{0}};
        nb[i] = split_at(defs[i].p, defs[i].n, 31, f, 6) >= 5 ? buckets_of(f[4], b[i]) : -1;
    }
    why = 0;
    mx_lock();
    for (long i = 0; i < nd && !why; i++) {
        Part f[6] = {{0}};
        split_at(defs[i].p, defs[i].n, 31, f, 6);
        if (nb[i] >= 0) mx_metric(f, b[i], nb[i]);
    }
    Mx *r = mx_copy;
    mem_copy((char *)r, (char *)mx, (long)((char *)&mx->m[mx->nmetrics] - (char *)mx));
    mem_copy((char *)r->s, (char *)mx->s, mx->nseries * (long)sizeof(MxSeries));
    mx_unlock();
    if (why) rt_raise("R0016", str_from_cstr(why_buf), std_user_call(call));
    return render(r);
}

/* __metric(op, key, x) (std.metrics only): 1 add, 2 set, 3 observe, 4 the registry as text. */
Str *rt_metric(long op, Str *key, long xbits, Node *call)
{
    GC_ROOT(&key);
    mx_get(call);
    Str *r;
    if (op == 4) {
        r = mx_text(key, call);
    } else {
        Part f[6] = {{0}};
        long n = split_at(key->data, key->len, 31, f, 6);
        if (n == 6) {
            double x;
            mem_copy((char *)&x, (char *)&xbits, 8);
            mx_update(op, f, x, call);
        }
        r = str_from_cstr("");
    }
    GC_UNROOT(1);
    return r;
}
