/* Spec 055: log records (std.log). The record's JSON pieces (message, fields: redacted and cut) are built by
 * std/log.vo; here: the per-process state (level, request id, extra secret words, the records a test
 * wrote), the time stamp, the secret-like name rule (also the checker's, W0506) and the one write per record.
 * The stamp is an output, not a recorded clock read (research R2): real time under `vo run`, the test clock
 * under `vo test`, the last replayed value under `vo replay`. */
#include "vo.h"

static char log_req[512]; /* the request id being answered (raw bytes); empty: none */
static long log_req_len;
static char log_kept[CAP_MAX]; /* this test's records, each ending in a newline */
static long log_kept_len;
static char log_words[1024]; /* redact_logs words, lower case, each ending in a newline */
static long log_words_len;
long clock_stamp; /* the clock's last value under a fixed or replayed clock (rt_effects.c, replay.c) */

static char *const level_names[] = {"debug", "info", "warn", "error"};
static char *const secret_words[] = {"password", "passwd", "secret", "token", "key", "authorization", "cookie", "session", 0};

static int lower_c(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* Whether the n bytes at w (one word, any case) equal the lower-case word s of length k. */
static int word_is(char *w, long n, char *s, long k)
{
    if (n != k) return 0;
    for (long i = 0; i < n; i++)
        if (lower_c((unsigned char)w[i]) != (unsigned char)s[i]) return 0;
    return 1;
}

static int secret_word(char *w, long n, int extra)
{
    for (long i = 0; secret_words[i]; i++)
        if (word_is(w, n, secret_words[i], cstr_len(secret_words[i]))) return 1;
    if (!extra) return 0;
    for (long i = 0, start = 0; i < log_words_len; i++)
        if (log_words[i] == '\n') {
            if (word_is(w, n, log_words + start, i - start)) return 1;
            start = i + 1;
        }
    return 0;
}

/* Whether a name is secret-like (data-model.md): one of its words (split at `_`, `-`, `.` and lower-to-upper
 * case changes, compared without case) is a secret word; extra: also the redact_logs words. */
int secret_like(char *p, long n, int extra)
{
    long start = 0;
    for (long i = 0; i <= n; i++) {
        int c = i < n ? (unsigned char)p[i] : 0;
        int cut = !c || c == '_' || c == '-' || c == '.';
        int camel = i > 0 && c >= 'A' && c <= 'Z' && p[i - 1] >= 'a' && p[i - 1] <= 'z';
        if (cut || camel) {
            if (i > start && secret_word(p + start, i - start, extra)) return 1;
            start = cut ? i + 1 : i;
        }
    }
    return 0;
}

static void two(SB *sb, long v)
{
    sb_byte(sb, (int)('0' + v / 10 % 10));
    sb_byte(sb, (int)('0' + v % 10));
}

/* `"YYYY-MM-DDTHH:MM:SS.mmmZ"` for ms since 1970 (UTC; days to a civil date as in H. Hinnant's algorithm). */
static void stamp_into(SB *sb, long ms)
{
    if (ms < 0) ms = 0;
    long days = ms / 86400000, rest = ms % 86400000;
    long z = days + 719468, era = z / 146097, doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    long d = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9, y = yoe + era * 400 + (m <= 2);
    sb_byte(sb, '"');
    two(sb, y / 100);
    two(sb, y);
    sb_byte(sb, '-');
    two(sb, m);
    sb_byte(sb, '-');
    two(sb, d);
    sb_byte(sb, 'T');
    two(sb, rest / 3600000);
    sb_byte(sb, ':');
    two(sb, rest / 60000 % 60);
    sb_byte(sb, ':');
    two(sb, rest / 1000 % 60);
    sb_byte(sb, '.');
    sb_byte(sb, (int)('0' + rest / 100 % 10));
    two(sb, rest % 100);
    sb_cstr(sb, "Z\"");
}

/* Writes one record: level 0-3, message and fields as JSON text (fields: `"a":1,"b":2` or empty). Below the
 * --log-level it is skipped, except in a test, where it goes to the test's output and its kept records. */
void rt_log_write(long level, Str *message, Str *fields)
{
    if (!cap_on && level < opt_log_level) return;
    SB *sb = sb_new();
    GC_ROOT(&sb);
    sb_cstr(sb, "{\"time\":");
    stamp_into(sb, clock_fixed || rcd_mode == 2 ? clock_stamp : sys_clock_real_ms());
    sb_cstr(sb, ",\"level\":\"");
    sb_cstr(sb, level_names[level & 3]);
    sb_cstr(sb, "\",\"message\":");
    sb_str(sb, message);
    if (log_req_len) {
        sb_cstr(sb, ",\"request_id\":\"");
        json_escape_into(sb, log_req, log_req_len);
        sb_byte(sb, '"');
    }
    if (fields->len) {
        sb_byte(sb, ',');
        sb_str(sb, fields);
    }
    sb_cstr(sb, "}\n");
    Str *line = sb_finish(sb);
    GC_UNROOT(1);
    if (cap_on) {
        cap_bytes(line->data, line->len);
        if (log_kept_len + line->len <= CAP_MAX) { /* ponytail: a test keeps its first 64 KiB of records */
            mem_copy(log_kept + log_kept_len, line->data, line->len);
            log_kept_len += line->len;
        }
        return;
    }
    out_bytes(2, line->data, line->len);
    out_flush(); /* one write per record, in order with the program's other output */
}

/* Before each test: no request id, no kept records, no extra words. */
void rt_log_reset(void)
{
    log_req_len = log_kept_len = log_words_len = 0;
}

/* __log(op, a, b) (std.log only): 0-3 write a record of that level (message a, fields b); 4 the request id
 * being answered (a; empty: none); 5 the records kept in this test, one per line; 6 one more secret word;
 * 7 "1" when a is secret-like, else "". */
Str *rt_log(long op, Str *a, Str *b)
{
    switch (op) {
    case 0: case 1: case 2: case 3: rt_log_write(op, a, b); break;
    case 4:
        log_req_len = a->len < (long)sizeof log_req ? a->len : (long)sizeof log_req;
        mem_copy(log_req, a->data, log_req_len);
        break;
    case 5: return str_new(log_kept, log_kept_len);
    case 6:
        if (a->len && log_words_len + a->len + 1 <= (long)sizeof log_words) { /* ponytail: 1 KiB of extra words */
            for (long i = 0; i < a->len; i++) log_words[log_words_len++] = (char)lower_c((unsigned char)a->data[i]);
            log_words[log_words_len++] = '\n';
        }
        break;
    case 7: return str_from_cstr(secret_like(a->data, a->len, 1) ? "1" : "");
    }
    return str_from_cstr("");
}
