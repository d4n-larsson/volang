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
static long rcd_pending_kind; /* a read/write being checked: written as `denied` if R0010 ends the run */
static Str *rcd_pending_path;
static Vec *rp_events;        /* replay: Event records */
static long rp_next;          /* replay: index of the next event */
static Str *rp_program;       /* replay: header fields */
static Str *rp_fprint;
static long rp_seed;
static long rp_clock = -1;
static Vec *rp_args; /* strs */
static long rp_saw_end;

static const char *const ev_names[] = {0, "read_line", "read_all", "read_file", "write_file", "env", "now_ms", "end",
                                       "http"};
static const char *const ev_results[] = {"ok", "err", "denied"};
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
                                      "makes an HTTP request to `$1`"};
static const char *const n_table[] = {0,
                                      "a line of stdin",
                                      "the rest of stdin",
                                      "a read of `$1`",
                                      "a write of `$1`",
                                      "the variable `$1`",
                                      "a clock reading",
                                      "the end",
                                      "an HTTP request to `$1`"};

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
        rw(kind == EVK_ENV ? "\",\"name\":" : kind == EVK_HTTP ? "\",\"host\":" : "\",\"path\":");
        rw_str(name->data, name->len, 0);
        if (kind == EVK_HTTP) { /* spec 007: the port (int_value) */
            rw(",\"port\":");
            rw_dec(int_value);
        }
    } else {
        rw("\"");
    }
    if (result >= 0) {
        rw(",\"result\":\"");
        rw((char *)ev_results[result]);
        rw("\"");
    }
    if (kind == EVK_NOW_MS) {
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
        if (e->result == EVR_DENIED) {
            rt_check_path(EF_FREAD, path);         /* the same R0010 as the recorded run */
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
        if (e->result == EVR_DENIED) {
            rt_check_path(EF_FWRITE, path);
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

/* "host:port", for R0014's messages. */
static Str *host_port(Str *host, long port)
{
    SB *sb = sb_new();
    sb_str(sb, host);
    sb_byte(sb, ':');
    int_to_dec(sb, port);
    return sb_finish(sb);
}

/* Spec 007 (research R11): the host check (R0010) in every mode, then the exchange; replay returns
 * the recorded response without the network, and refuses another host or port (R0014). */
Variant *io_http_exchange(Str *host, long port, Str *request)
{
    Variant *r;
    GC_ROOT(&host);
    GC_ROOT(&request);
    rt_check_name(EF_NET, host);
    if (rcd_mode == 2) {
        Str *asked = host_port(host, port);
        if (rp_next >= rp_events->n) rp_mismatch(EVK_HTTP, asked, 0);
        Event *e = vec_get(rp_events, rp_next++);
        if (e->kind != EVK_HTTP || !str_eq(e->name, host) || e->port != port) rp_mismatch(EVK_HTTP, asked, e);
        r = rp_result(e);
    } else {
        r = rt_http_exchange(host, port, request);
        if (rcd_mode) rcd_event(EVK_HTTP, host, (long)(uint32_t)r->tag, r->pay.s, port);
    }
    GC_UNROOT(2);
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
                               event->kind == EVK_HTTP ? host_port(event->name, event->port) : event->name);
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

static const char *const rk_keys[] = {"kind",  "program", "fingerprint", "seed", "clock_ms", "args", "path",
                                      "name",  "result",  "value",       "status", "host",   "port", 0};

/* Where each string-valued key is stored (rk_keys order; 0: not a string). */
static Str **const rf_slots[] = {&rf_kind, &rf_program, &rf_fprint, 0, 0, 0, &rf_name, &rf_name, &rf_result,
                                 &rf_value, 0, &rf_name, 0};

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
    while (p < end && (unsigned)(*p - '0') < 10) v = v * 10 + (unsigned long)(*p++ - '0');
    if (p == digits) rp_bad();
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

/* One record into the rf_* fields: a flat object of strings, integers, true/false/null and (for
 * args) an array of strings. Anything else is T0008. */
void rp_line(char *p, char *end)
{
    rf_kind = 0;
    rf_name = 0;
    rf_result = 0;
    rf_value = 0;
    rf_isint = 0;
    rf_port = -1;
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
            if (field == 5) rf_args = v;
        } else if (*p == 't') { /* true */
            p += 4;
        } else if (*p == 'f') { /* false */
            p += 5;
        } else if (*p == 'n') { /* null (clock_ms) */
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
    for (long k = 1; k <= EVK_HTTP; k++)
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
                Event *e = alloc(EV_SIZE);
                e->kind = kind;
                e->name = rf_name;
                e->result = rp_result_of(rf_result);
                e->value.s = rf_value;
                if (rf_isint) e->value.i = rf_ivalue;
                e->port = rf_port;
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
    if (e->kind == EVK_NOW_MS) { /* now_ms: an integer */
        if (!rf_isint) rp_bad();
        return;
    }
    if (rf_isint) rp_bad(); /* everything else: no integer value */
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
    rp_read(text, len);
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
