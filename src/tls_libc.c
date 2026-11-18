/* The few C library functions the vendored TLS library calls that vo did not have (spec 019 research
 * R2; memcpy, memmove, memset and memcmp are in vo_c.c). Each works on explicit lengths. */
#include "vo.h"

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strchr(const char *s, int c);
char *strstr(const char *h, const char *n);
int getpid(void);
void exit(int code);
int printf(const char *fmt, ...);

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

char *strchr(const char *s, int c)
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return 0;
    }
}

char *strstr(const char *h, const char *n)
{
    size_t m = strlen(n);
    if (!m) return (char *)h;
    for (; *h; h++)
        if (*h == *n && mem_eq((char *)h, (char *)n, (long)m)) return (char *)h;
    return 0;
}

/* The random generator reseeds when the process id changes: vo forks tasks and mutant children. */
int getpid(void)
{
    return (int)vo_syscall0(SYS_getpid);
}

/* CORE-MATH's report of an input it cannot round (spec 020), printed just before exit: the format
 * text only, since that path is a library bug. */
int printf(const char *fmt, ...)
{
    out_cstr(2, (char *)fmt);
    return 0;
}

/* Only the vendored libraries' fatal paths could call this; vo treats it as an internal error (T0003). */
void exit(int code)
{
    (void)code;
    out_cstr(2, "error[T0003]: a vendored library stopped the program\n");
    vo_exit(EXIT_INTERNAL);
}
