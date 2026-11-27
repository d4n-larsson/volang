/* Runtime primitives of the standard library (spec 007 research R5; ported from src/stdprim.s,
 * spec 008). They are builtins whose names start with `__`, callable only from files under std/
 * (std_only_ok, checked in check_call). */
#include "vo.h"

/* 0 when name is a `__` primitive and node is not in a standard-library file; else 1. */
long std_only_ok(Str *name, Node *node)
{
    if (name->len < 2 || name->data[0] != '_' || name->data[1] != '_') return 1;
    return ((FileRec *)file_get(node->file))->std;
}

/* ---- exact decimal conversion for std.json ----
 * The language's own float text (literals, parse_float, str) takes the fast paths of
 * text_to_float/float_to_text. JSON needs 17 digits both ways and exact round trips, so these use
 * big integers: fixed 40-limb (2560-bit) little-endian numbers, enough for 2^1074 * 10^324. */
#define BN_LIMBS 40
typedef unsigned long BigNum[BN_LIMBS];

static BigNum bn_n, bn_d, bn_c;

static void bn_set(unsigned long *a, unsigned long v)
{
    for (int i = 0; i < BN_LIMBS; i++) a[i] = 0;
    a[0] = v;
}

static void bn_copy(unsigned long *dst, const unsigned long *src)
{
    for (int i = 0; i < BN_LIMBS; i++) dst[i] = src[i];
}

/* a *= k (k below 2^32; the result must fit). */
static void bn_mul_small(unsigned long *a, unsigned long k)
{
    unsigned long carry = 0;
    for (int i = 0; i < BN_LIMBS; i++) {
        unsigned __int128 p = (unsigned __int128)a[i] * k + carry;
        a[i] = (unsigned long)p;
        carry = (unsigned long)(p >> 64);
    }
}

/* a <<= n bits. */
static void bn_shl(unsigned long *a, long n)
{
    long limbs = n >> 6;
    int bits = (int)(n & 63);
    for (long i = BN_LIMBS - 1; i >= 0; i--) {
        long src = i - limbs;
        unsigned long v = 0;
        if (src >= 0) {
            v = a[src] << bits;
            if (bits && src >= 1) v |= a[src - 1] >> (64 - bits);
        }
        a[i] = v;
    }
}

static void bn_shr1(unsigned long *a)
{
    for (int i = 0; i < BN_LIMBS - 1; i++) a[i] = (a[i] >> 1) | (a[i + 1] << 63);
    a[BN_LIMBS - 1] >>= 1;
}

/* -1, 0 or 1. */
static int bn_cmp(const unsigned long *a, const unsigned long *b)
{
    for (int i = BN_LIMBS - 1; i >= 0; i--) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

/* a -= b (a >= b). */
static void bn_sub(unsigned long *a, const unsigned long *b)
{
    unsigned long borrow = 0;
    for (int i = 0; i < BN_LIMBS; i++) {
        unsigned long x = a[i], y = b[i];
        unsigned long d = x - y;
        unsigned long b1 = x < y;
        unsigned long b2 = d < borrow;
        a[i] = d - borrow;
        borrow = b1 | b2;
    }
}

/* The number of significant bits (0 for zero). */
static long bn_bitlen(const unsigned long *a)
{
    for (int i = BN_LIMBS - 1; i >= 0; i--) {
        if (a[i]) {
            long n = (long)i * 64;
            for (unsigned long v = a[i]; v; v >>= 1) n++;
            return n;
        }
    }
    return 0;
}

/* a *= base^n, one factor at a time (base 5 or 10). */
static void bn_pow_mul(unsigned long *a, unsigned long base, long n)
{
    for (; n > 0; n--) bn_mul_small(a, base);
}

static long bitlen64(unsigned long v)
{
    long n = 0;
    for (; v; v >>= 1) n++;
    return n;
}

/* floor(bn_n / bn_d) when it fits 64 bits; bn_n keeps the remainder. */
static unsigned long bn_div_small_q(void)
{
    long t = bn_bitlen(bn_n) - bn_bitlen(bn_d); /* the quotient has at most t + 1 bits */
    bn_copy(bn_c, bn_d);
    unsigned long q = 0;
    if (t < 0) return 0;
    bn_shl(bn_c, t);
    for (; t >= 0; t--) {
        q <<= 1;
        if (bn_cmp(bn_n, bn_c) >= 0) {
            bn_sub(bn_n, bn_c);
            q |= 1;
        }
        bn_shr1(bn_c);
    }
    return q;
}

/* exact_to_double in 128-bit integers, for w below 2^57 (17 digits) and 10^q within 10^21 either way:
 * the results are normal doubles. 0 when the operands do not fit. */
static int near_u128(unsigned long w, long q, unsigned long *out)
{
    typedef unsigned __int128 u128;
    if (!w || w >> 57 || q > 21 || q < -21) return 0;
    u128 n = w, ten = 1;
    for (long i = q < 0 ? -q : q; i > 0; i--) ten *= 10;
    long e2 = 0; /* the value is n * 2^e2, plus a little when sticky */
    int sticky = 0;
    if (q >= 0) {
        n *= ten; /* below 2^57 * 2^70 */
    } else {
        long tl = 0;
        for (u128 t = ten; t; t >>= 1) tl++;
        long s = 56 + tl - bitlen64(w); /* the quotient has 55 bits or more; w << s is below 2^(56 + 70) */
        n <<= s;
        sticky = n % ten != 0;
        n /= ten;
        e2 = -s;
    }
    long len = 0;
    for (u128 t = n; t; t >>= 1) len++;
    unsigned long mant;
    if (len > 53) {
        long drop = len - 53;
        u128 rest = n & (((u128)1 << drop) - 1), half = (u128)1 << (drop - 1);
        mant = (unsigned long)(n >> drop);
        e2 += drop;
        if (rest > half || (rest == half && (sticky || (mant & 1)))) mant++;
        if (mant >> 53) {
            mant >>= 1;
            e2++;
        }
    } else { /* exact (q >= 0) */
        mant = (unsigned long)n << (53 - len);
        e2 -= 53 - len;
    }
    long biased = e2 + 1075;
    if (biased < 1 || biased > 2046) return 0;
    *out = (unsigned long)biased << 52 | (mant & ((1UL << 52) - 1));
    return 1;
}

/* The double nearest to w * 10^q (ties to even), as bits; above the range: infinity. Big integers
 * only where 128 bits do not hold the operands (about 3 us against 30 ns). */
static unsigned long bn_n_to_double(long q);

unsigned long exact_to_double(unsigned long w, long q)
{
    const unsigned long inf = 2047UL << 52;
    unsigned long fast;
    if (!(w >> 53) && q >= -22 && q <= 22) { /* both operands exact doubles: one rounded operation */
        static const double ten[23] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
                                       1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
        union {
            double d;
            unsigned long u;
        } x = {.d = q >= 0 ? (double)w * ten[q] : (double)w / ten[-q]};
        return x.u;
    }
    if (near_u128(w, q, &fast)) return fast;
    if (w == 0 || q < -345) return 0;
    if (q > 310) return inf;
    bn_set(bn_n, w);
    return bn_n_to_double(q);
}

/* The digits d[0..nd) (ASCII, the first not '0', nd at most 700) times 10^q, as the nearest double: for
 * texts of more digits than a word holds (std.json; cut to 19 digits they rounded the wrong way near a
 * halfway point). 700 digits and the powers of five below fit the 40 limbs. */
unsigned long exact_digits_to_double(char *d, long nd, long q)
{
    if (nd + q > 310) return 2047UL << 52;
    if (nd + q < -345) return 0;
    bn_set(bn_n, 0);
    for (long i = 0; i < nd; i++) {
        bn_mul_small(bn_n, 10);
        unsigned long carry = (unsigned long)(d[i] - '0');
        for (int k = 0; carry && k < BN_LIMBS; k++) {
            bn_n[k] += carry;
            carry = bn_n[k] < carry;
        }
    }
    return bn_n_to_double(q);
}

/* bn_n times 10^q as the nearest double (ties to even); bn_n is not zero. */
static unsigned long bn_n_to_double(long q)
{
    const unsigned long inf = 2047UL << 52;
    bn_set(bn_d, 1); /* N / D = n * 10^q, scaled by 2^p */
    long p = q; /* the power of two: 10^q = 5^q * 2^q */
    if (q >= 0)
        bn_pow_mul(bn_n, 5, q);
    else
        bn_pow_mul(bn_d, 5, -q);
    long s = 55 + bn_bitlen(bn_d) - bn_bitlen(bn_n); /* so that N / D has 55 or 56 bits */
    p -= s;
    if (s >= 0)
        bn_shl(bn_n, s);
    else
        bn_shl(bn_d, -s);
    unsigned long quot = bn_div_small_q();
    int sticky = bn_bitlen(bn_n) != 0; /* the remainder is not zero */
    long bl = bitlen64(quot);
    long drop = bl - 53;
    long e = p + drop; /* the exponent of the kept mantissa */
    if (e < -1074) { /* a subnormal: drop more */
        drop += -1074 - e;
        e = -1074;
    }
    if (drop > bl) return 0; /* below half the smallest subnormal */
    unsigned long dropped = quot & ((1UL << drop) - 1);
    unsigned long mant = quot >> drop;
    unsigned long half = 1UL << (drop - 1);
    if (dropped > half || (dropped == half && (sticky || (mant & 1)))) {
        mant++;
        if (mant == 1UL << 53) {
            mant >>= 1;
            e++;
        }
    }
    if (mant < 1UL << 52) return mant; /* subnormal */
    long biased = e + 1075;
    if (biased >= 2047) return inf;
    return (mant - (1UL << 52)) | (unsigned long)biased << 52;
}

/* floor(x / 2^18) without relying on the right shift of a negative number (research R13). */
static long floor_div_2_18(long x)
{
    return x >= 0 ? x >> 18 : -((-x + (1L << 18) - 1) >> 18);
}

/* The n significant decimal digits of |x| (x finite, not zero), rounded to nearest with ties to
 * even, written to out as ASCII; returns k with |x| about d1.d2...dn * 10^k. */
long exact_digits(unsigned long bits, long n, char *out)
{
    long biased = (long)((bits >> 52) & 2047);
    unsigned long m = bits & ((1UL << 52) - 1); /* m * 2^e */
    long e = -1074;
    if (biased) {
        m |= 1UL << 52;
        e = biased - 1075;
    }
    bn_set(bn_n, m);
    bn_set(bn_d, 1);
    if (e >= 0)
        bn_shl(bn_n, e);
    else
        bn_shl(bn_d, -e);
    long k = floor_div_2_18((bitlen64(m) - 1 + e) * 78913); /* log10(2) * 2^18, then corrected */
    if (k >= 0)
        bn_pow_mul(bn_d, 10, k);
    else
        bn_pow_mul(bn_n, 10, -k);
    for (;;) { /* N >= 10 D: one more */
        bn_copy(bn_c, bn_d);
        bn_mul_small(bn_c, 10);
        if (bn_cmp(bn_n, bn_c) < 0) break;
        bn_mul_small(bn_d, 10);
        k++;
    }
    while (bn_cmp(bn_n, bn_d) < 0) { /* N < D: one less */
        bn_mul_small(bn_n, 10);
        k--;
    }
    for (long i = 0; i < n; i++) {
        int d = 0;
        while (bn_cmp(bn_n, bn_d) >= 0) {
            bn_sub(bn_n, bn_d);
            d++;
        }
        out[i] = (char)('0' + d);
        bn_mul_small(bn_n, 10);
    }
    bn_copy(bn_c, bn_d); /* round: 10 R against 5 D (N is 10 R now) */
    bn_mul_small(bn_c, 5);
    int c = bn_cmp(bn_n, bn_c);
    if (c > 0 || (c == 0 && ((out[n - 1] - '0') & 1))) {
        long i = n - 1;
        while (i >= 0 && out[i] == '9') out[i--] = '0';
        if (i >= 0) {
            out[i]++;
        } else { /* 99..9 rounded up: 10..0, one more power of ten */
            out[0] = '1';
            k++;
        }
    }
    return k;
}

/* Spec 024 (research R3): |x| (x finite) with exactly prec decimals: the exact value rounded at
 * 10^-prec, ties to even. Writes the integer digits (at least one) then the prec decimals, with no
 * point, to out (room: 310 + prec); returns the number of integer digits.
 * ponytail: big integers for every value, about 2.4 us per `{:.2f}` (tests/format_speed.py); upgrade:
 * a 128-bit path for |x| < 2^64 and prec <= 19, as digits15_u128 does for str. */
long exact_fixed(unsigned long bits, long prec, char *out)
{
    long biased = (long)((bits >> 52) & 2047);
    unsigned long m = bits & ((1UL << 52) - 1); /* m * 2^e */
    long e = -1074;
    if (biased) {
        m |= 1UL << 52;
        e = biased - 1075;
    }
    bn_set(bn_n, m);
    bn_set(bn_d, 1);
    if (e >= 0)
        bn_shl(bn_n, e);
    else
        bn_shl(bn_d, -e);
    long k = 1; /* integer digits: D is scaled by 10^(k-1) so that N < 10 D */
    for (;;) {
        bn_copy(bn_c, bn_d);
        bn_mul_small(bn_c, 10);
        if (bn_cmp(bn_n, bn_c) < 0) break;
        bn_mul_small(bn_d, 10);
        k++;
    }
    long n = k + prec;
    for (long i = 0; i < n; i++) { /* each digit: N / D (below 10), then N = 10 (N mod D) */
        int d = 0;
        while (bn_cmp(bn_n, bn_d) >= 0) {
            bn_sub(bn_n, bn_d);
            d++;
        }
        out[i] = (char)('0' + d);
        bn_mul_small(bn_n, 10);
    }
    bn_copy(bn_c, bn_d); /* round: 10 R against 5 D */
    bn_mul_small(bn_c, 5);
    int c = bn_cmp(bn_n, bn_c);
    if (c > 0 || (c == 0 && ((out[n - 1] - '0') & 1))) {
        long i = n - 1;
        while (i >= 0 && out[i] == '9') out[i--] = '0';
        if (i >= 0) {
            out[i]++;
        } else { /* 99.99 rounded up: one more integer digit */
            for (long j = n; j > 0; j--) out[j] = out[j - 1];
            out[0] = '1';
            k++;
        }
    }
    return k;
}

/* __text_float: "-"? digits ["." digits] with at most 19 significant digits, to the nearest
 * double (exact_to_double). */
long std_text_float(Str *s)
{
    char *p = s->data, *end = s->data + s->len;
    unsigned long sign = 0;
    if (p < end && *p == '-') {
        sign = 1UL << 63;
        p++;
    }
    char *last = 0; /* the last non-zero digit */
    for (char *q = p; q < end; q++)
        if (*q >= '1' && *q <= '9') last = q;
    unsigned long w = 0;
    long e = 0;
    int past = 0; /* past '.' */
    for (char *q = p; q < end; q++) {
        if (*q == '.') {
            past = 1;
            continue;
        }
        if (last && q <= last) {
            w = w * 10 + (unsigned long)(*q - '0');
            e -= past; /* a fraction digit */
        } else if (!past) {
            e++; /* trailing zeros of the integer part: a power of ten */
        }
    }
    return (long)(exact_to_double(w, e) | sign);
}

/* __float_text: a float as str writes it, the shortest text that reads back as the same double, in
 * plain decimal ("0.000025", "100.0"). std.json refuses NaN and the infinities before it gets here. */
Str *std_float_text(long bits)
{
    return float_to_str(bits);
}
