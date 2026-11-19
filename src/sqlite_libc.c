/* The C library functions SQLite calls that vo did not provide yet (spec 026 research R3), by their
 * standard names. Strings and characters: ASCII, no locale (the instrumented build uses the C
 * library's). Math: CORE-MATH's correctly rounded functions (third_party/core-math), or the exact
 * operation (fabs, trunc, fmod, sqrt), so SQL math gives the same bits on every platform, as std.math. */
#include "vo.h"

#ifndef VO_HOSTED
int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int isxdigit(int c);
int isupper(int c);
int islower(int c);
int isprint(int c);
int tolower(int c);
int toupper(int c);
char *strrchr(const char *s, int c);
int strncmp(const char *a, const char *b, size_t n);
void *memchr(const void *s, int c, size_t n);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);

int isspace(int c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

int isupper(int c)
{
    return c >= 'A' && c <= 'Z';
}

int islower(int c)
{
    return c >= 'a' && c <= 'z';
}

int isalpha(int c)
{
    return isupper(c) || islower(c);
}

int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

int isxdigit(int c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int isprint(int c)
{
    return c >= 0x20 && c < 0x7f;
}

int tolower(int c)
{
    return isupper(c) ? c + 32 : c;
}

int toupper(int c)
{
    return islower(c) ? c - 32 : c;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (; n; n--, a++, b++) {
        int d = (unsigned char)*a - (unsigned char)*b;
        if (d || !*a) return d;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    for (; n; n--, p++)
        if (*p == (unsigned char)c) return (void *)p;
    return 0;
}

char *strrchr(const char *s, int c)
{
    const char *last = 0;
    for (;; s++) {
        if (*s == (char)c) last = s;
        if (!*s) return (char *)last;
    }
}

size_t strspn(const char *s, const char *accept)
{
    size_t n = 0;
    for (; s[n]; n++) {
        const char *a = accept;
        while (*a && *a != s[n]) a++;
        if (!*a) break;
    }
    return n;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t n = 0;
    for (; s[n]; n++) {
        const char *r = reject;
        while (*r && *r != s[n]) r++;
        if (*r) break;
    }
    return n;
}
#endif

double cr_exp(double x);
double cr_log(double x);
double cr_log10(double x);
double cr_log2(double x);
double cr_pow(double x, double y);
double cr_sin(double x);
double cr_cos(double x);
double cr_tan(double x);
double cr_asin(double x);
double cr_acos(double x);
double cr_atan(double x);
double cr_atan2(double y, double x);
double cr_sinh(double x);
double cr_cosh(double x);
double cr_tanh(double x);
double cr_asinh(double x);
double cr_acosh(double x);
double cr_atanh(double x);
double fabs(double x);
double trunc(double x);
double fmod(double x, double y);
double sqrt(double x);
double exp(double x);
double log(double x);
double log10(double x);
double log2(double x);
double pow(double x, double y);
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double sinh(double x);
double cosh(double x);
double tanh(double x);
double asinh(double x);
double acosh(double x);
double atanh(double x);

typedef union {
    double f;
    uint64_t i;
} DBits;

double fabs(double x)
{
    DBits u = {x};
    u.i &= ~(1ULL << 63);
    return u.f;
}

double trunc(double x)
{
    DBits u = {x};
    int e = (int)(u.i >> 52 & 0x7ff) - 0x3ff + 12;
    if (e >= 52 + 12) return x;
    if (e < 12) e = 1;
    uint64_t m = ~0ULL >> e;
    if ((u.i & m) == 0) return x;
    u.i &= ~m;
    return u.f;
}

/* x - n * y with n = trunc(x / y), exactly (the bit-by-bit long division of musl's fmod). */
double fmod(double x, double y)
{
    DBits ux = {x}, uy = {y};
    int ex = (int)(ux.i >> 52 & 0x7ff), ey = (int)(uy.i >> 52 & 0x7ff);
    uint64_t sx = ux.i >> 63, i, uxi = ux.i;
    if (uy.i << 1 == 0 || y != y || ex == 0x7ff) return (x * y) / (x * y);
    if (uxi << 1 <= uy.i << 1) {
        if (uxi << 1 == uy.i << 1) return 0 * x;
        return x;
    }
    if (!ex) {
        for (i = uxi << 12; i >> 63 == 0; ex--, i <<= 1) {}
        uxi <<= -ex + 1;
    } else {
        uxi &= ~0ULL >> 12;
        uxi |= 1ULL << 52;
    }
    if (!ey) {
        for (i = uy.i << 12; i >> 63 == 0; ey--, i <<= 1) {}
        uy.i <<= -ey + 1;
    } else {
        uy.i &= ~0ULL >> 12;
        uy.i |= 1ULL << 52;
    }
    for (; ex > ey; ex--) {
        i = uxi - uy.i;
        if (i >> 63 == 0) {
            if (i == 0) return 0 * x;
            uxi = i;
        }
        uxi <<= 1;
    }
    i = uxi - uy.i;
    if (i >> 63 == 0) {
        if (i == 0) return 0 * x;
        uxi = i;
    }
    for (; uxi >> 52 == 0; uxi <<= 1, ex--) {}
    if (ex > 0) {
        uxi -= 1ULL << 52;
        uxi |= (uint64_t)ex << 52;
    } else {
        uxi >>= -ex + 1;
    }
    ux.i = uxi | sx << 63;
    return ux.f;
}

double sqrt(double x)
{
    return __builtin_sqrt(x); /* the IEEE square root instruction: correctly rounded everywhere */
}

double exp(double x) { return cr_exp(x); }
double log(double x) { return cr_log(x); }
double log10(double x) { return cr_log10(x); }
double log2(double x) { return cr_log2(x); }
double pow(double x, double y) { return cr_pow(x, y); }
double sin(double x) { return cr_sin(x); }
double cos(double x) { return cr_cos(x); }
double tan(double x) { return cr_tan(x); }
double asin(double x) { return cr_asin(x); }
double acos(double x) { return cr_acos(x); }
double atan(double x) { return cr_atan(x); }
double atan2(double y, double x) { return cr_atan2(y, x); }
double sinh(double x) { return cr_sinh(x); }
double cosh(double x) { return cr_cosh(x); }
double tanh(double x) { return cr_tanh(x); }
double asinh(double x) { return cr_asinh(x); }
double acosh(double x) { return cr_acosh(x); }
double atanh(double x) { return cr_atanh(x); }
