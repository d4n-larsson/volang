/* Values rendered in vo literal syntax (test failures, map key errors; ported from
 * src/rt_render.s, spec 008). Recursive over nested values, without a depth limit (as before:
 * values are acyclic and built by the program, so their depth is bounded by the heap). */
#include "vo.h"

/* Spec 006 (research R14): when not 0, rendering stops once the builder holds more than this many
 * bytes; render_capped sets it. 0 everywhere else (expect_eq, counterexamples: full values).
 * ponytail: one fixed cap of 200 bytes per value; a per-use limit if anyone needs longer values. */
long render_cap;

static int over_cap(SB *sb)
{
    return render_cap && sb->len > render_cap;
}

/* A str as a quoted literal with the four escapes. */
void render_str_lit(SB *sb, Str *str)
{
    char *p = str->data;
    long n = str->len;
    sb_byte(sb, '"');
    for (; n && !over_cap(sb); n--) {
        int c = (unsigned char)*p++;
        int esc = c == 10 ? 'n' : c == 9 ? 't' : (c == '\\' || c == '"') ? c : 0;
        if (esc) {
            sb_byte(sb, '\\');
            sb_byte(sb, esc);
        } else {
            sb_byte(sb, c);
        }
    }
    sb_byte(sb, '"');
}

long render_depth; /* nesting of render_value (rt_raise resets it: a raise unwinds the rendering) */

void render_value(SB *sb, long value, Type *type)
{
    POLL(1);
    if (over_cap(sb)) return;
    /* spec 013 (L10): too deep (or cyclic) for the native stack or the C root stack: R0005, or an
     * ellipsis while a runtime error report is being rendered (no error inside a report) */
    /* (the depth limit keeps the unwind small and the result the same on every build) */
    if ((unsigned long)__builtin_frame_address(0) < (unsigned long)stack_floor + 0x10000 ||
        gc_roots_n > ROOTS_LIMIT - 4096 || render_depth >= 50000) {
        if (rt_reporting) {
            sb_cstr(sb, "...");
            return;
        }
        rt_raise_c("R0005", "Stack exhausted: the value is too deeply nested (or cyclic) to show.", (Node *)cur_node);
    }
    render_depth++;
    GC_ROOT(&value);
    type = rt_concrete(type); /* type parameters: the running frame's type arguments */
    GC_ROOT(&type);
    switch (type->kind) {
    case TY_INT: int_to_dec(sb, value); break;
    case TY_FLOAT: float_to_text(sb, value); break;
    case TY_BOOL: sb_cstr(sb, value ? "true" : "false"); break;
    case TY_STR: render_str_lit(sb, (Str *)value); break;
    case TY_LIST: { /* [a, b] */
        List *l = (List *)value;
        sb_byte(sb, '[');
        for (long i = 0; i < l->len && !over_cap(sb); i++) {
            if (i) sb_cstr(sb, ", ");
            render_value(sb, l->data[i].i, type->a.t);
        }
        sb_byte(sb, ']');
        break;
    }
    case TY_MAP: { /* {k: v, ...} in insertion order */
        Map *m = (Map *)value;
        int any = 0; /* written any? */
        sb_byte(sb, '{');
        for (long i = 0; i < m->used && !over_cap(sb); i++) {
            MapEntry *e = &m->entries[i];
            if (!e->live) continue;
            if (any) sb_cstr(sb, ", ");
            any = 1;
            render_value(sb, e->key.i, type->a.t);
            sb_cstr(sb, ": ");
            render_value(sb, e->val.i, type->b.t);
        }
        sb_byte(sb, '}');
        break;
    }
    case TY_RECORD: { /* new T { f: v, ... } */
        Record *r = (Record *)value;
        sb_cstr(sb, "new ");
        sb_str(sb, type->decl.n->a.n->a.s);
        if (type->b.v) ty_args_into(sb, type->b.v); /* an instance: new Name[args] { ... } */
        sb_cstr(sb, " { ");
        for (long i = 0;; i++) {
            Node *m = member_at(type->decl.n->b.v, i);
            if (!m) break;
            if (i) sb_cstr(sb, ", ");
            sb_str(sb, m->a.n->a.s);
            sb_cstr(sb, ": ");
            long field = r->fields[i].i;
            render_value(sb, field, field_type_in(type, m->b.n->type));
        }
        sb_cstr(sb, " }");
        break;
    }
    case TY_VARIANT: { /* T.case or T.case(v) */
        Variant *v = (Variant *)value;
        sb_str(sb, type->decl.n->a.n->a.s);
        sb_byte(sb, '.');
        Node *c = member_at(type->decl.n->b.v, (long)(uint32_t)v->tag);
        sb_str(sb, c->a.n->a.s);
        if (!c->b.n) break;
        sb_byte(sb, '(');
        render_value(sb, v->pay.i, field_type_in(type, c->b.n->type));
        sb_byte(sb, ')');
        break;
    }
    case TY_RESULT: { /* ok(v), ok() or err("m") */
        Variant *v = (Variant *)value;
        long tag = (long)(uint32_t)v->tag;
        sb_cstr(sb, tag ? "err(" : "ok(");
        Type *pt = case_payload(type, tag);
        if (pt) render_value(sb, v->pay.i, pt);
        sb_byte(sb, ')');
        break;
    }
    case TY_FUNC: /* a function value: its type, as <fn(int) -> int> */
        sb_byte(sb, '<');
        ty_name_into(sb, type);
        sb_byte(sb, '>');
        break;
    default: break;
    }
    GC_UNROOT(2);
    render_depth--;
}

/* render_value, shortened to at most 200 bytes (cut at a UTF-8 boundary) with "..." appended when
 * anything was left out (spec 006: traces and the values in runtime errors). */
void render_capped(SB *sb, long value, Type *type)
{
    long start = sb->len; /* where this value starts */
    long outer = render_cap; /* the cap of an enclosing render (restored below) */
    render_cap = start + 200;
    render_value(sb, value, type);
    render_cap = outer;
    if (sb->len - start <= 200) return;
    long cut = start + 200; /* cut back to a character boundary at most 200 bytes in */
    while (cut > start && ((unsigned char)sb->ptr[cut] & 0xc0) == 0x80) cut--;
    sb->len = cut;
    sb_cstr(sb, "...");
}
