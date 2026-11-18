#ifndef VO_SHIM_FENV_H
#define VO_SHIM_FENV_H
/* Spec 020 (research R2): CORE-MATH reads the rounding mode and raises exception flags. vo never
 * changes the rounding mode (always to nearest) and never reads the flags, so the mode is constant
 * and the flag functions do nothing. */
#define FE_TONEAREST 0
#define FE_DOWNWARD 1
#define FE_UPWARD 2
#define FE_TOWARDZERO 3
#define FE_INVALID 1
#define FE_DIVBYZERO 2
#define FE_OVERFLOW 4
#define FE_UNDERFLOW 8
#define FE_INEXACT 16
#define FE_ALL_EXCEPT 31
typedef struct { unsigned v; } fenv_t;
typedef unsigned fexcept_t;
static inline int fegetround(void) { return FE_TONEAREST; }
static inline int feraiseexcept(int e) { (void)e; return 0; }
static inline int feclearexcept(int e) { (void)e; return 0; }
static inline int fetestexcept(int e) { (void)e; return 0; }
static inline int feholdexcept(fenv_t *env) { env->v = 0; return 0; }
static inline int feupdateenv(const fenv_t *env) { (void)env; return 0; }
static inline int fegetexceptflag(fexcept_t *f, int e) { (void)e; *f = 0; return 0; }
static inline int fesetexceptflag(const fexcept_t *f, int e) { (void)f; (void)e; return 0; }
#endif
