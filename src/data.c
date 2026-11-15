/* The error catalog as embedded data (ported from src/data.s, spec 008): the single source of
 * truth for `vo explain` and code names. The documents themselves are in build/embedded.c. */
#include "vo.h"

/* Start of the "## CODE " heading line in errors.md, or 0. code is 5 characters. */
char *catalog_find(char *code)
{
    const char *p = errors_md, *end = errors_md + errors_md_len;
    while (p + 9 <= end) {
        if (p[0] == '#' && p[1] == '#' && p[2] == ' ' && p[8] == ' ' && mem_eq((char *)p + 3, code, 5))
            return (char *)p;
        while (p < end && *p++ != '\n') { /* skip to the next line */
        }
    }
    return 0;
}

/* The code's snake_case name as a str, or 0 when the code is unknown. */
Str *catalog_name(char *code)
{
    char *p = catalog_find(code);
    if (!p) return 0;
    p += 9;
    char *q = p;
    while (*q && *q != '\n') q++;
    return str_new(p, q - p);
}

/* The whole "## CODE ..." section up to the next "## " heading, as a str; 0 if unknown. */
Str *catalog_section(char *code)
{
    char *start = catalog_find(code);
    if (!start) return 0;
    const char *end = errors_md + errors_md_len;
    char *t = start + 1;
    while (t < end && !(t[-1] == '\n' && t[0] == '#' && t[1] == '#' && t[2] == ' ')) t++;
    return str_new(start, t - start);
}

/* 1 when the code's section says "Fixable: yes" or "Fixable: sometimes" (a fix may be offered). */
int catalog_fixable(char *code)
{
    Str *s = catalog_section(code);
    if (!s) return 0;
    char *p = s->data, *end = s->data + s->len;
    for (; p + 13 <= end; p++)
        if (p[0] == 'F' && p[7] == ':') return p[9] != 'n'; /* yes or sometimes: a fix may be offered */
    return 0;
}
