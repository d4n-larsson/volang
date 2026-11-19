# Freestanding declarations for vendored code

vo links no C library (constitution, "Implementation Constraints"). The vendored libraries include a
few standard headers; these files declare only the types, constants and functions they use. The
functions are implemented in `src/vo_c.c` and `src/tls_libc.c` (the math functions CORE-MATH needs in
`src/math_libm.c`; the string, character and math functions SQLite needs in `src/sqlite_libc.c`, the
math ones through CORE-MATH); `stddef.h`, `stdint.h`, `stdarg.h` and
`stdbool.h` come from the compiler itself.

`fenv.h` (spec 020, CORE-MATH) is complete in itself: vo always rounds to nearest and never reads the
floating-point exception flags, so `fegetround` returns `FE_TONEAREST` and the flag functions do nothing.
`printf` only prints its format text: CORE-MATH calls it just before `exit` when it meets an input it
cannot round, which ends as an internal error (T0003).
