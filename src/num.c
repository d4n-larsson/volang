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

/* Decimal text ("-"? digits ["." digits]) to f64 bits. Digits up to the last non-zero one form an
 * exact mantissa (at most 15 significant digits); trailing zeros only move the power of ten. */
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
    double x = (double)(long)mant;
    if (negative) x = -x;
    return scale10(double_to_bits(x), power);
}

/* Validates and converts text for parse_float: 0 = ok, 2 = malformed.
 * Accepts "-"? digits ["." digits] with at most 15 significant digits. */
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
    if (significant > 15) return 2;
    *out = text_to_float(p, n);
    return 0;
}

/* Round to nearest, ties to even, as RISC-V `fcvt.l.d ... rne` (research R13): no libm, and no
 * SSE4.1 on x86-64. Below 2^52, adding and subtracting 2^52 rounds in the default rounding mode
 * (ties to even); from 2^52 on, every double is an integer already. */
static long round_half_even(double v)
{
    const double two52 = 4503599627370496.0;
    if (v >= two52 || v <= -two52) return (long)v;
    double t = v >= 0 ? (v + two52) - two52 : (v - two52) + two52;
    return (long)t;
}

/* ponytail: 15 significant digits, not shortest round-trip; upgrade: Ryu port */
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
    double a = bits_to_double((long)abs_bits); /* e = floor(log10(|x|)) by repeated scaling */
    long e = 0;
    while (10.0 <= a) {
        a /= 10.0;
        e++;
    }
    while (a < 1.0) {
        a *= 10.0;
        e--;
    }
    long digits;
    long tries = 0; /* correction rounds */
    for (;;) {
        digits = round_half_even(bits_to_double(scale10((long)abs_bits, 14 - e)));
        if (tries >= 3) break;
        tries++;
        if (digits < 100000000000000L) {
            e--;
            continue;
        }
        if (digits >= 1000000000000000L) {
            e++;
            continue;
        }
        break;
    }
    char d[15];
    for (int i = 14; i >= 0; i--) { /* 15 digits of the value, as the assembly (signed division) */
        d[i] = (char)('0' + digits % 10);
        digits /= 10;
    }
    if (e >= 0) {
        long i = 0;
        for (; i <= e; i++) *w++ = i < 15 ? d[i] : '0'; /* the integer part, padded with zeros */
        *w++ = '.';
        frac = w;
        for (; i < 15; i++) *w++ = d[i];
        if (w == frac) *w++ = '0';
    } else {
        *w++ = '0'; /* "0." then -e-1 zeros, then the digits */
        *w++ = '.';
        frac = w;
        for (long z = -e - 1; z > 0; z--) *w++ = '0';
        for (int i = 0; i < 15; i++) *w++ = d[i];
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
