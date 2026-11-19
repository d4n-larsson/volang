/* vo's freestanding declarations for vendored code (constitution 1.5.0): only what it uses. ASCII only,
 * no locale (src/vo_c.c). */
#ifndef VO_SHIM_CTYPE_H
#define VO_SHIM_CTYPE_H
int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int isalnum(int c);
int isxdigit(int c);
int isupper(int c);
int islower(int c);
int isprint(int c);
int tolower(int c);
int toupper(int c);
#endif
