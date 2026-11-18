#ifndef VO_SHIM_STDIO_H
#define VO_SHIM_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct VoShimFile FILE;
int snprintf(char *s, size_t n, const char *fmt, ...);
int vsnprintf(char *s, size_t n, const char *fmt, va_list ap);
int printf(const char *fmt, ...);
#endif
