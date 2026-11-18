#ifndef VO_SHIM_MATH_H
#define VO_SHIM_MATH_H
/* Spec 020: CORE-MATH's pow.h includes <math.h> for NAN and INFINITY only. */
#define NAN __builtin_nan("")
#define INFINITY __builtin_inf()
#endif
