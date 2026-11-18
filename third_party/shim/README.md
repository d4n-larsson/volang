# Freestanding declarations for vendored code

vo links no C library (constitution, "Implementation Constraints"). The vendored libraries include a
few standard headers; these files declare only the types, constants and functions they use. The
functions are implemented in `src/vo_c.c` and `src/tls_libc.c`; `stddef.h`, `stdint.h`, `stdarg.h` and
`stdbool.h` come from the compiler itself.
