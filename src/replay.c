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
static Str *rcd_sent;    /* spec 054: ... and, in a test's recording, its headers and body */
static long rcd_pending_kind; /* a read/write being checked: written as `denied` if R0010 ends the run */
static Str *rcd_to;           /* spec 017: rename's new path, written as "to" */
static List *rcd_args;        /* spec 017: a run's argv, written as "args" (without the program) */
static Str *rcd_pending_path;
static Vec *rp_events;        /* replay: Event records */
static long rp_next;          /* replay: index of the next event */
static Str *rp_program;       /* replay: header fields */
static Str *rp_fprint;
static long rcd_written;       /* spec 054: events written since the recording started */
static long rp_seed;
static long rp_clock = -1;
static Vec *rp_args; /* strs */
static long rp_saw_end;

static const char *const ev_names[] = {0,         "read_line", "read_all",    "read_file", "write_file",
                                       "env",     "now_ms",    "end",         "http",      "listen",
                                       "request", "response",  "info",        "list_dir",  "append_file",
                                       "make_dir", "delete",   "rename",      "run",       "sql",
                                       "setup",   "setup_end", "worker_ended", /* spec 028 */
                                       "entropy", /* spec 035 */
                                       "tcp_open", "tcp_tls", "tcp_recv", "tcp_send", "tcp_listen", "tcp_accept", /* spec 039 */
                                       "file_block", "proc_line", "proc_end", /* spec 040 */
                                       "worker", /* spec 047 */
                                       "local_tz"}; /* spec 048 */
static const char *const ev_results[] = {"ok", "err", "denied"};
static Str *run_desc(Str *program, Str *joined, Str *dir);
static Str *rt_id;     /* spec 054: the replayed test's FILE::NAME (0 outside tests) */
static long rt_missing;
static Str *rp_has(Event *e);
static Str *rt_record_cmd(void);
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
                                      "finishes a worker's setup",
                                      "answers a request whose worker ended",
                                      "draws random bytes (another count than recorded, or none recorded)",
                                      "connects to $1",
                                      "starts TLS on the connection to $1",
                                      "waits for bytes from $1",
                                      "writes to $1",
                                      "listens on `$1`",
                                      "accepts a connection on `$1`",
                                      "reads a piece of the file `$1` (another piece than recorded, or none recorded)",
                                      "waits for a line from `$1`",
                                      "ends `$1`",
                                      "talks to a worker (`$1`)"};
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
                                      "the end of a worker's setup",
                                      "a request whose worker ended",
                                      "a draw of random bytes",
                                      "a connection to $1",
                                      "the start of TLS on the connection to $1",
                                      "bytes received from $1",
                                      "a failed write to $1",
                                      "a listener on `$1`",
                                      "a connection accepted on `$1`",
                                      "a piece of the file `$1`",
                                      "a line from `$1`",
                                      "the end of `$1`",
                                      "a worker's answer to `$1`"};

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
    rcd_written++; /* spec 054: a test that wrote none keeps no recording */
    long bytes = value && !rw_valid_utf8(value->data, value->len);
    rw(J_HEAD);
    rw((char *)ev_names[kind]);
    if (name) {
        rw(kind == EVK_ENV                ? "\",\"name\":"
           : kind == EVK_HTTP           ? "\",\"host\":"
           : kind == EVK_SERVE_LISTEN   ? "\",\"address\":"
           : kind == EVK_WORKER         ? "\",\"name\":" /* spec 047 */
           : kind == EVK_RUN            ? "\",\"program\":"
           : kind >= EVK_PROC_LINE      ? "\",\"program\":"
           : kind == EVK_FILE_BLOCK     ? "\",\"path\":"
           : kind >= EVK_TCP_OPEN       ? "\",\"name\":"
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
        if (kind == EVK_RUN && rcd_to) { /* spec 057: the working directory, unless "./" */
            rw(",\"dir\":");
            rw_str(rcd_to->data, rcd_to->len, 0);
        }
        if (kind == EVK_FILE_BLOCK) { /* spec 040: where the block starts */
            rw(",\"offset\":");
            rw_dec(int_value);
        }
        if (kind == EVK_HTTP) { /* spec 007: the port (int_value); spec 009: the request line */
            rw(",\"port\":");
            rw_dec(int_value);
            if (rcd_tls) rw(",\"scheme\":\"https\""); /* spec 019 (absent: http) */
            if (rcd_request) {
                rw(",\"request\":");
                rw_str(rcd_request->data, rcd_request->len, 0);
            }
            if (rcd_sent) { /* spec 054: a test's request headers and body, for review and replay */
                rw(",\"sent\":");
                rw_str(rcd_sent->data, rcd_sent->len, !rw_valid_utf8(rcd_sent->data, rcd_sent->len));
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

/* Spec 048: local_zone. What the machine said is recorded; replay returns it and reads no setting. */
Str *io_local_tz(void)
{
    if (rcd_mode == 2) return rp_take(EVK_LOCAL_TZ, 0)->value.p;
    Str *b = rt_local_tz();
    if (rcd_mode) {
        GC_ROOT(&b);
        rcd_event(EVK_LOCAL_TZ, 0, -1, b, 0);
        GC_UNROOT(1);
    }
    return b;
}

/* Spec 035: random_bytes. The bytes drawn are recorded; replay returns them (R0014 for another count). */
Str *io_entropy(long n, Node *call)
{
    if (rcd_mode == 2) {
        Str *b = rp_take(EVK_ENTROPY, 0)->value.p;
        if (b->len != n) rp_mismatch_now(EVK_ENTROPY, 0);
        return b;
    }
    Str *b = rt_entropy(n, call);
    if (rcd_mode) {
        GC_ROOT(&b);
        rcd_event(EVK_ENTROPY, 0, -1, b, 0);
        GC_UNROOT(1);
    }
    return b;
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
        if (rt_testing() && rt_redacted(name, 0) && !((Variant *)r)->tag) /* spec 054: the value is not kept */
            rcd_event(EVK_ENV, name, EVR_OK, str_from_cstr("[redacted]"), 0);
        else
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

/* ---- spec 054: redaction in test recordings (research R4) ---- */

static Vec *rt_redact;   /* the names to redact: --redact when recording, the header's list when replaying */
static long rt_redacted_n(char *p, long len, int header);

/* Whether name is redacted: a header (case-insensitive; also the fixed ones) or a query parameter or
 * variable (exact). */
long rt_redacted(Str *name, int header)
{
    return rt_redacted_n(name->data, name->len, header);
}

/* rt_redacted on bytes (nothing allocated). */
static long rt_redacted_n(char *p, long len, int header)
{
    static const char *const fixed[] = {"authorization", "proxy-authorization", "cookie", "set-cookie", 0};
    if (header)
        for (int i = 0; fixed[i]; i++) {
            long n = cstr_len((char *)fixed[i]);
            if (len != n) continue;
            long k = 0;
            while (k < n && (p[k] | 32) == fixed[i][k]) k++;
            if (k == n) return 1;
        }
    for (long i = 0; rt_redact && i < rt_redact->n; i++) {
        Str *r = vec_get(rt_redact, i);
        if (r->len != len) continue;
        long k = 0;
        while (k < r->len && (header ? (r->data[k] | 32) == (p[k] | 32) : r->data[k] == p[k])) k++;
        if (k == r->len) return 1;
    }
    return 0;
}

/* What follows the request line: the headers and the body. */
static Str *request_rest(Str *request)
{
    long i = 0;
    while (i < request->len && request->data[i] != '\n') i++;
    return i < request->len ? str_new(request->data + i + 1, request->len - i - 1) : str_new("", 0);
}

/* The body after the blank line of a head (all of text when there is none). */
static Str *body_after_head(Str *text)
{
    for (long i = 0; i + 3 < text->len; i++)
        if (mem_eq(text->data + i, "\r\n\r\n", 4)) return str_new(text->data + i + 4, text->len - i - 4);
    return text;
}

/* text with the value of every redacted header replaced by [redacted]; answer: text is a response (its
 * status line is kept as it is). */
static Str *rt_redact_head(Str *text, int answer)
{
    SB *sb = sb_new();
    GC_ROOT(&text);
    GC_ROOT(&sb);
    long i = 0;
    if (answer) { /* the status line */
        while (i < text->len && text->data[i] != '\n') i++;
        if (i < text->len) i++;
        sb_bytes(sb, text->data, i);
    }
    while (i < text->len) {
        long e = i;
        while (e < text->len && text->data[e] != '\n') e++;
        long n = e - i - (e > i && text->data[e - 1] == '\r');
        if (n == 0) { /* the blank line: the body follows as it is */
            sb_bytes(sb, text->data + i, text->len - i);
            break;
        }
        long colon = i;
        while (colon < i + n && text->data[colon] != ':') colon++;
        if (colon < i + n && rt_redacted_n(text->data + i, colon - i, 1)) {
            sb_bytes(sb, text->data + i, colon - i);
            sb_cstr(sb, ": [redacted]");
            sb_bytes(sb, text->data + i + n, e - i - n); /* the \r */
        } else {
            sb_bytes(sb, text->data + i, e - i);
        }
        if (e < text->len) sb_byte(sb, '\n');
        i = e + 1;
    }
    Str *out = sb_finish(sb);
    GC_UNROOT(2);
    return out;
}

/* The request line with the value of every redacted query parameter replaced by [redacted]. */
static Str *rt_redact_line(Str *line)
{
    long q = 0;
    while (q < line->len && line->data[q] != '?') q++;
    if (q == line->len || !rt_redact) return line;
    SB *sb = sb_new();
    GC_ROOT(&line);
    GC_ROOT(&sb);
    sb_bytes(sb, line->data, q + 1);
    long i = q + 1;
    while (i < line->len) {
        long e = i;
        while (e < line->len && line->data[e] != '&' && line->data[e] != ' ') e++;
        long eq = i;
        while (eq < e && line->data[eq] != '=') eq++;
        if (eq < e && rt_redacted_n(line->data + i, eq - i, 0)) {
            sb_bytes(sb, line->data + i, eq - i + 1);
            sb_cstr(sb, "[redacted]");
        } else {
            sb_bytes(sb, line->data + i, e - i);
        }
        if (e < line->len) sb_byte(sb, line->data[e]);
        i = e + 1;
    }
    Str *out = sb_finish(sb);
    GC_UNROOT(2);
    return out;
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
    rt_check_net(host, port); /* spec 039: the host, or the host with this port */
    Str *line = request_line(request);
    GC_ROOT(&line);
    Str *sent = 0; /* spec 054: in a test, the headers and body after the line, redacted */
    GC_ROOT(&sent);
    if (rt_testing()) {
        line = rt_redact_line(line);
        sent = rt_redact_head(request_rest(request), 0);
    }
    long own_ms = net_timeout_next; /* std.http's send: this request's own timeout (a replay has no waits) */
    net_timeout_next = 0;
    net_capped = 0;
    if (rcd_mode == 2) {
        Str *asked = http_desc(host, port, line, tls);
        if (rp_next >= rp_events->n) rp_mismatch(EVK_HTTP, asked, 0);
        Event *e = vec_get(rp_events, rp_next++);
        if (e->kind != EVK_HTTP || !str_eq(e->name, host) || e->port != port || e->tls != tls ||
            (e->request && !str_eq(e->request, line))) /* older recordings: host and port only */
            rp_mismatch(EVK_HTTP, asked, e);
        if (sent && e->sent && !str_eq(body_after_head(e->sent), body_after_head(sent)))
            rp_mismatch_sent(asked, 1); /* spec 054: the same request with another body */
        r = rp_result(e);
    } else {
        net_cap_ms = own_ms > 0 ? sys_clock_ms() + own_ms : 0;
        r = tls ? tls_exchange(host, port, request) : rt_http_exchange(host, port, request);
        net_cap_ms = 0;
        if (rcd_mode) {
            rcd_request = line;
            rcd_tls = tls;
            rcd_sent = sent;
            Str *answer = sent && !r->tag ? rt_redact_head(r->pay.s, 1) : r->pay.s; /* spec 054: Set-Cookie, named */
            rcd_event(EVK_HTTP, host, (long)(uint32_t)r->tag, answer, port);
            rcd_request = 0;
            rcd_tls = 0;
            rcd_sent = 0;
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
    GC_UNROOT(4);
    return r;
}

long io_now_ms(void)
{
    if (!rcd_mode) return rt_now_ms();
    if (rcd_mode == 2) return clock_stamp = rp_take(EVK_NOW_MS, 0)->value.i; /* spec 055: the stamp of later log records */
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

/* spec 039: the next event when it is of this kind and name (taken), else 0: for an event that is
 * recorded only when something went wrong (a failed write). */
Event *rp_peek(long kind, Str *name)
{
    if (rp_next >= rp_events->n) return 0;
    Event *e = vec_get(rp_events, rp_next);
    if (e->kind != kind || !e->name || !str_eq(e->name, name)) return 0;
    rp_next++;
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
    if (rt_id) { /* spec 054: a replayed test; only this test fails */
        Str *cmd = rt_record_cmd();
        GC_ROOT(&cmd);
        if (rt_missing)
            m = msg("The test $1, but it has no recording in `$2`: record it with `$3`.", asked,
                    str_from_cstr((char *)opt_replay_dir), cmd);
        else if (event)
            m = msg("The test $1, but its recording has $2 next: re-record it with `$3`.", asked, rp_has(event), cmd);
        else
            m = msg("The test $1, but its recording has no more inputs: re-record it with `$2`.", asked, cmd, 0);
        GC_UNROOT(2);
        rt_raise("R0014", m, (void *)cur_node);
    }
    if (event) {
        Str *has = rp_describe((char *)n_table[event->kind],
                               event->kind == EVK_HTTP ? http_desc(event->name, event->port, event->request, event->tls)
                               : event->kind == EVK_RUN ? run_desc(event->name, event->request, event->dir)
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

/* "program arg arg" (spec 057: and " in DIR" when not "./") for R0014's texts (a joined argument str: NUL bytes as spaces). */
static Str *run_desc(Str *program, Str *joined, Str *dir)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_str(sb, program);
    for (long i = 0; joined && i < joined->len; i++) {
        if (!i) sb_byte(sb, ' ');
        sb_byte(sb, joined->data[i] ? joined->data[i] : ' ');
    }
    if (dir) { /* spec 057 */
        sb_cstr(sb, " in ");
        sb_str(sb, dir);
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
    Str *asked = kind == EVK_RUN ? run_desc(name, extra, to) : name;
    GC_ROOT(&asked);
    if (rp_next >= rp_events->n) rp_mismatch(kind, asked, 0);
    Event *e = vec_get(rp_events, rp_next++);
    if (e->kind != kind || !e->name || !str_eq(e->name, name) ||
        (e->result != EVR_DENIED && extra && (!e->request || !str_eq(e->request, extra))) ||
        (kind == EVK_RUN && (!e->dir != !to || (to && !str_eq(e->dir, to))))) /* spec 057: the directory */
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
static Str *rf_sent;   /* spec 054: a test's http event: what was sent after the request line */
static Str *rf_dir;    /* spec 057: a run event's working directory */
static Vec *rf_redact; /* spec 054: the header's redacted names */

static const char *const rk_keys[] = {"kind",   "program", "fingerprint", "seed", "clock_ms", "args",    "path",
                                      "name",   "result",  "value",       "status", "host",     "port",    "request",
                                      "address", "to", "scheme", "op", "sql", "offset", "sent", "test", "redact", "dir", 0};
static long rf_args_line; /* spec 017: this line had "args" (a run event's arguments) */

/* Where each string-valued key is stored (rk_keys order; 0: not a string). */
static Str **const rf_slots[] = {&rf_kind, &rf_program, &rf_fprint, 0, 0, 0, &rf_name, &rf_name, &rf_result,
                                 &rf_value, 0, &rf_name, 0, &rf_request, &rf_name, &rf_request, &rf_scheme,
                                 &rf_op, &rf_sql, 0, &rf_sent, 0, 0, &rf_dir};

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
    rf_sent = 0;
    rf_dir = 0;
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
        } else if ((*p == '[' && field != 5 && field != 22) || *p == '{') {
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
            if (field == 22) rf_redact = v; /* spec 054 */
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
            } else if (field == 12 || field == 19) { /* spec 040: a file block's offset */
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
    for (long k = 1; k <= EVK_LAST; k++)
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
                e->sent = rf_sent; /* spec 054 */
                if (kind == EVK_SQL) { /* spec 026: the statement, path or operation, and the whole line */
                    e->name = rf_sql ? rf_sql : rf_name ? rf_name : rf_op;
                    e->request = rf_line;
                }
                if (kind == EVK_PROC_LINE || kind == EVK_PROC_END) e->name = rf_program; /* spec 040 */
                if (kind == EVK_RUN) { /* spec 017: the program, and the arguments joined by NUL bytes */
                    e->name = rf_program;
                    e->request = rf_args_line ? join_args(rf_args) : 0;
                    e->dir = rf_dir; /* spec 057 */
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
    if (e->kind == EVK_SETUP || e->kind == EVK_SETUP_END || e->kind == EVK_WORKER_ENDED) return; /* spec 028: markers */
    if (e->kind == EVK_ENTROPY || e->kind == EVK_LOCAL_TZ) { /* spec 035, 048: the bytes */
        if (rf_isint || !e->value.p) rp_bad();
        return;
    }
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
    if (e->kind == EVK_WORKER) { /* spec 047: the operation, and the frames it passed */
        if (!e->name || !e->value.p) rp_bad();
        return;
    }
    if (e->kind >= EVK_INFO) { /* spec 017: path or program, a result, `to` and `args`, and the value */
        if (!e->name || e->result < 0) rp_bad();
        if (e->result == EVR_DENIED) { /* only what records it: files and a run (fs.c, process.c) */
            if (e->kind > EVK_RUN && e->kind != EVK_FILE_BLOCK) rp_bad();
            return;
        }
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
    if (app_entry) rp_program = app_entry; /* spec 034: a built executable replays its own program */
    if (!app_entry && !rp_program_ok(rp_program)) rp_bad(); /* spec 013 (C5) */
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

/* In replay, after a request: when its worker ended without answering (killed from outside), its block is
 * passed over up to its response (the handler is not run: what it would have asked for was never
 * recorded); 1 then. */
int rp_block_ended(void)
{
    for (long i = rp_next; i < rp_events->n; i++) {
        long kind = ((Event *)vec_get(rp_events, i))->kind;
        if (kind == EVK_SERVE_RESPONSE) return 0;
        if (kind == EVK_WORKER_ENDED) {
            while (i < rp_events->n && ((Event *)vec_get(rp_events, i))->kind != EVK_SERVE_RESPONSE) i++;
            rp_next = i + 1;
            return 1;
        }
    }
    return 0;
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

/* ---- spec 054: one recording per test (research R1, R5; contracts/cli.md) ----
 * `vo test --record DIR` records each test into DIR/<file>/<name>.jsonl, `vo test --replay DIR` feeds each
 * test its own file. The runner calls rt_begin after the test's heap is fresh and rt_end after its body;
 * everything here lives on that heap or in globals (which the collector scans). */
Str *rt_mode;          /* "recorded", "replayed" or 0 (live): the current test's word for its report */
Vec *rt_notes;         /* Str: the current test's notes (unused recorded inputs, a changed program) */
static Str *rt_path;   /* the current test's recording file */
static Str *rt_tmp;    /* ... and the file being written (renamed to rt_path when the test ends) */
static FBuf *rt_fb;    /* one buffer for every test's recording */

/* The path of a test's recording: DIR/<file relative to the entry's directory>/<name>.jsonl, the name with
 * letters, digits, - and . kept, a space as _, every other byte as %XX. */
static Str *rt_file_of(Node *test)
{
    static const char hx[] = "0123456789ABCDEF";
    char *dir = (char *)(opt_record ? opt_record : opt_replay_dir);
    char *entry = ((char **)pos_args)[0];
    long cut = 0; /* the entry's directory, left out of the file's path */
    for (long i = 0; entry[i]; i++)
        if (entry[i] == '/') cut = i + 1;
    Str *file = file_get((int)test->file)->path;
    if (cut > file->len || !mem_eq(file->data, entry, cut)) cut = 0;
    SB *sb = sb_new();
    sb_cstr(sb, dir);
    if (sb->len && sb->ptr[sb->len - 1] != '/') sb_byte(sb, '/');
    long flen = file->len - cut; /* the file's path without `.vo`: no folder looks like a source file */
    if (flen > 3 && mem_eq(file->data + file->len - 3, ".vo", 3)) flen -= 3;
    sb_bytes(sb, file->data + cut, flen);
    sb_byte(sb, '/');
    Str *name = test->a.s;
    for (long i = 0; i < name->len; i++) {
        unsigned char c = (unsigned char)name->data[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.') {
            sb_byte(sb, (char)c);
        } else if (c == ' ') {
            sb_byte(sb, '_');
        } else {
            sb_byte(sb, '%');
            sb_byte(sb, hx[c >> 4]);
            sb_byte(sb, hx[c & 15]);
        }
    }
    sb_cstr(sb, ".jsonl");
    return sb_finish(sb);
}

static Str *rt_id_of(Node *test)
{
    SB *sb = sb_new();
    sb_str(sb, file_get((int)test->file)->path);
    sb_cstr(sb, "::");
    sb_str(sb, test->a.s);
    return sb_finish(sb);
}

/* Before a test's body: start its recording, or load it. */
void rt_begin(Node *test)
{
    rt_mode = 0;
    rt_notes = 0;
    if (!opt_record && !opt_replay_dir) return;
    rt_path = rt_file_of(test);
    rt_id = rt_id_of(test);
    rt_notes = vec_new();
    if (opt_record) {
        /* written beside DIR's files and moved into the test's folder (made then) only when the test
           received something, so a test that did not leaves no folder behind */
        mkdir_p(str_from_cstr((char *)opt_record));
        SB *tb = sb_new();
        sb_cstr(tb, (char *)opt_record);
        sb_cstr(tb, "/.recording.tmp");
        rt_tmp = sb_finish(tb);
        if (!rt_fb) {
            rt_fb = fbuf_open(rt_tmp->data);
        } else {
            rt_fb->fd = sys_open(rt_tmp->data, O_WRCREAT);
            rt_fb->len = 0;
            rt_fb->err = rt_fb->fd < 0;
        }
        rcd_fb = rt_fb && rt_fb->fd >= 0 ? rt_fb : 0;
        if (!rcd_fb) rcd_failed = 1;
        rcd_mode = 1;
        rw("{\"v\":1,\"kind\":\"recording\",\"program\":");
        char *program = ((char **)pos_args)[0];
        rw_str(program, cstr_len(program), 0);
        rw(",\"fingerprint\":\"");
        rw_hex64(rp_fingerprint());
        rw("\",\"seed\":");
        rw_dec(opt_seed_set ? opt_seed : 1);
        rw(",\"clock_ms\":");
        rw_dec(opt_clock_set ? opt_clock : 0);
        rw(",\"test\":");
        rw_str(rt_id->data, rt_id->len, 0);
        rw(",\"redact\":[");
        for (long i = 0; opt_redact && i < opt_redact->n; i++) {
            Str *n = vec_get(opt_redact, i);
            if (i) rw(",");
            rw_str(n->data, n->len, 0);
        }
        rw("]}\n");
        rcd_written = 0;
        rt_redact = opt_redact;
        rt_mode = str_from_cstr("recorded");
        return;
    }
    char *text; /* --replay */
    long len = read_whole_file(rt_path->data, &text);
    rt_missing = len < 0;
    rp_saw_end = 0;
    rp_next = 0;
    if (rt_missing) {
        rp_events = vec_new();
        rp_fprint = 0;
    } else {
        rf_redact = 0;
        GC_ROOT(&text); /* collected memory once tests run (vo replay reads it before the collector starts) */
        rp_read(text, len);
        GC_UNROOT(1);
    }
    rt_redact = rt_missing ? 0 : rf_redact;
    rcd_mode = 2;
    rt_mode = str_from_cstr("replayed");
}

/* What the recording has in event e, for messages: "an HTTP request GET /x to h:80", "the file `a`", ... */
static Str *rp_has(Event *e)
{
    return rp_describe((char *)n_table[e->kind], e->kind == EVK_HTTP  ? http_desc(e->name, e->port, e->request, e->tls)
                                                 : e->kind == EVK_RUN ? run_desc(e->name, e->request, e->dir)
                                                                      : e->name);
}

/* The command that records the current test again. */
static Str *rt_record_cmd(void)
{
    SB *sb = sb_new();
    sb_cstr(sb, "vo test --record ");
    sb_cstr(sb, (char *)opt_replay_dir);
    sb_cstr(sb, " --test '");
    sb_str(sb, rt_id);
    sb_byte(sb, '\'');
    return sb_finish(sb);
}

/* After a test's body, however it ended: failed when it did not pass; timed_out keeps an older
 * recording (what it received is cut short). Replay adds the test's notes. */
void rt_end(long failed, long timed_out)
{
    if (rcd_mode == 1 && rt_tmp) {
        if (rcd_pending_kind) {
            rcd_event_raw(rcd_pending_kind, rcd_pending_path, EVR_DENIED, 0, 0);
            rcd_pending_kind = 0;
        }
        long wrote = rcd_written;
        if (rcd_fb && wrote && !timed_out) {
            rw(J_HEAD);
            rw("end");
            rw("\",\"status\":");
            rw_dec(failed ? 1 : 0);
            rw(J_CLOSE);
            fbuf_flush(rcd_fb);
        }
        long err = !rcd_fb || rt_fb->err;
        if (rt_fb && rt_fb->fd >= 0) sys_close((int)rt_fb->fd);
        if (rt_fb) rt_fb->fd = -1;
        rcd_mode = 0;
        rcd_fb = 0;
        if (!wrote) rt_mode = 0; /* nothing from outside: it ran as always */
        if (timed_out || err || !wrote) sys_unlink(rt_tmp->data);
        if (!timed_out && !err && !wrote) sys_unlink(rt_path->data); /* nothing received: no recording */
        if (!timed_out && !err && wrote) {
            long slash = rt_path->len;
            while (slash > 0 && rt_path->data[slash - 1] != '/') slash--;
            if (slash > 1) mkdir_p(str_new(rt_path->data, slash - 1));
            sys_rename(rt_tmp->data, rt_path->data);
        }
        if (err && wrote) out_unwritable(rt_path->data);
    } else if (rcd_mode == 2 && rt_id) {
        rcd_mode = 0;
        if (rt_missing) rt_mode = 0; /* no recording, and it asked for nothing: it ran as always */
        if (!rt_missing && rp_next < rp_events->n) { /* FR-007: inputs the test no longer asked for */
            SB *sb = sb_new();
            int_to_dec(sb, rp_events->n - rp_next);
            Str *count = sb_finish(sb);
            Str *first = rp_has(vec_get(rp_events, rp_next));
            vec_push(rt_notes, msg(rp_events->n - rp_next == 1 ? "$1 recorded input was not used: $2"
                                                                : "$1 recorded inputs were not used, the first: $2",
                                   count, first, 0));
        }
        if (!rt_missing && !opt_mutate && rp_fprint) { /* FR-008 */
            unsigned long h = (unsigned long)rp_fingerprint();
            SB *sb = sb_new();
            for (int shift = 60; shift >= 0; shift -= 4) sb_byte(sb, hexd[(h >> shift) & 15]);
            if (!str_eq(sb_finish(sb), rp_fprint)) vec_push(rt_notes, str_from_cstr("the program changed since this test was recorded"));
        }
    }
    rt_tmp = 0;
    rt_id = 0;
    rp_events = 0;
    rt_redact = 0;
}

/* Whether the running recording or replay is a test's (sends and request bodies are kept and compared). */
long rt_testing(void)
{
    return rt_id != 0;
}

/* R0014 in a replayed test: it sent other bytes to name (a connection), or an HTTP request (name: its description) with
 * another body, than recorded. */
void rp_mismatch_sent(Str *name, int http)
{
    Str *cmd = rt_record_cmd();
    GC_ROOT(&cmd);
    Str *m = http ? msg("The test makes an HTTP request $1 with another body than its recording has: re-record it with `$2`.", name, cmd, 0)
                  : msg("The test sends other bytes to `$1` than its recording has: re-record it with `$2`.", name, cmd, 0);
    GC_UNROOT(1);
    rt_raise("R0014", m, (void *)cur_node);
}

/* FR-015: the recordings under DIR, in the folders of the program's files, that name none of their tests
 * (paths relative to DIR). 0 without --record or --replay. */
Vec *rt_unused(void)
{
    if (!opt_record && !opt_replay_dir) return 0;
    char *dir = (char *)(opt_record ? opt_record : opt_replay_dir);
    long dlen = cstr_len(dir);
    Vec *out = vec_new();
    GC_ROOT(&out);
    char *buf = alloc(32768);
    GC_ROOT(&buf);
    for (long f = 0; f < file_count; f++) {
        Vec *items = file_get((int)f)->ast ? file_get((int)f)->ast->a.v : 0;
        Vec *want = vec_new(); /* the recording paths of this file's tests */
        GC_ROOT(&want);
        Str *folder = 0;
        GC_ROOT(&folder);
        for (long i = 0; items && i < items->n; i++) {
            Node *t = vec_get(items, i);
            if (t->kind != N_TEST) continue;
            Str *pth = rt_file_of(t);
            vec_push(want, pth);
            if (!folder) {
                long cut = pth->len;
                while (cut > 0 && pth->data[cut - 1] != '/') cut--;
                folder = str_new(pth->data, cut ? cut - 1 : 0);
            }
        }
        long fd = folder ? sys_open(folder->data, O_DIRECTORY) : -1;
        for (long got; fd >= 0 && (got = sys_getdents64((int)fd, buf, 32768)) > 0;) {
            for (long off = 0; off < got;) {
                char *ent = buf + off; /* linux_dirent64: name at 19, reclen at 16 */
                off += *(uint16_t *)(ent + 16);
                char *name = ent + 19;
                long n = cstr_len(name);
                if (n < 7 || !mem_eq(name + n - 6, ".jsonl", 6)) continue;
                SB *sb = sb_new();
                sb_str(sb, folder);
                sb_byte(sb, '/');
                sb_cstr(sb, name);
                Str *full = sb_finish(sb);
                long used = 0;
                for (long k = 0; k < want->n && !used; k++) used = str_eq(vec_get(want, k), full);
                if (!used) {
                    long skip = dlen + (dir[dlen - 1] == '/' ? 0 : 1);
                    GC_ROOT(&full);
                    vec_push(out, str_new(full->data + skip, full->len - skip));
                    GC_UNROOT(1);
                }
            }
        }
        if (fd >= 0) sys_close((int)fd);
        GC_UNROOT(2);
    }
    GC_UNROOT(2);
    return out;
}
