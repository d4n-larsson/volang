/* Structural edits (spec 006 US2): `vo edit rename` and `vo edit add-field`, verified by checking
 * the edited text a second time in the same process (research R7-R10; ported from src/edit.s,
 * spec 008).
 *
 * An edit is a list of changes (file, [start, end), old text, new text) over the files of the
 * first check (generation 0). The edited texts are registered as generation 1 and the program is
 * checked again from them; the edit is accepted only if the errors are unchanged and, for a rename,
 * the target's references are exactly the renamed places. Then every changed file is written
 * (temporary file, then rename), all or none.
 * ponytail: the second check runs in the same process after resetting the per-program state
 * (diag_reset, a new file generation); a fresh process would be cleaner but is not reliable under
 * qemu-user. tests/cases/edit/recheck_identity guards the reset. */
#include "vo.h"

static Vec *ed_changes;  /* Change records, sorted by file and start */
static Vec *ed_files;    /* the program's files (generation 0 ids) */
static Vec *ed_new_ids;  /* their generation-1 ids, in the same order */
static Vec *ed_old_errs; /* {code cstr, path str} of the first check's errors */
static Vec *ed_sites;    /* rename: the renamed references (log entries of generation 0) */
static RefLog *ed_decl;  /* rename: the target's declaration entry (generation 0) */
static Str *ed_summary;  /* "renamed a to b" / "added field f to R" */
static Str *ed_action;   /* "Renaming `a` to `b`" (refusal messages) */

#define CHANGE(i) ((Change *)ed_changes->data[i].p)

/* ---- changes ---- */

/* Adds a change (the old text is taken from the file); one already at the same place is kept. */
void ed_add(long file, long start, long end, Str *new_text)
{
    Str *old = str_new(file_get((int)file)->text + start, end - start);
    Change *c = alloc(CH_SIZE);
    c->file = file;
    c->start = start;
    c->end = end;
    c->old = old;
    c->new_ = new_text;
    if (!ed_changes) ed_changes = vec_new();
    long at = 0; /* sorted insert by (file, start); same place: drop */
    for (; at < ed_changes->n; at++) {
        Change *o = CHANGE(at);
        if (file < o->file) break;
        if (file > o->file) continue;
        if (start < o->start) break;
        if (start == o->start && end == o->end) return;
    }
    vec_push(ed_changes, c);
    for (long j = ed_changes->n - 1; j > at; j--) ed_changes->data[j] = ed_changes->data[j - 1];
    ed_changes->data[at].p = c;
}

/* The file's text with its changes applied, as a NUL-terminated str. */
Str *ed_text(long file)
{
    SB *sb = sb_new();
    long done = 0; /* copied up to here */
    for (long i = 0; ed_changes && i < ed_changes->n; i++) {
        Change *c = vec_get(ed_changes, i);
        if (c->file != file) continue;
        sb_bytes(sb, file_get((int)file)->text + done, c->start - done);
        sb_str(sb, c->new_);
        done = c->end;
    }
    FileRec *f = file_get((int)file);
    sb_bytes(sb, f->text + done, f->len - done);
    return sb_finish(sb);
}

/* An offset across the changes of a file: old to new (back = 0), or new to old (back = 1; inside a
 * replacement it maps to the change's start). */
long ed_map(long file, long off, long back)
{
    long delta = 0; /* new - old so far */
    for (long i = 0; ed_changes && i < ed_changes->n; i++) {
        Change *c = CHANGE(i);
        if (c->file != file) continue;
        long new_len = c->new_->len;
        if (!back) {
            if (c->end > off) break; /* changes that end at or before off */
        } else {
            long new_start = c->start + delta;
            if (off < new_start) break;
            if (off < new_start + new_len) return c->start; /* inside the replacement */
        }
        delta += new_len - (c->end - c->start);
    }
    return back ? off - delta : off + delta;
}

/* ---- checks of names ---- */

/* 1 when name is an identifier that is not a keyword or a builtin. */
long ed_valid_name(Str *name)
{
    if (!name->len || (name->data[0] >= '0' && name->data[0] <= '9')) return 0;
    for (long i = 0; i < name->len; i++) {
        int c = (unsigned char)name->data[i];
        if (c != '_' && !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9')) return 0;
    }
    if (keyword_kind(name->data, name->len) != T_IDENT) return 0; /* not a keyword */
    return !builtin_id(name);                                    /* not a builtin */
}

/* T0006 edit_refused, exit 65 (nothing is written). */
void ed_refuse(Str *message)
{
    toolchain_error((int)opt_json, (int)report_fd, "T0006", message, 0);
    vo_exit(EXIT_CHECK);
}

void ed_refuse_arg(char *tmpl, Str *arg)
{
    ed_refuse(msg(tmpl, arg, 0, 0));
}

/* ---- rename (research R8) ---- */

void ed_rename(char *target, char *new_name)
{
    Str *name = str_from_cstr(new_name);
    long keys[2];
    q_resolve(target, keys);
    if (!keys[0]) /* (0, id): a builtin */
        ed_refuse_arg("`$1` is a builtin: builtins cannot be renamed.", str_from_cstr(target));
    Vec *refs = q_matches(keys[0], keys[1], 0); /* every reference, sorted */
    RefLog *decl = q_decl_entry(refs);
    if (!decl)
        ed_refuse_arg("`$1` is not a name of the program: point at a declaration or a use of one.",
                      str_from_cstr(target));
    ed_decl = decl;
    ed_sites = refs;
    Node *dn = decl->node; /* the old name: the declaration's text */
    Str *old = str_new(file_get((int)dn->file)->text + dn->start, (long)dn->end - dn->start);
    Node *item = decl->key_a.n;
    int is_fn = decl->key_b.i == -1 && item->a.n == dn && item->kind == N_FN;
    if (is_fn && str_eq_cstr(old, "main")) /* main cannot be renamed */
        ed_refuse(str_from_cstr("`main` cannot be renamed: `vo run` starts the program at the function named `main`."));
    if (str_eq(old, name)) ed_refuse_arg("`$1` already has that name.", old);
    if (!ed_valid_name(name))
        ed_refuse_arg("`$1` cannot be a new name: it must be a name that is not a keyword or a builtin.", name);
    for (long i = 0; i < refs->n; i++) { /* a change at every reference (each node spans the name) */
        Node *n = ((RefLog *)vec_get(refs, i))->node;
        ed_add(n->file, n->start, n->end, name);
    }
    if (is_fn) ed_effects_line(item, old, name); /* a function's line in vo.effects */
    ed_summary = msg("renamed `$1` to `$2`", old, name, 0);
    ed_action = msg("Renaming `$1` to `$2`", old, name, 0);
}

/* The first declaration entry of a sorted match list (q_decl_first order), or 0. */
RefLog *q_decl_entry(Vec *matches)
{
    for (long i = 0; i < matches->n; i++) {
        RefLog *r = matches->data[i].p;
        if (r->role == RR_DECL) return r;
    }
    return 0;
}

/* Renames the function's `path:name` line in the program's vo.effects, if there is one. */
void ed_effects_line(Node *fn_decl, Str *old_name, Str *new_name)
{
    long entry = (long)vec_get(ed_files, ed_files->n - 1); /* the entry: the last file of the program */
    long record = rec_find((int)entry);
    if (record < 0) return;
    Str *path = rec_relpath((int)fn_decl->file); /* "path" as the record writes it */
    FileRec *f = file_get((int)record);
    char *text = f->text;
    long len = f->len;
    for (long line = 0; line < len;) {
        long name_at = line + path->len + 1; /* path ':' name, then ' ' or the end of the line */
        long name_end = name_at + old_name->len;
        if (name_end <= len && mem_eq(text + line, path->data, path->len) && text[name_at - 1] == ':' &&
            mem_eq(text + name_at, old_name->data, old_name->len) &&
            (name_end == len || text[name_end] == ' ' || text[name_end] == '\n')) {
            ed_add(record, name_at, name_end, new_name);
            return;
        }
        while (line < len && text[line++] != '\n') {} /* next line */
    }
}

/* ---- add-field (research R9) ---- */

/* p[0..n) without surrounding spaces, as a str. */
Str *ed_trim(char *p, long n)
{
    while (n && *p == ' ') {
        p++;
        n--;
    }
    while (n && p[n - 1] == ' ') n--;
    return str_new(p, n);
}

/* The number of spaces at the start of the line containing off; *start gets the line's start. */
long ed_line_indent(long file, long off, long *start)
{
    long s = line_start((int)file, pos_line((int)file, off));
    *start = s;
    char *p = file_get((int)file)->text + s;
    long n = 0;
    while (p[n] == ' ') n++;
    return n;
}

/* The declaration's new line: the field after the last one, with its indentation, or before the
 * closing `}` when the record has no field. */
static void ed_field_decl_change(Node *decl, Str *name, Str *type)
{
    SB *sb = sb_new();
    long file = 0, at = 0;
    int placed = 0;
    Node *last = q_last_member(decl->b.v); /* the last field */
    if (last) {
        file = last->file;
        long line_start_at;
        long indent = ed_line_indent(file, last->start, &line_start_at);
        long next = pos_line((int)file, q_last_member(decl->b.v)->end) + 1; /* after the field's line */
        if (next <= file_get((int)file)->nlines) {
            at = line_start((int)file, next);
            while (indent-- > 0) sb_byte(sb, ' ');
            placed = 1;
        }
    }
    if (!placed) { /* no field: before the closing `}` */
        file = decl->file;
        at = (long)decl->end - 1;
        sb_cstr(sb, "    ");
    }
    sb_str(sb, name);
    sb_cstr(sb, ": ");
    sb_str(sb, type);
    sb_cstr(sb, ";\n");
    ed_add(file, at, at, sb_finish(sb));
}

/* The last non-comment member of a member vec, or 0. */
Node *q_last_member(Vec *members)
{
    for (long i = members->n - 1; i >= 0; i--)
        if (members->data[i].n->kind != N_COMMENT) return members->data[i].n;
    return 0;
}

/* `, FIELD: DEFAULT` after a literal's last field. */
static void ed_literal_change(Node *lit, Str *name, Str *dflt)
{
    long file = lit->file;
    SB *sb = sb_new();
    Vec *inits = lit->b.v; /* the initialisers */
    long at;
    if (inits->n) {
        at = ((Node *)vec_get(inits, inits->n - 1))->end; /* after the last one */
        long l1 = pos_line((int)file, at);
        long l2 = pos_line((int)file, (long)lit->end - 1);
        if (l1 != l2) { /* the closing brace on a later line: a new line, same indentation */
            sb_byte(sb, ',');
            sb_byte(sb, '\n');
            long ls;
            long indent = ed_line_indent(file, ((Node *)vec_get(inits, inits->n - 1))->start, &ls);
            while (indent-- > 0) sb_byte(sb, ' ');
        } else {
            sb_cstr(sb, ", ");
        }
    } else { /* no initialiser (only reachable for records with no fields) */
        at = (long)lit->end - 1;
        sb_byte(sb, ' ');
    }
    sb_str(sb, name);
    sb_cstr(sb, ": ");
    sb_str(sb, dflt);
    ed_add(file, at, at, sb_finish(sb));
}

/* "FIELD: TYPE = DEFAULT" split at the first ':' and the first '=' after it; 0 when malformed. */
static int ed_split_field(Str *spec, Str **name, Str **type, Str **dflt)
{
    char *p = spec->data;
    long n = spec->len, i = 0;
    for (; i < n && p[i] != ':'; i++)
        if (p[i] == ';') return 0;
    if (i >= n) return 0;
    long colon = i++;
    for (; i < n && p[i] != '='; i++)
        if (p[i] == ';') return 0;
    if (i >= n) return 0;
    long eq = i;
    for (i = eq + 1; i < n; i++) /* the default must not hold ';' or a newline either */
        if (p[i] == ';' || p[i] == '\n') return 0;
    *name = ed_trim(p, colon);
    *type = ed_trim(p + colon + 1, eq - colon - 1);
    *dflt = ed_trim(p + eq + 1, n - eq - 1);
    return (*type)->len && (*dflt)->len && ed_valid_name(*name);
}

/* spec is "FIELD: TYPE = DEFAULT" (the words after RECORD joined by spaces). */
void ed_add_field(char *record, Str *spec)
{
    Str *name = 0, *type = 0, *dflt = 0;
    if (!ed_split_field(spec, &name, &type, &dflt))
        ed_refuse_arg("`$1` is not a field: write `FIELD: TYPE = DEFAULT`, for example `discount: int = 0`.", spec);
    Str *rname = str_from_cstr(record);
    Node *decl = q_item_named(rname, 1L << N_RECORD);
    if (!decl) ed_refuse_arg("`$1` is not a record.", rname);
    if (member_index(decl->b.v, name) >= 0) ed_refuse(msg("`$1` already has a field `$2`.", rname, name, 0));
    ed_field_decl_change(decl, name, type);
    Vec *lits = q_matches((long)decl, -2, 0); /* every literal of the record: key (record, -2) */
    for (long i = 0; i < lits->n; i++) ed_literal_change(((RefLog *)vec_get(lits, i))->node, name, dflt);
    ed_summary = msg("added field `$1` to `$2`", name, rname, 0);
    ed_action = msg("Adding field `$1` to `$2`", name, rname, 0);
}

/* ---- verification (research R7) ---- */

/* Remembers the first check's errors as {code, path}. */
void ed_save_errors(void)
{
    ed_old_errs = vec_new();
    for (long i = 0; i < diag_n; i++) {
        Diag *d = diag_arr[i];
        if (d->sev != SEV_ERROR) continue;
        W *e = alloc(16);
        e[0].c = (char *)d->code;
        e[1].s = file_get((int)d->file)->path;
        vec_push(ed_old_errs, e);
    }
}

/* Checks the edited program (generation 1) with the reference log on; the new entry's file id. */
long ed_recheck(void)
{
    diag_reset();
    q_log = 0;
    q_exprs = 0;
    ld_hard = 0;
    p_hard = 0;
    te_related = 0;
    cur_gen = 1;
    ed_new_ids = vec_new();
    long entry = -1; /* the entry is the last file of the program */
    for (long i = 0; i < ed_files->n; i++) {
        long old = (long)vec_get(ed_files, i);
        Str *text = ed_text(old);
        entry = file_add(file_get((int)old)->path, text->data, text->len);
        vec_push(ed_new_ids, (void *)entry);
    }
    q_collect = 1;
    check_program((int)entry, 0);
    q_members_all();
    q_collect = 0;
    return entry;
}

/* The generation-0 id of a generation-1 program file (or new_file itself). */
long ed_old_file(long new_file)
{
    for (long i = 0; ed_new_ids && i < ed_new_ids->n; i++)
        if (ed_new_ids->data[i].i == new_file) return ed_files->data[i].i;
    return new_file;
}

long ed_new_file(long old_file)
{
    for (long i = 0; i < ed_files->n; i++)
        if (ed_files->data[i].i == old_file) return ed_new_ids->data[i].i;
    return old_file;
}

/* A span of the edited text (generation 1) as a span of the current text. */
void ed_map_span_back(long *file, long *start, long *end)
{
    *file = ed_old_file(*file);
    *start = ed_map(*file, *start, 1);
    *end = ed_map(*file, *end, 1);
}

/* Keeps only the second check's errors that the first check did not have (by code and file),
 * with their spans mapped to the current text; returns their number, or -1 when an error of the
 * first check disappeared. */
long ed_new_errors(void)
{
    long unmatched = ed_old_errs->n;
    long kept = 0;
    for (long i = 0; i < diag_n; i++) {
        Diag *d = diag_arr[i];
        if (d->sev != SEV_ERROR) continue; /* warnings are dropped */
        if (ed_match_old(d)) {
            unmatched--;
            continue;
        }
        d->fixes.p = 0; /* fixes would be for the edited text */
        ed_map_span_back(&d->file, &d->start, &d->end);
        for (Fix *r = d->related.p; r; r = r->next) ed_map_span_back(&r->file, &r->start, &r->end);
        diag_arr[kept++] = d;
    }
    diag_n = kept;
    diag_errors = kept;
    diag_warnings = 0;
    return unmatched ? -1 : kept;
}

/* 1 when an unmatched first-check error has the diagnostic's code and file path (it is then used up). */
long ed_match_old(Diag *d)
{
    Str *path = file_get((int)d->file)->path;
    for (long i = 0; i < ed_old_errs->n; i++) {
        W *e = vec_get(ed_old_errs, i);
        if (!e[0].c) continue; /* used up */
        if (!cstr_eq(e[0].c, (char *)d->code) || !str_eq(e[1].s, path)) continue;
        e[0].c = 0;
        return 1;
    }
    return 0;
}

/* T0006: the rename would change what a name refers to. */
static void ed_refuse_meaning(Node *dn)
{
    SB *sb = sb_new();
    human_loc(sb, (int)dn->file, dn->start);
    ed_refuse(msg("$1 would change what a name refers to at $2: nothing was written.", ed_action, sb_finish(sb), 0));
}

/* Rename: in the second check, the target (found by its declaration's new place) must have
 * exactly the renamed references. */
void ed_check_refs(void)
{
    Node *dn = ed_decl->node;
    long new_start = ed_map(dn->file, dn->start, 0); /* the declaration's new start */
    long new_file = ed_new_file(dn->file);
    for (long i = 0; q_log && i < q_log->n; i++) { /* its entry in the new log */
        RefLog *r = vec_get(q_log, i);
        if (r->role != RR_DECL || r->node->file != new_file || r->node->start != new_start) continue;
        Vec *now = q_matches(r->key_a.i, r->key_b.i, 0); /* its references now */
        if (ed_sites->n != now->n) break;
        long j = 0; /* each one is a renamed place */
        for (; j < now->n; j++) {
            Node *n = ((RefLog *)vec_get(now, j))->node;
            if (!ed_is_site(n->file, n->start)) break;
        }
        if (j == now->n) return;
        break;
    }
    ed_refuse_meaning(dn);
}

/* 1 when a renamed reference starts there (in the edited text). */
long ed_is_site(long new_file, long new_start)
{
    long file = ed_old_file(new_file);
    long start = ed_map(file, new_start, 1); /* the start in the current text */
    for (long i = 0; i < ed_sites->n; i++) {
        Node *n = ((RefLog *)vec_get(ed_sites, i))->node;
        if (n->file == file && n->start == start) return 1;
    }
    return 0;
}

/* ---- output and writing (research R10) ---- */

void ed_print_change(Change *c)
{
    SB *sb = sb_new();
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"edit\",\"file\":");
        json_str_into(sb, file_get((int)c->file)->path);
        sb_cstr(sb, ",\"start\":");
        json_pos(sb, (int)c->file, c->start);
        sb_cstr(sb, ",\"end\":");
        json_pos(sb, (int)c->file, c->end);
        sb_cstr(sb, ",\"text\":");
        json_str_into(sb, c->old);
        sb_cstr(sb, ",\"replacement\":");
        json_str_into(sb, c->new_);
        sb_cstr(sb, "}\n");
    } else { /* FILE:LINE:COL OLD -> NEW, or FILE:LINE:COL + NEW */
        human_loc(sb, (int)c->file, c->start);
        if (c->old->len) {
            sb_cstr(sb, " ");
            sb_visible(sb, c->old);
            sb_cstr(sb, " -> ");
        } else {
            sb_cstr(sb, " + ");
        }
        sb_visible(sb, c->new_);
        sb_byte(sb, '\n');
    }
    q_out(sb);
}

/* The number of files with at least one change. */
long ed_changed_files(void)
{
    long n = 0, last = -1;
    for (long i = 0; ed_changes && i < ed_changes->n; i++) {
        if (CHANGE(i)->file == last) continue;
        last = CHANGE(i)->file;
        n++;
    }
    return n;
}

/* The last line: what was done, the counts, the dry-run note; or the JSON summary. */
void ed_finish(long written)
{
    SB *sb = sb_new();
    long n = ed_changes ? ed_changes->n : 0;
    if (opt_json) {
        sb_cstr(sb, ",\"changes\":");
        int_to_dec(sb, n);
        sb_cstr(sb, ",\"changed\":");
        int_to_dec(sb, ed_changed_files());
        sb_cstr(sb, ",\"written\":");
        sb_cstr(sb, written ? "true" : "false");
        sb_byte(sb, 0);
        Str *extra = str_from_cstr(sb->ptr);
        summary_emit(1, (int)report_fd, cmd_name(), ed_files->n, extra);
        return;
    }
    sb_visible_plain(sb, ed_summary);
    sb_cstr(sb, ": ");
    int_to_dec(sb, n);
    sb_cstr(sb, n == 1 ? " change in " : " changes in ");
    long files = ed_changed_files();
    int_to_dec(sb, files);
    sb_cstr(sb, files == 1 ? " file" : " files");
    if (!written) sb_cstr(sb, " (dry run, nothing written)");
    sb_byte(sb, '\n');
    q_out(sb);
}

/* The summary without its backquotes (text output reads "renamed a to b"). */
void sb_visible_plain(SB *sb, Str *str)
{
    for (long i = 0; i < str->len; i++)
        if (str->data[i] != '`') sb_byte(sb, (unsigned char)str->data[i]);
}

/* "<path>.vo-edit.tmp", NUL-terminated. */
Str *ed_tmp_path(long file)
{
    Str *path = file_get((int)file)->path;
    SB *sb = sb_new();
    sb_str(sb, path);
    sb_cstr(sb, ".vo-edit.tmp");
    return sb_finish(sb);
}

/* Writes text to the file's temporary path; 0 on success. */
long ed_write_tmp(long file, Str *text)
{
    long fd = sys_open(ed_tmp_path(file)->data, O_WRCREAT);
    if (fd < 0) return 1;
    long r = write_checked((int)fd, text->data, text->len);
    sys_close((int)fd);
    return r;
}

/* Removes the temporary files written so far (after a failure). */
void ed_unlink_tmps(void)
{
    long last = -1;
    for (long i = 0; i < ed_changes->n; i++) {
        long file = CHANGE(i)->file;
        if (file == last) continue;
        last = file;
        sys_unlink(ed_tmp_path(file)->data);
    }
}

/* T0006: a file could not be written. */
static void ed_cannot_write(long file)
{
    Str *path = file_get((int)file)->path;
    ed_refuse(msg("$1: cannot write `$2`; nothing was changed.", ed_action, path, 0));
}

/* Every changed file to its temporary path, then each renamed over its file. A failed temporary
 * write removes the temporary files; a failed rename writes the files already renamed back with
 * their original text. Either way T0006, and nothing is left changed (FR-012). */
void ed_write_all(void)
{
    long last = -1;
    for (long i = 0; i < ed_changes->n; i++) { /* temporary files */
        long file = CHANGE(i)->file;
        if (file == last) continue;
        last = file;
        if (!ed_write_tmp(file, ed_text(file))) continue;
        ed_unlink_tmps();
        ed_cannot_write(file);
    }
    last = -1;
    for (long i = 0; i < ed_changes->n; i++) { /* then the renames */
        long file = CHANGE(i)->file;
        if (file == last) continue;
        last = file;
        Str *tmp = ed_tmp_path(file);
        if (sys_rename(tmp->data, file_get((int)file)->path->data) >= 0) continue;
        ed_undo(file, i + 1); /* undo: the files renamed so far get their old text back */
        ed_unlink_tmps();
        ed_cannot_write(file);
    }
}

/* Writes the original text back to every changed file before failed_file (changes [0, upto)). */
void ed_undo(long failed_file, long upto)
{
    (void)upto;
    long last = -1;
    for (long i = 0; i < ed_changes->n; i++) {
        long file = CHANGE(i)->file;
        if (file == failed_file) return;
        if (file == last) continue;
        last = file;
        FileRec *f = file_get((int)file);
        Str *path = f->path;
        write_file_safely(path, str_new(f->text, f->len));
    }
}

/* ---- the command ---- */

/* T0006 with the second check's new errors (printed first). */
static void ed_refuse_new_errors(void)
{
    long n = diag_n;
    SB *sb = sb_new();
    Str *what;
    if (n == 1) {
        what = str_from_cstr("1 new error");
    } else {
        int_to_dec(sb, n);
        what = msg("$1 new errors", sb_finish(sb), 0, 0);
    }
    Str *m = msg("$1 would not check ($2): nothing was written.", ed_action, what, 0);
    diag_emit_all((int)opt_json, (int)report_fd);
    ed_refuse(m);
}

/* `vo edit rename|add-field ENTRY ...` (contracts/cli.md). */
int cmd_edit(void)
{
    char **args = (char **)pos_args;
    long n = pos_n;
    if (!n)
        usage_error(str_from_cstr("`edit` needs an edit: `vo edit rename ENTRY TARGET NEW_NAME` or `vo edit add-field "
                                  "ENTRY RECORD FIELD: TYPE = DEFAULT`."));
    long kind = 0;
    if (cstr_eq(args[0], "rename"))
        kind = 1;
    else if (cstr_eq(args[0], "add-field"))
        kind = 2;
    else
        usage_error_arg("Unknown edit `$1`. Edits: rename, add-field.", args[0]);
    if (kind == 1 && n != 4) /* rename: exactly 4 words; add-field: at least 4 */
        usage_error(str_from_cstr("`edit rename` takes the entry file, the target and the new name: `vo edit rename "
                                  "main.vo old_name new_name`."));
    if (kind == 2 && n < 4)
        usage_error(str_from_cstr("`edit add-field` takes the entry file, the record and the field: `vo edit "
                                  "add-field main.vo Item discount: int = 0`."));
    q_load(args[1]); /* the first check, with the reference log */
    ed_files = ld_files();
    ed_save_errors();
    if (kind == 1) {
        ed_rename(args[2], args[3]);
    } else {
        SB *sb = sb_new(); /* add-field: the words after RECORD, joined by spaces */
        for (long i = 3; i < n; i++) {
            if (i != 3) sb_byte(sb, ' ');
            sb_cstr(sb, args[i]);
        }
        ed_add_field(args[2], sb_finish(sb));
    }
    ed_recheck(); /* the second check, on the edited text */
    if (ld_hard) ed_refuse_new_errors();
    long errors = ed_new_errors();
    if (errors < 0)
        ed_refuse(msg("$1 would change the errors the program already has: nothing was written.", ed_action, 0, 0));
    if (errors > 0) ed_refuse_new_errors();
    if (kind == 1) ed_check_refs(); /* rename: the references must be exactly the renamed places */
    if (!opt_dry_run) ed_write_all(); /* accepted: write the files (unless --dry-run), then list the changes */
    for (long i = 0; ed_changes && i < ed_changes->n; i++) ed_print_change(vec_get(ed_changes, i));
    ed_finish(!opt_dry_run);
    return 0;
}
