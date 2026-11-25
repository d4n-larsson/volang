/* std.math's primitives (spec 020): __math(op, x, y) -> float and __math_int(op, x) -> int, callable
 * only from std/math.vo. The transcendental functions are the vendored CORE-MATH's correctly rounded
 * cr_* functions (third_party/core-math), so every result is the same on every platform; sqrt is the
 * hardware's, which IEEE 754 requires to be correctly rounded. Out-of-domain arguments and poles stop
 * with R0007 at the user's call (research R5); everything else follows IEEE 754 and C Annex F. */
#include "vo.h"

double cr_exp(double x);
double cr_log(double x);
double cr_sin(double x);
double cr_cos(double x);
double cr_tan(double x);
double cr_asin(double x);
double cr_acos(double x);
double cr_atan(double x);
double cr_atan2(double y, double x);
double cr_pow(double x, double y);
double floor(double x); /* src/math_libm.c: exact, by the bits */
double ceil(double x);
double round(double x);

typedef union {
    long i;
    double d;
} Bits;

enum { M_PI_OP, M_SQRT, M_EXP, M_LOG, M_SIN, M_COS, M_TAN, M_ASIN, M_ACOS, M_ATAN, M_ATAN2, M_POW };

/* R0007's message for a NaN result from arguments that are not NaN, by op. */
static char *const domain_msg[] = {
    [M_SQRT] = "`sqrt` needs an argument of at least 0.0, found $1.",
    [M_LOG] = "`log` needs an argument greater than 0.0, found $1.",
    [M_SIN] = "`sin` needs a finite argument, found $1.",
    [M_COS] = "`cos` needs a finite argument, found $1.",
    [M_TAN] = "`tan` needs a finite argument, found $1.",
    [M_ASIN] = "`asin` needs an argument from -1.0 to 1.0, found $1.",
    [M_ACOS] = "`acos` needs an argument from -1.0 to 1.0, found $1.",
    [M_POW] = "`pow` needs a non-negative base or an integer exponent, found $1 and $2.",
};

static long bits_of(double d)
{
    Bits b = {.d = d};
    return b.i;
}

static double double_of(long i)
{
    Bits b = {.i = i};
    return b.d;
}

/* The call in the user's code: a primitive runs inside a std/ function, so the report points at the
 * call of the outermost standard-library function instead (the frame's call node). */
Node *std_user_call(Node *node)
{
    for (Frame *f = (Frame *)cur_frame; f && f->fn && f->call; f = f->parent) {
        if (!((FileRec *)file_get((int)f->fn->file))->std) break;
        node = f->call;
    }
    return node;
}

static void raise_math(char *tmpl, long x, long y, int two, Node *node)
{
    Str *a = float_to_str(x), *b = 0;
    GC_ROOT(&a); /* (rt_raise does not return: no unroot) */
    if (two) b = float_to_str(y);
    rt_raise("R0007", msg(tmpl, a, b, 0), std_user_call(node));
}

long math_call(long op, long xb, long yb, Node *node)
{
    double x = double_of(xb), y = double_of(yb), r;
    switch (op) {
    case M_PI_OP: return bits_of(0x1.921fb54442d18p+1);
    case M_SQRT: r = __builtin_sqrt(x); break;
    case M_EXP: r = cr_exp(x); break;
    case M_LOG: r = cr_log(x); break;
    case M_SIN: r = cr_sin(x); break;
    case M_COS: r = cr_cos(x); break;
    case M_TAN: r = cr_tan(x); break;
    case M_ASIN: r = cr_asin(x); break;
    case M_ACOS: r = cr_acos(x); break;
    case M_ATAN: r = cr_atan(x); break;
    case M_ATAN2: r = cr_atan2(x, y); break; /* __math(10, y, x): C's argument order */
    case M_POW: r = cr_pow(x, y); break;
    default: rt_raise_c("R0007", "Unknown math operation.", node); return 0;
    }
    if (r != r && x == x && y == y) raise_math(domain_msg[op], xb, yb, op == M_POW, node);
    /* The two poles, where IEEE gives an infinity instead of NaN. */
    if (op == M_LOG && x == 0.0) raise_math(domain_msg[M_LOG], xb, 0, 0, node);
    if (op == M_POW && x == 0.0 && y < 0.0)
        raise_math("`pow` of $1 needs an exponent of at least 0.0, found $2.", xb, yb, 1, node);
    return bits_of(r);
}

/* floor, ceil or round (halves away from zero) of x as an int; R0007 for NaN, an infinity or a
 * result outside the int range. Rounded as a double first (exact), then checked, then converted. */
long math_int(long op, long xb, Node *node)
{
    static char *const tmpl[] = {0, "`floor` needs a finite value within the int range, found $1.",
                                 "`ceil` needs a finite value within the int range, found $1.",
                                 "`round` needs a finite value within the int range, found $1."};
    double x = double_of(xb);
    double r = op == 1 ? floor(x) : op == 2 ? ceil(x) : round(x);
    if (r == r && r >= -9223372036854775808.0 && r < 9223372036854775808.0) return (long)r;
    raise_math(tmpl[op], xb, 0, 0, node);
    return 0;
}

/* std.bits's primitive (spec 043): __bits(op, a, b) on the 64-bit two's complement form of an int.
 * All of it is unsigned arithmetic, so nothing here is undefined or implementation-defined in C: no
 * signed shift, no signed overflow, no shift by 64. A count outside 0 to 63 is R0007 at the user's call. */
long bits_op(long op, long a, long b, Node *node)
{
    unsigned long x = (unsigned long)a, y = (unsigned long)b, r;
    if (op >= 5 && op <= 9 && y > 63)
        rt_raise("R0007", msg("A shift count must be 0 to 63, found $1.", int_to_str(b), 0, 0), std_user_call(node));
    switch (op) {
    case 1: r = x & y; break;
    case 2: r = x | y; break;
    case 3: r = x ^ y; break;
    case 4: r = ~x; break;
    case 5: r = x << y; break;
    case 6: r = x >> y; /* arithmetic: the sign bit fills the top */
        if (a < 0 && y) r |= ~0UL << (64 - y);
        break;
    case 7: r = x >> y; break;
    case 8: r = y ? x << y | x >> (64 - y) : x; break;
    case 9: r = y ? x >> y | x << (64 - y) : x; break;
    case 10: r = x + y; break;
    case 11: r = x - y; break;
    case 12: r = x * y; break;
    default: rt_raise_c("R0007", "Unknown bits operation.", node); return 0;
    }
    return (long)r;
}
