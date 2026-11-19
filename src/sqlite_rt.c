/* std.sqlite's runtime (spec 026): connections, the primitives __sql_open, __sql_close, __sql_exec,
 * __sql_query, __sql_tx and __sql_prepare, values in and out (the std variant Value: int, float, text,
 * bytes, null, in that order), the authorizer (no clock or randomness in SQL, three pragmas refused),
 * the progress handler (--timeout) and the busy handler. SQLite's memory is its own heap (memsys5 over
 * one reserved region), never the collected heap. No runtime error is raised while SQLite is on the
 * stack: a refused file (src/sqlite_vfs.c) or a timeout makes the SQLite call fail, and the error is
 * raised after it returns. */
#include "vo.h"
#include <sqlite3.h>

#define HEAP_SIZE (64L << 20)
#define MAX_DBS 64
#define MAX_STMTS 64
#define V_INT 0
#define V_FLOAT 1
#define V_TEXT 2
#define V_BYTES 3
#define V_NULL 4
#define M_NOW "`'now'` reads the clock: compute the time in vo (std.time) and pass it as a parameter."

extern Str *sqlite_vfs_denied;
extern long sqlite_vfs_denied_bit;

typedef struct {
    Str *text; /* in the collected heap: the slot table is in .bss, which the collector scans */
    sqlite3_stmt *stmt;
} Cached;

typedef struct {
    sqlite3 *db;
    long pid;
    long serial; /* opening order, for closing what a test opened */
    int readonly, in_tx;
    long bits; /* EF_FREAD | EF_FWRITE it was opened with; 0 in memory */
    Str *path;
    Cached stmts[MAX_STMTS];
    long next_stmt;
} Slot;

#define REPLAYED ((sqlite3 *)1) /* a connection vo replay answers for: SQLite never sees it */

static Slot slots[MAX_DBS];
static long serial_next;
static int started, timed_out;
static char *auth_msg; /* the message of the last authorizer refusal */
static sqlite3 *scratch;  /* an in-memory connection for parsing (__sql_prepare, the checker) */

/* ---- start-up ---- */

static void start(void)
{
    if (started) return;
    started = 1;
    /* ponytail: one fixed 64 MiB heap for all connections, not counted in --max-memory; a setting if
       programs need more */
    void *heap = (void *)vo_syscall6(SYS_mmap, 0, HEAP_SIZE, 3 /* read, write */, 0x22 | 0x4000 /* private,
                                     anonymous, no reserve */, -1, 0);
    sqlite3_config(SQLITE_CONFIG_HEAP, heap, (int)HEAP_SIZE, 64);
    sqlite3_initialize();
}

/* ---- the authorizer, progress and busy handlers ---- */

static int ieq(const char *a, const char *b)
{
    if (!a) return 0;
    for (; *a && *b; a++, b++)
        if ((*a | 0x20) != (*b | 0x20)) return 0;
    return !*a && !*b;
}

static int authorize(void *u, int action, const char *a, const char *b, const char *c, const char *d)
{
    (void)u;
    (void)c;
    (void)d;
    if (action == SQLITE_FUNCTION && (ieq(b, "current_timestamp") || ieq(b, "current_date") || ieq(b, "current_time"))) {
        auth_msg = ieq(b, "current_timestamp") ? "`CURRENT_TIMESTAMP` reads the clock: compute the time in vo (std.time) and pass it as a parameter."
                   : ieq(b, "current_date")    ? "`CURRENT_DATE` reads the clock: compute the date in vo (std.time) and pass it as a parameter."
                                               : "`CURRENT_TIME` reads the clock: compute the time in vo (std.time) and pass it as a parameter.";
        return SQLITE_DENY;
    }
    if (action == SQLITE_FUNCTION && (ieq(b, "random") || ieq(b, "randomblob"))) {
        auth_msg = ieq(b, "random") ? "`random()` reads randomness: compute the value in vo (std.random) and pass it as a parameter."
                                    : "`randomblob()` reads randomness: compute the value in vo (std.random) and pass it as a parameter.";
        return SQLITE_DENY;
    }
    if (action == SQLITE_PRAGMA && (ieq(a, "temp_store_directory") || ieq(a, "data_store_directory") ||
                                    (ieq(a, "journal_mode") && ieq(b, "wal")))) {
        static char buf[96];
        const char *name = ieq(a, "journal_mode") ? "journal_mode = WAL" : a;
        long n = 0;
        for (const char *s = "PRAGMA "; *s; s++) buf[n++] = *s;
        for (const char *s = name; *s && n < 80; s++) buf[n++] = *s;
        for (const char *s = " is not available in vo"; *s; s++) buf[n++] = *s;
        buf[n] = 0;
        auth_msg = buf;
        return SQLITE_DENY;
    }
    return SQLITE_OK;
}

/* every 1,000 virtual-machine steps: the --timeout budget; 1 interrupts the statement */
static int progress(void *u)
{
    (void)u;
    rt_budget -= 64;
    if (rt_budget > 0) return 0;
    rt_budget = BUDGET_UNITS;
    if (deadline_ms && sys_clock_ms() >= deadline_ms) {
        timed_out = 1;
        return 1;
    }
    return 0;
}

/* a locked database: retry every 10 ms, at most 5 s, never past the deadline (research R10) */
static int busy(void *u, int count)
{
    (void)u;
    if (count >= 500 || (deadline_ms && sys_clock_ms() + 10 >= deadline_ms)) return 0;
    long ts[2] = {0, 10000000};
    vo_syscall5(SYS_ppoll, 0, 0, (long)ts, 0, 8);
    return 1;
}

static void setup(sqlite3 *db)
{
    sqlite3_extended_result_codes(db, 1);
    sqlite3_set_authorizer(db, authorize, 0);
    sqlite3_progress_handler(db, 1000, progress, 0);
    sqlite3_busy_handler(db, busy, 0);
    sqlite3_db_config(db, SQLITE_DBCONFIG_DEFENSIVE, 1, (int *)0);
    sqlite3_db_config(db, SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0, (int *)0);
}

/* ---- errors ---- */

/* "KIND: MESSAGE" for SQLite result code rc on db */
static Str *err_text(sqlite3 *db, int rc)
{
    int p = rc & 0xff;
    char *kind = p == SQLITE_CONSTRAINT                     ? "constraint"
                 : p == SQLITE_BUSY || p == SQLITE_LOCKED   ? "busy"
                 : p == SQLITE_READONLY                     ? "readonly"
                 : p == SQLITE_CORRUPT || p == SQLITE_NOTADB ? "corrupt"
                 : p == SQLITE_NOMEM || p == SQLITE_FULL    ? "nomem"
                                                            : "error";
    const char *m = auth_msg ? auth_msg : db ? sqlite3_errmsg(db) : sqlite3_errstr(rc); /* a refusal: vo's text */
    SB *sb = sb_new();
    sb_cstr(sb, kind);
    sb_cstr(sb, ": ");
    sb_cstr(sb, (char *)m);
    return sb_finish(sb);
}

static void record_denied(long bit, Str *target);

/* After SQLite returned: a refused file is R0010, a timeout R0009 (both at the user's call). */
static void after_sqlite(Node *call)
{
    if (sqlite_vfs_denied) {
        Str *p = sqlite_vfs_denied;
        sqlite_vfs_denied = 0;
        record_denied(sqlite_vfs_denied_bit, p);
        cur_node = (long)std_user_call(call);
        rt_denied(sqlite_vfs_denied_bit, p);
    }
    if (timed_out) {
        timed_out = 0;
        rt_timeout(std_user_call(call));
    }
}

static Variant *err_result(Str *text)
{
    return make_result(1, (long)text);
}

/* ---- the SQL tokenizer (shared with src/check_sqlite.c) ---- */

/* Scans one SQL text: counts `?` parameters, finds the first named or numbered parameter (:a @a $a
 * ?N) and where a second statement starts after a top-level `;`. Strings, quoted identifiers and
 * comments are skipped. */
void sql_scan(const char *s, long n, SqlScan *out)
{
    out->qmarks = 0;
    out->named_at = -1;
    out->named_len = 0;
    out->second_at = -1;
    out->named_count = 0;
    out->now_at = -1;
    int after_semicolon = 0;
    for (long i = 0; i < n;) {
        char c = s[i];
        if (c == '\'' || c == '"' || c == '`' || c == '[') { /* a string or quoted identifier */
            char close = c == '[' ? ']' : c;
            if (c == '\'' && i + 4 < n && (s[i + 1] | 0x20) == 'n' && (s[i + 2] | 0x20) == 'o' && (s[i + 3] | 0x20) == 'w' &&
                s[i + 4] == '\'' && out->now_at < 0)
                out->now_at = i; /* 'now': the date functions would read the clock (research R6) */
            i++;
            while (i < n) {
                if (s[i] == close) {
                    if (close != ']' && i + 1 < n && s[i + 1] == close) {
                        i += 2;
                        continue;
                    }
                    break;
                }
                i++;
            }
            i++;
            continue;
        }
        if (c == '-' && i + 1 < n && s[i + 1] == '-') {
            while (i < n && s[i] != '\n') i++;
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(s[i] == '*' && s[i + 1] == '/')) i++;
            i += 2;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
            continue;
        }
        if (c == ';') {
            after_semicolon = 1;
            i++;
            continue;
        }
        if (after_semicolon && out->second_at < 0) out->second_at = i;
        if (c == '?') {
            long j = i + 1;
            while (j < n && s[j] >= '0' && s[j] <= '9') j++;
            if (j > i + 1) {
                if (out->named_at < 0) {
                    out->named_at = i;
                    out->named_len = j - i;
                }
                if (out->named_count < 32) {
                    out->named_starts[out->named_count] = i;
                    out->named_lens[out->named_count] = j - i;
                }
                out->named_count++;
            } else {
                out->qmarks++;
            }
            i = j;
            continue;
        }
        if ((c == ':' || c == '@' || c == '$') && i + 1 < n &&
            ((s[i + 1] | 0x20) >= 'a' && (s[i + 1] | 0x20) <= 'z' ? 1 : s[i + 1] == '_')) {
            long j = i + 1;
            while (j < n && (((s[j] | 0x20) >= 'a' && (s[j] | 0x20) <= 'z') || (s[j] >= '0' && s[j] <= '9') || s[j] == '_')) j++;
            if (out->named_at < 0) {
                out->named_at = i;
                out->named_len = j - i;
            }
            if (out->named_count < 32) {
                out->named_starts[out->named_count] = i;
                out->named_lens[out->named_count] = j - i;
            }
            out->named_count++;
            i = j;
            continue;
        }
        i++;
    }
}

/* ---- parsing (parse_sql and the checker) ---- */

/* 1 when SQLite's error on a statement it could not prepare is about the text itself (syntax, an
 * incomplete statement, a token, a refused function or pragma), not about names in a schema. */
static int text_error(int rc, const char *m)
{
    if ((rc & 0xff) == SQLITE_AUTH || auth_msg) return 1;
    for (const char *p = m; *p; p++) {
        const char *k[] = {"syntax error", "incomplete input", "unrecognized token", 0};
        for (int i = 0; k[i]; i++) {
            const char *a = p, *b = k[i];
            while (*b && *a == *b) a++, b++;
            if (!*b) return 1;
        }
    }
    return 0;
}

/* SQL text checked as parse_sql and the checker check it: 0 when fine, else the message, with the byte
 * offset of the problem in *at (-1 when unknown). */
char *sql_check_text(const char *s, long n, long *at)
{
    static char msgbuf[256];
    start();
    *at = -1;
    SqlScan sc;
    sql_scan(s, n, &sc);
    if (sc.second_at >= 0) {
        *at = sc.second_at;
        return "One statement per `sql(...)`: split it into one call per statement.";
    }
    if (sc.named_at >= 0) {
        *at = sc.named_at;
        return "Parameters are `?`, taken from the list in order; named and numbered parameters are a second spelling.";
    }
    if (sc.now_at >= 0) {
        *at = sc.now_at;
        return M_NOW;
    }
    if (!scratch) {
        sqlite3_open_v2(":memory:", &scratch, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "vo");
        setup(scratch);
    }
    sqlite3_stmt *st = 0;
    auth_msg = 0;
    int rc = sqlite3_prepare_v2(scratch, s, (int)n, &st, 0);
    if (st) sqlite3_finalize(st);
    if (rc == SQLITE_OK) return 0;
    const char *m = auth_msg ? auth_msg : sqlite3_errmsg(scratch);
    if (!text_error(rc, m)) return 0; /* a schema error: the schema is only known at run time */
    *at = sqlite3_error_offset(scratch);
    long k = 0;
    for (; m[k] && k < 255; k++) msgbuf[k] = m[k];
    msgbuf[k] = 0;
    return msgbuf;
}

/* __sql_prepare(text): ok("") or err(message) */
Variant *sql_prim_prepare(Str *text)
{
    long at;
    char *m = sql_check_text(text->data, text->len, &at);
    return m ? err_result(str_from_cstr(m)) : make_result(0, (long)str_from_cstr(""));
}

/* ---- connections ---- */

static long my_pid(void)
{
    return vo_syscall0(SYS_getpid);
}

static Slot *slot_of(long id, Node *call)
{
    Slot *s = id >= 0 && id < MAX_DBS ? &slots[id] : 0;
    if (!s || !s->db)
        rt_raise("R0007", str_from_cstr("No open database with this id: it was closed, or built by hand."), std_user_call(call));
    if (s->pid != my_pid()) rt_raise("R0007", str_from_cstr("Open the database inside the task."), std_user_call(call));
    /* research R14: the connection's effects, checked against the running grant (R0010) */
    if (s->bits) {
        cur_node = (long)std_user_call(call);
        rt_check_effect(EF_FREAD); /* the bits, then their scopes */
        rt_check_path(EF_FREAD, s->path);
        if (s->bits & EF_FWRITE) {
            rt_check_effect(EF_FWRITE);
            rt_check_path(EF_FWRITE, s->path);
        }
    }
    return s;
}

static void close_slot(Slot *s)
{
    for (long i = 0; i < MAX_STMTS; i++) {
        if (s->stmts[i].stmt) sqlite3_finalize(s->stmts[i].stmt);
        s->stmts[i].stmt = 0;
        s->stmts[i].text = 0;
    }
    if (s->db != REPLAYED) sqlite3_close_v2(s->db);
    s->db = 0;
    s->path = 0;
}

static Str *event_ident(const char *op, long id, Str *path, Str *text, List *params);
static Variant *event(Str *ident, int op, Str *name, Variant *r, Node *call);
enum { OP_OPEN, OP_EXEC, OP_QUERY, OP_TX, OP_CLOSE };
static Str *rec_ident; /* the call being recorded (for a refusal inside SQLite); in .bss, scanned */

/* The SQLite part of __sql_open: ok(id) or err. */
static Variant *open_db(Str *path, long mode, long id, Node *call)
{
    int flags = SQLITE_OPEN_EXRESCODE;
    flags |= mode == 2   ? SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY
             : mode == 0 ? SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
                         : SQLITE_OPEN_READONLY;
    sqlite3 *db = 0;
    int rc = sqlite3_open_v2(mode == 2 ? ":memory:" : path->data, &db, flags, "vo");
    after_sqlite(call);
    if (rc == SQLITE_OK) {
        setup(db);
        if (mode != 2) { /* touch the file now, so a missing or corrupt one is an err here, not later */
            rc = sqlite3_exec(db, "select count(*) from sqlite_schema", 0, 0, 0);
            after_sqlite(call);
        }
    }
    if (rc != SQLITE_OK) {
        Str *e = err_text(db, rc);
        sqlite3_close_v2(db);
        return err_result(e);
    }
    slots[id].db = db;
    return make_result(0, id);
}

/* __sql_open(path, mode): mode 0 read-write (creates), 1 read-only, 2 in memory; ok(id) or err */
Variant *sql_prim_open(Str *path, long mode, Node *call)
{
    start();
    GC_ROOT(&path);
    long id = 0;
    while (id < MAX_DBS && slots[id].db) id++;
    if (id == MAX_DBS) {
        GC_UNROOT(1);
        if (mode == 2) /* open_memory returns a Database, not a result */
            rt_raise("R0007", str_from_cstr("Too many open databases (64): close some."), std_user_call(call));
        return err_result(str_from_cstr("error: too many open databases (64)"));
    }
    if (mode != 2) {
        int nul = path->len == 0;
        for (long i = 0; i < path->len; i++) nul |= path->data[i] == 0;
        if (nul || (path->len == 8 && mem_eq(path->data, ":memory:", 8))) {
            GC_UNROOT(1);
            return err_result(str_from_cstr(nul ? "error: a database path must be a non-empty text without NUL bytes"
                                                : "error: use open_memory() for an in-memory database"));
        }
        cur_node = (long)std_user_call(call);
        rt_check_path(EF_FREAD, path); /* outside SQLite: R0010 directly */
        if (mode == 0) rt_check_path(EF_FWRITE, path);
    }
    const char *op = mode == 0 ? "open" : mode == 1 ? "open_readonly" : "open_memory";
    Str *ident = rcd_mode ? event_ident(op, -1, mode == 2 ? 0 : path, 0, 0) : 0;
    GC_ROOT(&ident);
    Str *name = mode == 2 ? str_from_cstr((char *)op) : path;
    GC_ROOT(&name);
    Variant *r = rcd_mode == 2 ? 0 : open_db(path, mode, id, call);
    r = event(ident, OP_OPEN, name, r, call);
    GC_UNROOT(3);
    if (r->tag != 0) {
        if (mode == 2) rt_raise("R0007", r->pay.s, std_user_call(call));
        return r;
    }
    if (r->pay.i != id) rp_bad(); /* replayed: the connection this run would get */
    Slot *s = &slots[id];
    if (!s->db) s->db = REPLAYED;
    s->pid = my_pid();
    s->serial = serial_next++;
    s->readonly = mode == 1;
    s->in_tx = 0;
    s->bits = mode == 2 ? 0 : mode == 1 ? EF_FREAD : EF_FREAD | EF_FWRITE;
    s->path = mode == 2 ? 0 : path;
    s->next_stmt = 0;
    return r;
}

/* __sql_close(id) */
void sql_prim_close(long id, Node *call)
{
    Slot *s = id >= 0 && id < MAX_DBS ? &slots[id] : 0;
    if (!s || !s->db || s->pid != my_pid()) slot_of(id, call); /* raises; closing needs no effect */
    Str *ident = rcd_mode ? event_ident("close", id, 0, 0, 0) : 0;
    GC_ROOT(&ident);
    Str *nm = str_from_cstr("close");
    GC_ROOT(&nm);
    event(ident, OP_CLOSE, nm, rcd_mode == 2 ? 0 : make_result(0, 0), call);
    GC_UNROOT(2);
    close_slot(s);
}

/* Closes the connections opened since serial (a test's), or all (serial 0, the end of the program). */
void sql_close_since(long serial)
{
    long pid = started ? my_pid() : 0;
    for (long i = 0; i < MAX_DBS; i++)
        if (slots[i].db && slots[i].serial >= serial && slots[i].pid == pid) close_slot(&slots[i]);
}

long sql_serial(void)
{
    return serial_next;
}

/* ---- statements ---- */

/* The prepared statement for text on s (cached), or 0 with *rc. */
static sqlite3_stmt *prepare(Slot *s, Str *text, int *rc, Node *call)
{
    for (long i = 0; i < MAX_STMTS; i++)
        if (s->stmts[i].stmt && str_eq(s->stmts[i].text, text)) {
            sqlite3_reset(s->stmts[i].stmt);
            sqlite3_clear_bindings(s->stmts[i].stmt);
            return s->stmts[i].stmt;
        }
    SqlScan sc;
    sql_scan(text->data, text->len, &sc);
    if (sc.second_at >= 0) {
        *rc = -1;
        return 0;
    }
    if (sc.now_at >= 0) {
        *rc = -2;
        return 0;
    }
    sqlite3_stmt *st = 0;
    auth_msg = 0;
    *rc = sqlite3_prepare_v3(s->db, text->data, (int)text->len, SQLITE_PREPARE_PERSISTENT, &st, 0);
    after_sqlite(call);
    if (*rc != SQLITE_OK) return 0;
    /* ponytail: 64 statements per connection, replaced round-robin; an LRU if programs thrash */
    Cached *c = &s->stmts[s->next_stmt];
    s->next_stmt = (s->next_stmt + 1) % MAX_STMTS;
    if (c->stmt) sqlite3_finalize(c->stmt);
    c->stmt = st;
    c->text = text;
    return st;
}

/* Binds the Value list; 0, or the error text. */
static Str *bind(sqlite3_stmt *st, List *params)
{
    int want = sqlite3_bind_parameter_count(st);
    if (want != params->len)
        return msg("error: the statement has $1 parameter(s) and the list has $2", int_to_str(want),
                   int_to_str(params->len), 0);
    for (long i = 0; i < params->len; i++) {
        Variant *v = params->data[i].p;
        int k = (int)i + 1;
        switch (v->tag) {
        case V_INT: sqlite3_bind_int64(st, k, v->pay.i); break;
        case V_FLOAT: {
            double d;
            mem_copy((char *)&d, (char *)&v->pay.i, 8);
            sqlite3_bind_double(st, k, d);
            break;
        }
        case V_TEXT: sqlite3_bind_text64(st, k, v->pay.s->data, (sqlite3_uint64)v->pay.s->len, SQLITE_TRANSIENT, SQLITE_UTF8); break;
        case V_BYTES: sqlite3_bind_blob64(st, k, v->pay.s->data, (sqlite3_uint64)v->pay.s->len, SQLITE_TRANSIENT); break;
        default: sqlite3_bind_null(st, k); break;
        }
    }
    return 0;
}

static Variant *value_of(sqlite3_stmt *st, int col)
{
    int t = sqlite3_column_type(st, col);
    long tag = V_NULL, pay = 0;
    if (t == SQLITE_INTEGER) {
        tag = V_INT;
        pay = sqlite3_column_int64(st, col);
    } else if (t == SQLITE_FLOAT) {
        double d = sqlite3_column_double(st, col);
        tag = V_FLOAT;
        mem_copy((char *)&pay, (char *)&d, 8);
    } else if (t == SQLITE_TEXT || t == SQLITE_BLOB) {
        const char *p = t == SQLITE_TEXT ? (const char *)sqlite3_column_text(st, col) : sqlite3_column_blob(st, col);
        long n = sqlite3_column_bytes(st, col);
        Str *b = str_new((char *)(p ? p : ""), n);
        tag = t == SQLITE_TEXT && utf8_validate(b->data, b->len) < 0 ? V_TEXT : V_BYTES;
        pay = (long)b;
    }
    return make_result((int)tag, pay); /* a Variant of the std type Value: tag = case index */
}

static Str *one_statement_msg(void)
{
    return str_from_cstr("error: One statement per `sql(...)`: split it into one call per statement.");
}

/* The SQLite part of __sql_exec. */
static Variant *exec_db(Slot *s, Str *text, List *params, Node *call)
{
    GC_ROOT(&text);
    GC_ROOT(&params);
    int rc;
    sqlite3_stmt *st = prepare(s, text, &rc, call);
    Variant *res;
    if (!st) {
        res = err_result(rc == -1 ? one_statement_msg() : rc == -2 ? str_from_cstr("error: " M_NOW) : err_text(s->db, rc));
    } else {
        Str *e = bind(st, params);
        if (e) {
            res = err_result(e);
        } else {
            sqlite3_set_last_insert_rowid(s->db, 0);
            auth_msg = 0;
            while ((rc = sqlite3_step(st)) == SQLITE_ROW) {}
            after_sqlite(call);
            if (rc != SQLITE_DONE) {
                res = err_result(err_text(s->db, rc));
            } else {
                List *l = list_new(2);
                GC_ROOT(&l);
                list_push(l, sqlite3_changes64(s->db));
                list_push(l, sqlite3_last_insert_rowid(s->db));
                res = make_result(0, (long)l);
                GC_UNROOT(1);
            }
            sqlite3_reset(st);
        }
    }
    GC_UNROOT(2);
    return res;
}

/* The SQLite part of __sql_query. */
static Variant *query_db(Slot *s, Str *text, List *params, Node *call)
{
    GC_ROOT(&text);
    GC_ROOT(&params);
    int rc;
    sqlite3_stmt *st = prepare(s, text, &rc, call);
    Variant *res = 0;
    if (!st) {
        res = err_result(rc == -1 ? one_statement_msg() : rc == -2 ? str_from_cstr("error: " M_NOW) : err_text(s->db, rc));
        GC_UNROOT(2);
        return res;
    }
    Str *e = bind(st, params);
    if (e) {
        GC_UNROOT(2);
        return err_result(e);
    }
    List *rows = list_new(8), *row = 0;
    Variant *v = 0;
    GC_ROOT(&rows);
    GC_ROOT(&row);
    GC_ROOT(&v);
    int ncol = sqlite3_column_count(st);
    row = list_new(ncol);
    for (int c = 0; c < ncol; c++) {
        const char *nm = sqlite3_column_name(st, c);
        v = make_result(V_TEXT, (long)str_from_cstr((char *)(nm ? nm : "")));
        list_push(row, (long)v);
    }
    list_push(rows, (long)row);
    auth_msg = 0;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        row = list_new(ncol);
        for (int c = 0; c < ncol; c++) {
            v = value_of(st, c);
            list_push(row, (long)v);
        }
        list_push(rows, (long)row);
    }
    after_sqlite(call);
    res = rc == SQLITE_DONE ? make_result(0, (long)rows) : err_result(err_text(s->db, rc));
    sqlite3_reset(st);
    GC_UNROOT(5);
    return res;
}

/* The SQLite part of __sql_tx. */
static Variant *tx_db(Slot *s, long op, Node *call)
{
    if (op == 1 && s->in_tx) return err_result(str_from_cstr("error: a transaction is already open"));
    int rc = sqlite3_exec(s->db, op == 1 ? "BEGIN" : op == 2 ? "COMMIT" : "ROLLBACK", 0, 0, 0);
    after_sqlite(call);
    if (rc != SQLITE_OK) {
        if (op == 2) sqlite3_exec(s->db, "ROLLBACK", 0, 0, 0); /* a failed commit leaves nothing open */
        s->in_tx = 0;
        return err_result(err_text(s->db, rc));
    }
    s->in_tx = op == 1;
    return make_result(0, 0);
}

/* __sql_exec(id, text, params): ok([changed, last_id]) or err */
Variant *sql_prim_exec(long id, Str *text, List *params, Node *call)
{
    GC_ROOT(&text);
    GC_ROOT(&params);
    Slot *s = slot_of(id, call);
    Str *ident = rcd_mode ? event_ident("exec", id, 0, text, params) : 0;
    GC_ROOT(&ident);
    Variant *r = event(ident, OP_EXEC, text, rcd_mode == 2 ? 0 : exec_db(s, text, params, call), call);
    GC_UNROOT(3);
    return r;
}

/* __sql_query(id, text, params): ok([names..., then rows]) or err; each a list of Values */
Variant *sql_prim_query(long id, Str *text, List *params, Node *call)
{
    GC_ROOT(&text);
    GC_ROOT(&params);
    Slot *s = slot_of(id, call);
    Str *ident = rcd_mode ? event_ident("query", id, 0, text, params) : 0;
    GC_ROOT(&ident);
    Variant *r = event(ident, OP_QUERY, text, rcd_mode == 2 ? 0 : query_db(s, text, params, call), call);
    GC_UNROOT(3);
    return r;
}

/* __sql_tx(id, op): 1 begin, 2 commit, 3 rollback */
Variant *sql_prim_tx(long id, long op, Node *call)
{
    Slot *s = slot_of(id, call);
    const char *name = op == 1 ? "begin" : op == 2 ? "commit" : "rollback";
    Str *ident = rcd_mode ? event_ident(name, id, 0, 0, 0) : 0;
    GC_ROOT(&ident);
    Str *nm = str_from_cstr((char *)name);
    GC_ROOT(&nm);
    Variant *r = event(ident, OP_TX, nm, rcd_mode == 2 ? 0 : tx_db(s, op, call), call);
    GC_UNROOT(2);
    return r;
}

/* ---- spec 026: recording and replay (contracts/recording.md): one `sql` event per primitive call. Its
 * line starts with what the program asked for (the identity, built by event_ident), which replay
 * compares byte for byte, and ends with the outcome, which replay returns without calling SQLite. ---- */

static const char hexd[] = "0123456789abcdef";

/* A JSON string: " and \ escaped, control bytes and (with bytes) every byte >= 0x80 as \u00XX, which the
 * recording reader turns back into that byte. */
static void enc_str(SB *sb, Str *s, int bytes)
{
    sb_byte(sb, '"');
    for (long i = 0; i < s->len; i++) {
        int c = (unsigned char)s->data[i];
        if (c == '"' || c == '\\') {
            sb_byte(sb, '\\');
            sb_byte(sb, c);
        } else if (c < 0x20 || (c >= 0x80 && bytes)) {
            sb_cstr(sb, "\\u00");
            sb_byte(sb, hexd[c >> 4]);
            sb_byte(sb, hexd[c & 15]);
        } else {
            sb_byte(sb, c);
        }
    }
    sb_byte(sb, '"');
}

static void enc_int(SB *sb, long v)
{
    Str *s = int_to_str(v);
    sb_bytes(sb, s->data, s->len);
}

/* {"int":5} {"float":"3ff8000000000000"} (the bits) {"text":"..."} {"bytes":"..."} {"null":true} */
static void enc_value(SB *sb, Variant *v)
{
    switch (v->tag) {
    case V_INT: sb_cstr(sb, "{\"int\":"); enc_int(sb, v->pay.i); break;
    case V_FLOAT:
        sb_cstr(sb, "{\"float\":\"");
        for (int k = 60; k >= 0; k -= 4) sb_byte(sb, hexd[(v->pay.u >> k) & 15]);
        sb_byte(sb, '"');
        break;
    case V_TEXT: sb_cstr(sb, "{\"text\":"); enc_str(sb, v->pay.s, 0); break;
    case V_BYTES: sb_cstr(sb, "{\"bytes\":"); enc_str(sb, v->pay.s, 1); break;
    default: sb_cstr(sb, "{\"null\":true"); break;
    }
    sb_byte(sb, '}');
}

static void enc_values(SB *sb, List *l)
{
    sb_byte(sb, '[');
    for (long i = 0; i < l->len; i++) {
        if (i) sb_byte(sb, ',');
        enc_value(sb, l->data[i].p);
    }
    sb_byte(sb, ']');
}

static Str *event_ident(const char *op, long id, Str *path, Str *text, List *params)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, "{\"v\":1,\"kind\":\"sql\",\"op\":\"");
    sb_cstr(sb, (char *)op);
    sb_byte(sb, '"');
    if (id >= 0) {
        sb_cstr(sb, ",\"db\":");
        enc_int(sb, id);
    }
    if (path) {
        sb_cstr(sb, ",\"path\":");
        enc_str(sb, path, 0);
    }
    if (text) {
        sb_cstr(sb, ",\"sql\":");
        enc_str(sb, text, 0);
    }
    if (params) {
        sb_cstr(sb, ",\"params\":");
        enc_values(sb, params);
    }
    rec_ident = sb_finish(sb);
    GC_UNROOT(1);
    return rec_ident;
}

static void write_line(SB *sb)
{
    sb_cstr(sb, "}\n");
    Str *s = sb_finish(sb);
    rw_bytes(s->data, s->len);
}

/* Recording: the call's line with its outcome r. */
static void record(Str *ident, int op, Variant *r)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_bytes(sb, ident->data, ident->len);
    if (r->tag != 0) {
        sb_cstr(sb, ",\"result\":\"err\",\"error\":");
        enc_str(sb, r->pay.s, 0);
    } else {
        sb_cstr(sb, ",\"result\":\"ok\"");
        if (op == OP_OPEN) {
            sb_cstr(sb, ",\"db\":");
            enc_int(sb, r->pay.i);
        } else if (op == OP_EXEC) {
            List *l = r->pay.p;
            sb_cstr(sb, ",\"changed\":");
            enc_int(sb, l->data[0].i);
            sb_cstr(sb, ",\"last_id\":");
            enc_int(sb, l->data[1].i);
        } else if (op == OP_QUERY) {
            List *rows = r->pay.p, *names = rows->data[0].p;
            sb_cstr(sb, ",\"columns\":[");
            for (long i = 0; i < names->len; i++) {
                Str *n = ((Variant *)names->data[i].p)->pay.s;
                if (i) sb_byte(sb, ',');
                enc_str(sb, n, !rw_valid_utf8(n->data, n->len));
            }
            sb_cstr(sb, "],\"rows\":[");
            for (long i = 1; i < rows->len; i++) {
                if (i > 1) sb_byte(sb, ',');
                enc_values(sb, rows->data[i].p);
            }
            sb_byte(sb, ']');
        }
    }
    write_line(sb);
    GC_UNROOT(1);
}

/* Recording: a file SQLite was refused (R0010 follows), as the outcome of the call being recorded. */
static void record_denied(long bit, Str *target)
{
    if (rcd_mode != 1 || !rec_ident) return;
    GC_ROOT(&target);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_bytes(sb, rec_ident->data, rec_ident->len);
    sb_cstr(sb, bit == EF_FWRITE ? ",\"result\":\"denied\",\"effect\":\"file.write\",\"target\":"
                                 : ",\"result\":\"denied\",\"effect\":\"file.read\",\"target\":");
    enc_str(sb, target, !rw_valid_utf8(target->data, target->len));
    write_line(sb);
    GC_UNROOT(2);
    rec_ident = 0;
}

/* ---- reading an outcome back (T0008 when it is not what event() writes) ---- */

static char *pp, *pe;

static int lit(const char *s)
{
    long n = 0;
    while (s[n]) n++;
    if (pe - pp < n || !mem_eq(pp, (char *)s, n)) return 0;
    pp += n;
    return 1;
}

static void need(const char *s)
{
    if (!lit(s)) rp_bad();
}

static long pint(void)
{
    long v;
    pp = rp_int(pp, pe, &v);
    return v;
}

static Str *pstr(void)
{
    Str *s;
    pp = rp_string(pp, pe, &s);
    return s;
}

static Variant *pvalue(void)
{
    need("{\"");
    long tag = V_NULL, pay = 0;
    Str *s = 0;
    GC_ROOT(&s);
    if (lit("int\":")) {
        tag = V_INT;
        pay = pint();
    } else if (lit("float\":")) {
        s = pstr();
        if (s->len != 16) rp_bad();
        for (long i = 0; i < 16; i++) {
            int c = (unsigned char)s->data[i];
            int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            if (d < 0) rp_bad();
            pay = (long)(((unsigned long)pay << 4) | (unsigned long)d);
        }
        tag = V_FLOAT;
    } else if (lit("text\":")) {
        s = pstr();
        if (utf8_validate(s->data, s->len) >= 0) rp_bad();
        tag = V_TEXT;
    } else if (lit("bytes\":")) {
        s = pstr();
        tag = V_BYTES;
    } else {
        need("null\":true");
    }
    if (s && tag != V_FLOAT) pay = (long)s;
    need("}");
    Variant *v = make_result((int)tag, pay);
    GC_UNROOT(1);
    return v;
}

/* [value, ...] (names: [str, ...] as text values) */
static List *pvalues(int names)
{
    List *l = list_new(4);
    Variant *v = 0;
    Str *s = 0;
    GC_ROOT(&l);
    GC_ROOT(&v);
    GC_ROOT(&s);
    need("[");
    if (!lit("]")) {
        do {
            if (names) {
                s = pstr();
                v = make_result(V_TEXT, (long)s);
            } else {
                v = pvalue();
            }
            list_push(l, (long)v);
        } while (lit(","));
        need("]");
    }
    GC_UNROOT(3);
    return l;
}

/* Replay: the outcome of the next event, which must be this call (R0014 otherwise). */
static Variant *replayed(Str *ident, int op, Str *name)
{
    Str *line = rp_take_sql(ident, name); /* kept by the recording's events */
    pp = line->data + ident->len;
    pe = line->data + line->len;
    Variant *r = 0;
    List *l = 0, *row = 0;
    GC_ROOT(&r);
    GC_ROOT(&l);
    GC_ROOT(&row);
    need(",\"result\":\"");
    if (lit("ok\"")) {
        if (op == OP_OPEN) {
            need(",\"db\":");
            r = make_result(0, pint());
        } else if (op == OP_EXEC) {
            l = list_new(2);
            need(",\"changed\":");
            list_push(l, pint());
            need(",\"last_id\":");
            list_push(l, pint());
            r = make_result(0, (long)l);
        } else if (op == OP_QUERY) {
            l = list_new(8);
            need(",\"columns\":");
            row = pvalues(1);
            list_push(l, (long)row);
            need(",\"rows\":[");
            if (!lit("]")) {
                do {
                    row = pvalues(0);
                    list_push(l, (long)row);
                } while (lit(","));
                need("]");
            }
            r = make_result(0, (long)l);
        } else {
            r = make_result(0, 0);
        }
    } else if (lit("err\",\"error\":")) {
        row = (List *)pstr(); /* (rooted) */
        r = err_result((Str *)row);
    } else {
        need("denied\",\"effect\":\"file.");
        long bit = lit("write\"") ? EF_FWRITE : (need("read\""), EF_FREAD);
        need(",\"target\":");
        Str *target = pstr();
        need("}");
        if (pp != pe) rp_bad();
        rt_denied(bit, target); /* as the recorded run stopped */
    }
    need("}");
    if (pp != pe) rp_bad();
    GC_UNROOT(3);
    return r;
}

/* One primitive call: recorded with its outcome r, or (replaying, r is 0) answered from the recording. */
static Variant *event(Str *ident, int op, Str *name, Variant *r, Node *call)
{
    rec_ident = 0;
    if (rcd_mode == 2) {
        cur_node = (long)std_user_call(call);
        return replayed(ident, op, name);
    }
    if (rcd_mode == 1) {
        GC_ROOT(&r);
        record(ident, op, r);
        GC_UNROOT(1);
    }
    return r;
}
