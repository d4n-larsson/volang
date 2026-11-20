/* Recordings and replay (spec 006 US5): what a run receives from outside, written with
 * `vo run --record`, fed back by `vo replay` (research R15-R17, contracts/recording.md; ported from
 * src/replay.s, spec 008).
 *
 * Every outside input reaches a program through the builtin stubs of rt_call.s, which call the
 * io_* wrappers below for read_line, read_all, read_file, write_file, env and now_ms. With
 * rcd_mode 0 they go straight to the runtime; 1 records each result after it; 2 takes the next
 * recorded event instead of touching the outside. Arguments and the seed go in the header.
 * Writing the recording allocates nothing (the end is written from vo_exit, on every way out). */
#include "vo.h"

long rcd_mode;                /* 0 off, 1 recording, 2 replaying */
static void *rcd_fb;          /* the recording (FBuf), or 0 */
static long rcd_failed;       /* it could not be created (T0009 at exit) */
static Str *rcd_request; /* spec 009: the request line of the http event being written */
static int rcd_tls;      /* spec 019: ... and whether it went over HTTPS */
static long rcd_pending_kind; /* a read/write being checked: written as `denied` if R0010 ends the run */
static Str *rcd_to;           /* spec 017: rename's new path, written as "to" */
static List *rcd_args;        /* spec 017: a run's argv, written as "args" (without the program) */
static Str *rcd_pending_path;
static Vec *rp_events;        /* replay: Event records */
static long rp_next;          /* replay: index of the next event */
static Str *rp_program;       /* replay: header fields */
static Str *rp_fprint;
static long rp_seed;
static long rp_clock = -1;
static Vec *rp_args; /* strs */
static long rp_saw_end;

static const char *const ev_names[] = {0,         "read_line", "read_all",    "read_file", "write_file",
                                       "env",     "now_ms",    "end",         "http",      "listen",
                                       "request", "response",  "info",        "list_dir",  "append_file",
                                       "make_dir", "delete",   "rename",      "run",       "sql",
                                       "setup",   "setup_end"}; /* spec 028 */
static const char *const ev_results[] = {"ok", "err", "denied"};
static Str *run_desc(Str *program, Str *joined);
static const char hexd[] = "0123456789abcdef";

/* What the program asked for (d_) and what the recording has (n_), by event kind. */
static const char *const d_table[] = {0,
                                      "reads a line of stdin",
                                      "reads the rest of stdin",
                                      "reads the file `$1`",
                                      "writes the file `$1`",
                                      "reads the variable `$1`",
                                      "reads the clock",
                                      "the end",
                                      "makes an HTTP request $1",
                                      "serves on `$1`",
                                      "waits for an HTTP request to answer",
                                      "sends an HTTP response",
                                      "looks at `$1`",
                                      "lists the directory `$1`",
                                      "appends to `$1`",
                                      "creates the directory `$1`",
                                      "deletes `$1`",
                                      "renames `$1`",
                                      "runs `$1`",
                                      "makes the database call `$1`",
                                      "starts serving with workers",
                                      "finishes a worker's setup"};
static const char *const n_table[] = {0,
                                      "a line of stdin",
                                      "the rest of stdin",
                                      "a read of `$1`",
                                      "a write of `$1`",
                                      "the variable `$1`",
                                      "a clock reading",
                                      "the end",
                                      "an HTTP request $1",
                                      "a server on `$1`",
                                      "a received HTTP request",
                                      "a sent HTTP response",
                                      "a look at `$1`",
                                      "a listing of `$1`",
                                      "an append to `$1`",
                                      "a directory created at `$1`",
                                      "a delete of `$1`",
                                      "a rename of `$1`",
                                      "a run of `$1`",
                                      "the database call `$1`",
                                      "a worker's setup",
                                      "the end of a worker's setup"};

#define J_HEAD "{\"v\":1,\"kind\":\""
#define J_CLOSE "}\n"

/* ---- writing (no allocation) ---- */

/* Writes a C string to the recording. */
void rw(char *s)
{
    if (rcd_fb) fb_cstr(rcd_fb, s);
}

/* Writes an integer (negative ones with a sign) to the recording. */
void rw_dec(long v)
{
    if (!rcd_fb) return;
    if (v < 0) {
        rw("-");
        v = (long)(0UL - (unsigned long)v);
    }
    fb_dec(rcd_fb, v);
}

/* 1 when p[0..n) is valid UTF-8. */
long rw_valid_utf8(char *p, long n)
{
    while (n > 0) {
        if ((unsigned char)*p < 0x80) {
            p++;
            n--;
            continue;
        }
        long k = utf8_seq_len(p, n);
        if (!k) return 0;
        p += k;
        n -= k;
    }
    return 1;
}

/* A quoted JSON string: `"`, `\`, control bytes and (with bytes) every byte >= 0x80 escaped, the
 * last two as \u00XX; anything else as it is. */
void rw_str(char *p, long n, long bytes)
{
    if (!rcd_fb) return;
    rw("\"");
    for (; n > 0; n--) {
        int c = (unsigned char)*p++;
        char out[6];
        if (c == '"' || c == '\\') { /* \" or \\ */
            out[0] = '\\';
            out[1] = (char)c;
            fbuf_write(rcd_fb, out, 2);
        } else if (c < 0x20 || (c >= 0x80 && bytes)) { /* \u00XX */
            out[0] = '\\';
            out[1] = 'u';
            out[2] = '0';
            out[3] = '0';
            out[4] = hexd[c >> 4];
            out[5] = hexd[c & 15];
            fbuf_write(rcd_fb, out, 6);
        } else { /* the byte as it is */
            out[0] = (char)c;
            fbuf_write(rcd_fb, out, 1);
        }
    }
    rw("\"");
}

/* One event record. name: the path or variable (str) or 0; result: EVR_* or -1 for none;
 * value: a str, or 0 (then int_value is written when kind is now_ms). */
void rcd_event(long kind, Str *name, long result, Str *value, long int_value)
{
    long bytes = value && !rw_valid_utf8(value->data, value->len);
    rw(J_HEAD);
    rw((char *)ev_names[kind]);
    if (name) {
        rw(kind == EVK_ENV                ? "\",\"name\":"
           : kind == EVK_HTTP           ? "\",\"host\":"
           : kind == EVK_SERVE_LISTEN   ? "\",\"address\":"
           : kind == EVK_RUN            ? "\",\"program\":"
                                        : "\",\"path\":");
        rw_str(name->data, name->len, 0);
        if (kind == EVK_RENAME && rcd_to) { /* spec 017 */
            rw(",\"to\":");
            rw_str(rcd_to->data, rcd_to->len, 0);
        }
        if (kind == EVK_RUN && rcd_args) {
            rw(",\"args\":[");
            for (long i = 1; i < rcd_args->len; i++) {
                Str *a = (Str *)rcd_args->data[i].p;
                if (i > 1) rw(",");
                rw_str(a->data, a->len, !rw_valid_utf8(a->data, a->len));
            }
            rw("]");
        }
        if (kind == EVK_HTTP) { /* spec 007: the port (int_value); spec 009: the request line */
            rw(",\"port\":");
            rw_dec(int_value);
            if (rcd_tls) rw(",\"scheme\":\"https\""); /* spec 019 (absent: http) */
            if (rcd_request) {
                rw(",\"request\":");
                rw_str(rcd_request->data, rcd_request->len, 0);
            }
        }
    } else {
        rw("\"");
    }
    if (result >= 0) {
        rw(",\"result\":\"");
        rw((char *)ev_results[result]);
        rw("\"");
    }
    if (kind == EVK_SETUP) { /* spec 028: the worker's number */
        rw(",\"worker\":");
        rw_dec(int_value);
    } else if (kind == EVK_NOW_MS || (kind == EVK_SERVE_LISTEN && result == EVR_OK)) { /* spec 011: the listener */
        rw(",\"value\":");
        rw_dec(int_value);
    } else if (value) {
        rw(",\"value\":");
        rw_str(value->data, value->len, bytes);
        if (bytes) rw(",\"bytes\":true");
    }
    rw(J_CLOSE);
}

/* Records a result value: ok(str)/ok() or err(str). */
void rcd_result(long kind, Str *name, Variant *result)
{
    rcd_event(kind, name, (long)(uint32_t)result->tag, result->pay.s, 0);
}

/* ---- start and end ---- */

/* FNV-1a 64 over each program file's path and text, in load order. */
long rp_fingerprint(void)
{
    long h = (long)0xcbf29ce484222325UL;
    Vec *files = ld_files();
    for (long i = 0; i < files->n; i++) {
        FileRec *f = file_get((int)(long)vec_get(files, i));
        h = fnv_bytes(h, f->path->data, f->path->len);
        h = fnv_bytes(h, f->text, f->len);
    }
    return h;
}

long fnv_bytes(long h, char *p, long n)
{
    unsigned long x = (unsigned long)h;
    for (; n > 0; n--) x = (x ^ (unsigned char)*p++) * 0x100000001b3UL;
    return (long)x;
}

/* 16 hex digits. */
void rw_hex64(long v)
{
    char out[16];
    unsigned long x = (unsigned long)v;
    for (int i = 15; i >= 0; i--) {
        out[i] = hexd[x & 15];
        x >>= 4;
    }
    if (rcd_fb) fbuf_write(rcd_fb, out, 16);
}

/* With --record: opens the recording and writes its header; fixes the seed first (from the clock
 * when --seed was not given) so the header can name it. */
void rcd_start(void)
{
    if (!opt_record) return;
    if (!opt_seed_set) rt_set_option(1, sys_clock_ns());
    rcd_fb = fbuf_open((char *)opt_record);
    if (!rcd_fb) rcd_failed = 1;
    rcd_mode = 1;
    rw("{\"v\":1,\"kind\":\"recording\",\"program\":");
    char *program = ((char **)pos_args)[0];
    rw_str(program, cstr_len(program), 0);
    rw(",\"fingerprint\":\"");
    rw_hex64(rp_fingerprint());
    rw("\",\"seed\":");
    rw_dec(opt_seed);
    rw(",\"clock_ms\":");
    if (opt_clock_set)
        rw_dec(opt_clock);
    else
        rw("null");
    rw(",\"args\":[");
    for (long i = 0; i < prog_argc; i++) {
        if (i) rw(",");
        char *arg = ((char **)prog_argv)[i];
        rw_str(arg, cstr_len(arg), 0);
    }
    rw("]}\n");
}

/* Completes the recording (a pending read or write that R0010 stopped is written as `denied`, then
 * the `end` record) and returns the exit status (74 when it could not be written). No allocation. */
long rcd_finish(long status)
{
    if (rcd_mode != 1) return status;
    rcd_mode = 0;
    if (rcd_fb) {
        if (rcd_pending_kind) rcd_event_raw(rcd_pending_kind, rcd_pending_path, EVR_DENIED, 0, 0);
        rw(J_HEAD);
        rw("end");
        rw("\",\"status\":");
        rw_dec(status);
        rw(J_CLOSE);
        fbuf_flush(rcd_fb);
        if (!((FBuf *)rcd_fb)->err) return status;
    }
    out_unwritable((char *)opt_record);
    return status ? status : EXIT_OUTPUT;
}

/* rcd_event with rcd_mode set to 1 for the call (rcd_finish has turned it off). */
void rcd_event_raw(long kind, Str *name, long result, Str *value, long int_value)
{
    rcd_event(kind, name, result, value, int_value);
}

/* ---- the wrappers the builtin stubs call ---- */

void *io_read_line(Node *node)
{
    if (!rcd_mode) return rt_read_line(node);
    if (rcd_mode == 2) return rp_result(rp_take(EVK_READ_LINE, 0));
    Variant *r = rt_read_line(node);
    rcd_result(EVK_READ_LINE, 0, r);
    return r;
}

void *io_read_all(Node *node)
{
    if (!rcd_mode) return rt_read_all(node);
    if (rcd_mode == 2) return rp_take(EVK_READ_ALL, 0)->value.p;
    Str *s = rt_read_all(node);
    rcd_event(EVK_READ_ALL, 0, -1, s, 0);
    return s;
}

/* The path check (R0010) and the read; replayed `denied` repeats the check's error. */
void *io_read_file(Str *path)
{
    void *r;
    GC_ROOT(&path);
    if (rcd_mode == 2) {
        Event *e = rp_take(EVK_READ_FILE, path);
        rt_check_path(EF_FREAD, path); /* spec 013 (M1, L2): every file event, lexically, whatever its result */
        if (e->result == EVR_DENIED) {         /* the same R0010 as the recorded run */
            rp_mismatch_now(EVK_READ_FILE, path); /* (allowed now: the program or its grant changed) */
        }
        r = rp_result(e);
    } else {
        if (rcd_mode == 1) { /* recording: `denied` if the check stops the run */
            rcd_pending_kind = EVK_READ_FILE;
            rcd_pending_path = path;
        }
        rt_check_path(EF_FREAD, path);
        rcd_pending_kind = 0;
        r = rt_read_file(path);
        if (rcd_mode) rcd_result(EVK_READ_FILE, path, r);
    }
    GC_UNROOT(1);
    return r;
}

/* Replay writes nothing and returns the recorded result. */
void *io_write_file(Str *path, Str *text)
{
    void *r;
    GC_ROOT(&path);
    GC_ROOT(&text);
    if (rcd_mode == 2) {
        Event *e = rp_take(EVK_WRITE_FILE, path);
        rt_check_path(EF_FWRITE, path); /* spec 013 (M1, L2): every file event, lexically, whatever its result */
        if (e->result == EVR_DENIED) {
            rp_mismatch_now(EVK_WRITE_FILE, path);
        }
        r = rp_result(e);
    } else {
        if (rcd_mode == 1) {
            rcd_pending_kind = EVK_WRITE_FILE;
            rcd_pending_path = path;
        }
        rt_check_path(EF_FWRITE, path);
        rcd_pending_kind = 0;
        r = rt_write_file(path, text);
        if (rcd_mode) rcd_result(EVK_WRITE_FILE, path, r);
    }
    GC_UNROOT(2);
    return r;
}

void *io_env(Str *name)
{
    if (!rcd_mode) return rt_env(name);
    void *r;
    GC_ROOT(&name);
    if (rcd_mode == 2) {
        rt_check_name(EF_ENV, name); /* the name is still checked against the grant (no outside data) */
        r = rp_result(rp_take(EVK_ENV, name));
    } else {
        r = rt_env(name);
        rcd_result(EVK_ENV, name, r);
    }
    GC_UNROOT(1);
    return r;
}

/* For R0014's messages: "`GET /x` to `host:port`", or "to `host:port`" without a request line. */
static Str *http_desc(Str *host, long port, Str *request, long tls)
{
    SB *sb = sb_new();
    if (request) {
        sb_byte(sb, '`');
        sb_str(sb, request);
        sb_cstr(sb, "` ");
    }
    sb_cstr(sb, "to `");
    if (tls) sb_cstr(sb, "https://"); /* spec 019: plain HTTP keeps its old description */
    sb_str(sb, host);
    sb_byte(sb, ':');
    int_to_dec(sb, port);
    sb_byte(sb, '`');
    return sb_finish(sb);
}

/* Spec 009: the request line without the protocol ("GET /status?x=1"): what replay compares. */
static Str *request_line(Str *request)
{
    long end = 0;
    while (end < request->len && request->data[end] != '\r' && request->data[end] != '\n') end++;
    long cut = end; /* drop a final " HTTP/..." */
    while (cut > 0 && request->data[cut - 1] != ' ') cut--;
    if (cut > 0 && end - cut >= 5 && mem_eq(request->data + cut, "HTTP/", 5)) end = cut - 1;
    return str_new(request->data, end);
}

/* Spec 007 (research R11): the host check (R0010) in every mode, then the exchange; replay returns
 * the recorded response without the network, and refuses another host or port (R0014). */
static Variant *io_exchange(Str *host, long port, Str *request, int tls);

Variant *io_http_exchange(Str *host, long port, Str *request)
{
    return io_exchange(host, port, request, 0);
}

/* spec 019: the same over TLS (std.http with an https:// URL). */
Variant *io_https_exchange(Str *host, long port, Str *request)
{
    return io_exchange(host, port, request, 1);
}

static Variant *io_exchange(Str *host, long port, Str *request, int tls)
{
    Variant *r;
    GC_ROOT(&host);
    GC_ROOT(&request);
    rt_check_name(EF_NET, host);
    Str *line = request_line(request);
    GC_ROOT(&line);
    if (rcd_mode == 2) {
        Str *asked = http_desc(host, port, line, tls);
        if (rp_next >= rp_events->n) rp_mismatch(EVK_HTTP, asked, 0);
        Event *e = vec_get(rp_events, rp_next++);
        if (e->kind != EVK_HTTP || !str_eq(e->name, host) || e->port != port || e->tls != tls ||
            (e->request && !str_eq(e->request, line))) /* older recordings: host and port only */
            rp_mismatch(EVK_HTTP, asked, e);
        r = rp_result(e);
    } else {
        r = tls ? tls_exchange(host, port, request) : rt_http_exchange(host, port, request);
        if (rcd_mode) {
            rcd_request = line;
            rcd_tls = tls;
            rcd_event(EVK_HTTP, host, (long)(uint32_t)r->tag, r->pay.s, port);
            rcd_request = 0;
            rcd_tls = 0;
        }
    }
    if (!r->tag) { /* spec 013 (R4): the raw response (as recorded) is framed here, by the server's rules */
        HttpFrame f = {0};
        GC_ROOT(&r);
        GC_ROOT(&f.body);
        GC_ROOT(&f.out);
        Str *raw = r->pay.s;
        long st = http_frame(&f, raw->data, raw->len, 1, 1);
        if (st > 0) {
            r = make_result(0, (long)f.out);
        } else {
            SB *sb = sb_new();
            GC_ROOT(&sb);
            sb_cstr(sb, "malformed response: ");
            sb_cstr(sb, st == -413 ? "the body is larger than 16 MiB" : st == -431 ? "the head is larger than 64 KiB" : f.why);
            r = make_result(1, (long)sb_finish(sb));
            GC_UNROOT(1);
        }
        GC_UNROOT(3);
    }
    GC_UNROOT(3);
    return r;
}

long io_now_ms(void)
{
    if (!rcd_mode) return rt_now_ms();
    if (rcd_mode == 2) return rp_take(EVK_NOW_MS, 0)->value.i;
    long v = rt_now_ms();
    rcd_event(EVK_NOW_MS, 0, -1, 0, v);
    return v;
}

/* ---- replay ---- */

/* The value a recorded result stands for: ok(value) / ok() / err(value). */
void *rp_result(Event *event)
{
    return make_result((int)event->result, event->value.i);
}

/* The next event, which must be of this kind (and name); otherwise R0014. */
Event *rp_take(long kind, Str *name)
{
    if (rp_next >= rp_events->n) rp_mismatch(kind, name, 0); /* nothing left */
    Event *e = vec_get(rp_events, rp_next++);
    if (e->kind != kind || (name && (!e->name || !str_eq(e->name, name)))) rp_mismatch(kind, name, e);
    return e;
}

/* One of the d_/n_ templates with the name filled in. */
Str *rp_describe(char *tmpl, Str *name)
{
    return name ? msg(tmpl, name, 0, 0) : str_from_cstr(tmpl);
}

/* R0014: what the program asked for, and what the recording has next (or that it has no more). */
void rp_mismatch(long kind, Str *name, Event *event)
{
    Str *asked = rp_describe((char *)d_table[kind], name);
    Str *m;
    GC_ROOT(&asked);
    if (event) {
        Str *has = rp_describe((char *)n_table[event->kind],
                               event->kind == EVK_HTTP ? http_desc(event->name, event->port, event->request, event->tls)
                               : event->kind == EVK_RUN ? run_desc(event->name, event->request)
                                                        : event->name);
        m = msg("The program $1, but the recording has $2 next: replay the program that was recorded, or record this "
                "one.",
                asked, has, 0);
    } else {
        m = msg("The program $1, but the recording has no more inputs: replay the program that was recorded, or "
                "record this one.",
                asked, 0, 0);
    }
    GC_UNROOT(1);
    rt_raise("R0014", m, (void *)cur_node);
}

/* R0014 for a recorded `denied` that the program is now allowed to do: the event just taken. */
void rp_mismatch_now(long kind, Str *name)
{
    rp_mismatch(kind, name, vec_get(rp_events, rp_next - 1));
}

/* ---- spec 026: std.sqlite events (contracts/recording.md of specs/026-sqlite) ---- */

/* Writes n bytes to the recording. */
void rw_bytes(char *p, long n)
{
    if (rcd_fb) fbuf_write(rcd_fb, p, n);
}

/* The next event, which must be the database call whose line starts with ident (its operation,
 * connection, path or statement and parameters: what the program asked for) followed by its outcome;
 * returns the line. name: the statement, path or operation, for R0014. */
Str *rp_take_sql(Str *ident, Str *name)
{
    if (rp_next >= rp_events->n) rp_mismatch(EVK_SQL, name, 0);
    Event *e = vec_get(rp_events, rp_next++);
    Str *line = e->request;
    if (e->kind == EVK_SQL && line->len > ident->len && mem_eq(line->data, ident->data, ident->len) &&
        line->data[ident->len] == ',')
        return line;
    if (e->kind == EVK_SQL && str_eq(e->name, name)) /* the same call with other arguments */
        rt_raise("R0014",
                 msg("The program makes the database call `$1` with other arguments (connection or parameters) than "
                     "the recording: replay the program that was recorded, or record this one.",
                     name, 0, 0),
                 (void *)cur_node);
    rp_mismatch(EVK_SQL, name, e);
    return 0;
}

/* ---- spec 017: std.fs and std.process events ---- */

/* The strs of v (a Vec of the reader, or the List of a run's argv from index 1) joined by NUL bytes. */
static Str *join_strs(W *data, long from, long n)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    for (long i = from; i < n; i++) {
        Str *s = (Str *)data[i].p;
        if (i > from) sb_byte(sb, 0);
        sb_bytes(sb, s->data, s->len);
    }
    GC_UNROOT(1);
    return sb_finish(sb);
}

static Str *join_args(Vec *v)
{
    return join_strs(v->data, 0, v->n);
}

/* "program arg arg" for R0014's texts (a joined argument str: NUL bytes as spaces). */
static Str *run_desc(Str *program, Str *joined)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_str(sb, program);
    for (long i = 0; joined && i < joined->len; i++) {
        if (!i) sb_byte(sb, ' ');
        sb_byte(sb, joined->data[i] ? joined->data[i] : ' ');
    }
    GC_UNROOT(1);
    return sb_finish(sb);
}

/* While recording: name is being checked; R0010 ends the run with it written as `denied`. */
void io_pend(long kind, Str *name)
{
    if (rcd_mode != 1) return;
    rcd_pending_kind = kind;
    rcd_pending_path = name;
}

/* While recording: the event of an operation that ran (to: rename's new path; argv: a run's). */
void io_record(long kind, Str *name, Str *to, List *argv, Variant *r)
{
    rcd_pending_kind = 0;
    if (rcd_mode != 1) return;
    rcd_to = to;
    rcd_args = argv;
    rcd_result(kind, name, r);
    rcd_to = 0;
    rcd_args = 0;
}

/* While replaying: the next event, which must be this operation (R0014 otherwise; a recorded `denied`
 * that is allowed now too); its recorded result. The grant was checked before. */
Variant *io_replay(long kind, Str *name, Str *to, List *argv)
{
    GC_ROOT(&name);
    GC_ROOT(&to);
    Str *extra = kind == EVK_RUN ? join_strs(argv->data, 1, argv->len) : to;
    GC_ROOT(&extra);
    Str *asked = kind == EVK_RUN ? run_desc(name, extra) : name;
    GC_ROOT(&asked);
    if (rp_next >= rp_events->n) rp_mismatch(kind, asked, 0);
    Event *e = vec_get(rp_events, rp_next++);
    if (e->kind != kind || !e->name || !str_eq(e->name, name) ||
        (e->result != EVR_DENIED && extra && (!e->request || !str_eq(e->request, extra))))
        rp_mismatch(kind, asked, e);
    if (e->result == EVR_DENIED) rp_mismatch(kind, asked, e); /* allowed now: the program or its grant changed */
    GC_UNROOT(4);
    return rp_result(e);
}

/* ---- reading a recording ---- */

static Str *rf_kind; /* fields of the line being read */
static Str *rf_program;
static Str *rf_fprint;
static long rf_seed;
static long rf_clock = -1;
static Vec *rf_args;
static Str *rf_name;
static Str *rf_result;
static Str *rf_value;
static long rf_ivalue;
static long rf_isint;
static long rf_status;
static long rf_port = -1; /* spec 007: http events */
static Str *rf_request;   /* spec 009 */
static Str *rf_scheme;    /* spec 019: "https" on an HTTPS http event */
static Str *rf_op;        /* spec 026: an sql event's operation and statement */
static Str *rf_sql;
static Str *rf_line; /* ... and its whole line */

static const char *const rk_keys[] = {"kind",   "program", "fingerprint", "seed", "clock_ms", "args",    "path",
                                      "name",   "result",  "value",       "status", "host",     "port",    "request",
                                      "address", "to", "scheme", "op", "sql", 0};
static long rf_args_line; /* spec 017: this line had "args" (a run event's arguments) */

/* Where each string-valued key is stored (rk_keys order; 0: not a string). */
static Str **const rf_slots[] = {&rf_kind, &rf_program, &rf_fprint, 0, 0, 0, &rf_name, &rf_name, &rf_result,
                                 &rf_value, 0, &rf_name, 0, &rf_request, &rf_name, &rf_request, &rf_scheme,
                                 &rf_op, &rf_sql};

/* T0008 invalid_recording, exit 64. */
void rp_bad(void)
{
    Str *path = str_from_cstr(((char **)pos_args)[0]);
    Str *m = msg("`$1` is not a complete recording: record the run again with `vo run --record FILE`.", path, 0, 0);
    toolchain_error((int)opt_json, (int)report_fd, "T0008", m, 0);
    vo_exit(EXIT_USAGE);
}

/* Skips spaces. */
char *rp_ws(char *p, char *end)
{
    while (p < end && *p == ' ') p++;
    return p;
}

/* A JSON string at p (after its opening quote is checked) into *out as a str; returns the
 * position after the closing quote. \u00XX stands for one byte (contracts/recording.md). */
char *rp_string(char *p, char *end, Str **out)
{
    if (p >= end || *p != '"') rp_bad();
    p++;
    SB *sb = sb_new();
    for (;;) {
        if (p >= end) rp_bad();
        int c = (unsigned char)*p++;
        if (c == '"') break;
        if (c == '\\') { /* an escape */
            if (p >= end) rp_bad();
            c = (unsigned char)*p++;
            if (c == 'n') {
                c = 10;
            } else if (c == 't') {
                c = 9;
            } else if (c == 'r') {
                c = 13;
            } else if (c == 'u') { /* \uXXXX: only 00XX is written */
                if (p + 4 > end) rp_bad();
                long v = 0;
                for (int k = 0; k < 4; k++) {
                    int d = (unsigned char)*p++;
                    if ((unsigned)(d - '0') < 10)
                        d -= '0';
                    else if ((unsigned)(d - 'a') < 6)
                        d -= 87; /* a-f */
                    else
                        rp_bad();
                    v = v * 16 + d;
                }
                if (v >= 256) rp_bad();
                c = (int)v;
            } /* \" \\ \/: the character itself */
        }
        sb_byte(sb, c);
    }
    *out = sb_finish(sb);
    return p;
}

char *rp_int(char *p, char *end, long *out)
{
    unsigned long v = 0;
    int negative = 0;
    if (p >= end) rp_bad();
    if (*p == '-') {
        negative = 1;
        p++;
    }
    char *digits = p;
    while (p < end && (unsigned)(*p - '0') < 10) {
        unsigned long d = (unsigned long)(*p++ - '0');
        if (v > (0x8000000000000000UL - d) / 10) rp_bad(); /* spec 013 (L6): over 64 bits */
        v = v * 10 + d;
    }
    if (p == digits || (!negative && v > 0x7fffffffffffffffUL)) rp_bad();
    *out = negative ? (long)(0UL - v) : (long)v;
    return p;
}

/* The index of a known key (rk_keys), or -1. */
long rp_key(Str *str)
{
    for (long i = 0; rk_keys[i]; i++)
        if (str_eq_cstr(str, (char *)rk_keys[i])) return i;
    return -1;
}

/* Spec 026: skips an array or object (an sql event's params, columns and rows: src/sqlite_rt.c reads
 * them from the line). */
static char *rp_skip(char *p, char *end)
{
    long depth = 0;
    do {
        if (p >= end) rp_bad();
        if (*p == '"') {
            Str *s;
            p = rp_string(p, end, &s);
            continue;
        }
        depth += (*p == '[' || *p == '{') - (*p == ']' || *p == '}');
        p++;
    } while (depth > 0);
    return p;
}

/* One record into the rf_* fields: a flat object of strings, integers, true/false/null and (for
 * args) an array of strings; other arrays and objects are skipped. Anything else is T0008. */
void rp_line(char *p, char *end)
{
    rf_kind = 0;
    rf_name = 0;
    rf_result = 0;
    rf_value = 0;
    rf_isint = 0;
    rf_port = -1;
    rf_request = 0;
    rf_scheme = 0;
    rf_op = 0;
    rf_sql = 0;
    rf_program = 0;
    rf_args_line = 0;
    p = rp_ws(p, end);
    if (p >= end || *p != '{') rp_bad();
    p++;
    for (;;) {
        Str *key; /* "key": */
        p = rp_ws(rp_string(rp_ws(p, end), end, &key), end);
        if (p >= end || *p != ':') rp_bad();
        p = rp_ws(p + 1, end);
        long field = rp_key(key); /* which field (-1: ignored) */
        if (p >= end) rp_bad();
        if (*p == '"') { /* a string */
            Str *s;
            p = rp_string(p, end, &s);
            if (field >= 0 && rf_slots[field]) *rf_slots[field] = s;
        } else if ((*p == '[' && field != 5) || *p == '{') {
            p = rp_skip(p, end);
        } else if (*p == '[') { /* an array of strings (args) */
            p++;
            Vec *v = vec_new();
            for (;;) {
                p = rp_ws(p, end);
                if (p >= end) rp_bad();
                if (*p == ']') break;
                Str *s;
                p = rp_ws(rp_string(p, end, &s), end);
                vec_push(v, s);
                if (p >= end) rp_bad();
                if (*p == ',') p++;
            }
            p++;
            if (field == 5) {
                rf_args = v;
                rf_args_line = 1;
            }
        } else if (*p == 't') { /* true (spec 013, L6: the literals exactly) */
            if (end - p < 4 || !mem_eq(p, "true", 4)) rp_bad();
            p += 4;
        } else if (*p == 'f') { /* false */
            if (end - p < 5 || !mem_eq(p, "false", 5)) rp_bad();
            p += 5;
        } else if (*p == 'n') { /* null (clock_ms) */
            if (end - p < 4 || !mem_eq(p, "null", 4)) rp_bad();
            p += 4;
            if (field == 4) rf_clock = -1;
        } else { /* an integer */
            long v;
            p = rp_int(p, end, &v);
            if (field == 3) {
                rf_seed = v;
            } else if (field == 4) {
                rf_clock = v;
            } else if (field == 9) {
                rf_ivalue = v;
                rf_isint = 1;
            } else if (field == 10) {
                rf_status = v;
            } else if (field == 12) {
                rf_port = v;
            }
        }
        p = rp_ws(p, end); /* , or } */
        if (p >= end) rp_bad();
        int c = (unsigned char)*p++;
        if (c == ',') continue;
        if (c == '}') return;
        rp_bad();
    }
}

/* EVK_* for an event kind name; 0 for "recording"; -1 when unknown. */
long rp_kind_of(Str *str)
{
    if (!str) return -1;
    if (str_eq_cstr(str, "recording")) return 0;
    for (long k = 1; k <= EVK_SETUP_END; k++)
        if (str_eq_cstr(str, (char *)ev_names[k])) return k;
    return -1;
}

/* EVR_* for "ok", "err", "denied"; -1 when there is none. */
long rp_result_of(Str *str)
{
    if (!str) return -1;
    if (str_eq_cstr(str, "ok")) return EVR_OK;
    if (str_eq_cstr(str, "err")) return EVR_ERR;
    if (str_eq_cstr(str, "denied")) return EVR_DENIED;
    rp_bad();
    return -1;
}

/* Every line: the header first, then events, then `end` (nothing after it). */
void rp_read(char *text, long len)
{
    char *p = text, *end = text + len;
    long records = 0;
    rp_events = vec_new();
    while (p < end) {
        char *le = p; /* the end of the line */
        while (le < end && *le != '\n') le++;
        if (le != p) { /* (not an empty line) */
            if (rp_saw_end) rp_bad(); /* nothing after `end` */
            rp_line(p, le);
            long kind = rp_kind_of(rf_kind);
            if (kind < 0) rp_bad();
            if (!records) { /* the header comes first */
                if (kind || !rf_program) rp_bad();
                rp_program = rf_program;
                rp_fprint = rf_fprint;
                rp_seed = rf_seed;
                rp_clock = rf_clock;
                rp_args = rf_args;
            } else if (!kind) { /* a second header */
                rp_bad();
            } else if (kind == EVK_END) {
                rp_saw_end = 1;
            } else { /* an event */
                rf_line = kind == EVK_SQL ? str_new(p, le - p) : 0;
                Event *e = alloc(EV_SIZE);
                e->kind = kind;
                e->name = rf_name;
                e->result = rp_result_of(rf_result);
                e->value.s = rf_value;
                if (rf_isint) e->value.i = rf_ivalue;
                e->port = rf_port;
                e->request = rf_request;
                e->tls = rf_scheme && str_eq_cstr(rf_scheme, "https");
                if (kind == EVK_SQL) { /* spec 026: the statement, path or operation, and the whole line */
                    e->name = rf_sql ? rf_sql : rf_name ? rf_name : rf_op;
                    e->request = rf_line;
                }
                if (kind == EVK_RUN) { /* spec 017: the program, and the arguments joined by NUL bytes */
                    e->name = rf_program;
                    e->request = rf_args_line ? join_args(rf_args) : 0;
                }
                rp_check_event(e); /* the fields its kind needs (T0008 otherwise) */
                vec_push(rp_events, e);
            }
            records++;
        }
        p = le + 1;
    }
    if (!records || !rp_saw_end) rp_bad();
}

/* An event must have what replaying it needs: a path or name for files and env, a result where
 * there is one, a str value for ok/err results and read_all, an integer for now_ms. */
void rp_check_event(Event *e)
{
    if (e->kind == EVK_SETUP || e->kind == EVK_SETUP_END) return; /* spec 028: markers */
    if (e->kind == EVK_SQL) { /* spec 026: the outcome is checked when it is replayed */
        if (!rf_op || e->result < 0) rp_bad();
        return;
    }
    if (e->kind == EVK_NOW_MS) { /* now_ms: an integer */
        if (!rf_isint) rp_bad();
        return;
    }
    if (e->kind == EVK_SERVE_LISTEN) { /* spec 011: address; ok with the listener, or err with the reason */
        if (!e->name || e->result < 0 || e->result == EVR_DENIED || (e->result == EVR_OK) != (rf_isint != 0)) rp_bad();
        if (e->result == EVR_ERR && !e->value.p) rp_bad();
        return;
    }
    if (e->kind == EVK_SERVE_REQUEST || e->kind == EVK_SERVE_RESPONSE) { /* the bytes ("" for a stop) */
        if (rf_isint || !e->value.p) rp_bad();
        return;
    }
    if (rf_isint) rp_bad(); /* everything else: no integer value */
    if (e->kind >= EVK_INFO) { /* spec 017: path or program, a result, `to` and `args`, and the value */
        if (!e->name || e->result < 0) rp_bad();
        if (e->result == EVR_DENIED) return;
        if ((e->kind == EVK_RENAME || e->kind == EVK_RUN) && !e->request) rp_bad();
        if (e->result == EVR_OK && e->kind >= EVK_APPEND_FILE && e->kind <= EVK_RENAME) return; /* ok() */
        if (!e->value.p) rp_bad();
        return;
    }
    if (e->kind == EVK_HTTP) { /* host, port, ok/err and the response or the error text */
        if (!e->name || e->port < 0 || e->result < 0 || e->result == EVR_DENIED || !e->value.p) rp_bad();
        return;
    }
    if (e->kind == EVK_READ_ALL) {
        if (!e->value.p) rp_bad();
        return;
    }
    if (e->result < 0) rp_bad(); /* a result */
    if (e->kind != EVK_READ_LINE && !e->name) rp_bad(); /* files and env: the path or name */
    if (e->result == EVR_DENIED) { /* denied: only for files */
        if (e->kind != EVK_READ_FILE && e->kind != EVK_WRITE_FILE) rp_bad();
        return;
    }
    if (e->kind == EVK_WRITE_FILE && e->result == EVR_OK) return; /* ok() of write_file has no value */
    if (!e->value.p) rp_bad();
}

/* ---- the command ---- */

/* Spec 013 (C5): the recorded program must be a relative path ending in `.vo`, without `..` or empty
 * components, naming a regular file. */
static int rp_program_ok(Str *program)
{
    if (!program || program->len < 4 || program->data[0] == '/') return 0;
    char *p = program->data;
    long n = program->len;
    if (!mem_eq(p + n - 3, ".vo", 3)) return 0;
    for (long i = 0; i < n;) {
        long j = i;
        while (j < n && p[j] != '/') j++;
        if (j == i || (j - i == 2 && p[i] == '.' && p[i + 1] == '.')) return 0;
        for (long k = i; k < j; k++)
            if (!p[k]) return 0;
        i = j + 1;
    }
    char *text;
    return read_whole_file(p, &text) >= 0; /* opened without blocking; regular files only */
}

/* `vo replay RECORDING` (contracts/cli.md, contracts/recording.md). */
int cmd_replay(void)
{
    if (pos_n != 1) usage_error(str_from_cstr("`replay` takes one recording: `vo replay r.jsonl`."));
    char *path = ((char **)pos_args)[0];
    char *text;
    long len = read_whole_file(path, &text);
    if (len < 0) { /* unreadable: T0002, exit 66 */
        Str *ps = str_from_cstr(path);
        Str *m = msg("Cannot read the recording `$1`.", ps, 0, 0);
        toolchain_error((int)opt_json, (int)report_fd, "T0002", m, ps);
        return EXIT_NOINPUT;
    }
    if (gc_limit && len > gc_limit) rp_bad(); /* spec 013 (L6): a recording larger than --max-memory */
    rp_read(text, len);
    if (!rp_program_ok(rp_program)) rp_bad(); /* spec 013 (C5) */
    rt_set_option(1, rp_seed); /* the recorded run's seed, clock and arguments */
    if (rp_clock >= 0) rt_set_option(2, rp_clock);
    long n = rp_args ? rp_args->n : 0;
    char **argv = alloc(n * 8 + 8);
    prog_argv = (long)argv;
    prog_argc = n;
    for (long i = 0; i < n; i++) argv[i] = ((Str *)vec_get(rp_args, i))->data;
    long file = (long)load_source(rp_program); /* the program: as `vo run` would check it */
    check_program((int)file, 1);
    if (diag_errors) return finish_check(1);
    rp_note_changed();
    Node *main_fn = find_main(file_get((int)file)->ast->a.v);
    outputs_start(0);
    compile_program();
    rt_effects_start(0);
    rcd_mode = 2; /* from here every input comes from the recording */
    int status = run_main(main_fn);
    gc_report(2, "replay", 0);
    return status;
}

/* When the program's fingerprint differs from the recorded one: a note on stderr (contracts/reports.md). */
void rp_note_changed(void)
{
    unsigned long h = (unsigned long)rp_fingerprint();
    SB *sb = sb_new(); /* its 16 hex digits */
    for (int shift = 60; shift >= 0; shift -= 4) sb_byte(sb, hexd[(h >> shift) & 15]);
    Str *now = sb_finish(sb);
    if (rp_fprint && str_eq(now, rp_fprint)) return;
    SB *out = sb_new();
    if (opt_json) {
        sb_cstr(out, "{\"v\":1,\"kind\":\"replay\",\"program\":");
        json_str_into(out, rp_program);
        sb_cstr(out, ",\"changed\":true}\n");
    } else {
        sb_str(out, msg("note: the program differs from the recorded one (recorded $1)\n", rp_program, 0, 0));
    }
    out_bytes(2, out->ptr, out->len);
}

/* ---- spec 011: serving ---- */

static Str *sv_request; /* the request being answered: its line for replay notes */

/* result[int]: listens on address, which must be in the running grant (R0010 otherwise). In replay,
 * the recorded result, and no socket. */
Variant *io_serve_listen(Str *address)
{
    Variant *r;
    GC_ROOT(&address);
    rt_check_name(EF_SERVE, address);
    if (rcd_mode == 2) {
        Event *e = rp_take(EVK_SERVE_LISTEN, address);
        r = e->result == EVR_OK ? make_result(0, e->value.i) : make_result(1, e->value.i);
        if (e->result == EVR_OK) serve_deadline(0);
    } else {
        r = rt_serve_listen(address);
        if (rcd_mode) {
            long ok = r->tag == 0;
            rcd_event(EVK_SERVE_LISTEN, address, ok ? EVR_OK : EVR_ERR, ok ? 0 : r->pay.s, ok ? r->pay.i : 0);
        }
    }
    GC_UNROOT(1);
    return r;
}

/* result[str]: the next request, or ok("") when the server stops. In replay, the recorded requests;
 * the end of the recording stops the server. */
Variant *io_serve_next(long listener)
{
    Variant *r;
    if (rcd_mode == 2) {
        if (rp_next >= rp_events->n) {
            r = make_result(0, (long)str_from_cstr(""));
        } else {
            r = make_result(0, (long)rp_take(EVK_SERVE_REQUEST, 0)->value.s);
        }
        if (!r->pay.s->len) serve_deadline(1);
    } else {
        r = rt_serve_next(listener);
        if (rcd_mode && r->tag == 0) rcd_event(EVK_SERVE_REQUEST, 0, -1, r->pay.s, 0);
    }
    sv_request = r->tag == 0 ? r->pay.s : 0;
    return r;
}

/* The status code of a response text (0 when it has none) and where its body starts. */
static long sv_status(Str *text, long *body)
{
    long status = 0, i = 0;
    while (i < text->len && text->data[i] != ' ') i++;
    for (i++; i < text->len && text->data[i] >= '0' && text->data[i] <= '9'; i++) status = status * 10 + (text->data[i] - '0');
    *body = text->len;
    for (long j = 3; j < text->len; j++)
        if (mem_eq(text->data + j - 3, "\r\n\r\n", 4)) {
            *body = j + 1;
            break;
        }
    return status;
}

/* In replay: a note on stderr when the response differs from the recorded one (never fatal). */
static void sv_note_changed(Str *recorded, Str *now)
{
    if (str_eq(recorded, now)) return;
    long rb, nb;
    long rs = sv_status(recorded, &rb), ns = sv_status(now, &nb);
    Str *line = sv_request ? request_line(sv_request) : str_from_cstr("");
    SB *out = sb_new();
    if (opt_json) {
        sb_cstr(out, "{\"v\":1,\"kind\":\"response_changed\",\"request\":");
        json_str_into(out, line);
        sb_cstr(out, ",\"recorded_status\":");
        int_to_dec(out, rs);
        sb_cstr(out, ",\"status\":");
        int_to_dec(out, ns);
        sb_cstr(out, ",\"recorded_body\":");
        json_str_into(out, str_new(recorded->data + rb, recorded->len - rb));
        sb_cstr(out, ",\"body\":");
        json_str_into(out, str_new(now->data + nb, now->len - nb));
        sb_cstr(out, "}\n");
    } else {
        sb_cstr(out, "note: the response to `");
        sb_str(out, line);
        sb_cstr(out, "` changed: recorded ");
        int_to_dec(out, rs);
        sb_cstr(out, " (");
        int_to_dec(out, recorded->len - rb);
        sb_cstr(out, " bytes), now ");
        int_to_dec(out, ns);
        sb_cstr(out, " (");
        int_to_dec(out, now->len - nb);
        sb_cstr(out, " bytes)\n");
    }
    out_bytes(2, out->ptr, out->len);
}

/* Sends the response; in replay, compares it with the recorded one instead. */
void io_serve_reply(Str *text)
{
    GC_ROOT(&text);
    if (rcd_mode == 2) {
        sv_note_changed(rp_take(EVK_SERVE_RESPONSE, 0)->value.s, text);
    } else {
        rt_serve_reply(text);
        if (rcd_mode) rcd_event(EVK_SERVE_RESPONSE, 0, -1, text, 0);
    }
    GC_UNROOT(1);
}

/* ---- spec 028: recordings of servers with workers ---- */

/* In replay: past any `setup` blocks (the other workers' setups, R9); 1 when an event follows. */
int rp_skip_setups(void)
{
    while (rp_next < rp_events->n && ((Event *)vec_get(rp_events, rp_next))->kind == EVK_SETUP) {
        while (rp_next < rp_events->n && ((Event *)vec_get(rp_events, rp_next))->kind != EVK_SETUP_END) rp_next++;
        rp_next++;
    }
    return rp_next < rp_events->n;
}

/* ---- spec 016: recordings of tasks (research R10) ---- */

/* In a task: its events go to the parent, which writes them in task order. */
void rcd_task_child(void)
{
    if (rcd_mode == 1 && rcd_fb) ((FBuf *)rcd_fb)->fd = -5; /* TM_RECORD (task.c) */
}

/* In a task, at its end: a read or write that R0010 stopped is `denied`, as rcd_finish writes it. */
void rcd_task_end(void)
{
    if (rcd_mode != 1 || !rcd_fb) return;
    if (rcd_pending_kind) rcd_event_raw(rcd_pending_kind, rcd_pending_path, EVR_DENIED, 0, 0);
    rcd_pending_kind = 0;
    fbuf_flush(rcd_fb);
}

/* Before a task starts: nothing buffered is inherited. */
void rcd_flush(void)
{
    if (rcd_mode == 1 && rcd_fb) fbuf_flush(rcd_fb);
}

/* In the parent: a task's events. */
void rcd_take(char *p, long n)
{
    if (rcd_mode == 1 && rcd_fb) fbuf_write(rcd_fb, p, n);
}

/* Replay: where the next event is (tasks replay one at a time, each from where the last ended). */
long rp_position(void)
{
    return rp_next;
}

void rp_set_position(long next)
{
    if (rcd_mode == 2) rp_next = next;
}
