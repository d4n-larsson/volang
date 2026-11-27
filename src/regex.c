/* Regular expressions (spec 025): a parser, a compiler to a small instruction program and a Pike VM
 * (research R1). Every match runs in time linear in the text: all threads advance together one code
 * point at a time, a state is entered once per step, and the first thread to match cuts the threads
 * of lower priority, which gives Perl's leftmost-first submatches. The parser also serves the checker
 * (re_check: literal patterns are E0319/E0320 at check time). Patterns and texts are valid UTF-8 (vo
 * strings). Contracts: specs/025-regular-expressions/contracts/pattern.md. */
#include "vo.h"

enum { I_CHAR, I_CLASS, I_ANY, I_ANYNL, I_SPLIT, I_JMP, I_SAVE, I_ASSERT, I_LOOP, I_EMPTYEXIT, I_MATCH };
enum { A_BOT, A_EOT, A_BOL, A_EOL, A_WORD, A_NOTWORD };
enum { K_LIT, K_CLASS, K_ANY, K_ASSERT, K_GROUP, K_CAT, K_ALT, K_REP, K_EMPTY };

#define MAX_PATTERN 10000
#define MAX_COUNT 1000
#define MAX_CODE 20000
#define MAX_DEPTH 200
#define MAX_GROUPS 1000
#define F_I 1
#define F_M 2
#define F_S 4

typedef struct {
    int kind, a, b, greedy, child, next, flagsonly;
} RNode;

/* A compiled program: instructions (op, x, y), class ranges (lo, hi pairs), groups. */
typedef struct {
    int *code;    /* 3 ints per instruction */
    long ncode;
    int *islot;   /* per instruction: the hidden slot of its innermost loop, or -1 */
    int *ranges;
    long nranges;
    long ngroups; /* capture groups, group 0 included */
    long nslots;  /* captures: 2 per group, then one hidden slot per loop (its entry position) */
    char *names;  /* group 1..n names, each followed by \n ("" unnamed); perm memory, never collected */
    long names_len;
    unsigned char first[256]; /* bytes a match can start with */
    int any_first;            /* no first-byte filter */
    int anchored;             /* starts with \A-like ^ (no (?m)) */
} Prog;

/* ---- growable scratch arrays (perm memory, only ever grown: bounded by the limits) ---- */

static void *grow(void *p, long *cap, long need, long elem)
{
    if (need <= *cap) return p;
    long c = *cap ? *cap : 256;
    while (c < need) c *= 2;
    void *q = perm_alloc(c * elem);
    if (*cap) mem_copy(q, p, *cap * elem);
    *cap = c;
    return q;
}

static RNode *nodes;
static long nnodes, cap_nodes;
static int *rng; /* class ranges while parsing and compiling */
static long nrng, cap_rng;
static int *tmp; /* one class's ranges before sorting */
static long ntmp, cap_tmp;
static int *code;
static long ncode, cap_code;
static int *islot; /* per instruction: its innermost loop's slot */
static long cap_islot;
static int cur_slot = -1;
static long *gname; /* per group: start, end of its name in the pattern (or -1) */
static long cap_gname;

/* ---- parser ---- */

static char *P;
static long PN, pos, depth, ngroups, nloops;
static int flags, failed;
static ReErr *E;

/* Records the first error: no allocation here (the parser runs while the collector is on, and
 * nothing would root a message); re_err_text builds the text afterwards. */
static int fail(long s, long e, char *codestr, char *tmpl, long as, long ae)
{
    if (!failed) {
        failed = 1;
        E->start = s;
        E->end = e;
        E->code = codestr;
        E->tmpl = tmpl;
        E->as = as;
        E->ae = ae;
        E->fix = 0;
        E->msg = 0;
    }
    return -1;
}

/* The code point at i; its length in *len. */
static int decode_at(char *s, long n, long i, long *len)
{
    unsigned c = (unsigned char)s[i];
    if (c < 0x80 || i + 1 >= n) {
        *len = 1;
        return (int)c;
    }
    long k = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
    if (i + k > n) k = n - i;
    int v = (int)(c & (0x7F >> k));
    for (long j = 1; j < k; j++) v = v << 6 | ((unsigned char)s[i + j] & 0x3F);
    *len = k;
    return v;
}

static int new_node(int kind, int a, int b)
{
    nodes = grow(nodes, &cap_nodes, nnodes + 1, sizeof(RNode));
    RNode *n = &nodes[nnodes];
    n->kind = kind;
    n->a = a;
    n->b = b;
    n->greedy = 1;
    n->child = -1;
    n->next = -1;
    n->flagsonly = 0;
    return (int)nnodes++;
}

static void add_tmp(int lo, int hi)
{
    tmp = grow(tmp, &cap_tmp, ntmp + 2, sizeof(int));
    tmp[ntmp++] = lo;
    tmp[ntmp++] = hi;
}

static int is_word(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/* The shorthand \d \w \s (negated when upper case) into tmp. */
static void add_shorthand(int e)
{
    long from = ntmp;
    int low = e | 0x20;
    if (low == 'd') add_tmp('0', '9');
    if (low == 'w') {
        add_tmp('0', '9');
        add_tmp('A', 'Z');
        add_tmp('_', '_');
        add_tmp('a', 'z');
    }
    if (low == 's') {
        add_tmp('\t', '\r'); /* \t \n \v \f \r */
        add_tmp(' ', ' ');
    }
    if (e >= 'A' && e <= 'Z') { /* complement of the ranges just added (they are sorted) */
        long k = (ntmp - from) / 2;
        int pairs[8];
        for (long i = 0; i < k * 2; i++) pairs[i] = tmp[from + i];
        ntmp = from;
        int lo = 0;
        for (long i = 0; i < k; i++) {
            if (pairs[2 * i] > lo) add_tmp(lo, pairs[2 * i] - 1);
            lo = pairs[2 * i + 1] + 1;
        }
        add_tmp(lo, 0x10FFFF);
    }
}

/* Sorts and merges tmp's ranges, folds ASCII case (F_I), negates, and appends them to rng; returns
 * a K_CLASS node. */
static int finish_class(long from, int neg)
{
    if (flags & F_I) {
        long end = ntmp;
        for (long i = from; i < end; i += 2) {
            int lo = tmp[i], hi = tmp[i + 1];
            int a = lo > 'a' ? lo : 'a', b = hi < 'z' ? hi : 'z';
            if (a <= b) add_tmp(a - 32, b - 32);
            a = lo > 'A' ? lo : 'A';
            b = hi < 'Z' ? hi : 'Z';
            if (a <= b) add_tmp(a + 32, b + 32);
        }
    }
    long k = (ntmp - from) / 2;
    for (long i = 1; i < k; i++) { /* insertion sort by lo (classes are small) */
        int lo = tmp[from + 2 * i], hi = tmp[from + 2 * i + 1];
        long j = i - 1;
        while (j >= 0 && tmp[from + 2 * j] > lo) {
            tmp[from + 2 * j + 2] = tmp[from + 2 * j];
            tmp[from + 2 * j + 3] = tmp[from + 2 * j + 1];
            j--;
        }
        tmp[from + 2 * j + 2] = lo;
        tmp[from + 2 * j + 3] = hi;
    }
    long m = 0; /* merged pairs, in place */
    for (long i = 0; i < k; i++) {
        int lo = tmp[from + 2 * i], hi = tmp[from + 2 * i + 1];
        if (m && lo <= tmp[from + 2 * m - 1] + 1) {
            if (hi > tmp[from + 2 * m - 1]) tmp[from + 2 * m - 1] = hi;
        } else {
            tmp[from + 2 * m] = lo;
            tmp[from + 2 * m + 1] = hi;
            m++;
        }
    }
    long start = nrng;
    if (neg) {
        int lo = 0;
        for (long i = 0; i < m; i++) {
            if (tmp[from + 2 * i] > lo) {
                rng = grow(rng, &cap_rng, nrng + 2, sizeof(int));
                rng[nrng++] = lo;
                rng[nrng++] = tmp[from + 2 * i] - 1;
            }
            lo = tmp[from + 2 * i + 1] + 1;
        }
        if (lo <= 0x10FFFF) {
            rng = grow(rng, &cap_rng, nrng + 2, sizeof(int));
            rng[nrng++] = lo;
            rng[nrng++] = 0x10FFFF;
        }
    } else {
        rng = grow(rng, &cap_rng, nrng + 2 * m, sizeof(int));
        for (long i = 0; i < 2 * m; i++) rng[nrng++] = tmp[from + i];
    }
    ntmp = from;
    return new_node(K_CLASS, (int)start, (int)((nrng - start) / 2));
}

#define M_UNSUPPORTED_ESC "Unsupported escape `$1`: use `^`/`$`, or write the character itself."
#define M_BACKREF "Backreferences are not supported: vo patterns run in linear time. Capture both parts and compare them in code."

/* An escape at pos (the backslash) that stands for one code point, or a shorthand (*shorthand set,
 * the ranges added to tmp); -1 after an error. in_class: inside [...]. */
static int parse_escape(int *shorthand, int in_class)
{
    long s = pos;
    *shorthand = 0;
    if (pos + 1 >= PN) return fail(s, PN, "E0319", "The pattern ends with `\\`: write `\\\\` for a backslash.", -1, -1);
    long len;
    int e = decode_at(P, PN, pos + 1, &len);
    pos += 1 + len;
    switch (e) {
    case 'd': case 'D': case 'w': case 'W': case 's': case 'S':
        add_shorthand(e);
        *shorthand = 1;
        return 0;
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case 'f': return '\f';
    case 'v': return '\v';
    case 'k': return fail(s, pos, "E0320", M_BACKREF, -1, -1);
    case 'p': case 'P':
        return fail(s, pos, "E0320", "Unicode classes are not supported; vo's `\\w` and `\\d` are ASCII. List the characters in a class.", -1, -1);
    case 'b': case 'B':
        if (!in_class) break;
        return fail(s, pos, "E0320", M_UNSUPPORTED_ESC, s, pos);
    }
    if (e >= '1' && e <= '9') return fail(s, pos, "E0320", M_BACKREF, -1, -1);
    if (e < 128 && is_word(e)) return fail(s, pos, "E0320", M_UNSUPPORTED_ESC, s, pos);
    return e; /* an escaped metacharacter, punctuation or any other character: itself */
}

static int parse_alt(void);

/* A literal code point as a node (two-case class under (?i)). */
static int lit_node(int c)
{
    if ((flags & F_I) && ((c | 0x20) >= 'a' && (c | 0x20) <= 'z')) {
        long from = ntmp;
        add_tmp(c, c);
        return finish_class(from, 0);
    }
    return new_node(K_LIT, c, 0);
}

static int parse_class(void)
{
    long cs = pos++;
    int neg = 0;
    if (pos < PN && P[pos] == '^') {
        neg = 1;
        pos++;
    }
    long from = ntmp;
    int first = 1;
    for (;;) {
        if (pos >= PN) return fail(cs, cs + 1, "E0319", "The class is not closed: add `]`.", -1, -1);
        if (P[pos] == ']' && !first) {
            pos++;
            break;
        }
        first = 0;
        long is = pos, len;
        int sh = 0, lo;
        if (P[pos] == '\\') {
            lo = parse_escape(&sh, 1);
            if (failed) return -1;
        } else {
            lo = decode_at(P, PN, pos, &len);
            pos += len;
        }
        if (pos + 1 < PN && P[pos] == '-' && P[pos + 1] != ']') {
            long dash = pos++;
            int sh2 = 0, hi;
            if (P[pos] == '\\') {
                hi = parse_escape(&sh2, 1);
                if (failed) return -1;
            } else {
                hi = decode_at(P, PN, pos, &len);
                pos += len;
            }
            if (sh || sh2)
                return fail(is, pos, "E0319", "A range cannot start or end with a shorthand such as `\\d`.", -1, -1);
            if (hi < lo) return fail(is, pos, "E0319", "Range `$1` is reversed.", is, pos);
            (void)dash;
            add_tmp(lo, hi);
        } else if (!sh) {
            add_tmp(lo, lo);
        }
    }
    return finish_class(from, neg);
}

/* `{n}`, `{n,}` or `{n,m}` at pos: 1 with *mn, *mx (-1: no maximum) and pos after it, or 0 (pos kept). */
static int parse_count(long *mn, long *mx)
{
    long p = pos + 1, a = 0, b = -1, digits = 0;
    while (p < PN && P[p] >= '0' && P[p] <= '9') {
        if (a <= 100000) a = a * 10 + (P[p] - '0');
        p++;
        digits++;
    }
    if (!digits || p >= PN) return 0;
    if (P[p] == '}') {
        b = a;
    } else if (P[p] == ',') {
        p++;
        long d2 = 0, v = 0;
        while (p < PN && P[p] >= '0' && P[p] <= '9') {
            if (v <= 100000) v = v * 10 + (P[p] - '0');
            p++;
            d2++;
        }
        if (p >= PN || P[p] != '}') return 0;
        b = d2 ? v : -1;
    } else {
        return 0;
    }
    *mn = a;
    *mx = b;
    pos = p + 1;
    return 1;
}

#define M_BRACE "`{` must start a repetition such as `{2}` or `{2,5}`; write `\\{` for the character."

static int parse_group(void)
{
    long gs = pos++;
    int saved = flags, cap = -1;
    if (pos < PN && P[pos] == '?') {
        pos++;
        char c = pos < PN ? P[pos] : 0;
        if (c == ':') {
            pos++;
        } else if (c == '<' && pos + 1 < PN && (P[pos + 1] == '=' || P[pos + 1] == '!')) {
            return fail(gs, pos + 2, "E0320", "Lookbehind is not supported: vo patterns run in linear time. Capture what comes before and check it in code.", -1, -1);
        } else if (c == '<') {
            long ns = ++pos;
            while (pos < PN && is_word((unsigned char)P[pos])) pos++;
            if (pos >= PN || P[pos] != '>' || pos == ns || (P[ns] >= '0' && P[ns] <= '9'))
                return fail(gs, pos < PN ? pos + 1 : PN, "E0319",
                            "A group name is letters, digits and `_`, not starting with a digit: `(?<name>...)`.", -1, -1);
            for (long g = 1; g <= ngroups; g++) {
                long a = gname[2 * g], b = gname[2 * g + 1];
                if (a >= 0 && b - a == pos - ns && mem_eq(P + a, P + ns, pos - ns))
                    return fail(gs, pos + 1, "E0319", "Group name `$1` is used twice.", ns, pos);
            }
            pos++;
            if (ngroups + 1 >= MAX_GROUPS) return fail(gs, pos, "E0319", "More than 1000 groups.", -1, -1);
            cap = (int)++ngroups;
            gname = grow(gname, &cap_gname, 2 * ngroups + 2, sizeof(long));
            gname[2 * cap] = ns;
            gname[2 * cap + 1] = pos - 1;
        } else if (c == 'P' && pos + 1 < PN && P[pos + 1] == '<') {
            fail(gs, pos + 2, "E0320", "Named groups are written `(?<name>...)`.", -1, -1);
            E->fix = "(?<";
            return -1;
        } else if (c == 'P') {
            return fail(gs, pos + 1, "E0320", M_BACKREF, -1, -1);
        } else if (c == '=' || c == '!') {
            return fail(gs, pos + 1, "E0320", "Lookahead is not supported: vo patterns run in linear time. Match more and check the rest in code.", -1, -1);
        } else if (c == '>') {
            return fail(gs, pos + 1, "E0320", "Atomic groups and possessive quantifiers are not supported (and not needed: matching never backtracks).", -1, -1);
        } else if (c == '#') {
            return fail(gs, pos + 1, "E0320", "Comments and the `x` flag are not supported.", -1, -1);
        } else if (c == '(') {
            return fail(gs, pos + 1, "E0320", "Conditionals are not supported.", -1, -1);
        } else { /* flags: (?i) (?-i) (?i:...) */
            int on = 1, any = 0, f = flags;
            for (; pos < PN && P[pos] != ')' && P[pos] != ':'; pos++) {
                char k = P[pos];
                int bit = k == 'i' ? F_I : k == 'm' ? F_M : k == 's' ? F_S : 0;
                if (k == '-' && on) {
                    on = 0;
                    continue;
                }
                if (k == 'x') return fail(gs, pos + 1, "E0320", "Comments and the `x` flag are not supported.", -1, -1);
                if (!bit)
                    return fail(gs, pos + 1, "E0319",
                                "Unknown group syntax `$1`: groups are `(...)`, `(?:...)`, `(?<name>...)` and the flags `(?i)`, `(?m)`, `(?s)`.",
                                gs, pos + 1);
                f = on ? f | bit : f & ~bit;
                any = 1;
            }
            if (pos >= PN) return fail(gs, gs + 1, "E0319", "The group is not closed: add `)`.", -1, -1);
            if (!any) return fail(gs, pos + 1, "E0319", "Unknown group syntax `$1`: groups are `(...)`, `(?:...)`, `(?<name>...)` and the flags `(?i)`, `(?m)`, `(?s)`.", gs, pos + 1);
            if (P[pos] == ')') { /* the rest of the enclosing group */
                pos++;
                flags = f;
                int n = new_node(K_EMPTY, 0, 0);
                nodes[n].flagsonly = 1;
                return n;
            }
            pos++; /* ':' */
            flags = f;
        }
    } else {
        if (ngroups + 1 >= MAX_GROUPS) return fail(gs, pos, "E0319", "More than 1000 groups.", -1, -1);
        cap = (int)++ngroups;
        gname = grow(gname, &cap_gname, 2 * ngroups + 2, sizeof(long));
        gname[2 * cap] = -1;
        gname[2 * cap + 1] = -1;
    }
    if (++depth > MAX_DEPTH) return fail(gs, gs + 1, "E0319", "Groups nested more than 200 deep.", -1, -1);
    int child = parse_alt();
    depth--;
    flags = saved;
    if (failed) return -1;
    if (pos >= PN || P[pos] != ')') return fail(gs, gs + 1, "E0319", "The group is not closed: add `)`.", -1, -1);
    pos++;
    int g = new_node(K_GROUP, cap, 0);
    nodes[g].child = child;
    return g;
}

static int parse_atom(void)
{
    long s = pos, len;
    char c = P[pos];
    switch (c) {
    case '(': return parse_group();
    case '[': return parse_class();
    case '.': pos++; return new_node(K_ANY, (flags & F_S) != 0, 0);
    case '^': pos++; return new_node(K_ASSERT, (flags & F_M) ? A_BOL : A_BOT, 0);
    case '$': pos++; return new_node(K_ASSERT, (flags & F_M) ? A_EOL : A_EOT, 0);
    case '*': case '+': case '?':
        return fail(s, s + 1, "E0319", "`$1` has nothing to repeat.", s, s + 1);
    case '{': {
        long mn, mx;
        if (parse_count(&mn, &mx)) return fail(s, pos, "E0319", "`$1` has nothing to repeat.", s, pos);
        return fail(s, s + 1, "E0319", M_BRACE, -1, -1);
    }
    case '\\': {
        long es = pos;
        if (pos + 1 < PN && (P[pos + 1] == 'b' || P[pos + 1] == 'B')) {
            pos += 2;
            return new_node(K_ASSERT, P[es + 1] == 'b' ? A_WORD : A_NOTWORD, 0);
        }
        int sh;
        long from = ntmp;
        int v = parse_escape(&sh, 0);
        if (failed) return -1;
        if (sh) return finish_class(from, 0);
        return lit_node(v);
    }
    default: {
        int v = decode_at(P, PN, pos, &len);
        pos += len;
        return lit_node(v);
    }
    }
}

static int parse_piece(void)
{
    int atom = parse_atom();
    if (failed) return -1;
    int quantified = 0;
    while (pos < PN) {
        long qs = pos, mn, mx;
        char c = P[pos];
        if (c == '*' || c == '+' || c == '?') {
            mn = c == '+';
            mx = c == '?' ? 1 : -1;
            pos++;
        } else if (c == '{') {
            if (!parse_count(&mn, &mx)) return fail(qs, qs + 1, "E0319", M_BRACE, -1, -1);
            if (mn > MAX_COUNT || mx > MAX_COUNT) return fail(qs, pos, "E0319", "Repetition count above 1000.", -1, -1);
            if (mx >= 0 && mn > mx)
                return fail(qs, pos, "E0319", "Repetition `$1` has its minimum above its maximum.", qs, pos);
        } else {
            break;
        }
        RNode *a = &nodes[atom];
        if (a->kind == K_ASSERT || a->flagsonly)
            return fail(qs, pos, "E0319", "`$1` has nothing to repeat.", qs, pos);
        if (quantified)
            return fail(qs, pos, "E0319", "A quantifier cannot follow another quantifier: put the first in a group.", -1, -1);
        int greedy = 1;
        if (pos < PN && P[pos] == '?') {
            greedy = 0;
            pos++;
        } else if (pos < PN && P[pos] == '+') {
            return fail(qs, pos + 1, "E0320", "Atomic groups and possessive quantifiers are not supported (and not needed: matching never backtracks).", -1, -1);
        }
        int r = new_node(K_REP, (int)mn, (int)mx);
        nodes[r].greedy = greedy;
        nodes[r].child = atom;
        atom = r;
        quantified = 1;
    }
    return atom;
}

/* A sequence until `|`, `)` or the end. */
static int parse_cat(void)
{
    int first = -1, last = -1, count = 0;
    while (pos < PN && P[pos] != '|' && P[pos] != ')') {
        int p = parse_piece();
        if (failed) return -1;
        if (first < 0)
            first = p;
        else
            nodes[last].next = p;
        last = p;
        count++;
    }
    if (count == 0) return new_node(K_EMPTY, 0, 0);
    if (count == 1) return first;
    int c = new_node(K_CAT, 0, 0);
    nodes[c].child = first;
    return c;
}

static int parse_alt(void)
{
    int first = parse_cat();
    if (failed || pos >= PN || P[pos] != '|') return first;
    int alt = new_node(K_ALT, 0, 0);
    nodes[alt].child = first;
    int last = first;
    while (pos < PN && P[pos] == '|') {
        pos++;
        int c = parse_cat();
        if (failed) return -1;
        nodes[last].next = c;
        last = c;
    }
    return alt;
}

/* ---- compiler ---- */

static long emit(int op, int x, int y)
{
    if (ncode >= MAX_CODE) {
        fail(0, PN, "E0319", "Pattern is too large (more than 20000 instructions after repetitions).", -1, -1);
        return 0;
    }
    code = grow(code, &cap_code, (ncode + 1) * 3, sizeof(int));
    islot = grow(islot, &cap_islot, ncode + 1, sizeof(int));
    islot[ncode] = cur_slot;
    code[3 * ncode] = op;
    code[3 * ncode + 1] = x;
    code[3 * ncode + 2] = y;
    return ncode++;
}

static void comp(int ni)
{
    if (failed) return;
    RNode *n = &nodes[ni];
    switch (n->kind) {
    case K_LIT: emit(I_CHAR, n->a, 0); break;
    case K_CLASS: emit(I_CLASS, n->a, n->b); break;
    case K_ANY: emit(n->a ? I_ANYNL : I_ANY, 0, 0); break;
    case K_ASSERT: emit(I_ASSERT, n->a, 0); break;
    case K_EMPTY: break;
    case K_GROUP:
        if (n->a >= 0) emit(I_SAVE, 2 * n->a, 0);
        comp(n->child);
        if (nodes[ni].a >= 0) emit(I_SAVE, 2 * nodes[ni].a + 1, 0);
        break;
    case K_CAT:
        for (int c = n->child; c >= 0 && !failed; c = nodes[c].next) comp(c);
        break;
    case K_ALT: {
        long jumps[64], nj = 0, first_jump = -1; /* jumps chained through x when over 64 */
        for (int c = n->child; c >= 0 && !failed; c = nodes[c].next) {
            if (nodes[c].next < 0) {
                comp(c);
                break;
            }
            long split = emit(I_SPLIT, 0, 0);
            code[3 * split + 1] = (int)(split + 1);
            comp(c);
            long j = emit(I_JMP, -1, 0);
            if (nj < 64) {
                jumps[nj++] = j;
            } else {
                code[3 * j + 2] = (int)first_jump; /* y: the previous overflow jump */
                first_jump = j;
            }
            code[3 * split + 2] = (int)ncode;
        }
        if (failed) return;
        for (long i = 0; i < nj; i++) code[3 * jumps[i] + 1] = (int)ncode;
        for (long j = first_jump; j >= 0;) {
            long prev = code[3 * j + 2];
            code[3 * j + 1] = (int)ncode;
            code[3 * j + 2] = 0;
            j = prev;
        }
        break;
    }
    case K_REP: {
        int mn = n->a, mx = n->b, greedy = n->greedy, child = n->child;
        for (int i = 0; i < mn && !failed; i++) comp(child);
        if (mx < 0) { /* star: L: SPLIT body, out; body: LOOP-entry mark, x, LOOP slot (back to L if it advanced) */
            long slot = (long)(2 * ngroups + nloops++);
            long l = emit(I_SPLIT, 0, 0);
            emit(I_SAVE, (int)slot, 0);
            int outer = cur_slot;
            cur_slot = (int)slot; /* the body's states are told apart by whether this iteration consumed yet */
            comp(child);
            emit(I_LOOP, (int)slot, (int)l);
            cur_slot = outer;
            long out = ncode;
            code[3 * l + 1] = (int)(greedy ? l + 1 : out);
            code[3 * l + 2] = (int)(greedy ? out : l + 1);
        } else if (mx > mn) { /* optional copies: (x(x(x)?)?)?; an empty copy ends them (Python) */
            long slot = (long)(2 * ngroups + nloops++);
            long splits[MAX_COUNT + 1], exits[MAX_COUNT + 1], ns = 0;
            for (int i = mn; i < mx && !failed; i++) {
                splits[ns] = emit(I_SPLIT, 0, 0);
                emit(I_SAVE, (int)slot, 0);
                comp(child);
                exits[ns++] = emit(I_EMPTYEXIT, (int)slot, 0);
            }
            if (failed) return;
            for (long i = 0; i < ns; i++) {
                long s = splits[i];
                code[3 * s + 1] = (int)(greedy ? s + 1 : ncode);
                code[3 * s + 2] = (int)(greedy ? ncode : s + 1);
                code[3 * exits[i] + 2] = (int)ncode;
            }
        }
        break;
    }
    }
}

/* Adds to set the first bytes of code point range lo..hi. */
static void first_bytes(unsigned char *set, int lo, int hi)
{
    for (int c = lo; c <= hi && c < 0x80; c++) set[c] = 1;
    if (hi >= 0x80) /* any lead byte of a longer sequence */
        for (int b = 0xC2; b <= 0xF4; b++) set[b] = 1;
}

/* The bytes a match can start with: from instruction 0, following everything that consumes nothing
 * (assertions are assumed to hold: a superset). */
static void compute_first(Prog *p)
{
    long n = p->ncode, sp = 0;
    static char *seen;
    static long *stack, cap_seen, cap_stack;
    seen = grow(seen, &cap_seen, n + 1, 1); /* perm scratch: this runs while the collector is on */
    stack = grow(stack, &cap_stack, 2 * (n + 1), 8);
    for (long i = 0; i <= n; i++) seen[i] = 0;
    stack[sp++] = 0;
    for (int i = 0; i < 256; i++) p->first[i] = 0;
    p->any_first = 0;
    while (sp) {
        long pc = stack[--sp];
        if (seen[pc]) continue;
        seen[pc] = 1;
        int *in = p->code + 3 * pc;
        switch (in[0]) {
        case I_CHAR: {
            int c = in[1];
            if (c < 0x80) p->first[c] = 1;
            else first_bytes(p->first, c, c);
            break;
        }
        case I_CLASS:
            for (int r = 0; r < in[2]; r++) first_bytes(p->first, p->ranges[in[1] + 2 * r], p->ranges[in[1] + 2 * r + 1]);
            break;
        case I_ANY: case I_ANYNL: case I_MATCH: p->any_first = 1; break;
        case I_SPLIT: stack[sp++] = in[2]; stack[sp++] = in[1]; break;
        case I_JMP: stack[sp++] = in[1]; break;
        case I_LOOP: case I_EMPTYEXIT: stack[sp++] = in[2]; stack[sp++] = pc + 1; break;
        default: stack[sp++] = pc + 1; break; /* SAVE, ASSERT */
        }
    }
}

/* Parses and compiles pat into the scratch arrays; 0 and *E filled on error. */
static int build(char *pat, long n, ReErr *err)
{
    P = pat;
    PN = n;
    pos = 0;
    depth = 0;
    ngroups = 0;
    nloops = 0;
    cur_slot = -1;
    flags = 0;
    failed = 0;
    E = err;
    nnodes = 0;
    nrng = 0;
    ntmp = 0;
    ncode = 0;
    gname = grow(gname, &cap_gname, 2, sizeof(long));
    if (n > MAX_PATTERN) {
        fail(0, n, "E0319", "Pattern is longer than 10000 bytes.", -1, -1);
        return 0;
    }
    int root = parse_alt();
    if (!failed && pos < PN) fail(pos, pos + 1, "E0319", "Unmatched `)`: write `\\)` for the character.", -1, -1);
    if (failed) return 0;
    ngroups++; /* group 0 */
    emit(I_SAVE, 0, 0);
    comp(root);
    emit(I_SAVE, 1, 0);
    emit(I_MATCH, 0, 0);
    if (!failed && ncode * (2 * ngroups + nloops + 1) > 2000000) /* the VM's capture buffers (2x per list) */
        fail(0, PN, "E0319", "Pattern is too large (more than 20000 instructions after repetitions).", -1, -1);
    return !failed;
}

/* The names of groups 1..n, each followed by \n ("" for an unnamed group). */
static Str *group_names(void)
{
    SB *sb = sb_new();
    for (long g = 1; g < ngroups; g++) {
        if (gname[2 * g] >= 0) sb_bytes(sb, P + gname[2 * g], gname[2 * g + 1] - gname[2 * g]);
        sb_byte(sb, '\n');
    }
    return sb_finish(sb);
}

/* The text of a pattern error: its template with $1 the pattern bytes it names. */
Str *re_err_text(char *pat, ReErr *e)
{
    if (e->as < 0) return str_from_cstr(e->tmpl);
    Str *arg = str_new(pat + e->as, e->ae - e->as);
    return msg(e->tmpl, arg, 0, 0);
}

/* For the checker: the number of groups (group 0 not counted) and their names, or -1 with *err. */
long re_check(char *pat, long n, ReErr *err, Str **names)
{
    if (!build(pat, n, err)) return -1;
    if (names) *names = group_names();
    return ngroups - 1;
}

/* ---- the compile cache (research R3) ---- */

#define SLOTS 16
typedef struct {
    char *key; /* the pattern's bytes (perm copy) */
    long key_len, cap_key, used;
    Prog prog;
    long cap_code, cap_ranges, cap_islot, cap_names;
} Slot;
static Slot slots[SLOTS];
static long next_slot;

/* The compiled program of pattern p, or 0 with *err (a message ending with its byte offset). */
static Prog *re_get(Str *p, Str **err)
{
    for (long i = 0; i < SLOTS; i++)
        if (slots[i].used && slots[i].key_len == p->len && mem_eq(slots[i].key, p->data, p->len))
            return &slots[i].prog;
    ReErr e;
    if (!build(p->data, p->len, &e)) {
        Str *text = re_err_text(p->data, &e), *at = 0; /* p is rooted by the caller */
        GC_ROOT(&text);
        at = int_to_str(e.start);
        GC_ROOT(&at);
        SB *sb = sb_new();
        sb_cstr(sb, "at byte ");
        sb_str(sb, at);
        sb_cstr(sb, ": ");
        sb_str(sb, text);
        *err = sb_finish(sb);
        GC_UNROOT(2);
        return 0;
    }
    /* ponytail: round-robin replacement over 16 slots; an LRU if programs thrash in a loop */
    Slot *s = &slots[next_slot];
    next_slot = (next_slot + 1) % SLOTS;
    s->used = 0;
    s->key = grow(s->key, &s->cap_key, p->len + 1, 1);
    s->key_len = p->len;
    mem_copy(s->key, p->data, p->len);
    s->prog.code = grow(s->prog.code, &s->cap_code, ncode * 3, sizeof(int));
    mem_copy((char *)s->prog.code, (char *)code, ncode * 3 * (long)sizeof(int));
    s->prog.ncode = ncode;
    s->prog.islot = grow(s->prog.islot, &s->cap_islot, ncode, sizeof(int));
    mem_copy((char *)s->prog.islot, (char *)islot, ncode * (long)sizeof(int));
    s->prog.ranges = grow(s->prog.ranges, &s->cap_ranges, nrng + 1, sizeof(int));
    mem_copy((char *)s->prog.ranges, (char *)rng, nrng * (long)sizeof(int));
    s->prog.nranges = nrng;
    s->prog.ngroups = ngroups;
    s->prog.nslots = 2 * ngroups + nloops;
    Str *nm = group_names();
    s->prog.names = grow(s->prog.names, &s->cap_names, nm->len + 1, 1);
    mem_copy(s->prog.names, nm->data, nm->len);
    s->prog.names_len = nm->len;
    s->prog.anchored = code[3 * 1] == I_ASSERT && code[3 * 1 + 1] == A_BOT;
    compute_first(&s->prog);
    s->used = 1;
    return &s->prog;
}

/* ---- the Pike VM ---- */

typedef struct {
    long *pc;    /* pcs in priority order */
    long *caps;  /* nslots per thread */
    long n;
    long *dense; /* sparse set of pcs entered this step */
    long *sparse;
    long nset;
} TList;

static TList lists[2];
static long cap_threads, cap_slots;
static long *stk, *cur, *blank;

/* The VM's buffers for a program of nc instructions and ns capture slots (perm memory, only grown).
 * The captures are sized from this program's nc * ns, which the size limit bounds: sized from the
 * largest nc and the largest ns seen, two patterns that were each fine asked for their product. */
static long cap_caps; /* words in each list's caps */

static void ensure_vm(long nc, long ns)
{
    long need = 2 * (nc + 1) * (ns + 1); /* a state can be entered twice per step (add_thread) */
    if (need > cap_caps) {
        long w = cap_caps > 1024 ? cap_caps : 1024;
        while (w < need) w *= 2;
        for (int i = 0; i < 2; i++) lists[i].caps = perm_alloc(w * 8);
        cap_caps = w;
    }
    if (nc + 1 > cap_threads) {
        long c = cap_threads > 64 ? cap_threads : 64;
        while (c < nc + 1) c *= 2;
        for (int i = 0; i < 2; i++) {
            lists[i].pc = perm_alloc(2 * c * 8);
            lists[i].dense = perm_alloc(2 * c * 8); /* a LOOP's "empty" exit is entered as pc + ncode */
            lists[i].sparse = perm_alloc(2 * c * 8);
        }
        stk = perm_alloc((6 * c + 8) * 8); /* at most 3 pushes per entered state, two entries per LOOP */
        cap_threads = c;
    }
    if (ns + 1 > cap_slots) {
        long sl = cap_slots > 8 ? cap_slots : 8;
        while (sl < ns + 1) sl *= 2;
        cur = perm_alloc(sl * 8);
        blank = perm_alloc(sl * 8);
        cap_slots = sl;
    }
}

static void list_reset(TList *l)
{
    l->n = 0;
    l->nset = 0;
}

static int set_has(TList *l, long pc)
{
    long i = l->sparse[pc];
    return i < l->nset && l->dense[i] == pc;
}

static void set_add(TList *l, long pc)
{
    l->sparse[pc] = l->nset;
    l->dense[l->nset++] = pc;
}

static int assert_holds(int kind, char *s, long n, long p)
{
    switch (kind) {
    case A_BOT: return p == 0;
    case A_EOT: return p == n;
    case A_BOL: return p == 0 || s[p - 1] == '\n';
    case A_EOL: return p == n || s[p] == '\n';
    default: {
        int before = p > 0 && is_word((unsigned char)s[p - 1]), after = p < n && is_word((unsigned char)s[p]);
        return (before != after) == (kind == A_WORD);
    }
    }
}

/* Follows everything that consumes nothing from pc at text position p, in priority order, adding
 * the consuming instructions (and MATCH) to l with their captures. Explicit stack: entries are a pc
 * (>= 0) or a capture restore (-1 - slot, then the value). */
static void add_thread(Prog *pr, TList *l, long pc0, long *caps, char *s, long n, long p)
{
    long ns = pr->nslots, sp = 0;
    for (long i = 0; i < ns; i++) cur[i] = caps[i];
    stk[sp++] = pc0;
    while (sp) {
        long top = stk[--sp];
        if (top < 0) { /* restore */
            cur[-1 - top] = stk[--sp];
            continue;
        }
        long pc = top;
        int *in = pr->code + 3 * pc;
        long sl = pr->islot[pc], key = pc;
        if (sl >= 0 && cur[sl] == p) key = pc + pr->ncode; /* inside a loop iteration that has consumed nothing yet */
        if (set_has(l, key)) continue;
        set_add(l, key);
        switch (in[0]) {
        case I_JMP: stk[sp++] = in[1]; break;
        case I_SPLIT:
            stk[sp++] = in[2];
            stk[sp++] = in[1];
            break;
        case I_SAVE:
            stk[sp++] = cur[in[1]];
            stk[sp++] = -1 - in[1];
            cur[in[1]] = p;
            stk[sp++] = pc + 1;
            break;
        case I_ASSERT:
            if (assert_holds(in[1], s, n, p)) stk[sp++] = pc + 1;
            break;
        case I_LOOP: /* an iteration that advanced loops again; an empty one leaves (Python). A body's
                        states are entered once with an empty iteration and once with a consuming one per
                        step (key above), so an empty iteration after a consuming one keeps its own
                        priority and captures, as in Python's backtracking; still linear (2x states). */
            stk[sp++] = cur[in[1]] != p ? in[2] : pc + 1;
            break;
        case I_EMPTYEXIT: /* after an optional copy: an empty one ends the repetition */
            stk[sp++] = cur[in[1]] == p ? in[2] : pc + 1;
            break;
        default: {
            long k = l->n++;
            l->pc[k] = pc;
            for (long i = 0; i < ns; i++) l->caps[k * ns + i] = cur[i];
        }
        }
    }
}

static int in_class(Prog *pr, int idx, int count, int c)
{
    int lo = 0, hi = count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (c < pr->ranges[idx + 2 * mid]) hi = mid - 1;
        else if (c > pr->ranges[idx + 2 * mid + 1]) lo = mid + 1;
        else return 1;
    }
    return 0;
}

/* ponytail: a Pike VM only (O(text x program), 10 MB of logs in about 0.5 s); a lazy DFA for the
 * match extent if a measured target fails (tests/regex_speed.py).
 * The leftmost-first match starting at or after byte from; an empty match at byte forbid is skipped
 * (Python's rule after an empty match). 1 with out[0..2g-1] (bytes, -1 for groups that did not take
 * part), or 0. */
static int re_exec(Prog *pr, char *s, long n, long from, long forbid, long *out)
{
    long ns = pr->nslots;
    ensure_vm(pr->ncode, ns);
    TList *cl = &lists[0], *nl = &lists[1];
    list_reset(cl);
    for (long i = 0; i < ns; i++) blank[i] = -1;
    int matched = 0;
    long work = 0;
    for (long p = from;;) {
        if (!matched && (p == from || !pr->anchored)) {
            if (cl->n == 0) list_reset(cl); /* nothing alive: no marks carry over from an earlier position */
            if (cl->n == 0 && !pr->any_first) { /* skip to a byte a match can start with */
                while (p < n && !pr->first[(unsigned char)s[p]]) p++;
                if (p >= n) break; /* without any_first, every match consumes a code point */
            }
            add_thread(pr, cl, 0, blank, s, n, p);
        }
        long len = 1;
        int c = p < n ? decode_at(s, n, p, &len) : -1;
        if (cl->n == 0) { /* nothing alive here (an assertion failed): the next position */
            if (matched || p >= n || pr->anchored) break;
            list_reset(cl); /* the states entered at p do not carry over to p + len */
            p += len;
            continue;
        }
        list_reset(nl);
        for (long t = 0; t < cl->n; t++) {
            long pc = cl->pc[t];
            long *tc = cl->caps + t * ns;
            int *in = pr->code + 3 * pc;
            int ok = 0;
            switch (in[0]) {
            case I_MATCH:
                if (tc[0] == forbid && p == forbid) continue; /* an empty match where one just ended */
                for (long i = 0; i < 2 * pr->ngroups; i++) out[i] = tc[i];
                matched = 1;
                t = cl->n; /* cut: the lower-priority threads end here */
                continue;
            case I_CHAR: ok = c == in[1]; break;
            case I_CLASS: ok = c >= 0 && in_class(pr, in[1], in[2], c); break;
            case I_ANY: ok = c >= 0 && c != '\n'; break;
            case I_ANYNL: ok = c >= 0; break;
            }
            if (ok) add_thread(pr, nl, pc + 1, tc, s, n, p + len);
        }
        work += cl->n + 1;
        if (work >= 4096) { /* --timeout: one work unit per 64 thread steps */
            rt_budget -= work >> 6;
            work = 0;
            if (rt_budget <= 0) rt_poll();
        }
        TList *x = cl;
        cl = nl;
        nl = x;
        if (p >= n) break;
        p += len;
    }
    return matched;
}

/* ---- primitives (research R4) ---- */

/* __re_compile(pattern): ok(group names) or err(message). */
Variant *re_prim_compile(Str *p)
{
    GC_ROOT(&p);
    Str *err = 0;
    Prog *pr = re_get(p, &err);
    Str *payload = pr ? str_new(pr->names, pr->names_len) : err;
    GC_ROOT(&payload);
    Variant *r = make_result(pr ? 0 : 1, (long)payload);
    GC_UNROOT(2);
    return r;
}

/* __re(op, pattern, text): op 1 test, 2 first match, 3 every match (flat byte offsets). */
List *re_prim(long op, Str *p, Str *s, Node *call)
{
    GC_ROOT(&p);
    GC_ROOT(&s);
    Str *err = 0;
    Prog *pr = re_get(p, &err);
    if (!pr) rt_raise("R0007", msg("Invalid pattern $1", err, 0, 0), std_user_call(call));
    cur_node = (long)std_user_call(call); /* a timeout inside the match reports the user's call */
    long g2 = 2 * pr->ngroups;
    long *out = alloc((g2 + 1) * 8);
    GC_ROOT(&out);
    List *l = list_new(op == 3 ? 16 : g2);
    GC_ROOT(&l);
    long from = 0, forbid = -1;
    for (;;) {
        if (!re_exec(pr, s->data, s->len, from, forbid, out)) break;
        if (op == 1) {
            list_push(l, 1);
            break;
        }
        for (long i = 0; i < g2; i++) list_push(l, out[i]);
        if (op == 2) break;
        if (out[1] == out[0]) { /* empty: the next one may not be empty here */
            forbid = out[1];
            from = out[1];
        } else {
            forbid = -1;
            from = out[1];
        }
        if (from > s->len) break;
    }
    GC_UNROOT(4);
    return l;
}

/* ---- replacements ($1, ${1}, ${name}, $$) ---- */

#define M_DOLLAR "`$` must be followed by a group number, `{name}` or `$` (write `$$` for a dollar sign)."

/* The group named name[0..len) among names (each followed by \n), or -1. */
static long group_by_name(char *names, long nlen, char *name, long len)
{
    long g = 1, start = 0;
    for (long i = 0; i < nlen; i++) {
        if (names[i] != '\n') continue;
        if (i - start == len && len && mem_eq(names + start, name, len)) return g;
        g++;
        start = i + 1;
    }
    return -1;
}

/* Parses replacement r (ngroups counts group 0; -1: unknown, no group check) into triples appended
 * to l when l is given: (-1, a, b) the literal bytes r[a..b), (g, 0, 0) group g. 1, or 0 with *err. */
int re_repl_parse(char *r, long n, long ngroups_, char *names, long names_len, ReErr *err, List *l)
{
    long lit = 0;
    for (long i = 0; i < n;) {
        if (r[i] != '$') {
            i++;
            continue;
        }
        if (l && i > lit) {
            list_push(l, -1);
            list_push(l, lit);
            list_push(l, i);
        }
        long s = i++, g = -1;
        if (i < n && r[i] == '$') { /* $$: the second one is the text */
            lit = i++;
            continue;
        }
        if (i < n && r[i] >= '0' && r[i] <= '9') {
            g = 0;
            while (i < n && r[i] >= '0' && r[i] <= '9') {
                if (g < 100000) g = g * 10 + (r[i] - '0');
                i++;
            }
        } else if (i < n && r[i] == '{') {
            long ns = ++i;
            while (i < n && r[i] != '}' && is_word((unsigned char)r[i])) i++;
            if (i >= n || r[i] != '}' || i == ns) {
                err->start = s;
                err->end = i < n ? i + 1 : n;
                err->msg = str_from_cstr(M_DOLLAR);
                return 0;
            }
            if (r[ns] >= '0' && r[ns] <= '9') {
                g = 0;
                for (long k = ns; k < i; k++) {
                    if (r[k] < '0' || r[k] > '9') {
                        err->start = s;
                        err->end = i + 1;
                        err->msg = str_from_cstr(M_DOLLAR);
                        return 0;
                    }
                    if (g < 100000) g = g * 10 + (r[k] - '0');
                }
            } else if (ngroups_ >= 0) {
                g = group_by_name(names, names_len, r + ns, i - ns);
                if (g < 0) {
                    err->start = s;
                    err->end = i + 1;
                    err->msg = msg("The pattern has no group named `$1`.", str_new(r + ns, i - ns), 0, 0);
                    return 0;
                }
            } else {
                g = 0; /* a name, pattern unknown: checked at run time */
            }
            i++;
        } else {
            err->start = s;
            err->end = i < n ? i + 1 : n;
            err->msg = str_from_cstr(M_DOLLAR);
            return 0;
        }
        if (ngroups_ >= 0 && g >= ngroups_) {
            err->start = s;
            err->end = i;
            err->msg = msg("The pattern has no group $1.", int_to_str(g), 0, 0);
            return 0;
        }
        if (l) {
            list_push(l, g);
            list_push(l, 0);
            list_push(l, 0);
        }
        lit = i;
    }
    if (l && n > lit) {
        list_push(l, -1);
        list_push(l, lit);
        list_push(l, n);
    }
    return 1;
}

/* __re_repl(pattern, replacement): the replacement's parts (re_repl_parse); R0007 when it is bad. */
List *re_prim_repl(Str *p, Str *r, Node *call)
{
    GC_ROOT(&p);
    GC_ROOT(&r);
    Str *err = 0;
    Prog *pr = re_get(p, &err);
    if (!pr) rt_raise("R0007", msg("Invalid pattern $1", err, 0, 0), std_user_call(call));
    List *l = list_new(8);
    GC_ROOT(&l);
    ReErr e;
    if (!re_repl_parse(r->data, r->len, pr->ngroups, pr->names, pr->names_len, &e, l))
        rt_raise("R0007", e.msg, std_user_call(call));
    GC_UNROOT(3);
    return l;
}
