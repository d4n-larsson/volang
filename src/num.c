/* Number <-> text conversion (research §3, §11; ported from src/num.s, spec 008 step 4). */
#include "vo.h"

/* The decimal digits of v, written backwards ending at end; returns the first digit's address. */
static char *dec_digits(char *end, unsigned long u)
{
    do {
        *--end = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    return end;
}

/* |v| as unsigned: the minimum integer stays 2^63 (research R13: no signed overflow). */
static unsigned long magnitude(long v)
{
    return v < 0 ? 0UL - (unsigned long)v : (unsigned long)v;
}

void int_to_dec(SB *sb, long v)
{
    char buf[32];
    if (v < 0) sb_byte(sb, '-');
    char *start = dec_digits(buf + sizeof buf, magnitude(v));
    sb_bytes(sb, start, buf + sizeof buf - start);
}

/* The decimal text of v: digits on the stack, then one string (spec 004: one allocation, not a
 * builder's three). */
Str *int_to_str(long v)
{
    char buf[32];
    char *start = dec_digits(buf + sizeof buf, magnitude(v));
    if (v < 0) *--start = '-';
    return str_new(start, buf + sizeof buf - start);
}

/* 0 = ok, 1 = overflow, 2 = malformed. Accepts an optional leading '-'. */
int dec_to_int(char *p, long n, long *out)
{
    int negative = 0;
    if (n <= 0) return 2;
    if (*p == '-') {
        negative = 1;
        p++;
        n--;
        if (n <= 0) return 2;
    }
    unsigned long m = 0;
    for (; n > 0; n--, p++) {
        int d = *p - '0';
        if (d < 0 || d > 9) return 2;
        if (m > ~0UL / 10) return 1; /* the multiplication would carry out of 64 bits */
        m *= 10;
        if (m + (unsigned long)d < m) return 1;
        m += (unsigned long)d;
    }
    if (!negative) {
        if (m >= 1UL << 63) return 1; /* positive limit is 2^63 - 1 */
        *out = (long)m;
    } else {
        if (m > 1UL << 63) return 1; /* negative limit is 2^63 */
        *out = (long)(0UL - m);
    }
    return 0;
}

static const double pow10[23] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22,
};

static double bits_to_double(long bits)
{
    double d;
    memcpy(&d, &bits, 8);
    return d;
}

static long double_to_bits(double d)
{
    long bits;
    memcpy(&bits, &d, 8);
    return bits;
}

/* bits (an f64) times 10^k, applied in exact steps of at most 10^22. */
long scale10(long bits, long k)
{
    double x = bits_to_double(bits);
    while (k > 22) {
        x *= pow10[22];
        k -= 22;
    }
    while (k < -22) {
        x /= pow10[22];
        k += 22;
    }
    if (k >= 0)
        x *= pow10[k];
    else
        x /= pow10[-k];
    return double_to_bits(x);
}

/* Decimal text ("-"? digits ["." digits], at most 17 significant digits) to the nearest f64, as
 * bits. Digits up to the last non-zero one form the mantissa; trailing zeros only move the power of
 * ten. A mantissa below 2^53 with a power of at most 22 either way is one exact operation; anything
 * else goes through big integers (exact_to_double), so every text reads as the nearest double and
 * what float_to_text writes reads back as the same value. */
long text_to_float(char *p, long n)
{
    int negative = 0;
    if (n > 0 && *p == '-') {
        negative = 1;
        p++;
        n--;
    }
    long last = -1; /* offset of the last non-zero digit */
    for (long i = 0; i < n; i++)
        if (p[i] >= '1' && p[i] <= '9') last = i;
    unsigned long mant = 0;
    long power = 0;
    int point = 0;
    for (long i = 0; i < n; i++) {
        if (p[i] == '.') {
            point = 1;
            continue;
        }
        if (i > last) {
            if (!point) power++; /* trailing zeros of the integer part multiply by ten */
            continue;
        }
        mant = mant * 10 + (unsigned long)(p[i] - '0');
        power -= point; /* fraction digits divide by ten */
    }
    if (mant >> 53 || power > 22 || power < -22)
        return (long)(exact_to_double(mant, power) | (negative ? 1UL << 63 : 0));
    double x = (double)(long)mant;
    if (negative) x = -x;
    return scale10(double_to_bits(x), power);
}

/* Validates and converts text for parse_float: 0 = ok, 2 = malformed.
 * Accepts "-"? digits ["." digits] with at most 17 significant digits. */
int float_parse(char *p, long n, long *out)
{
    char *q = p;
    long m = n;
    if (m <= 0) return 2;
    if (*q == '-') {
        q++;
        m--;
        if (m <= 0) return 2;
    }
    long before = 0, after = 0, since_nonzero = 0, significant = 0;
    int point = 0, nonzero = 0;
    for (; m > 0; m--, q++) {
        if (*q == '.') {
            if (point) return 2;
            point = 1;
            continue;
        }
        int d = *q - '0';
        if (d < 0 || d > 9) return 2;
        if (point)
            after++;
        else
            before++;
        if (!nonzero) {
            if (d == 0) continue;
            nonzero = 1;
        }
        since_nonzero++; /* count up to here; trailing zeros are taken back below */
        if (d != 0) significant = since_nonzero;
    }
    if (before == 0) return 2;
    if (point && after == 0) return 2;
    if (significant > 17) return 2;
    *out = text_to_float(p, n);
    return 0;
}

/* round(x / 10^(e-n+1)) (n digits when e is right) for the finite positive double with these bits, rounded once (ties to even),
 * in 128-bit integers: x = m * 2^b, so the quotient is m * 10^p / 2^-b or m * 2^b / 10^-p. Returns 0
 * where that does not fit (|x| below about 1e-8 or above about 1e34): the caller uses big integers. */
static int digits_u128(unsigned long abs_bits, long e, int nd, unsigned long *out)
{
    typedef unsigned __int128 u128;
    long biased = (long)(abs_bits >> 52);
    unsigned long m = abs_bits & ((1UL << 52) - 1);
    long b = -1074;
    if (biased) {
        m |= 1UL << 52;
        b = biased - 1075;
    }
    long p = nd - 1 - e;
    u128 q, rem, half;
    int odd;
    if (p >= 0) { /* m * 10^p < 2^53 * 10^22 < 2^127, then an exact shift right */
        if (p > 22 || b >= 0 || b < -127) return 0;
        u128 n = m;
        for (long i = 0; i < p; i++) n *= 10;
        long s = -b;
        q = n >> s;
        rem = n & (((u128)1 << s) - 1);
        half = (u128)1 << (s - 1);
        odd = (int)(q & 1);
        if (rem > half || (rem == half && odd)) q++;
    } else { /* m * 2^b over 10^-p (times 2^-b when b < 0), one division */
        if (-p > 19 || b > 73) return 0;
        u128 n = m, den = 1;
        for (long i = 0; i < -p; i++) den *= 10;
        if (b >= 0)
            n <<= b;
        else
            den <<= -b;
        q = n / den;
        rem = n % den;
        u128 twice = rem * 2; /* rem < den < 2^67: no overflow */
        if (twice > den || (twice == den && (q & 1))) q++;
    }
    *out = (unsigned long)q;
    return 1;
}

/* The nd significant digits (15 to 17) of the finite positive double with these bits at d, rounded
 * once (ties to even); returns e, the power of ten of the first, starting from the estimate e.
 * 128-bit integers where they fit, else big integers (src/stdprim.c, about 30 times slower). */
static long digits_of(unsigned long abs_bits, long e, int nd, char *d)
{
    unsigned long lo = 1, digits = 0;
    for (int i = 1; i < nd; i++) lo *= 10;
    int fast = 0;
    for (int tries = 0; tries < 3; tries++) {
        fast = digits_u128(abs_bits, e, nd, &digits);
        if (!fast) break;
        if (digits < lo) {
            e--;
        } else if (digits >= lo * 10) {
            e++;
        } else {
            break;
        }
    }
    if (!fast || digits < lo || digits >= lo * 10) return exact_digits(abs_bits, nd, d);
    if (digits == lo) { /* 10..0: really that, or 9.99.. rounded up with e one too high? */
        unsigned long below;
        if (!digits_u128(abs_bits, e - 1, nd, &below)) return exact_digits(abs_bits, nd, d);
        if (below < lo * 10) {
            digits = below;
            e--;
        }
    }
    for (int i = nd - 1; i >= 0; i--) {
        d[i] = (char)('0' + digits % 10);
        digits /= 10;
    }
    return e;
}

/* Whether nd digits can stand for the double with these bits: the digits at d (its value rounded to
 * nd digits, first digit worth 10^*e), or the number one step above or below them, which can be the
 * one that reads back when the value is a power of two (the doubles below it lie closer than those
 * above). Leaves the digits that read back at d and their power of ten in *e. */
static int digits_read_back(char *d, int nd, long *e, unsigned long abs_bits)
{
    unsigned long w = 0, lo = 1;
    for (int i = 0; i < nd; i++) w = w * 10 + (unsigned long)(d[i] - '0');
    for (int i = 1; i < nd; i++) lo *= 10;
    for (int step = 0; step < 3; step++) {
        unsigned long c = step == 0 ? w : step == 1 ? w + 1 : w - 1;
        long ce = *e;
        if (c >= lo * 10) { /* 99..9 + 1 */
            c = lo;
            ce++;
        }
        if (c < lo) continue; /* 10..0 - 1 has fewer digits: tried before */
        if (exact_to_double(c, ce - nd + 1) != abs_bits) continue;
        for (int i = nd - 1; i >= 0; i--) {
            d[i] = (char)('0' + c % 10);
            c /= 10;
        }
        *e = ce;
        return 1;
    }
    return 0;
}

/* The shortest text that reads back as the same double (text_to_float), so parse_float(str(x)) == x:
 * the fewest significant digits that do, 15 to 17 (from 1 for the subnormals, which hold fewer),
 * and of those the nearest to the value. Python's repr gives the same digits.
 * ponytail: up to three roundings and several read-backs per value, in big integers outside about
 * 1e-6 to 1e30; upgrade: a Ryu port if printing floats shows up in profiles. */
void float_to_text(SB *sb, long bits)
{
    double x = bits_to_double(bits);
    if (x != x) {
        sb_cstr(sb, "nan"); /* any NaN, whatever its sign and payload (research R13) */
        return;
    }
    if (x == -1.0 / 0.0) {
        sb_cstr(sb, "-inf");
        return;
    }
    if (x == 1.0 / 0.0) {
        sb_cstr(sb, "inf");
        return;
    }
    GC_ROOT(&sb);
    char *out = alloc(400); /* output buffer (allocated, as in the assembly: memory figures) */
    GC_UNROOT(1);
    char *w = out;
    if (bits < 0) *w++ = '-';
    unsigned long abs_bits = (unsigned long)bits << 1 >> 1;
    char *frac;
    if (abs_bits == 0) {
        *w++ = '0';
        *w++ = '.';
        *w++ = '0';
        sb_bytes(sb, out, w - out);
        return;
    }
    /* First the 15 significant digits of the exact value, rounded once (ties to even), and e, the power of
     * ten of the first. Scaling by a power of ten in floating point first (before spec 020) rounded
     * twice and got about 4% of values wrong in the last digit. 128-bit integers where they fit, else
     * big integers (src/stdprim.c, about 30 times slower). */
    char d[17];
    int nd = 15;
    double a = bits_to_double((long)abs_bits); /* e = floor(log10(|x|)), estimated, then checked */
    long e = 0;
    while (10.0 <= a) {
        a /= 10.0;
        e++;
    }
    while (a < 1.0) {
        a *= 10.0;
        e--;
    }
    long e0 = e; /* each rounding starts from the estimate: a carry (9.99 to 10.0) moves only its own e */
    if (!(abs_bits >> 52)) nd = 1;
    for (;; nd++) {
        e = digits_of(abs_bits, e0, nd, d);
        if (nd == 17 || digits_read_back(d, nd, &e, abs_bits)) break;
    }
    if (e >= 0) {
        long i = 0;
        for (; i <= e; i++) *w++ = i < nd ? d[i] : '0'; /* the integer part, padded with zeros */
        *w++ = '.';
        frac = w;
        for (; i < nd; i++) *w++ = d[i];
        if (w == frac) *w++ = '0';
    } else {
        *w++ = '0'; /* "0." then -e-1 zeros, then the digits */
        *w++ = '.';
        frac = w;
        for (long z = -e - 1; z > 0; z--) *w++ = '0';
        for (int i = 0; i < nd; i++) *w++ = d[i];
    }
    while (w > frac + 1 && w[-1] == '0') w--; /* trim trailing zeros, keep one fraction digit */
    sb_bytes(sb, out, w - out);
}

Str *float_to_str(long bits)
{
    SB *sb = sb_new();
    GC_ROOT(&sb);
    float_to_text(sb, bits);
    Str *s = sb_finish(sb);
    GC_UNROOT(1);
    return s;
}

/* ---- spec 024: one placeholder of `format` ---- */

/* Inserts `,` every three digits of the n digits at d (in place; room for them); returns the length. */
static long group3(char *d, long n)
{
    long commas = (n - 1) / 3, len = n + commas;
    for (long i = n - 1, o = len - 1, k = 0; i >= 0; i--, k++) {
        if (k && k % 3 == 0) d[o--] = ',';
        d[o--] = d[i];
    }
    return len;
}

/* value (of type kind: TY_INT, TY_FLOAT, TY_BOOL or TY_STR) as one placeholder with the packed
 * spec (FS_* in vo.h; contracts/language.md "Output"). Integer arithmetic only: the same bytes on
 * every platform. */
Str *fmt_piece(long value, long kind, long spec)
{
    if (!spec) { /* `{}`: what str gives */
        if (kind == TY_STR) return (Str *)value;
        if (kind == TY_BOOL) return bool_str(value);
        return kind == TY_FLOAT ? float_to_str(value) : int_to_str(value);
    }
    long width = FS_WIDTH(spec), align = spec & 3;
    char num[1100]; /* sign + up to 309 + 103 grouped integer digits + '.' + 100 decimals */
    char *body = num + 1, sign = 0;
    long n = 0;     /* body length */
    int zero_ok = 1; /* the zero flag applies (finite numbers) */
    Str *s = 0;
    if (kind == TY_STR || kind == TY_BOOL) {
        s = kind == TY_STR ? (Str *)value : bool_str(value);
        long cps = cp_count(s->data, s->len);
        if (cps >= width) return s;
        GC_ROOT(&s);
        SB *sb = sb_new();
        long pad = width - cps, left = align == 2 ? pad : align == 3 ? pad / 2 : 0;
        for (long i = 0; i < left; i++) sb_byte(sb, ' ');
        sb_bytes(sb, s->data, s->len);
        for (long i = left; i < pad; i++) sb_byte(sb, ' ');
        Str *out = sb_finish(sb);
        GC_UNROOT(1);
        return out;
    }
    if (kind == TY_INT) {
        unsigned long u = magnitude(value);
        if (value < 0) sign = '-';
        long radix = FS_RADIX(spec) == 1 ? 16 : FS_RADIX(spec) == 2 ? 2 : 10;
        char tmp[70], *e = tmp + sizeof tmp, *p = e;
        do {
            *--p = "0123456789abcdef"[u % (unsigned long)radix];
            u /= (unsigned long)radix;
        } while (u);
        n = e - p;
        mem_copy(body, p, n);
        if (spec & FS_GROUP) n = group3(body, n);
    } else { /* TY_FLOAT */
        unsigned long bits = (unsigned long)value, abs_bits = bits << 1 >> 1;
        int special = (abs_bits >> 52) == 2047;
        if (bits >> 63 && !(special && abs_bits << 12)) sign = '-'; /* NaN prints without a sign, as str */
        if (special) {
            zero_ok = 0;
            n = 3;
            mem_copy(body, (char *)(abs_bits << 12 ? "nan" : "inf"), 3);
        } else if (spec & FS_PREC_ON) {
            long prec = FS_PREC(spec);
            char d[420];
            long k = exact_fixed(abs_bits, prec, d); /* k integer digits, then prec decimals */
            mem_copy(body, d, k);
            n = (spec & FS_GROUP) ? group3(body, k) : k;
            if (prec) {
                body[n++] = '.';
                mem_copy(body + n, d + k, prec);
                n += prec;
            }
        } else { /* str's digits */
            Str *t = float_to_str((long)abs_bits);
            long dot = 0;
            while (dot < t->len && t->data[dot] != '.') dot++;
            mem_copy(body, t->data, dot);
            n = (spec & FS_GROUP) ? group3(body, dot) : dot;
            mem_copy(body + n, t->data + dot, t->len - dot);
            n += t->len - dot;
        }
    }
    if (!sign && (spec & FS_PLUS)) sign = '+';
    long len = n + (sign != 0);
    char out[2200]; /* width (at most 1000) or the number (at most 1,100 bytes) */
    char *w = out;
    long pad = width > len ? width - len : 0;
    if ((spec & FS_ZERO) && zero_ok) { /* zeros between the sign and the digits */
        if (sign) *w++ = sign;
        for (long i = 0; i < pad; i++) *w++ = '0';
        pad = 0;
    }
    long left = align == 1 ? 0 : align == 3 ? pad / 2 : pad;
    for (long i = 0; i < left; i++) *w++ = ' ';
    if (sign && !((spec & FS_ZERO) && zero_ok)) *w++ = sign;
    mem_copy(w, body, n);
    w += n;
    for (long i = left; i < pad; i++) *w++ = ' ';
    return str_new(out, w - out);
}
