
#ifndef FFPP_AVSTRING_H
#define FFPP_AVSTRING_H
#include <stddef.h>
#include <string.h>
static size_t av_strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = strlen(src);
    if (size) {
        size_t n = (len >= size) ? size - 1 : len;
        memcpy(dst, src, n);
        dst[n] = '\0';
    }
    return len;
}
static size_t av_strlcat(char *dst, const char *src, size_t size)
{
    size_t dlen = strlen(dst);
    if (dlen < size) return dlen + av_strlcpy(dst + dlen, src, size - dlen);
    return dlen + strlen(src);
}
#endif

/* strtok_r wrapper (ISO C, no locale extras needed by libpostproc) */
static char *av_strtok(char *s, const char *delim, char **saveptr)
{
    char *tok;
    if (!s) s = *saveptr;
    s += strspn(s, delim);
    if (!*s) { *saveptr = s; return NULL; }
    tok = s;
    s = strpbrk(tok, delim);
    if (s) { *s = 0; *saveptr = s + 1; }
    else   { *saveptr = tok + strlen(tok); }
    return tok;
}
