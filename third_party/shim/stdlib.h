#ifndef VO_SHIM_STDLIB_H
#define VO_SHIM_STDLIB_H
#include <stddef.h>
void *calloc(size_t n, size_t size);
void free(void *p);
void abort(void);
void exit(int code);
#endif
