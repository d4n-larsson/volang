#ifndef VO_SHIM_ERRNO_H
#define VO_SHIM_ERRNO_H
/* Spec 020: CORE-MATH's pow sets errno on overflow and underflow; vo never reads it. A name of vo's
 * own (defined in src/math_libm.c), so the hosted build never meets the C library's errno. */
extern int vo_cm_errno;
#define errno vo_cm_errno
#define EDOM 33
#define ERANGE 34
#endif
