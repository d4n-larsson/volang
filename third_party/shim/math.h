#ifndef VO_SHIM_MATH_H
#define VO_SHIM_MATH_H
/* Spec 020: CORE-MATH's pow.h includes <math.h> for NAN and INFINITY only. */
#define NAN __builtin_nan("")
#define INFINITY __builtin_inf()
/* Spec 026: SQLite's math functions, by their standard names (src/sqlite_libc.c: CORE-MATH, or exact). */
double fabs(double x);
double trunc(double x);
double floor(double x);
double ceil(double x);
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
#endif
