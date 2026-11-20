/* The effects record `vo.effects` (contracts/effects-record.md; ported from src/effects_record.s,
 * spec 008).
 *
 * An entry is one top-level function or test: path (relative to the project root), name
 * (`name` or `test "name"`), declared effect set, and where to point diagnostics (the function's
 * name, or the record line for entries read from the file). The record is found in the nearest
 * ancestor directory of the entry file and registered in the file table, so E0510 and W0502 can
 * point at its lines. Comparing the record with the program yields changes: added, widened
 * (growth: E0504 in `vo check`), removed, narrowed (W0502) and renamed (W0503). */
#include "vo.h"

typedef struct Rec {
    Str *path;
    Str *name;
    EffSet *set;
    Node *item; /* the function or test node, 0 for record entries */
    long file;
    long start;
    long end;
    long used; /* matched by a change already */
} Rec;
CHECK_SIZE(Rec, 64);

typedef struct RecChange {
    long kind;
    Rec *new_;
    Rec *old;
    EffSet *delta;
} RecChange;
CHECK_SIZE(RecChange, 32);

enum { CH_ADDED = 1, CH_WIDENED, CH_REMOVED, CH_NARROWED, CH_RENAMED };

static const char *const change_words[] = {0, "added", "widened", "removed", "narrowed", "renamed"};

#define RECORD "/vo.effects"
#define J_FILE ",\"file\":"

Str *rec_root; /* the project root (absolute, "" for /) */

/* The file's path relative to the project root (absolute when it lies outside the root);
 * "@NAME/path.vo" for a file of a listed package (spec 007). */
Str *rec_relpath(int file)
{
    Str *path = rt_resolve(file_get(file)->path);
    if (!path) return 0;
    Str *in_pkg = pkg_relpath(path);
    if (in_pkg) return in_pkg;
    Str *root = rec_root;
    if (!root || !path_under(path, root)) return path;
    long k = root->len + 1; /* strip "<root>/" */
    return str_new(path->data + k, path->len - k);
}

/* Registers the nearest vo.effects (entry's directory and up) and returns its file id, or -1.
 * Sets rec_root to its directory, or to the entry's directory when there is none. */
long rec_find(int entry)
{
    return find_up(entry, RECORD, (void **)&rec_root);
}

/* Registers the nearest file named slash_name ("/vo.effects") in entry's directory or above and
 * returns its file id, or -1. *dir_out: its directory, or the entry's directory when there is none. */
long find_up(int entry, char *slash_name, void **dir_out)
{
    Str *dir = rt_resolve(file_get(entry)->dir);
    if (!dir) dir = rt_start_cwd();
    return find_up_dir(dir, slash_name, dir_out);
}

/* find_up from an absolute directory ("" is /). */
long find_up_dir(Str *dir, char *slash_name, void **dir_out)
{
    *dir_out = dir;
    for (;;) {
        SB *sb = sb_new(); /* <dir>/NAME */
        sb_str(sb, dir);
        sb_cstr(sb, slash_name);
        Str *path = sb_finish(sb);
        char *text;
        long len = read_whole_file(path->data, &text);
        if (len >= 0) {
            int id = file_add(path, text, len);
            *dir_out = dir;
            return id;
        }
        long k = dir->len; /* up one directory */
        if (!k) return -1;
        do k--;
        while (dir->data[k] != '/' && k);
        dir = str_new(dir->data, k);
    }
}

void *rec_entry(Str *path, Str *name, EffSet *set)
{
    Rec *e = alloc(sizeof(Rec));
    e->path = path;
    e->name = name;
    e->set = set;
    return e;
}

/* Order of entries: path, then name, byte-wise. */
int rec_cmp(void *a, void *b)
{
    Rec *x = a, *y = b;
    int c = str_cmp(x->path, y->path);
    return c ? c : str_cmp(x->name, y->name);
}

/* Insertion sort by rec_cmp (stable). */
void rec_sort(Vec *v)
{
    W *d = v->data;
    long n = v->n;
    for (long i = 1; i < n; i++) {
        for (long j = i; j > 0 && rec_cmp(d[j - 1].p, d[j].p) > 0; j--) {
            W t = d[j - 1];
            d[j - 1] = d[j];
            d[j] = t;
        }
    }
}

/* Entries of every function and test in the program's files (a vec of file ids), sorted. */
Vec *rec_entries(Vec *files)
{
    Vec *out = vec_new();
    for (long i = 0; i < files->n; i++) {
        int id = (int)(long)vec_get(files, i);
        if (file_get(id)->std) continue; /* spec 007: the standard library is not in the record */
        Str *path = rec_relpath(id);
        Node *ast = file_get(id)->ast;
        if (!ast) continue;
        Vec *items = ast->a.v;
        for (long j = 0; j < items->n; j++) {
            Node *it = vec_get(items, j);
            Fx *fx = item_fx(it);
            if (!fx) continue;
            EffSet *set = fx->declared;
            if (!set) set = es_new(0);
            Rec *e = rec_entry(path, fx_item_name(it), set);
            e->item = it;
            e->file = id;
            if (it->kind == N_FN) { /* point at the name, or the `test` keyword */
                e->start = it->a.n->start;
                e->end = it->a.n->end;
            } else {
                e->start = it->start;
                e->end = (long)it->start + 4;
            }
            vec_push(out, e);
        }
    }
    rec_sort(out);
    return out;
}

long rec_skip_spaces(char *p, long i, long n)
{
    while (i < n && p[i] == ' ') i++;
    return i;
}

/* The effect parameter of the program's function `match` named p[0..n), or 0 (spec 026). */
static Type *rec_evar(Rec *match, char *p, long n)
{
    Vec *evars = match && match->set ? match->set->evars : 0;
    for (long i = 0; evars && i < evars->n; i++) {
        Str *name = ty_param_name(vec_get(evars, i));
        if (name->len == n && mem_eq(name->data, p, n)) return vec_get(evars, i);
    }
    return 0;
}

/* Effects in `uses` syntax (`console, file.write("out/")`) as a set, or 0 when malformed. An effect
 * parameter (`uses E`, a capitalized name) is the one of that name the program's function `match`
 * declares; ponytail: a name it no longer declares is left out (the bits still compare), so a renamed
 * parameter shows as added; keep names if that misleads. */
EffSet *rec_parse_set(char *p, long n, void *match)
{
    EffSet *set = es_new(0);
    long i = 0;
    for (;;) {
        i = rec_skip_spaces(p, i, n);
        if (i >= n) return 0; /* an effect is expected */
        long from = i;
        if ((unsigned)(p[i] - 'A') < 26) { /* an effect parameter */
            while (i < n && (p[i] == '_' || (unsigned)((p[i] | 0x20) - 'a') < 26 || (unsigned)(p[i] - '0') < 10)) i++;
            Type *ev = rec_evar(match, p + from, i - from);
            if (ev) es_add_evar(set, ev);
            i = rec_skip_spaces(p, i, n);
            if (i >= n) return set;
            if (p[i] != ',') return 0;
            i++;
            continue;
        }
        while (i < n && (p[i] == '.' || p[i] == '_' || (unsigned)(p[i] - 'a') < 26)) i++;
        if (i == from) return 0;
        long bit = es_bit_by_name(p + from, i - from);
        if (!bit) return 0;
        if (i < n && p[i] == '(') {
            i++;
            for (;;) {
                i = rec_skip_spaces(p, i, n);
                if (i >= n || p[i] != '"') return 0;
                i++;
                SB *sb = sb_new();
                for (;;) {
                    if (i >= n) return 0;
                    int c = (unsigned char)p[i];
                    if (c == '"') break;
                    if (c == '\\') { /* the next byte as it is */
                        if (++i >= n) return 0;
                        c = (unsigned char)p[i];
                    }
                    sb_byte(sb, c);
                    i++;
                }
                i++;
                if (!es_add_scope(set, bit, sb_finish(sb))) return 0; /* "./" normalizes to "" */
                i = rec_skip_spaces(p, i, n);
                if (i >= n) return 0;
                int c = (unsigned char)p[i++];
                if (c == ',') continue;
                if (c != ')') return 0;
                break;
            }
        } else {
            set->mask |= (unsigned long)bit;
        }
        i = rec_skip_spaces(p, i, n);
        if (i >= n) return set;
        if (p[i] != ',') return 0;
        i++;
    }
}

/* One record line [start, end): an entry, or 0 when it cannot be read. */
static Rec *rec_parse_line(char *text, long start, long end, Vec *program)
{
    long colon = start; /* ':' ends the path */
    while (colon < end && text[colon] != ':') colon++;
    if (colon >= end) return 0;
    long name_end;
    EffSet *set;
    if (end - start >= 5 && mem_eq(text + end - 5, " pure", 5)) {
        name_end = end - 5;
        set = es_new(0);
    } else {
        long u = colon + 1; /* " uses " after the name */
        for (;; u++) {
            if (u + 6 > end) return 0;
            if (mem_eq(text + u, " uses ", 6)) break;
        }
        name_end = u;
        Rec *probe = rec_entry(str_new(text + start, colon - start), str_new(text + colon + 1, u - colon - 1), 0);
        set = rec_parse_set(text + u + 6, end - (u + 6), program ? rec_lookup(program, probe) : 0);
        if (!set) return 0;
    }
    Str *path = str_new(text + start, colon - start);
    Str *name = str_new(text + colon + 1, name_end - colon - 1);
    return rec_entry(path, name, set);
}

/* The entries of a registered vo.effects; E0510 for each line that cannot be read. */
Vec *rec_parse(int file, Vec *program)
{
    FileRec *f = file_get(file);
    char *text = f->text;
    long len = f->len;
    Vec *out = vec_new();
    for (long start = 0; start < len;) {
        long end = start;
        while (end < len && text[end] != '\n') end++;
        if (start != end && text[start] != '#') { /* not an empty or comment line */
            Rec *e = rec_parse_line(text, start, end, program);
            if (e) {
                e->file = file;
                e->start = start;
                e->end = end;
                vec_push(out, e);
            } else {
                diag_add("E0510", SEV_ERROR, file, start, end,
                         str_from_cstr("This line of vo.effects cannot be read: write `<path>:<name> uses <effects>` "
                                       "or `<path>:<name> pure`."));
            }
        }
        start = end + 1;
    }
    rec_sort(out);
    return out;
}

void *rec_change(long kind, void *new_entry, void *old_entry, EffSet *delta)
{
    RecChange *c = alloc(sizeof(RecChange));
    c->kind = kind;
    c->new_ = new_entry;
    c->old = old_entry;
    c->delta = delta;
    return c;
}

/* The entry with the same path and name, or 0. */
void *rec_lookup(Vec *entries, void *entry)
{
    for (long i = 0; i < entries->n; i++) {
        Rec *e = vec_get(entries, i);
        if (!rec_cmp(e, entry)) return e;
    }
    return 0;
}

/* The changes from the record to the program, in the order of the new entries, then removals. */
Vec *rec_compare(Vec *old, Vec *new_entries)
{
    Vec *changes = vec_new();
    for (long i = 0; i < new_entries->n; i++) {
        Rec *n = vec_get(new_entries, i);
        Rec *o = rec_lookup(old, n);
        if (!o) { /* a new function: growth unless pure */
            if (es_is_empty(n->set)) continue;
            vec_push(changes, rec_change(CH_ADDED, n, 0, n->set));
            continue;
        }
        o->used = 1;
        EffSet *growth = es_missing(o->set, n->set);
        if (!es_is_empty(growth)) /* growth; a reduction of other effects is reported too */
            vec_push(changes, rec_change(es_lacks_bits(o->set, growth) ? CH_ADDED : CH_WIDENED, n, o, growth));
        EffSet *reduction = es_missing(n->set, o->set);
        if (es_is_empty(reduction)) continue;
        vec_push(changes, rec_change(es_lacks_bits(n->set, reduction) ? CH_REMOVED : CH_NARROWED, n, o, reduction));
    }
    for (long i = 0; i < old->n; i++) { /* old entries no longer in the program */
        Rec *o = vec_get(old, i);
        if (o->used || es_is_empty(o->set)) continue;
        int renamed = 0; /* a new function in the same file with the same effects: a rename */
        for (long j = 0; j < changes->n && !renamed; j++) {
            RecChange *c = vec_get(changes, j);
            if (c->kind != CH_ADDED || c->old) continue;
            if (!str_eq(c->new_->path, o->path) || !es_equal(c->new_->set, o->set)) continue;
            c->kind = CH_RENAMED; /* ponytail: pairs by identical effects in file order; match bodies if mispairing shows up */
            c->old = o;
            renamed = 1;
        }
        if (!renamed) vec_push(changes, rec_change(CH_REMOVED, 0, o, o->set));
    }
    return changes;
}

/* For each call in the entry's function that brings in effects beyond old_set: a related note on
 * diag (when diag != 0) and a JSON `via` element in sb (when sb != 0). */
void rec_via(Diag *d, SB *sb, void *entry, EffSet *old_set)
{
    Rec *e = entry;
    if (!e->item) return;
    Fx *caller = item_fx(e->item);
    if (!caller) return;
    Vec *edges = fx_edges();
    if (!edges) return;
    int written = 0; /* JSON elements */
    for (long i = 0; i < edges->n; i++) {
        W *edge = vec_get(edges, i); /* {caller FX, callee decl, call} */
        if (edge[0].fx != caller) continue;
        Fx *cf = item_fx(edge[1].n);
        if (!cf || !cf->declared) continue;
        if (es_is_empty(es_missing(old_set, cf->declared))) continue;
        Str *effects = fx_render(cf->declared); /* the callee's effects */
        Node *call = edge[2].n;
        if (d) {
            Str *m = msg("`$1` calls `$2`, which uses $3.", e->name, fx_item_name(edge[1].n), effects);
            diag_add_related(d, (int)call->file, call->start, call->end, m);
        }
        if (!sb) continue;
        if (written) sb_byte(sb, ',');
        written = 1;
        sb_cstr(sb, "{\"function\":");
        json_str_into(sb, e->name);
        sb_cstr(sb, ",\"calls\":");
        json_str_into(sb, fx_item_name(edge[1].n));
        sb_cstr(sb, J_FILE);
        json_str_into(sb, rec_relpath((int)call->file));
        sb_cstr(sb, ",\"start\":");
        json_pos(sb, (int)call->file, call->start);
        sb_byte(sb, '}');
    }
}

/* `vo check` after a clean check: compares the program with its vo.effects (E0504, W0502, W0503),
 * or W0504 when a multi-file program has none. */
void rec_review(int entry, Vec *files)
{
    long record = rec_find(entry);
    if (record < 0) {
        long own = 0; /* the standard library is not the program's (spec 007) */
        for (long i = 0; i < files->n; i++) own += !file_get((int)(long)vec_get(files, i))->std;
        if (own >= 2)
            diag_add("W0504", SEV_WARNING, entry, 0, 0,
                     str_from_cstr("This program has no effects record: create vo.effects with `vo effects --update`."));
        return;
    }
    long errors = diag_errors;
    Vec *old = rec_parse((int)record, rec_entries(files));
    if (diag_errors != errors) return; /* a malformed record: E0510 only */
    Vec *changes = rec_compare(old, rec_entries(files));
    for (long i = 0; i < changes->n; i++) rec_change_diag(vec_get(changes, i));
}

/* Spec 031: a related note at each constant the function's own clause uses as a scope, so a reviewer
 * sees why a function whose text did not change gained or lost a capability. */
static void rec_const_notes(Diag *d, Rec *r)
{
    if (!d || !r || !r->item) return;
    Fx *fx = item_fx(r->item);
    Node *uses = fx ? fx->uses : 0;
    if (!uses) return;
    for (long i = 0; i < uses->a.v->n; i++) {
        Node *eff = vec_get(uses->a.v, i);
        for (long j = 0; eff->b.v && j < eff->b.v->n; j++) {
            Node *sc = vec_get(eff->b.v, j);
            if (!(sc->flags & NF_CONST) || sc->kind != N_STR || !sc->b.n || sc->b.n->kind != N_CONST) continue;
            Node *decl = sc->b.n;
            SB *sb = sb_new();
            sb_cstr(sb, "`");
            sb_str(sb, decl->a.n->a.s);
            sb_cstr(sb, "` is ");
            render_str_lit(sb, sc->a.s);
            sb_cstr(sb, " here.");
            diag_add_related(d, (int)decl->file, decl->a.n->start, decl->a.n->end, sb_finish(sb));
        }
    }
}

void rec_change_diag(void *change)
{
    RecChange *c = change;
    Rec *n = c->new_, *o = c->old;
    Str *delta = fx_render(c->delta);
    switch (c->kind) {
    case CH_RENAMED:
        diag_add("W0503", SEV_WARNING, (int)n->file, n->start, n->end,
                 msg("`$1` was `$2`; its effects are unchanged ($3).", n->name, o->name, delta));
        return;
    case CH_REMOVED:
    case CH_NARROWED:
        if (n) /* the function is still there: fewer effects */
            rec_const_notes(diag_add("W0502", SEV_WARNING, (int)n->file, n->start, n->end,
                                     msg("`$1` no longer uses $2.", n->name, delta, 0)),
                            n);
        else /* removed: point at its record line */
            diag_add("W0502", SEV_WARNING, (int)o->file, o->start, o->end,
                     msg("`$1` was removed; it used $2.", o->name, delta, 0));
        return;
    default: { /* growth: E0504 with the calls that brought it in */
        Diag *d = diag_add("E0504", SEV_ERROR, (int)n->file, n->start, n->end,
                           msg("`$1` gains $2; approve with `vo effects --update`.", n->name, delta, 0));
        rec_via(d, 0, n, o ? o->set : es_new(0));
        rec_const_notes(d, n);
        return;
    }
    }
}

/* One record line: `<path>:<name> uses <effects>` or `<path>:<name> pure`, with a newline. */
void rec_line(SB *sb, void *entry)
{
    Rec *e = entry;
    sb_str(sb, e->path);
    sb_byte(sb, ':');
    sb_str(sb, e->name);
    rec_uses_text(sb, e->set);
    sb_byte(sb, '\n');
}

/* ` uses <effects>`, or ` pure`. */
void rec_uses_text(SB *sb, EffSet *set)
{
    if (es_is_empty(set)) {
        sb_cstr(sb, " pure");
        return;
    }
    sb_cstr(sb, " uses ");
    es_render(set, sb);
}

/* `uses <effects>` or `pure` as a str, for effect_change records ("removed" for no entry). */
Str *rec_before_after(void *entry)
{
    Rec *e = entry;
    if (!e) return str_from_cstr("removed");
    SB *sb = sb_new();
    if (es_is_empty(e->set)) {
        sb_cstr(sb, "pure");
    } else {
        sb_cstr(sb, "uses ");
        es_render(e->set, sb);
    }
    return sb_finish(sb);
}

/* A change as an `effect_change` record, or a human line (`+ path:fn: effects`). */
void rec_print_change(SB *sb, void *change, int json)
{
    RecChange *c = change;
    Rec *e = c->new_ ? c->new_ : c->old; /* the entry that names the function */
    Str *delta = fx_render(c->delta);
    if (!json) {
        sb_cstr(sb, c->kind <= CH_WIDENED ? "+ " : c->kind == CH_RENAMED ? "~ " : "- ");
        sb_str(sb, e->path);
        sb_byte(sb, ':');
        if (c->kind == CH_RENAMED) {
            sb_str(sb, c->old->name);
            sb_cstr(sb, " -> ");
        }
        sb_str(sb, e->name);
        sb_cstr(sb, ": ");
        sb_str(sb, delta);
        sb_byte(sb, '\n');
        return;
    }
    sb_cstr(sb, "{\"v\":1,\"kind\":\"effect_change\",\"change\":\"");
    sb_cstr(sb, (char *)change_words[c->kind]);
    sb_cstr(sb, "\"" J_FILE);
    json_str_into(sb, e->path);
    sb_cstr(sb, ",\"function\":");
    json_str_into(sb, e->name);
    if (c->kind == CH_RENAMED) {
        sb_cstr(sb, ",\"old_function\":");
        json_str_into(sb, c->old->name);
    }
    sb_cstr(sb, ",\"effects\":");
    json_str_into(sb, delta);
    sb_cstr(sb, ",\"before\":");
    json_str_into(sb, c->old ? rec_before_after(c->old) : str_from_cstr("pure")); /* new: nothing before */
    sb_cstr(sb, ",\"after\":");
    json_str_into(sb, rec_before_after(c->new_));
    sb_cstr(sb, ",\"via\":[");
    if (c->kind <= CH_WIDENED) rec_via(0, sb, c->new_, c->old ? c->old->set : es_new(0));
    sb_cstr(sb, "]}\n");
}

/* `vo effects`: the capability summary, or (update) rewrite vo.effects and print its changes. */
int rec_command(int entry, Vec *files, int update, int json)
{
    long record = opt_app_effects ? -1 : rec_find(entry); /* file id or -1 (spec 034: none in an executable) */
    Vec *old = vec_new();
    if (record >= 0) {
        old = rec_parse((int)record, rec_entries(files));
        if (diag_errors) return 1; /* a malformed record: report, change nothing */
    }
    Vec *entries = rec_entries(files);
    SB *out = sb_new();
    if (!update) {
        for (long i = 0; i < entries->n; i++) { /* summary: main, then the entry file's tests */
            Rec *e = vec_get(entries, i);
            if (e->file != entry) continue;
            if (e->item->kind != N_TEST && !str_eq_cstr(e->name, "main")) continue;
            if (opt_app_effects && e->item->kind == N_TEST) continue; /* spec 034: what the executable may do */
            if (json) {
                sb_cstr(out, "{\"v\":1,\"kind\":\"capabilities\",\"entry\":");
                json_str_into(out, e->name);
                sb_cstr(out, J_FILE);
                json_str_into(out, e->path);
                sb_cstr(out, ",\"uses\":");
                json_str_into(out, fx_render(e->set));
                sb_cstr(out, "}\n");
            } else { /* `main uses ...` */
                sb_str(out, e->name);
                rec_uses_text(out, e->set);
                sb_byte(out, '\n');
            }
        }
        if (!json && !opt_app_effects) { /* then the record lines that `--update` would write */
            sb_byte(out, '\n');
            for (long i = 0; i < entries->n; i++) rec_line(out, vec_get(entries, i));
        }
    } else { /* update: print the changes, write the file */
        Vec *changes = rec_compare(old, entries);
        for (long i = 0; i < changes->n; i++) rec_print_change(out, vec_get(changes, i), json);
        SB *rec = sb_new(); /* the new record: kept leading comments, then the lines */
        if (record >= 0) {
            FileRec *f = file_get((int)record);
            long i = 0;
            while (i < f->len && f->text[i] == '#') { /* leading `#` lines */
                long j = i;
                while (j < f->len && f->text[j++] != '\n') {}
                sb_bytes(rec, f->text + i, j - i);
                i = j;
            }
        } else {
            sb_cstr(rec, "# vo effects record: update with `vo effects --update`\n");
        }
        for (long i = 0; i < entries->n; i++) rec_line(rec, vec_get(entries, i));
        SB *p = sb_new(); /* <root>/vo.effects */
        sb_str(p, rec_root);
        sb_cstr(p, RECORD);
        Str *path = sb_finish(p);
        write_file_safely(path, sb_finish(rec));
    }
    out_str(1, sb_finish(out));
    return 0;
}
