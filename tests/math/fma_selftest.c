/* Spec 020 (research R3, task T008): src/math_libm.c's software fma and rounding functions against
 * the x86-64 instructions (vfmadd, roundsd), bit for bit. Hosted, built by `make math-selftest` with
 * -mfma -msse4.1, linked with src/math_libm.c. Exits 1 on any mismatch; skips when the CPU has no
 * FMA. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

double vo_soft_fma(double a, double b, double c);
double floor(double x);
double ceil(double x);
double round(double x);
double roundeven(double x);
double fmax(double a, double b);
double fmin(double a, double b);

static uint64_t bits(double x)
{
    uint64_t u;
    memcpy(&u, &x, 8);
    return u;
}

static double dbl(uint64_t u)
{
    double x;
    memcpy(&x, &u, 8);
    return x;
}

/* Two NaNs are equal whatever their payloads; everything else bit for bit. */
static int same(double a, double b)
{
    if (a != a && b != b) return 1;
    return bits(a) == bits(b);
}

static uint64_t state = 0x9e3779b97f4a7c15ULL;
static uint64_t next(void)
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

static long checked, failed;

static void check_fma(double a, double b, double c)
{
    double want = __builtin_fma(a, b, c), got = vo_soft_fma(a, b, c);
    checked++;
    if (!same(want, got) && failed++ < 10)
        printf("fma(%a, %a, %a): want %a, got %a\n", a, b, c, want, got);
}

static void check1(const char *name, double x, double want, double got)
{
    checked++;
    if (!same(want, got) && failed++ < 10) printf("%s(%a): want %a, got %a\n", name, x, want, got);
}

static double ref_round(double x)
{
    double t = __builtin_trunc(x);
    if (__builtin_fabs(x - t) >= 0.5) t += __builtin_copysign(1.0, x);
    return __builtin_copysign(t, x);
}

/* A double with a random fraction and an exponent in lo..hi (biased), random sign. */
static double rand_in(int lo, int hi)
{
    uint64_t r = next();
    uint64_t e = (uint64_t)(lo + (int)(next() % (uint64_t)(hi - lo + 1)));
    return dbl((r & 0x800fffffffffffffULL) | (e << 52));
}

int main(void)
{
    if (!__builtin_cpu_supports("fma") || !__builtin_cpu_supports("sse4.1")) {
        printf("math-selftest: skipped (this CPU has no FMA or SSE4.1)\n");
        return 0;
    }
    static const uint64_t edge_bits[] = {
        0, 1, 2, 0x000fffffffffffffULL, 0x0010000000000000ULL, 0x0010000000000001ULL,
        0x3fefffffffffffffULL, 0x3ff0000000000000ULL, 0x3ff0000000000001ULL, 0x3fe0000000000000ULL,
        0x4000000000000000ULL, 0x4330000000000000ULL, 0x4330000000000001ULL, 0x3cb0000000000000ULL,
        0x7fefffffffffffffULL, 0x7fe0000000000000ULL, 0x7ff0000000000000ULL, 0x7ff8000000000000ULL,
        0x1ff0000000000000ULL, 0x5ff0000000000000ULL, 0x2000000000000000ULL, 0x0360000000000000ULL,
        0x3ca0000000000000ULL, 0x3fd5555555555555ULL, 0x400921fb54442d18ULL,
    };
    int n_edge = (int)(sizeof edge_bits / sizeof edge_bits[0]);
    double edge[64];
    for (int i = 0; i < n_edge; i++) {
        edge[2 * i] = dbl(edge_bits[i]);
        edge[2 * i + 1] = dbl(edge_bits[i] | (1ULL << 63));
    }
    int ne = 2 * n_edge;

    /* 1. every triple of edge values */
    for (int i = 0; i < ne; i++)
        for (int j = 0; j < ne; j++)
            for (int k = 0; k < ne; k++) check_fma(edge[i], edge[j], edge[k]);

    /* 2. random bit patterns */
    for (long i = 0; i < 30000000; i++) check_fma(dbl(next()), dbl(next()), dbl(next()));

    /* 3. near cancellation: c close to -(a*b), and exponents that overlap */
    for (long i = 0; i < 30000000; i++) {
        double a = rand_in(900, 1150), b = rand_in(900, 1150);
        double p = a * b;
        uint64_t pc = bits(-p) + (next() % 9) - 4;
        check_fma(a, b, (i & 1) ? dbl(pc) : rand_in(800, 1250));
    }

    /* 4. results near and below the subnormal range, and near overflow */
    for (long i = 0; i < 20000000; i++) {
        double a = rand_in(0, 600), b = rand_in(0, 600), c = rand_in(0, 120);
        check_fma(a, b, c);
        a = rand_in(1500, 2046);
        b = rand_in(500, 560);
        check_fma(a, b, -rand_in(2000, 2046));
    }

    /* 5. small integers and simple fractions (exact sums, signed zeros) */
    for (int i = -20; i <= 20; i++)
        for (int j = -20; j <= 20; j++)
            for (int k = -20; k <= 20; k++) check_fma(i * 0.25, j * 0.5, k * 0.125);

    /* 6. the rounding functions */
    for (long i = 0; i < 20000000; i++) {
        double x = (i & 1) ? dbl(next()) : rand_in(1000, 1080);
        if (i % 4 == 2) x = (double)((int64_t)(next() % 4000) - 2000) * 0.25;
        check1("floor", x, __builtin_floor(x), floor(x));
        check1("ceil", x, __builtin_ceil(x), ceil(x));
        check1("roundeven", x, __builtin_roundeven(x), roundeven(x));
        check1("round", x, ref_round(x), round(x));
    }
    for (int i = 0; i < ne; i++) {
        double x = edge[i];
        check1("floor", x, __builtin_floor(x), floor(x));
        check1("ceil", x, __builtin_ceil(x), ceil(x));
        check1("roundeven", x, __builtin_roundeven(x), roundeven(x));
        check1("round", x, ref_round(x), round(x));
        for (int j = 0; j < ne; j++) {
            double y = edge[j];
            double mx = x != x ? y : y != y ? x : x > y ? x : x < y ? y : (bits(x) >> 63) ? y : x;
            double mn = x != x ? y : y != y ? x : x < y ? x : x > y ? y : (bits(x) >> 63) ? x : y;
            checked += 2;
            if (!same(mx, fmax(x, y)) && failed++ < 10) printf("fmax(%a, %a): got %a\n", x, y, fmax(x, y));
            if (!same(mn, fmin(x, y)) && failed++ < 10) printf("fmin(%a, %a): got %a\n", x, y, fmin(x, y));
        }
    }

    printf("math-selftest: %ld checks, %ld mismatches\n", checked, failed);
    return failed != 0;
}
