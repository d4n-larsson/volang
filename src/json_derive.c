/* Derived JSON conversion, spec 029 (research R2, contracts/json.md): `__to_json(v)` and
 * `__from_json(j, mode)` walk the concrete type of the value (as src/task.c's transfer form does) and
 * build or read std.json's `Json` values. Decoding is strict and collects every problem with its path;
 * record fields' annotations (src/check_annot.c) give limits and defaults.
 * mode bit 1: texts are accepted for scalars (and lists of scalars) in the top-level record's fields
 * other than `body` (std.web's path, query and header values; settings from the environment).
 * mode bit 2: problems come back one per line as `L|S \t path \t reason` (L: a limit, S: the shape),
 * for std.web's problem bodies; otherwise as `path: reason` joined by `; `.
 * mode bit 4: std.web's request form (research R7): j is {"path": {name: text}, "query": {name: [text]},
 * "headers": {lower-case name: text}, "body": any (when there is one)} and the record's fields take a
 * path variable of their name, the body (`body`), a header (`@header`) or the query parameter of their
 * name; unknown query parameters and headers are ignored. */
#include "vo.h"

enum { J_NULL, J_BOOL, J_INT, J_NUM, J_TEXT, J_ARRAY, J_OBJECT }; /* std.json's Json cases, in order */

#define JD_DEPTH 512

static long jv(long tag, long pay)
{
    return (long)pg_new_variant(tag, pay);
}

static double as_dbl(long bits)
{
    union {
        long i;
        double d;
    } u = {bits};
    return u.d;
}

static long as_bits(double d)
{
    union {
        double d;
        long i;
    } u = {d};
    return u.i;
}

/* The name of member i of a record or variant type. */
static Str *member_name(Type *t, long i)
{
    return member_at(t->decl.n->b.v, i)->a.n->a.s;
}

/* Whether t is std.json's own Json type: converted as itself. */
static int is_json_type(Type *t)
{
    if (t->kind != TY_VARIANT || !str_eq_cstr(t->decl.n->a.n->a.s, "Json")) return 0;
    FileRec *f = file_get((int)t->decl.n->file);
    return f->std && f->path->len >= 7 && mem_eq(f->path->data + f->path->len - 7, "json.vo", 7);
}

/* ---- to JSON ---- */

static long to_j(long v, Type *t, Node *call)
{
    check_stack(call); /* a value nested very deeply (or a cycle) is R0005, as for == */
    t = rt_concrete(t);
    if (is_json_type(t)) return v; /* a Json value is its own JSON form */
    GC_ROOT(&v);
    long r = 0;
    switch (t->kind) {
    case TY_INT: r = jv(J_INT, v); break;
    case TY_FLOAT: {
        double d = as_dbl(v);
        r = d != d || d - d != 0 ? jv(J_NULL, 0) : jv(J_NUM, v); /* NaN, infinities: null */
        break;
    }
    case TY_BOOL: r = jv(J_BOOL, v); break;
    case TY_STR: r = jv(J_TEXT, v); break;
    case TY_BYTES: r = jv(J_TEXT, (long)bytes_to_base64((Str *)v)); break;
    case TY_LIST: {
        List *l = (List *)v;
        List *out = list_new(l->len ? l->len : 1);
        GC_ROOT(&out);
        for (long i = 0; i < ((List *)v)->len; i++) {
            long e = to_j(((List *)v)->data[i].i, t->a.t, call);
            list_push(out, e);
        }
        r = jv(J_ARRAY, (long)out);
        GC_UNROOT(1);
        break;
    }
    case TY_MAP: {
        Map *out = map_new();
        GC_ROOT(&out);
        for (long i = 0; i < ((Map *)v)->used; i++) {
            MapEntry *me = &((Map *)v)->entries[i];
            if (!me->live) continue;
            long key = me->key.i;
            GC_ROOT(&key);
            long e = to_j(me->val.i, t->b.t, call);
            map_set(out, key, e, t_str);
            GC_UNROOT(1);
        }
        r = jv(J_OBJECT, (long)out);
        GC_UNROOT(1);
        break;
    }
    case TY_RECORD: {
        Map *out = map_new();
        GC_ROOT(&out);
        long n = pg_nmembers(t);
        for (long i = 0; i < n; i++) {
            long e = to_j(((Record *)v)->fields[i].i, pg_member(t, i), call);
            map_set(out, (long)member_name(t, i), e, t_str);
        }
        r = jv(J_OBJECT, (long)out);
        GC_UNROOT(1);
        break;
    }
    case TY_VARIANT: {
        long tag = (long)(uint32_t)((Variant *)v)->tag;
        Type *pt = pg_member(t, tag);
        if (!pt) {
            r = jv(J_TEXT, (long)member_name(t, tag));
            break;
        }
        Map *out = map_new();
        GC_ROOT(&out);
        long e = to_j(((Variant *)v)->pay.i, pt, call);
        map_set(out, (long)member_name(t, tag), e, t_str);
        r = jv(J_OBJECT, (long)out);
        GC_UNROOT(1);
        break;
    }
    default: rt_raise_c("R0007", "This value has no JSON form.", call); /* (E0328 prevents it) */
    }
    GC_UNROOT(1);
    return r;
}

long json_to(long v, Node *call)
{
    return to_j(v, ((Node *)vec_get(call->b.v, 0))->type, call);
}

/* ---- JSON text (std.web's answers): write_json(to_json(v)) without the Json values in between ---- */

static void jt_quote(SB *sb, Str *s)
{
    static const char hx[] = "0123456789abcdef";
    sb_byte(sb, '"');
    for (long i = 0; i < s->len; i++) { /* the escapes of std.json's quote */
        unsigned char c = (unsigned char)s->data[i];
        if (c == '"') sb_cstr(sb, "\\\"");
        else if (c == '\\') sb_cstr(sb, "\\\\");
        else if (c == '\n') sb_cstr(sb, "\\n");
        else if (c == '\t') sb_cstr(sb, "\\t");
        else if (c == 13) sb_cstr(sb, "\\r");
        else if (c == 8) sb_cstr(sb, "\\b");
        else if (c == 12) sb_cstr(sb, "\\f");
        else if (c < 0x20) {
            sb_cstr(sb, "\\u00");
            sb_byte(sb, hx[c >> 4]);
            sb_byte(sb, hx[c & 15]);
        } else sb_byte(sb, (char)c);
    }
    sb_byte(sb, '"');
}

static void jt_float(SB *sb, long bits)
{
    double d = as_dbl(bits);
    if (d != d || d - d != 0) sb_cstr(sb, "null"); /* (as to_json: NaN and infinities are null) */
    else sb_str(sb, std_float_text(bits));
}

static void jt_json(SB *sb, long j, Node *call);

static void jt_value(SB *sb, long v, Type *t, Node *call)
{
    check_stack(call);
    t = rt_concrete(t);
    if (is_json_type(t)) {
        jt_json(sb, v, call);
        return;
    }
    GC_ROOT(&v);
    switch (t->kind) {
    case TY_INT: sb_str(sb, int_to_str(v)); break;
    case TY_FLOAT: jt_float(sb, v); break;
    case TY_BOOL: sb_cstr(sb, v ? "true" : "false"); break;
    case TY_STR: jt_quote(sb, (Str *)v); break;
    case TY_BYTES: jt_quote(sb, bytes_to_base64((Str *)v)); break;
    case TY_LIST:
        sb_byte(sb, '[');
        for (long i = 0; i < ((List *)v)->len; i++) {
            if (i) sb_byte(sb, ',');
            jt_value(sb, ((List *)v)->data[i].i, t->a.t, call);
        }
        sb_byte(sb, ']');
        break;
    case TY_MAP: {
        sb_byte(sb, '{');
        int first = 1;
        for (long i = 0; i < ((Map *)v)->used; i++) {
            MapEntry *me = &((Map *)v)->entries[i];
            if (!me->live) continue;
            if (!first) sb_byte(sb, ',');
            first = 0;
            jt_quote(sb, me->key.s);
            sb_byte(sb, ':');
            jt_value(sb, ((Map *)v)->entries[i].val.i, t->b.t, call);
        }
        sb_byte(sb, '}');
        break;
    }
    case TY_RECORD:
        sb_byte(sb, '{');
        for (long i = 0; i < pg_nmembers(t); i++) {
            if (i) sb_byte(sb, ',');
            jt_quote(sb, member_name(t, i));
            sb_byte(sb, ':');
            jt_value(sb, ((Record *)v)->fields[i].i, pg_member(t, i), call);
        }
        sb_byte(sb, '}');
        break;
    case TY_VARIANT: {
        long tag = (long)(uint32_t)((Variant *)v)->tag;
        Type *pt = pg_member(t, tag);
        if (!pt) {
            jt_quote(sb, member_name(t, tag));
            break;
        }
        sb_byte(sb, '{');
        jt_quote(sb, member_name(t, tag));
        sb_byte(sb, ':');
        jt_value(sb, ((Variant *)v)->pay.i, pt, call);
        sb_byte(sb, '}');
        break;
    }
    default: rt_raise_c("R0007", "This value has no JSON form.", call); /* (E0328 prevents it) */
    }
    GC_UNROOT(1);
}

/* A Json value as write_json writes it (NaN and infinities: null, as to_json makes them). */
static void jt_json(SB *sb, long j, Node *call)
{
    check_stack(call);
    Variant *jj = (Variant *)j;
    long tag = (long)(uint32_t)jj->tag;
    GC_ROOT(&j);
    switch (tag) {
    case J_NULL: sb_cstr(sb, "null"); break;
    case J_BOOL: sb_cstr(sb, jj->pay.i ? "true" : "false"); break;
    case J_INT: sb_str(sb, int_to_str(jj->pay.i)); break;
    case J_NUM: jt_float(sb, jj->pay.i); break;
    case J_TEXT: jt_quote(sb, jj->pay.s); break;
    case J_ARRAY:
        sb_byte(sb, '[');
        for (long i = 0; i < ((List *)((Variant *)j)->pay.p)->len; i++) {
            if (i) sb_byte(sb, ',');
            jt_json(sb, ((List *)((Variant *)j)->pay.p)->data[i].i, call);
        }
        sb_byte(sb, ']');
        break;
    default: { /* J_OBJECT */
        sb_byte(sb, '{');
        int first = 1;
        for (long i = 0; i < ((Map *)((Variant *)j)->pay.p)->used; i++) {
            MapEntry *me = &((Map *)((Variant *)j)->pay.p)->entries[i];
            if (!me->live) continue;
            if (!first) sb_byte(sb, ',');
            first = 0;
            jt_quote(sb, me->key.s);
            sb_byte(sb, ':');
            jt_json(sb, ((Map *)((Variant *)j)->pay.p)->entries[i].val.i, call);
        }
        sb_byte(sb, '}');
    }
    }
    GC_UNROOT(1);
}

/* `__json_text(v)`: the compact JSON text of v by its type. */
Str *json_text(long v, Node *call)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    jt_value(sb, v, ((Node *)vec_get(call->b.v, 0))->type, call);
    Str *s = sb_finish(sb);
    GC_UNROOT(1);
    return s;
}

/* ---- from JSON ---- */

static List *jd_probs;      /* the problems found (Str), in document order (a collector root) */
static long jd_mode;
static char jd_path[4096];  /* the path of the value being read */
static long jd_plen;
static Node *jd_call;

static void jp_push(char *s, long n, int field)
{
    if (jd_plen + n + 4 >= (long)sizeof jd_path) return; /* (absurdly deep: the path stays cut) */
    if (field && jd_plen) jd_path[jd_plen++] = '.';
    mem_copy(jd_path + jd_plen, s, n);
    jd_plen += n;
}

/* A JSON object key as a path part: as it is when it is a plain name, otherwise quoted in brackets. */
static void path_key(Str *k)
{
    int plain = k->len > 0;
    for (long i = 0; i < k->len; i++) {
        char c = k->data[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            plain = 0;
    }
    if (plain) {
        jp_push(k->data, k->len, 1);
        return;
    }
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_byte(sb, '[');
    json_str_into(sb, k);
    sb_byte(sb, ']');
    jp_push(sb->ptr, sb->len, 0);
    GC_UNROOT(1);
}

static void path_index(long i)
{
    char buf[24];
    long n = 0;
    buf[n++] = '[';
    Str *d = int_to_str(i);
    mem_copy(buf + n, d->data, d->len);
    n += d->len;
    buf[n++] = ']';
    jp_push(buf, n, 0);
}

static void prob(int limit, Str *reason)
{
    GC_ROOT(&reason);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    char *p = jd_plen ? jd_path : "(root)";
    long n = jd_plen ? jd_plen : 6;
    if (jd_mode & 2) {
        sb_byte(sb, limit ? 'L' : 'S');
        sb_byte(sb, '\t');
        sb_bytes(sb, p, n);
        sb_byte(sb, '\t');
        sb_str(sb, reason);
    } else {
        sb_bytes(sb, p, n);
        sb_cstr(sb, ": ");
        sb_str(sb, reason);
    }
    Str *s = sb_finish(sb);
    list_push(jd_probs, (long)s);
    GC_UNROOT(2);
}

static char *const json_kinds[] = {"null", "boolean", "number", "number", "string", "array", "object"};

static char *want_name(Type *t)
{
    switch (t->kind) {
    case TY_INT: return "integer";
    case TY_FLOAT: return "number";
    case TY_BOOL: return "boolean";
    case TY_STR:
    case TY_BYTES: return "string";
    case TY_LIST: return "array";
    case TY_VARIANT: return "string or object";
    default: return "object";
    }
}

static void wrong(Type *t, long tag)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, "expected ");
    sb_cstr(sb, want_name(t));
    sb_cstr(sb, ", got ");
    sb_cstr(sb, json_kinds[tag]);
    prob(0, sb_finish(sb));
    GC_UNROOT(1);
}

/* The value of a field annotation's literal (checked by E0329) for field type ft. */
static long annot_value(Node *e, Type *ft)
{
    int neg = e->kind == N_UNARY;
    if (neg) e = e->a.n;
    switch (e->kind) {
    case N_INT: return neg ? -e->a.i : e->a.i;
    case N_FLOAT: return neg ? as_bits(-as_dbl(e->a.i)) : e->a.i;
    case N_BOOL: return e->a.i;
    case N_STR: return (long)e->a.s;
    case N_LIST: return (long)list_new(1);
    case N_VCASE: return (long)pg_new_variant(member_index(ft->decl.n->b.v, e->b.n->a.s), 0);
    default: return 0;
    }
}

static Node *annot(Node *field, char *name)
{
    for (long i = 0; field->c.v && i < field->c.v->n; i++) {
        Node *a = vec_get(field->c.v, i);
        if (a->a.n && str_eq_cstr(a->a.n->a.s, name)) return a;
    }
    return 0;
}

static double annot_num(Node *a, long k, Type *ft)
{
    long v = annot_value(vec_get(a->b.v, k), ft);
    return ft->kind == TY_FLOAT ? as_dbl(v) : (double)v;
}

/* A number of a limit as the reason shows it. */
static Str *num_text(double d, Type *ft)
{
    return ft->kind == TY_FLOAT ? float_to_str(as_bits(d)) : int_to_str((long)d);
}

/* The limits of field on its decoded value v; problems as limit problems. */
static void check_limits(Node *field, Type *ft, long v)
{
    Node *a = annot(field, "size");
    if (a) {
        long n = ft->kind == TY_STR ? utf8_chars((Str *)v) : ft->kind == TY_LIST ? ((List *)v)->len : ((Map *)v)->count;
        long lo = annot_value(vec_get(a->b.v, 0), ft), hi = annot_value(vec_get(a->b.v, 1), ft);
        char *unit = ft->kind == TY_STR ? "character" : "item";
        if (n < lo) prob(1, msg(lo == 1 ? "at least $1 $2, got $3" : "at least $1 $2s, got $3", int_to_str(lo), str_from_cstr(unit), int_to_str(n)));
        if (n > hi) prob(1, msg(hi == 1 ? "at most $1 $2, got $3" : "at most $1 $2s, got $3", int_to_str(hi), str_from_cstr(unit), int_to_str(n)));
    }
    a = annot(field, "range");
    if (a) {
        double d = ft->kind == TY_FLOAT ? as_dbl(v) : (double)v;
        double lo = annot_num(a, 0, ft), hi = annot_num(a, 1, ft);
        if (d < lo) prob(1, msg("at least $1", num_text(lo, ft), 0, 0));
        if (d > hi) prob(1, msg("at most $1", num_text(hi, ft), 0, 0));
    }
    a = annot(field, "pattern");
    if (a) {
        Str *p = ((Node *)vec_get(a->b.v, 0))->a.s;
        Str *whole = msg("^(?:$1)$", p, 0, 0);
        List *m = re_prim(1, whole, (Str *)v, jd_call);
        if (!m->len) prob(1, msg("does not match $1", p, 0, 0));
    }
}

static long from_j(long j, Type *t, long depth, int text_ok, int *ok);

/* A JSON text for a scalar field (mode bit 1): converted, or a problem. */
static long from_text(Str *s, Type *t, int *ok)
{
    switch (t->kind) {
    case TY_STR: return (long)s;
    case TY_INT: {
        long v;
        if (!dec_to_int(s->data, s->len, &v)) return v;
        prob(0, msg("expected integer, got \"$1\"", s, 0, 0));
        break;
    }
    case TY_FLOAT: {
        Variant *r = rt_parse_float(s);
        if (!r->tag) return r->pay.i;
        prob(0, msg("expected number, got \"$1\"", s, 0, 0));
        break;
    }
    case TY_BOOL:
        if (str_eq_cstr(s, "true")) return 1;
        if (str_eq_cstr(s, "false")) return 0;
        prob(0, msg("expected true or false, got \"$1\"", s, 0, 0));
        break;
    default: break;
    }
    *ok = 0;
    return 0;
}

static long from_record(Map *obj, Type *t, long depth, int *ok)
{
    long n = pg_nmembers(t);
    Record *rec = alloc_obj(OBJ_RECORD, n * 8);
    GC_ROOT(&rec);
    GC_ROOT(&obj);
    char *seen = alloc(n + 1);
    GC_ROOT(&seen);
    long base = jd_plen;
    for (long i = 0; i < obj->used; i++) { /* the keys, in document order */
        MapEntry *me = &obj->entries[i];
        if (!me->live) continue;
        Str *key = me->key.s;
        long f = member_index(t->decl.n->b.v, key);
        path_key(key);
        if (f < 0) {
            prob(0, str_from_cstr("unknown field"));
            *ok = 0;
        } else {
            Node *field = member_at(t->decl.n->b.v, f);
            Type *ft = rt_concrete(pg_member(t, f));
            int text_ok = (jd_mode & 1) && depth == 0 && !str_eq_cstr(key, "body");
            int fok = 1;
            long v = from_j(obj->entries[i].val.i, ft, depth + 1, text_ok, &fok);
            if (fok) {
                rec->fields[f].i = v;
                seen[f] = 1;
                long before = jd_probs->len;
                check_limits(field, ft, v);
                if (jd_probs->len != before) *ok = 0;
            } else {
                *ok = 0;
                seen[f] = 1;
            }
        }
        jd_plen = base;
    }
    for (long f = 0; f < n; f++) { /* the fields not given: their default, or missing */
        if (seen[f]) continue;
        Node *field = member_at(t->decl.n->b.v, f);
        Node *d = annot(field, "default");
        if (d) {
            rec->fields[f].i = annot_value(vec_get(d->b.v, 0), rt_concrete(pg_member(t, f)));
            continue;
        }
        jp_push(field->a.n->a.s->data, field->a.n->a.s->len, 1);
        prob(0, str_from_cstr("missing"));
        jd_plen = base;
        *ok = 0;
    }
    GC_UNROOT(3);
    return (long)rec;
}

static long from_variant(long j, Type *t, long depth, int *ok)
{
    Variant *jj = (Variant *)j;
    long tag = (long)(uint32_t)jj->tag;
    Str *name = 0;
    long pay = 0;
    if (tag == J_TEXT) {
        name = jj->pay.s;
    } else if (tag == J_OBJECT && ((Map *)jj->pay.p)->count == 1) {
        Map *m = jj->pay.p;
        for (long i = 0; i < m->used; i++)
            if (m->entries[i].live) name = m->entries[i].key.s, pay = m->entries[i].val.i;
    } else {
        wrong(t, tag);
        *ok = 0;
        return 0;
    }
    long c = member_index(t->decl.n->b.v, name);
    if (c < 0) {
        SB *sb = sb_new();
        GC_ROOT(&sb);
        sb_cstr(sb, "unknown case \"");
        sb_str(sb, name);
        sb_cstr(sb, "\" (expected one of: ");
        for (long i = 0; i < pg_nmembers(t); i++) {
            if (i) sb_cstr(sb, ", ");
            sb_str(sb, member_name(t, i));
        }
        sb_byte(sb, ')');
        prob(0, sb_finish(sb));
        GC_UNROOT(1);
        *ok = 0;
        return 0;
    }
    Type *pt = pg_member(t, c);
    if (!pt != (tag == J_TEXT)) { /* a case with a value written as text, or one without as an object */
        prob(0, msg(pt ? "case \"$1\" needs a value: {\"$1\": ...}" : "case \"$1\" has no value: write \"$1\"", name, 0, 0));
        *ok = 0;
        return 0;
    }
    if (!pt) return (long)pg_new_variant(c, 0);
    long base = jd_plen;
    path_key(name);
    long v = from_j(pay, pt, depth + 1, 0, ok);
    jd_plen = base;
    return *ok ? (long)pg_new_variant(c, v) : 0;
}

static long from_j(long j, Type *t, long depth, int text_ok, int *ok)
{
    t = rt_concrete(t);
    if (depth > JD_DEPTH) {
        prob(0, str_from_cstr("nested too deeply"));
        *ok = 0;
        return 0;
    }
    POLL(1);
    if (is_json_type(t)) return j; /* any JSON is a Json */
    GC_ROOT(&j);
    Variant *jj = (Variant *)j;
    long tag = (long)(uint32_t)jj->tag;
    long r = 0;
    if (text_ok && tag == J_TEXT && t->kind != TY_STR && t->kind != TY_LIST && t->kind != TY_VARIANT) {
        r = from_text(jj->pay.s, t, ok);
        GC_UNROOT(1);
        return r;
    }
    switch (t->kind) {
    case TY_INT:
        if (tag == J_INT) {
            r = jj->pay.i;
        } else if (tag == J_NUM) {
            double d = as_dbl(jj->pay.i);
            if (d == (double)(long)d && d > -9.2e18 && d < 9.2e18) r = (long)d;
            else prob(0, str_from_cstr(d >= 9.2e18 || d <= -9.2e18 ? "out of range for int" : "not an integer")), *ok = 0;
        } else {
            wrong(t, tag), *ok = 0;
        }
        break;
    case TY_FLOAT:
        if (tag == J_NUM) r = jj->pay.i;
        else if (tag == J_INT) r = as_bits((double)jj->pay.i);
        else wrong(t, tag), *ok = 0;
        break;
    case TY_BOOL:
        if (tag == J_BOOL) r = jj->pay.i;
        else wrong(t, tag), *ok = 0;
        break;
    case TY_STR:
        if (tag == J_TEXT) r = jj->pay.i;
        else wrong(t, tag), *ok = 0;
        break;
    case TY_BYTES:
        if (tag != J_TEXT) {
            wrong(t, tag), *ok = 0;
        } else {
            Variant *b = bytes_from_base64(jj->pay.s);
            if (b->tag) prob(0, str_from_cstr("not base64")), *ok = 0;
            else r = b->pay.i;
        }
        break;
    case TY_LIST: {
        if (tag != J_ARRAY) {
            wrong(t, tag), *ok = 0;
            break;
        }
        List *in = jj->pay.p;
        List *out = list_new(in->len ? in->len : 1);
        GC_ROOT(&out);
        long base = jd_plen;
        for (long i = 0; i < ((List *)((Variant *)j)->pay.p)->len; i++) {
            path_index(i);
            long v = from_j(((List *)((Variant *)j)->pay.p)->data[i].i, t->a.t, depth + 1, text_ok, ok);
            list_push(out, v);
            jd_plen = base;
        }
        r = (long)out;
        GC_UNROOT(1);
        break;
    }
    case TY_MAP: {
        if (tag != J_OBJECT) {
            wrong(t, tag), *ok = 0;
            break;
        }
        Map *out = map_new();
        GC_ROOT(&out);
        long base = jd_plen;
        Map *in = jj->pay.p;
        for (long i = 0; i < in->used; i++) {
            MapEntry *me = &((Map *)((Variant *)j)->pay.p)->entries[i];
            if (!me->live) continue;
            long key = me->key.i;
            GC_ROOT(&key);
            path_key((Str *)key);
            long v = from_j(me->val.i, t->b.t, depth + 1, 0, ok);
            map_set(out, key, v, t_str);
            GC_UNROOT(1);
            jd_plen = base;
        }
        r = (long)out;
        GC_UNROOT(1);
        break;
    }
    case TY_RECORD:
        if (tag != J_OBJECT) wrong(t, tag), *ok = 0;
        else r = from_record(jj->pay.p, t, depth, ok);
        break;
    case TY_VARIANT: r = from_variant(j, t, depth, ok); break;
    default: rt_raise_c("R0007", "This type has no JSON form.", jd_call); /* (E0328 prevents it) */
    }
    GC_UNROOT(1);
    return r;
}

/* A value of type t from one text of a request (a path variable, a query value, a header). */
static long text_value(Str *s, Type *t, int *ok)
{
    if (t->kind == TY_VARIANT) {
        long c = member_index(t->decl.n->b.v, s);
        if (c >= 0 && !pg_member(t, c)) return (long)pg_new_variant(c, 0);
        SB *sb = sb_new();
        GC_ROOT(&sb);
        sb_cstr(sb, "unknown case \"");
        sb_str(sb, s);
        sb_cstr(sb, "\" (expected one of: ");
        for (long i = 0; i < pg_nmembers(t); i++) {
            if (i) sb_cstr(sb, ", ");
            sb_str(sb, member_name(t, i));
        }
        sb_byte(sb, ')');
        prob(0, sb_finish(sb));
        GC_UNROOT(1);
        *ok = 0;
        return 0;
    }
    return from_text(s, t, ok);
}

/* The entry of a Json object (a Map) under key, or 0. */
static long obj_get(long obj, char *key)
{
    if (!obj) return 0;
    Map *m = ((Variant *)obj)->pay.p;
    Str *k = str_from_cstr(key);
    long e = map_find(m, (long)k, t_str);
    return e < 0 ? 0 : m->entries[e].val.i;
}

static long obj_get_str(long obj, Str *key)
{
    if (!obj) return 0;
    Map *m = ((Variant *)obj)->pay.p;
    long e = map_find(m, (long)key, t_str);
    return e < 0 ? 0 : m->entries[e].val.i;
}

static long web_record(long j, Type *t, int *ok)
{
    GC_ROOT(&j);
    long n = pg_nmembers(t);
    Record *rec = alloc_obj(OBJ_RECORD, n * 8);
    GC_ROOT(&rec);
    long path = obj_get(j, "path"), query = obj_get(j, "query"), headers = obj_get(j, "headers"), body = obj_get(j, "body");
    GC_ROOT(&path);
    GC_ROOT(&query);
    GC_ROOT(&headers);
    GC_ROOT(&body);
    int has_body_field = 0;
    for (long f = 0; f < n; f++) {
        Node *field = member_at(t->decl.n->b.v, f);
        Str *name = field->a.n->a.s;
        Type *ft = rt_concrete(pg_member(t, f));
        Node *h = annot(field, "header");
        int fok = 1, given = 1;
        long v = 0;
        jd_plen = 0;
        if (str_eq_cstr(name, "body")) {
            has_body_field = 1;
            if (body) v = from_j(body, ft, 1, 0, &fok);
            else given = 0;
        } else {
            long text = obj_get_str(path, name); /* a path variable */
            Str *src = name;
            if (!text && h) { /* a header: its name compared without case */
                src = ((Node *)vec_get(h->b.v, 0))->a.s;
                Map *hm = headers ? ((Variant *)headers)->pay.p : 0;
                for (long i = 0; hm && i < hm->used && !text; i++) {
                    Str *k = hm->entries[i].key.s;
                    if (!hm->entries[i].live || k->len != src->len) continue;
                    long d = 0;
                    while (d < k->len && ((k->data[d] | 32) == (src->data[d] | 32))) d++;
                    if (d == k->len) text = hm->entries[i].val.i;
                }
            }
            jp_push(src->data, src->len, 1);
            if (text) {
                v = text_value(((Variant *)text)->pay.s, ft, &fok);
            } else if (!h) { /* query values */
                long vals = obj_get_str(query, name);
                List *vs = vals ? ((Variant *)vals)->pay.p : 0;
                if (!vs || !vs->len) {
                    given = 0;
                } else if (ft->kind == TY_LIST) {
                    List *out = list_new(vs->len);
                    GC_ROOT(&out);
                    for (long i = 0; i < ((List *)((Variant *)obj_get_str(query, name))->pay.p)->len; i++) {
                        List *cur = ((Variant *)obj_get_str(query, name))->pay.p;
                        long x = text_value(((Variant *)cur->data[i].p)->pay.s, rt_concrete(ft->a.t), &fok);
                        list_push(out, x);
                    }
                    v = (long)out;
                    GC_UNROOT(1);
                } else if (vs->len > 1) {
                    prob(0, str_from_cstr("given more than once"));
                    fok = 0;
                } else {
                    v = text_value(((Variant *)vs->data[0].p)->pay.s, ft, &fok);
                }
            } else {
                given = 0;
            }
        }
        if (!given) {
            Node *d = annot(field, "default");
            if (d) {
                rec->fields[f].i = annot_value(vec_get(d->b.v, 0), ft);
            } else {
                if (!jd_plen) jp_push(name->data, name->len, 1);
                prob(0, str_from_cstr("missing"));
                *ok = 0;
            }
            continue;
        }
        if (!fok) {
            *ok = 0;
            continue;
        }
        rec->fields[f].i = v;
        long before = jd_probs->len;
        check_limits(field, ft, v);
        if (jd_probs->len != before) *ok = 0;
    }
    if (body && !has_body_field) {
        jd_plen = 0;
        jp_push("body", 4, 1);
        prob(0, str_from_cstr("this operation takes no body"));
        *ok = 0;
    }
    jd_plen = 0;
    GC_UNROOT(6);
    return (long)rec;
}

/* `__json_fields(xs)` with xs a list[T] (empty: only its type counts): T's field names, in order. */
List *json_fields(Node *call)
{
    Type *t = rt_concrete(rt_concrete(((Node *)vec_get(call->b.v, 0))->type)->a.t);
    List *out = list_new(4);
    if (t->kind != TY_RECORD) return out;
    GC_ROOT(&out);
    for (long i = 0; i < pg_nmembers(t); i++) list_push(out, (long)member_name(t, i));
    GC_UNROOT(1);
    return out;
}

/* result[T] for `__from_json(j, mode)`: T from the call's type. */
Variant *json_from(long j, long mode, Node *call)
{
    Type *t = rt_concrete(call->type)->a.t;
    jd_probs = list_new(4);
    jd_mode = mode;
    jd_plen = 0;
    jd_call = call;
    int ok = 1;
    long v = (mode & 4) && t->kind == TY_RECORD ? web_record(j, t, &ok) : from_j(j, t, 0, 0, &ok);
    GC_ROOT(&v);
    Variant *r;
    if (ok && !jd_probs->len) {
        r = make_result(0, v);
    } else {
        SB *sb = sb_new();
        GC_ROOT(&sb);
        for (long i = 0; i < jd_probs->len; i++) {
            if (i) sb_cstr(sb, mode & 2 ? "\n" : "; ");
            sb_str(sb, (Str *)jd_probs->data[i].p);
        }
        r = make_result(1, (long)sb_finish(sb));
        GC_UNROOT(1);
    }
    jd_probs = 0;
    GC_UNROOT(1);
    return r;
}
