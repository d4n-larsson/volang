/* The C math functions the vendored CORE-MATH needs and vo links no C library for (spec 020 research
 * R2, R3). GCC turns some of its builtins into calls when the processor has no instruction for them:
 * fma, fmax, fmin and roundeven on baseline x86-64, floor and round on RISC-V without Zfa. Each is
 * exact (fma rounds once, to nearest even), works on the bits, and gives the same result on every
 * platform. Standalone (no vo.h), so the hosted self-test (tests/math/fma_selftest.c) can build it. */
#include <stdint.h>

#if defined(__x86_64__) && !defined(__FMA__)
double fma(double a, double b, double c);
#endif
double vo_soft_fma(double a, double b, double c);
double floor(double x);
double ceil(double x);
double round(double x);
double roundeven(double x);
double fmax(double a, double b);
double fmin(double a, double b);

/* CORE-MATH's pow sets errno (third_party/shim/errno.h); nothing reads it. */
int vo_cm_errno;

typedef unsigned __int128 u128;

static uint64_t bits_of(double x)
{
    union { double d; uint64_t u; } v = { .d = x };
    return v.u;
}

static double of_bits(uint64_t u)
{
    union { double d; uint64_t u; } v = { .u = u };
    return v.d;
}

#define SIGN (1ULL << 63)
#define FRAC ((1ULL << 52) - 1)

static int is_nan(uint64_t u) { return (u & ~SIGN) > 0x7ff0000000000000ULL; }
static int biased_exp(uint64_t u) { return (int)((u >> 52) & 0x7ff); }

/* x = m * 2^e with m an integer below 2^53, for a finite non-zero x. */
static uint64_t split(uint64_t u, int *e)
{
    int be = biased_exp(u);
    if (!be) {
        *e = -1074;
        return u & FRAC;
    }
    *e = be - 1075;
    return (u & FRAC) | (1ULL << 52);
}

static int bitlen(u128 v)
{
    uint64_t hi = (uint64_t)(v >> 64), lo = (uint64_t)v;
    if (hi) return 128 - __builtin_clzll(hi);
    return lo ? 64 - __builtin_clzll(lo) : 0;
}

/* ponytail: software fma, about 10x slower than vfmadd; cpuid dispatch with a -mfma build of
 * CORE-MATH if SC-006 or the speed gate fails (spec 020 research R3). The product of the two 53-bit
 * significands is exact in 106 bits; both terms are placed at bit 124 of a 128-bit integer, so the
 * product has at least 19 and the addend at least 72 zero bits at the bottom. Aligning the smaller
 * term then loses bits only when it moves 19 or more places, and the lost bits become a sticky bit;
 * a subtraction can cancel many leading bits only when the shift is 0 or 1, which is exact. One
 * rounding to nearest even follows. */
double vo_soft_fma(double a, double b, double c)
{
    uint64_t ua = bits_of(a), ub = bits_of(b), uc = bits_of(c);
    if (is_nan(ua) || is_nan(ub) || is_nan(uc)) return a * b + c;
    int a_inf = biased_exp(ua) == 0x7ff, b_inf = biased_exp(ub) == 0x7ff, c_inf = biased_exp(uc) == 0x7ff;
    if (a_inf || b_inf) return a * b + c; /* inf * 0 is NaN; inf + -inf is NaN; else inf */
    if (c_inf) return c;                  /* a finite product cannot change an infinite addend */
    if (!(ua & ~SIGN) || !(ub & ~SIGN)) return a * b + c; /* exact zero product: IEEE zero signs */
    if (!(uc & ~SIGN)) return a * b;      /* one rounding; the sign of an underflowed product stays */

    int ea, eb, ec;
    uint64_t ma = split(ua, &ea), mb = split(ub, &eb), mc = split(uc, &ec);
    int sp = (int)((ua ^ ub) >> 63), sc = (int)(uc >> 63);
    u128 p = (u128)ma * mb;
    int lp = bitlen(p), lc = bitlen(mc);
    u128 big = p << (125 - lp), small = (u128)mc << (125 - lc);
    int ebig = ea + eb - (125 - lp), esmall = ec - (125 - lc), sbig = sp, ssmall = sc;
    if (esmall > ebig || (esmall == ebig && small > big)) {
        u128 t = big;
        big = small;
        small = t;
        int te = ebig;
        ebig = esmall;
        esmall = te;
        int ts = sbig;
        sbig = ssmall;
        ssmall = ts;
    }
    int d = ebig - esmall;
    if (d >= 128) {
        small = 1; /* sticky only: far below the last kept bit */
    } else if (d > 0) {
        u128 lost = small & (((u128)1 << d) - 1);
        small = (small >> d) | (lost != 0);
    }
    u128 r;
    int sign = sbig;
    if (sbig == ssmall) {
        r = big + small;
    } else {
        r = big - small;
        if (!r) return 0.0; /* an exact zero sum is +0 when rounding to nearest */
    }

    /* Round r * 2^ebig to a double. */
    int lr = bitlen(r);
    int e_top = ebig + lr - 1; /* floor(log2 |value|) */
    int q = (e_top >= -1022 ? e_top - 52 : -1074) ; /* exponent of the last kept bit */
    int s = q - ebig;          /* bits to drop */
    uint64_t mant;
    if (s <= 0) {
        mant = (uint64_t)(r << -s);
    } else if (s >= 128) {
        mant = 0; /* below half the smallest subnormal */
    } else {
        u128 rem = r & (((u128)1 << s) - 1), half = (u128)1 << (s - 1);
        mant = (uint64_t)(r >> s);
        if (rem > half || (rem == half && (mant & 1))) mant++;
    }
    if (mant >> 53) { /* rounding carried to a new binade */
        mant >>= 1;
        q++;
    }
    uint64_t out;
    if (mant >> 52) {
        int biased = q + 52 + 1023;
        if (biased >= 0x7ff) return sign ? -__builtin_inf() : __builtin_inf();
        out = ((uint64_t)biased << 52) | (mant & FRAC);
    } else {
        out = mant; /* subnormal or zero, q is -1074 */
    }
    return of_bits(out | ((uint64_t)sign << 63));
}

/* Only baseline x86-64 lacks the instruction; ARM64 (fmadd) and RISC-V (fmadd.d) never call fma. */
#if defined(__x86_64__) && !defined(__FMA__)
double fma(double a, double b, double c)
{
    return vo_soft_fma(a, b, c);
}
#endif

/* The unbiased exponent of a finite x, and the mask of its fraction bits below the units place. */
static uint64_t frac_mask(uint64_t u, int *e)
{
    *e = biased_exp(u) - 1023;
    return *e < 52 && *e >= 0 ? FRAC >> *e : 0;
}

double floor(double x)
{
    uint64_t u = bits_of(x);
    int e;
    uint64_t m = frac_mask(u, &e);
    if (e >= 52) return x; /* integer, infinite or NaN */
    if (e < 0) {
        if (!(u & ~SIGN)) return x;
        return (u & SIGN) ? -1.0 : 0.0;
    }
    if (!(u & m)) return x;
    if (u & SIGN) u += m;
    return of_bits(u & ~m);
}

double ceil(double x)
{
    uint64_t u = bits_of(x);
    int e;
    uint64_t m = frac_mask(u, &e);
    if (e >= 52) return x;
    if (e < 0) {
        if (!(u & ~SIGN)) return x;
        return (u & SIGN) ? -0.0 : 1.0;
    }
    if (!(u & m)) return x;
    if (!(u & SIGN)) u += m;
    return of_bits(u & ~m);
}

/* Halves away from zero. */
double round(double x)
{
    uint64_t u = bits_of(x);
    int e;
    uint64_t m = frac_mask(u, &e);
    if (e >= 52) return x;
    if (e < 0) return of_bits((u & SIGN) | (e == -1 ? bits_of(1.0) : 0));
    u += (1ULL << 51) >> e; /* add one half in magnitude */
    return of_bits(u & ~m);
}

/* Halves to even. */
double roundeven(double x)
{
    uint64_t u = bits_of(x);
    int e;
    uint64_t m = frac_mask(u, &e);
    if (e >= 52) return x;
    if (e < 0) return of_bits((u & SIGN) | (e == -1 && (u & FRAC) ? bits_of(1.0) : 0));
    uint64_t f = u & m, half = (m + 1) >> 1;
    uint64_t odd = e == 0 ? 1 : (u >> (52 - e)) & 1;
    u &= ~m;
    if (f > half || (f == half && odd)) u += m + 1; /* one unit; a carry moves to the next binade */
    return of_bits(u);
}

/* A NaN operand gives the other operand; -0 counts as less than +0, as fmaxnm (ARM64) and fmax.d
 * (RISC-V) do, so all three platforms agree. */
double fmax(double a, double b)
{
    uint64_t ua = bits_of(a), ub = bits_of(b);
    if (is_nan(ua)) return b;
    if (is_nan(ub)) return a;
    if (a == b) return (ua & SIGN) ? b : a;
    return a > b ? a : b;
}

double fmin(double a, double b)
{
    uint64_t ua = bits_of(a), ub = bits_of(b);
    if (is_nan(ua)) return b;
    if (is_nan(ub)) return a;
    if (a == b) return (ua & SIGN) ? a : b;
    return a < b ? a : b;
}
