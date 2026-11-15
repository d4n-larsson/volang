/* vo doc (spec 007 US1, research R7; ported from src/doc.s, spec 008): exact signatures from the
 * checker's own declarations, descriptions and examples from the comments above each item in the
 * library source, builtins from the reference's table. `vo doc [--json] [NAME]` (contracts/cli.md). */
#include "vo.h"

static char *const doc_modules[] = {"json", "time", "text", "random", "http", 0};

static Vec *doc_collect; /* doc_module and doc_builtins collect names here instead of printing */

#define J_HEAD "{\"v\":1,\"kind\":\"doc\",\"module\":"
#define J_NAME ",\"name\":"
#define J_ITEM ",\"item\":\""
#define J_SIG "\",\"signature\":"
#define J_DESC ",\"description\":"
#define J_CLOSE "}\n"

/* ---- small helpers ---- */

/* The text of a one-line comment node after its `#` and `skip` more characters. */
Str *doc_line_text(long file, Node *node, long skip)
{
    char *text = file_get((int)file)->text;
    long from = (long)node->start + 1 + skip; /* past `#` */
    if (from >= node->end) return str_new(0, 0);
    return str_new(text + from, node->end - from);
}

/* 1 when the comment is `#` followed by five spaces (an example line). */
long doc_is_example(long file, Node *node)
{
    char *p = file_get((int)file)->text + node->start;
    if ((long)node->end - node->start < 7) return 0;
    for (int i = 1; i < 6; i++)
        if (p[i] != ' ') return 0;
    return 1;
}

/* The description (out[0], the first comment line) and example (out[1]) of the item at index:
 * the comments on the lines right above it. Both 0 when it has none (an internal item). */
void doc_comments(long file, Vec *items, long index, Str **out)
{
    out[0] = 0;
    out[1] = 0;
    long next = pos_line((int)file, ((Node *)vec_get(items, index))->start); /* the line below a comment */
    while (--index >= 0) {
        Node *c = vec_get(items, index);
        if (c->kind != N_COMMENT) return;
        long line = pos_line((int)file, c->start);
        if (line + 1 != next) return;
        next = line;
        if (doc_is_example(file, c))
            out[1] = doc_line_text(file, c, 5); /* the topmost example line wins */
        else
            out[0] = doc_line_text(file, c, 1); /* the topmost description line wins */
    }
}

void doc_out(SB *sb)
{
    out_bytes(1, sb->ptr, sb->len);
}

/* ---- signatures ---- */

/* [T, U: ordered, E: effects] when the item has type parameters. */
void doc_tparams(SB *sb, Node *decl)
{
    Fx *fx = item_fx(decl);
    if (!fx || !fx->tparams || !fx->tparams->n) return;
    Vec *tps = fx->tparams;
    sb_byte(sb, '[');
    for (long i = 0; i < tps->n; i++) {
        if (i) sb_cstr(sb, ", ");
        Node *tp = vec_get(tps, i);
        sb_str(sb, tp->a.n->a.s);
        if (tp->b.i == TPC_ORDERED)
            sb_cstr(sb, ": ordered");
        else if (tp->b.i == TPC_EFFECTS)
            sb_cstr(sb, ": effects");
    }
    sb_byte(sb, ']');
}

/* fn name[T](a: A, b: B) -> R uses E; record Name { f: T; ... }; variant Name { c(T); d; }. */
void doc_signature(SB *sb, Node *item)
{
    if (item->kind != N_RECORD && item->kind != N_VARIANT) { /* a function */
        sb_cstr(sb, "fn ");
        sb_str(sb, item->a.n->a.s);
        doc_tparams(sb, item);
        sb_byte(sb, '(');
        Vec *params = item->b.v;
        for (long i = 0; i < params->n; i++) {
            if (i) sb_cstr(sb, ", ");
            Node *p = vec_get(params, i);
            sb_str(sb, p->a.n->a.s);
            sb_cstr(sb, ": ");
            ty_name_into(sb, p->type);
        }
        sb_byte(sb, ')');
        if (item->type && item->type->kind != TY_VOID) { /* the return type, unless void */
            sb_cstr(sb, " -> ");
            ty_name_into(sb, item->type);
        }
        Fx *fx = item_fx(item); /* effects */
        if (!fx || !fx->declared || es_is_empty(fx->declared)) return;
        sb_cstr(sb, " uses ");
        es_render(fx->declared, sb);
        return;
    }
    int record = item->kind == N_RECORD; /* record Name { f: T; ... } */
    sb_cstr(sb, record ? "record " : "variant ");
    sb_str(sb, item->a.n->a.s);
    doc_tparams(sb, item);
    sb_cstr(sb, " { ");
    for (long i = 0;; i++) {
        Node *m = member_at(item->b.v, i);
        if (!m) break;
        sb_str(sb, m->a.n->a.s);
        if (m->b.n) { /* the field's or the case's type */
            if (record) {
                sb_cstr(sb, ": ");
                ty_name_into(sb, m->b.n->type);
            } else {
                sb_byte(sb, '(');
                ty_name_into(sb, m->b.n->type);
                sb_byte(sb, ')');
            }
        }
        sb_byte(sb, ';');
        sb_byte(sb, ' ');
    }
    sb_byte(sb, '}');
}

/* {"name":NAME,"type":TYPE} (type 0: null). */
void doc_json_pair(SB *sb, Str *name, Type *type)
{
    sb_cstr(sb, "{\"name\":");
    json_str_into(sb, name);
    sb_cstr(sb, ",\"type\":");
    if (type)
        json_str_into(sb, ty_name(type));
    else
        sb_cstr(sb, "null");
    sb_byte(sb, '}');
}

/* Appends text as a JSON list of strings, cut at ", " outside parentheses ("" gives []). */
void doc_json_split(SB *sb, Str *text)
{
    sb_byte(sb, '[');
    if (text->len) {
        long from = 0, depth = 0, i = 0;
        while (i < text->len) {
            int c = (unsigned char)text->data[i];
            if (c == '(') depth++;
            if (c == ')') depth--;
            if (c == ',' && !depth) { /* a piece ends at this comma */
                doc_json_piece(sb, text, from, i);
                sb_byte(sb, ',');
                i += 2;
                from = i;
                continue;
            }
            i++;
        }
        doc_json_piece(sb, text, from, i);
    }
    sb_byte(sb, ']');
}

void doc_json_piece(SB *sb, Str *text, long from, long to)
{
    json_str_into(sb, str_new(text->data + from, to - from));
}

/* ,"tparams":[...],"params":[...],"returns":"T","effects":[...] for a function;
 * ,"tparams":[...],"fields":[...] or "cases":[...] for a record or variant. */
void doc_json_shape(SB *sb, Node *item)
{
    sb_cstr(sb, ",\"tparams\":"); /* the type parameters as written: "T", "T: ordered" */
    SB *tp = sb_new();
    doc_tparams(tp, item);
    Str *inner = tp->len ? str_new(tp->ptr + 1, tp->len - 2) : str_new(0, 0); /* without the brackets */
    doc_json_split(sb, inner);
    if (item->kind == N_FN) { /* a function: parameters, return type, effects */
        sb_cstr(sb, ",\"params\":[");
        Vec *params = item->b.v;
        for (long i = 0; i < params->n; i++) {
            if (i) sb_byte(sb, ',');
            Node *p = vec_get(params, i);
            doc_json_pair(sb, p->a.n->a.s, p->type);
        }
        sb_cstr(sb, "],\"returns\":");
        json_str_into(sb, ty_name(item->type));
        sb_cstr(sb, ",\"effects\":");
        SB *eff = sb_new();
        Fx *fx = item_fx(item);
        if (fx && fx->declared) es_render(fx->declared, eff);
        doc_json_split(sb, sb_finish(eff));
        return;
    }
    sb_cstr(sb, item->kind == N_RECORD ? ",\"fields\":[" : ",\"cases\":["); /* fields or cases */
    for (long i = 0;; i++) {
        Node *m = member_at(item->b.v, i);
        if (!m) break;
        if (i) sb_byte(sb, ',');
        doc_json_pair(sb, m->a.n->a.s, m->b.n ? m->b.n->type : 0);
    }
    sb_byte(sb, ']');
}

/* ---- one item ---- */

/* Text: the signature, the description indented, and (full) the example; or one JSON record. */
void doc_print_item(Str *module, Node *item, Str *desc, Str *example, long full)
{
    SB *sb = sb_new();
    SB *sig = sb_new(); /* the signature on its own */
    doc_signature(sig, item);
    if (opt_json) {
        sb_cstr(sb, J_HEAD);
        json_str_into(sb, module);
        sb_cstr(sb, J_NAME);
        json_str_into(sb, item->a.n->a.s);
        sb_cstr(sb, J_ITEM);
        sb_cstr(sb, item->kind == N_RECORD ? "record" : item->kind == N_VARIANT ? "variant" : "fn");
        sb_cstr(sb, J_SIG);
        json_str_into(sb, sb_finish(sig));
        doc_json_shape(sb, item);
        sb_cstr(sb, J_DESC);
        json_str_into(sb, desc);
        if (example) {
            sb_cstr(sb, ",\"example\":");
            json_str_into(sb, example);
        }
        sb_cstr(sb, J_CLOSE);
    } else {
        sb_bytes(sb, sig->ptr, sig->len);
        sb_byte(sb, '\n');
        sb_cstr(sb, "    ");
        sb_str(sb, desc);
        sb_byte(sb, '\n');
        if (full && example) {
            sb_cstr(sb, "    example: ");
            sb_str(sb, example);
            sb_byte(sb, '\n');
        }
    }
    doc_out(sb);
}

/* ---- a module ---- */

/* The file id of an embedded std/ file, registered once; -1 when there is none. */
long doc_std_file(Str *path)
{
    long id = file_find(path);
    return id >= 0 ? id : std_lookup(path);
}

/* Checks std/MODULE.vo and prints its documented items (all of them, or the one named item_name);
 * returns how many were printed, or -1 when there is no such module. */
long doc_module(char *module_word, Str *item_name)
{
    SB *sb = sb_new(); /* the path std/MODULE.vo and the module's name std.MODULE */
    sb_cstr(sb, "std/");
    sb_cstr(sb, module_word);
    sb_cstr(sb, ".vo");
    long file = doc_std_file(sb_finish(sb));
    if (file < 0) return -1;
    check_program((int)file, 0);
    SB *mb = sb_new();
    sb_cstr(mb, "std.");
    sb_cstr(mb, module_word);
    Str *module = sb_finish(mb); /* std.MODULE */
    Vec *items = file_get((int)file)->ast->a.v;
    long printed = 0;
    for (long i = 0; i < items->n; i++) {
        Node *it = vec_get(items, i);
        if (it->kind != N_FN && it->kind != N_RECORD && it->kind != N_VARIANT) continue;
        if (item_name && !str_eq(it->a.n->a.s, item_name)) continue; /* only the named item */
        Str *found[2];
        doc_comments(file, items, i, found);
        if (!found[0]) continue; /* internal: not documented */
        if (doc_collect) { /* collecting: std.MODULE.ITEM */
            Str *dotted = str_concat(module, str_from_cstr("."));
            vec_push(doc_collect, str_concat(dotted, it->a.n->a.s));
            continue;
        }
        doc_print_item(module, it, found[0], found[1], item_name != 0);
        printed++;
    }
    return printed;
}

/* ---- builtins (the reference's table) ---- */

/* The first `| ` row of the reference's Builtins table after its header and separator rows. */
char *doc_builtin_rows(void)
{
    char *p = (char *)ref_md;
    while (*p && !doc_starts(p, "## Builtins")) p = doc_next_line(p);
    if (!*p) return 0;
    do { /* skip to the table: header, then separator, then rows */
        p = doc_next_line(p);
        if (!*p) return 0;
    } while (*p != '|');
    return doc_next_line(doc_next_line(p)); /* past the header and the separator rows */
}

char *doc_next_line(char *p)
{
    while (*p)
        if (*p++ == '\n') break;
    return p;
}

long doc_starts(char *p, char *prefix)
{
    for (; *prefix; p++, prefix++)
        if (*p != *prefix) return 0;
    return 1;
}

/* The index-th cell of a table row, trimmed, with `\|` unescaped; the signature cell (1) also
 * without its backquotes. */
Str *doc_cell(char *row, long index)
{
    char *p = row;
    long cell = -1; /* cells start after each unescaped `|` */
    for (;;) {
        int c = (unsigned char)*p;
        if (!c || c == '\n') return str_new(0, 0);
        if (c == '\\') { /* `\|` */
            p += 2;
            continue;
        }
        if (c == '|' && ++cell == index) break;
        p++;
    }
    p++;
    while (*p == ' ') p++; /* skip leading spaces */
    SB *sb = sb_new();
    for (;;) { /* copy to the next unescaped `|` */
        int c = (unsigned char)*p;
        if (!c || c == '\n' || c == '|') break;
        if (c == '\\' && p[1] == '|') { /* `\|` is `|` */
            p++;
            c = '|';
        }
        sb_byte(sb, c);
        p++;
    }
    char *s = sb->ptr; /* trim trailing spaces, then (signature) the backquotes */
    long n = sb->len;
    while (n && s[n - 1] == ' ') n--;
    if (index == 1 && n >= 2 && s[0] == '`' && s[n - 1] == '`') {
        s++;
        n -= 2;
    }
    return str_new(s, n);
}

/* 1 when name is one of the ", "-separated names of a builtin row ("upper, lower"). */
long doc_names_has(Str *names, Str *name)
{
    char *p = names->data, *end = names->data + names->len;
    for (;;) {
        char *q = p; /* the name's end: the next "," or the end */
        while (q < end && *q != ',') q++;
        if (q - p == name->len) {
            long i = 0;
            while (i < name->len && p[i] == name->data[i]) i++;
            if (i == name->len) return 1;
        }
        p = q + 2; /* past ", " */
        if (p >= end) return 0;
    }
}

/* Prints every builtin row (name 0) or the one with this name; returns how many. */
long doc_builtins(Str *name)
{
    long printed = 0;
    for (char *row = doc_builtin_rows(); row && *row == '|'; row = doc_next_line(row)) {
        Str *names = doc_cell(row, 0);
        if (name && !doc_names_has(names, name)) continue;
        if (doc_collect) { /* collecting names for a suggestion */
            vec_push(doc_collect, names);
            continue;
        }
        Str *sig = doc_cell(row, 1);
        Str *notes = doc_cell(row, 2);
        doc_print_builtin(names, sig, notes);
        printed++;
    }
    return printed;
}

void doc_print_builtin(Str *names, Str *signature, Str *notes)
{
    SB *sb = sb_new();
    if (opt_json) {
        sb_cstr(sb, J_HEAD);
        json_str_into(sb, str_from_cstr("builtins"));
        sb_cstr(sb, J_NAME);
        json_str_into(sb, names);
        sb_cstr(sb, J_ITEM);
        sb_cstr(sb, "builtin");
        sb_cstr(sb, J_SIG);
        json_str_into(sb, signature);
        sb_cstr(sb, J_DESC);
        json_str_into(sb, notes);
        sb_cstr(sb, J_CLOSE);
    } else { /* NAMES SIGNATURE, then the notes indented */
        sb_str(sb, names);
        sb_byte(sb, ' ');
        sb_str(sb, signature);
        sb_byte(sb, '\n');
        if (notes->len) {
            sb_cstr(sb, "    ");
            sb_str(sb, notes);
            sb_byte(sb, '\n');
        }
    }
    doc_out(sb);
}

/* ---- the module list ---- */

/* Each standard module with the description from its first line (`# std.json: ...`). */
void doc_module_list(void)
{
    for (char *const *m = doc_modules; *m; m++) {
        SB *sb = sb_new(); /* the file's first line after "# std.MODULE: " */
        sb_cstr(sb, "std/");
        sb_cstr(sb, *m);
        sb_cstr(sb, ".vo");
        long file = doc_std_file(sb_finish(sb));
        if (file < 0) continue;
        FileRec *f = file_get((int)file);
        char *text = f->text, *eol = text;
        while (eol - text < f->len && *eol != '\n') eol++;
        char *from = text + cstr_len(*m) + 8; /* skip "# std." + name + ": " */
        Str *desc = from < eol ? str_new(from, eol - from) : str_new(0, 0);
        SB *nb = sb_new();
        sb_cstr(nb, "std.");
        sb_cstr(nb, *m);
        doc_print_module(sb_finish(nb), 0, desc);
    }
}

void doc_print_module(Str *name, void *unused, Str *description)
{
    (void)unused;
    SB *sb = sb_new();
    if (opt_json) {
        sb_cstr(sb, J_HEAD);
        json_str_into(sb, name);
        sb_cstr(sb, J_NAME);
        json_str_into(sb, name);
        sb_cstr(sb, J_ITEM);
        sb_cstr(sb, "module");
        sb_cstr(sb, J_SIG);
        json_str_into(sb, name);
        sb_cstr(sb, J_DESC);
        json_str_into(sb, description);
        sb_cstr(sb, J_CLOSE);
    } else {
        sb_cstr(sb, "  ");
        sb_str(sb, name);
        sb_byte(sb, ' ');
        sb_str(sb, description);
        sb_byte(sb, '\n');
    }
    doc_out(sb);
}

/* ---- the command ---- */

/* T0010 doc_not_found, exit 64, with the closest documented name when one is within two edits. */
void doc_not_found(Str *name)
{
    Vec *names = vec_new(); /* every documented name */
    doc_collect = names;
    doc_builtins(0);
    for (char *const *m = doc_modules; *m; m++) {
        doc_module(*m, 0);
        Str *prefix = str_from_cstr("std.");
        vec_push(names, str_concat(prefix, str_from_cstr(*m)));
    }
    Str *best = 0; /* the closest: distance below 3, first wins */
    long dist = 3;
    for (long i = 0; i < names->n; i++) {
        Str *cand = vec_get(names, i);
        long d = name_distance(cand, name);
        if (d >= dist) continue;
        dist = d;
        best = cand;
    }
    Str *m = msg("Nothing is documented as `$1`; `vo doc` lists the builtins and modules.", name, 0, 0);
    if (dist != 3) m = msg("$1 Did you mean `$2`?", m, best, 0);
    toolchain_error((int)opt_json, (int)report_fd, "T0010", m, 0);
    vo_exit(EXIT_USAGE);
}

void doc_summary(long items)
{
    if (!opt_json) return;
    SB *sb = sb_new();
    sb_cstr(sb, ",\"items\":");
    int_to_dec(sb, items);
    Str *extra = sb_finish(sb);
    summary_emit((int)opt_json, (int)report_fd, cmd_name(), 0, extra);
}

/* `vo doc [--json] [NAME]` (contracts/cli.md). */
int cmd_doc(void)
{
    if (!pos_n) { /* no NAME: builtins, then modules */
        if (!opt_json) out_cstr(1, "builtins:\n");
        long n = doc_builtins(0);
        if (!opt_json) out_cstr(1, "modules:\n");
        doc_module_list();
        doc_summary(n + 5);
        return 0;
    }
    if (pos_n != 1)
        usage_error(str_from_cstr("`doc` takes at most one name: `vo doc std.json` or `vo doc std.json.parse_json`."));
    char *name = ((char **)pos_args)[0];
    Str *written = str_from_cstr(name); /* NAME as written, for the message */
    if (doc_starts(name, "std.")) { /* std.MODULE[.ITEM] */
        char *module = name + 4;
        long dot = cstr_index_doc(module, '.');
        if (dot >= 0) { /* split at the dot: module word, item name */
            module[dot] = 0;
            if (doc_module(module, str_from_cstr(module + dot + 1)) > 0) {
                doc_summary(1);
                return 0;
            }
        } else { /* a module */
            long n = doc_module(module, 0);
            if (n > 0) {
                doc_summary(n);
                return 0;
            }
        }
    } else if (doc_builtins(str_from_cstr(name)) > 0) { /* a builtin */
        doc_summary(1);
        return 0;
    }
    doc_not_found(written);
    return 0;
}

long cstr_index_doc(char *s, int c)
{
    for (char *p = s; *p; p++)
        if ((unsigned char)*p == c) return p - s;
    return -1;
}
