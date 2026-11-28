/* vo doc (spec 007 US1, research R7; ported from src/doc.s, spec 008): exact signatures from the
 * checker's own declarations, descriptions and examples from the comments above each item in the
 * library source, builtins from the reference's table. `vo doc [--json] [NAME]` (contracts/cli.md). */
#include "vo.h"

static char *const doc_modules[] = {"json", "time", "text", "random", "http", "collections", "fs", "process", "math", "bytes", "regex", "sqlite", "web", "option", "crypto", "tcp", "bits", "decimal", "security", "lists", "workers", "log", "metrics", "sse", "websocket", 0};

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
 * the comments on the lines right above it, or right above its annotations (spec 027: `@allow` stands
 * between a function's documentation and the function). Both 0 when it has none (an internal item). */
void doc_comments(long file, Vec *items, long index, Str **out)
{
    out[0] = 0;
    out[1] = 0;
    long next = pos_line((int)file, ((Node *)vec_get(items, index))->start); /* the line below a comment */
    long top = index; /* the topmost comment of the block */
    int comments = 0;
    while (top > 0) {
        Node *c = vec_get(items, top - 1);
        if (c->kind != N_COMMENT && (c->kind != N_ANNOTATION || comments)) break;
        long line = pos_line((int)file, c->start);
        if (line + 1 != next) break;
        next = line;
        comments |= c->kind == N_COMMENT;
        top--;
    }
    SB *desc = 0; /* spec 023: every description line, joined by spaces (std uses one line) */
    GC_ROOT(&desc);
    for (long i = top; i < index; i++) {
        Node *c = vec_get(items, i);
        if (c->kind != N_COMMENT) continue; /* an annotation */
        if (doc_is_example(file, c)) {
            if (!out[1]) out[1] = doc_line_text(file, c, 5); /* the topmost example line wins */
            continue;
        }
        if (!desc) desc = sb_new();
        else sb_byte(desc, ' ');
        sb_str(desc, doc_line_text(file, c, 1));
    }
    if (desc) out[0] = sb_finish(desc);
    GC_UNROOT(1);
}

/* spec 018: a test's documentation: every comment line directly above items[index] (above its
 * annotations), `#` and one space removed, joined by newlines; "" when there is none. */
Str *doc_block(long file, Vec *items, long index)
{
    long next = pos_line((int)file, ((Node *)vec_get(items, index))->start);
    long top = index; /* the first comment of the block */
    for (long i = index - 1; i >= 0; i--) {
        Node *c = vec_get(items, i);
        if (c->kind != N_COMMENT && c->kind != N_ANNOTATION) break;
        long line = pos_line((int)file, c->start);
        if (line + 1 != next) break;
        next = line;
        if (c->kind == N_COMMENT) top = i;
        else if (top != index) break; /* an annotation above the comments ends the block */
    }
    SB *sb = sb_new();
    GC_ROOT(&sb);
    for (long i = top; i < index; i++) {
        Node *c = vec_get(items, i);
        if (c->kind != N_COMMENT) continue;
        if (sb->len) sb_byte(sb, '\n');
        char *text = file_get((int)file)->text;
        long from = (long)c->start + 1; /* past `#`, then one space */
        if (from < c->end && text[from] == ' ') from++;
        sb_bytes(sb, text + from, c->end - from);
    }
    GC_UNROOT(1);
    return sb_finish(sb);
}

void doc_out(SB *sb)
{
    out_bytes(1, sb->ptr, sb->len);
}

/* ---- signatures ---- */

/* [T, U: ordered, E: effects] when the item has type parameters. */
void doc_tparams(SB *sb, Node *decl)
{
    Vec *tps = ty_tparams_of(decl); /* (records and variants keep theirs on the declaration) */
    if (!tps || !tps->n) return;
    sb_byte(sb, '[');
    for (long i = 0; i < tps->n; i++) {
        if (i) sb_cstr(sb, ", ");
        Node *tp = vec_get(tps, i);
        sb_str(sb, tp->a.n->a.s);
        if (tp->b.i == TPC_ORDERED)
            sb_cstr(sb, ": ordered");
        else if (tp->b.i == TPC_EFFECTS)
            sb_cstr(sb, ": effects");
        else if (tp->b.i == TPC_JSON)
            sb_cstr(sb, ": json");
        else if (tp->b.i == TPC_KEY)
            sb_cstr(sb, ": key");
    }
    sb_byte(sb, ']');
}

/* fn name[T](a: A, b: B) -> R uses E; record Name { f: T; ... }; variant Name { c(T); d; }. */
/* ` { f: T; ... }`: a record's fields (spec 060: also a case's, from its hidden record). */
static void doc_fields(SB *sb, Node *record)
{
    sb_cstr(sb, " { ");
    for (long i = 0;; i++) {
        Node *m = member_at(record->b.v, i);
        if (!m) break;
        sb_str(sb, m->a.n->a.s);
        sb_cstr(sb, ": ");
        ty_name_into(sb, m->b.n->type);
        sb_cstr(sb, "; ");
    }
    sb_byte(sb, '}');
}

void doc_signature(SB *sb, Node *item)
{
    if (item->kind == N_CONST) { /* spec 031: const name: type = value */
        sb_cstr(sb, "const ");
        sb_str(sb, item->a.n->a.s);
        if (ty_bad(item->type)) return;
        sb_cstr(sb, ": ");
        sb_str(sb, ty_name(item->type));
        sb_cstr(sb, " = ");
        render_value(sb, item->c.n->a.i, item->type);
        return;
    }
    if (item->kind == N_EFFECTS) { /* spec 030: effects Name = e, ... (and what it stands for) */
        sb_cstr(sb, "effects ");
        sb_str(sb, item->a.n->a.s);
        sb_cstr(sb, " = ");
        fx_render_written(sb, item->b.n);
        if (!fx_names_set(item->b.n) || !item->c.p) return;
        sb_cstr(sb, " (");
        es_render(item->c.p, sb);
        sb_byte(sb, ')');
        return;
    }
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
        Fx *fx = item_fx(item); /* effects */
        int effects = fx && fx->declared && !es_is_empty(fx->declared);
        if (item->type && item->type->kind != TY_VOID) { /* the return type, unless void */
            int open = effects && item->type->kind == TY_FUNC && es_is_empty(item->type->decl.es);
            sb_cstr(sb, open ? " -> (" : " -> "); /* a pure function type would take the ` uses` */
            ty_name_into(sb, item->type);
            if (open) sb_byte(sb, ')');
        }
        if (!effects) return;
        sb_cstr(sb, " uses ");
        if (fx_names_set(fx->uses)) { /* spec 030: as written, then what it stands for */
            fx_render_written(sb, fx->uses);
            sb_cstr(sb, " (");
            es_render(fx->declared, sb);
            sb_byte(sb, ')');
            return;
        }
        es_render(fx->declared, sb);
        return;
    }
    int record = item->kind == N_RECORD; /* record Name { f: T; ... } */
    sb_cstr(sb, record ? "record " : "variant ");
    sb_str(sb, item->a.n->a.s);
    doc_tparams(sb, item);
    if (record) {
        doc_fields(sb, item);
        return;
    }
    sb_cstr(sb, " { ");
    for (long i = 0;; i++) {
        Node *m = member_at(item->b.v, i);
        if (!m) break;
        sb_str(sb, m->a.n->a.s);
        if (m->c.n) { /* spec 060: a case with fields */
            doc_fields(sb, m->c.n);
            sb_byte(sb, ' ');
            continue;
        } else if (m->b.n) { /* the case's type */
            sb_byte(sb, '(');
            ty_name_into(sb, m->b.n->type);
            sb_byte(sb, ')');
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
    if (item->kind == N_CONST) return; /* spec 031: the signature says it all */
    if (item->kind == N_EFFECTS) { /* spec 030: the effects it stands for */
        sb_cstr(sb, ",\"effects\":");
        SB *eff = sb_new();
        if (item->c.p) es_render(item->c.p, eff);
        doc_json_split(sb, sb_finish(eff));
        return;
    }
    sb_cstr(sb, item->kind == N_RECORD ? ",\"fields\":[" : ",\"cases\":["); /* fields or cases */
    for (long i = 0;; i++) {
        Node *m = member_at(item->b.v, i);
        if (!m) break;
        if (i) sb_byte(sb, ',');
        doc_json_pair(sb, m->a.n->a.s, m->b.n ? m->b.n->type : 0);
        if (m->c.n) { /* spec 060: a case with fields, `"fields":[...]` inside its pair */
            sb->len--;
            sb_cstr(sb, ",\"fields\":[");
            for (long j = 0;; j++) {
                Node *f = member_at(m->c.n->b.v, j);
                if (!f) break;
                if (j) sb_byte(sb, ',');
                doc_json_pair(sb, f->a.n->a.s, f->b.n->type);
            }
            sb_cstr(sb, "]}");
        }
    }
    sb_byte(sb, ']');
}

/* ---- one item ---- */

/* Text: the signature, the description indented, and (full) the example; or one JSON record. */
/* spec 023: the fields of an item's doc record, from "module" on (vo doc prints them in its record,
 * vo query type-at as "doc"); desc 0 leaves "description" out. */
void doc_item_fields(SB *sb, Str *module, Node *item, Str *desc, Str *example)
{
    SB *sig = sb_new(); /* the signature on its own */
    doc_signature(sig, item);
    sb_cstr(sb, "\"module\":");
    json_str_into(sb, module);
    sb_cstr(sb, J_NAME);
    json_str_into(sb, item->a.n->a.s);
    sb_cstr(sb, J_ITEM);
    sb_cstr(sb, item->kind == N_RECORD    ? "record"
                : item->kind == N_VARIANT ? "variant"
                : item->kind == N_EFFECTS ? "effects"
                : item->kind == N_CONST   ? "const"
                                          : "fn");
    sb_cstr(sb, J_SIG);
    json_str_into(sb, sb_finish(sig));
    doc_json_shape(sb, item);
    if (desc) {
        sb_cstr(sb, J_DESC);
        json_str_into(sb, desc);
    }
    if (example) {
        sb_cstr(sb, ",\"example\":");
        json_str_into(sb, example);
    }
}

void doc_print_item(Str *module, Node *item, Str *desc, Str *example, long full)
{
    SB *sb = sb_new();
    SB *sig = sb_new(); /* the signature on its own */
    doc_signature(sig, item);
    Str *use = 0; /* spec 058: how a program writes the item: `json.parse_json` after `import std.json;` */
    if (full && module && item->a.n && (item->flags & NF_EXPORT)) {
        long last = module->len;
        while (last > 0 && module->data[last - 1] != '.') last--;
        use = msg("$1.$2", str_new(module->data + last, module->len - last), item->a.n->a.s, 0);
    }
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"doc\",");
        doc_item_fields(sb, module, item, desc, example);
        if (use) {
            sb_cstr(sb, ",\"use\":");
            json_str_into(sb, use);
        }
        sb_cstr(sb, J_CLOSE);
    } else {
        sb_bytes(sb, sig->ptr, sig->len);
        sb_byte(sb, '\n');
        if (desc && desc->len) { /* none for an exported package item without a doc comment */
            sb_cstr(sb, "    ");
            sb_str(sb, desc);
            sb_byte(sb, '\n');
        }
        if (use) {
            sb_cstr(sb, "    use: import ");
            sb_str(sb, module);
            sb_cstr(sb, "; then ");
            sb_str(sb, use);
            sb_byte(sb, '\n');
        }
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
    return doc_file(file, sb_finish(mb), item_name);
}

/* The checked file's exported items as members of module (all of them, or the one named
 * item_name): printed, or collected into doc_collect; returns how many were printed. */
long doc_file(long file, Str *module, Str *item_name)
{
    Vec *items = file_get((int)file)->ast->a.v;
    long printed = 0;
    for (long i = 0; i < items->n; i++) {
        Node *it = vec_get(items, i);
        if (it->kind != N_FN && it->kind != N_RECORD && it->kind != N_VARIANT && it->kind != N_EFFECTS &&
            it->kind != N_CONST)
            continue;
        if (item_name && !str_eq(it->a.n->a.s, item_name)) continue; /* only the named item */
        if (!(it->flags & NF_EXPORT)) continue; /* spec 010: private to its file */
        Str *found[2];
        doc_comments(file, items, i, found);
        if (!found[0]) found[0] = str_from_cstr(""); /* exported without a doc comment (packages) */
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

/* ---- packages (spec 007 T044): modules of the nearest vo.packages from the working directory ---- */

/* Loads and checks a package module's file; a module with check errors stops `vo doc` (exit 65). */
static long doc_package_file(Str *path)
{
    long file = load_source(path);
    check_program((int)file, 0);
    if (diag_errors) vo_exit(finish_check(1));
    return file;
}

/* NAME as a package module or item (`money.format`, `money.format.cents`): prints it and returns
 * how many items were printed; -1 when NAME names no module of a listed package. */
long doc_package(Str *name)
{
    long dot = 0;
    while (dot < name->len && name->data[dot] != '.') dot++;
    if (dot == 0 || dot >= name->len) return -1;
    void *entry = pkg_for_doc(str_new(name->data, dot));
    if (!entry) return -1;
    Str *module = name, *item = 0;
    Str *path = pkg_doc_path(entry, str_new(name->data + dot + 1, name->len - dot - 1));
    if (!path) { /* MODULE.ITEM: split at the last dot */
        long last = name->len - 1;
        while (last > dot && name->data[last] != '.') last--;
        if (last == dot) return -1;
        module = str_new(name->data, last);
        item = str_new(name->data + last + 1, name->len - last - 1);
        path = pkg_doc_path(entry, str_new(name->data + dot + 1, last - dot - 1));
        if (!path) return -1;
    }
    return doc_file(doc_package_file(path), module, item);
}

/* The module's description: its first line, `# NAME.module: text` (or `# text`). */
static Str *doc_package_description(long file, Str *module)
{
    FileRec *f = file_get((int)file);
    char *text = f->text, *eol = text;
    while (eol - text < f->len && *eol != '\n') eol++;
    if (eol - text < 2 || text[0] != '#' || text[1] != ' ') return str_new(0, 0);
    char *from = text + 2;
    if (eol - from > module->len + 1 && mem_eq(from, module->data, module->len) && from[module->len] == ':')
        from += module->len + 1;
    while (from < eol && *from == ' ') from++;
    return str_new(from, eol - from);
}

/* Lists every module of every listed package after the standard ones; returns how many. */
long doc_package_list(void)
{
    pkg_load(rt_start_cwd());
    long n = 0;
    for (long i = 0; pkg_entries && i < pkg_entries->n; i++) {
        void *entry = pkg_for_doc(pkg_entry_name(vec_get(pkg_entries, i)));
        Vec *mods = pkg_doc_modules(entry);
        for (long k = 0; k < mods->n; k++) {
            Str *module = vec_get(mods, k);
            Str *rel = str_new(module->data + pkg_entry_name(entry)->len + 1,
                               module->len - pkg_entry_name(entry)->len - 1);
            long file = load_source(pkg_doc_path(entry, rel));
            doc_print_module(module, 0, doc_package_description(file, module));
            n++;
        }
    }
    return n;
}

/* Collects every package module and documented item name into doc_collect (for T0010). */
void doc_package_names(void)
{
    pkg_load(rt_start_cwd());
    for (long i = 0; pkg_entries && i < pkg_entries->n; i++) {
        void *entry = pkg_for_doc(pkg_entry_name(vec_get(pkg_entries, i)));
        Vec *mods = pkg_doc_modules(entry);
        for (long k = 0; k < mods->n; k++) {
            Str *module = vec_get(mods, k);
            vec_push(doc_collect, module);
            Str *rel = str_new(module->data + pkg_entry_name(entry)->len + 1,
                               module->len - pkg_entry_name(entry)->len - 1);
            doc_file(doc_package_file(pkg_doc_path(entry, rel)), module, 0);
        }
    }
}

/* ---- builtins (the reference's table) ---- */

/* The first `| ` row of the reference's Builtins table after its header and separator rows. */
/* The first row of the first builtin table at or after p (a `| Builtin | Signature | Notes |` header),
 * or 0. */
static char *doc_table_from(char *p)
{
    for (; p && *p; p = doc_next_line(p))
        if (doc_starts(p, "| Builtin | Signature | Notes |")) return doc_next_line(doc_next_line(p));
    return 0;
}

/* spec 023: every builtin table of the reference, in order (the main one under "## Builtins", and the
 * others such as the functions-as-values table), so every named builtin is documented. */
char *doc_builtin_rows(void)
{
    return doc_table_from((char *)ref_md);
}

/* The next builtin row after row: the next line of the table, or the first row of the next table. */
char *doc_next_builtin_row(char *row)
{
    char *p = doc_next_line(row);
    return p && *p == '|' ? p : doc_table_from(p);
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
    for (char *row = doc_builtin_rows(); row && *row == '|'; row = doc_next_builtin_row(row)) {
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

/* spec 023: the fields of a builtin's doc record, from "module" on. */
void doc_builtin_fields(SB *sb, Str *names, Str *signature, Str *notes)
{
    sb_cstr(sb, "\"module\":");
    json_str_into(sb, str_from_cstr("builtins"));
    sb_cstr(sb, J_NAME);
    json_str_into(sb, names);
    sb_cstr(sb, J_ITEM);
    sb_cstr(sb, "builtin");
    sb_cstr(sb, J_SIG);
    json_str_into(sb, signature);
    sb_cstr(sb, J_DESC);
    json_str_into(sb, notes);
}

void doc_print_builtin(Str *names, Str *signature, Str *notes)
{
    SB *sb = sb_new();
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"doc\",");
        doc_builtin_fields(sb, names, signature, notes);
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
    doc_package_names();
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
static Str *doc_module_of(long file); /* below, with the hover records */

/* spec 023: vo doc ENTRY.vo NAME: the item NAME of the program (any of its files, exported or not),
 * printed as library items are; T0010 with the nearest name when it has none. */
static int doc_program(char *entry, Str *name)
{
    q_load(entry);
    Vec *names = vec_new();
    for (long id = 0; id < file_count; id++) {
        FileRec *f = file_get((int)id);
        if (f->std || !f->ast) continue;
        Vec *items = f->ast->a.v;
        for (long i = 0; i < items->n; i++) {
            Node *it = vec_get(items, i);
            if (it->kind != N_FN && it->kind != N_RECORD && it->kind != N_VARIANT && it->kind != N_EFFECTS &&
            it->kind != N_CONST)
            continue;
            if (!str_eq(it->a.n->a.s, name)) {
                vec_push(names, it->a.n->a.s);
                continue;
            }
            Str *found[2];
            doc_comments(id, items, i, found);
            doc_print_item(doc_module_of(id), it, found[0], found[1], 1);
            doc_summary(1);
            return 0;
        }
    }
    Str *best = 0; /* the closest name, distance below 3 */
    long dist = 3;
    for (long i = 0; i < names->n; i++) {
        long d = name_distance(vec_get(names, i), name);
        if (d >= dist) continue;
        dist = d;
        best = vec_get(names, i);
    }
    Str *m = msg("Nothing is documented as `$1` in $2.", name, str_from_cstr(entry), 0);
    if (best) m = msg("$1 Did you mean `$2`?", m, best, 0);
    toolchain_error((int)opt_json, (int)report_fd, "T0010", m, 0);
    vo_exit(EXIT_USAGE);
    return 0;
}

int cmd_doc(void)
{
    if (!pos_n) { /* no NAME: builtins, then modules */
        if (!opt_json) out_cstr(1, "builtins:\n");
        long n = doc_builtins(0);
        if (!opt_json) out_cstr(1, "modules:\n");
        doc_module_list();
        long p = doc_package_list(); /* then the nearest manifest's package modules */
        doc_summary(n + 5 + p);
        return 0;
    }
    if (pos_n == 2) { /* spec 023: vo doc ENTRY.vo NAME, an item of the program */
        char *entry = ((char **)pos_args)[0];
        long n = cstr_len(entry);
        if (n > 3 && entry[n - 3] == '.' && entry[n - 2] == 'v' && entry[n - 1] == 'o')
            return doc_program(entry, str_from_cstr(((char **)pos_args)[1]));
    }
    if (pos_n != 1)
        usage_error(str_from_cstr("`doc` takes one name (`vo doc std.json`, `vo doc std.json.parse_json`) or a "
                                  "program and a name (`vo doc main.vo NAME`)."));
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
    } else if (cstr_index_doc(name, '.') >= 0) { /* a package module or item */
        long n = doc_package(written);
        if (n > 0) {
            doc_summary(n);
            return 0;
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

/* ---- spec 023: the doc record of a name (vo query type-at, vo doc ENTRY NAME) ---- */

long doc_entry_file = -1;

/* The module of a file: std.x for std/x.vo; else the path relative to the entry's directory, without
 * .vo, with / as . (geo, util.text, main). */
static Str *doc_module_of(long file)
{
    FileRec *f = file_get((int)file);
    Str *p = f->path;
    Str *pkg = f->std ? 0 : pkg_relpath(p); /* spec 058: "@units/sizes.vo" is the module units.sizes */
    if (pkg) p = str_new(pkg->data + 1, pkg->len - 1);
    long from = 0, to = p->len;
    if (to > 3 && mem_eq(p->data + to - 3, ".vo", 3)) to -= 3;
    if (!f->std && !pkg && doc_entry_file >= 0) { /* strip the entry's directory */
        Str *e = file_get((int)doc_entry_file)->path;
        long slash = e->len;
        while (slash > 0 && e->data[slash - 1] != '/') slash--;
        if (slash && slash <= p->len && mem_eq(p->data, e->data, slash)) from = slash;
    }
    SB *sb = sb_new();
    for (long i = from; i < to; i++) sb_byte(sb, p->data[i] == '/' ? '.' : p->data[i]);
    return sb_finish(sb);
}

/* Spec 058: a module's name as `vo doc` writes it (std.json, geo) and its description (the first comment
 * line of its file, without a leading `NAME:`), for hover on a module name. */
Str *doc_module_name(long file)
{
    return doc_module_of(file);
}

Str *doc_module_line(long file)
{
    return doc_package_description(file, doc_module_of(file));
}

/* Writes the fields of the doc record of what (key_a, key_b) of the reference log names (an item, a
 * field or case, a builtin with a row) into sb and returns 1, with its signature and description (0
 * when none) in *sig and *desc; returns 0 for anything else (locals, parameters, type parameters). */
int doc_record_for(SB *sb, long key_a, long key_b, Str **sig, Str **desc)
{
    *sig = 0;
    *desc = 0;
    if (!key_a) { /* a builtin by id: its row of the reference's table */
        const char *name = builtin_name_of(key_b);
        if (!name) return 0;
        Str *n = str_from_cstr((char *)name);
        for (char *row = doc_builtin_rows(); row && *row == '|'; row = doc_next_builtin_row(row)) {
            Str *names = doc_cell(row, 0);
            if (!doc_names_has(names, n)) continue;
            Str *s = doc_cell(row, 1), *notes = doc_cell(row, 2);
            doc_builtin_fields(sb, names, s, notes);
            SB *full = sb_new();
            sb_str(full, names);
            sb_byte(full, ' ');
            sb_str(full, s);
            *sig = sb_finish(full);
            *desc = notes->len ? notes : 0;
            return 1;
        }
        return 0;
    }
    Node *decl = (Node *)key_a;
    if (decl->kind != N_FN && decl->kind != N_RECORD && decl->kind != N_VARIANT && decl->kind != N_EFFECTS &&
        decl->kind != N_CONST)
        return 0; /* a local */
    long file = (long)decl->file;
    Str *module = doc_module_of(file);
    Str *found[2];
    if (key_b < 0) { /* the item itself (-2: a record literal) */
        Vec *items = file_get((int)file)->ast->a.v;
        long at = -1;
        for (long i = 0; i < items->n && at < 0; i++)
            if (vec_get(items, i) == decl) at = i;
        if (at < 0) return 0;
        doc_comments(file, items, at, found);
        doc_item_fields(sb, module, decl, found[0], found[1]);
        SB *s = sb_new();
        doc_signature(s, decl);
        *sig = sb_finish(s);
        *desc = found[0];
        return 1;
    }
    if (decl->kind == N_FN || decl->kind == N_EFFECTS || decl->kind == N_CONST) return 0;
    Vec *members = decl->b.v; /* a field or case: the key_b-th member, comments not counted */
    long at = -1;
    for (long i = 0, k = 0; i < members->n && at < 0; i++) {
        if (((Node *)vec_get(members, i))->kind == N_COMMENT) continue;
        if (k++ == key_b) at = i;
    }
    if (at < 0) return 0;
    Node *m = vec_get(members, at);
    doc_comments(file, members, at, found);
    int record = decl->kind == N_RECORD;
    SB *s = sb_new(); /* x: float / circle(float) / none */
    sb_str(s, m->a.n->a.s);
    if (m->b.n) {
        sb_cstr(s, record ? ": " : "(");
        ty_name_into(s, m->b.n->type);
        if (!record) sb_byte(s, ')');
    }
    *sig = sb_finish(s);
    *desc = found[0];
    sb_cstr(sb, "\"module\":");
    json_str_into(sb, module);
    sb_cstr(sb, J_NAME);
    json_str_into(sb, m->a.n->a.s);
    sb_cstr(sb, J_ITEM);
    sb_cstr(sb, record ? "field" : "case");
    sb_cstr(sb, "\",\"owner\":");
    json_str_into(sb, decl->a.n->a.s);
    sb_cstr(sb, ",\"signature\":");
    json_str_into(sb, *sig);
    if (found[0]) {
        sb_cstr(sb, J_DESC);
        json_str_into(sb, found[0]);
    }
    return 1;
}
