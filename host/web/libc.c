/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Minimal libc for the WebAssembly panel simulator: the few string
 * functions the firmware uses and a small snprintf (%s %c %d %u %x, width,
 * zero padding, '+', '-', '%%'). */
#include <string.h>
#include <stdio.h>

void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { unsigned char *p = d; const unsigned char *q = s; while (n--) *p++ = *q++; return d; }
void *memmove(void *d, const void *s, size_t n)
{
    unsigned char *p = d; const unsigned char *q = s;
    if (p < q) while (n--) *p++ = *q++;
    else { p += n; q += n; while (n--) *--p = *--q; }
    return d;
}
int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = a, *q = b;
    for (; n; --n, ++p, ++q) if (*p != *q) return *p - *q;
    return 0;
}
int strcmp(const char *a, const char *b) { while (*a && *a == *b) ++a, ++b; return (unsigned char)*a - (unsigned char)*b; }
char *strncpy(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; i < n && s[i]; ++i) d[i] = s[i];
    for (; i < n; ++i) d[i] = 0;
    return d;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) ++n; return n; }

int vsnprintf(char *out, size_t size, const char *f, va_list ap)
{
    size_t n = 0;
#define PUT(ch) do { if (n + 1 < size) out[n] = (ch); ++n; } while (0)
    for (; *f; ++f) {
        if (*f != '%') { PUT(*f); continue; }
        ++f;
        int left = 0, zero = 0, plus = 0, width = 0;
        for (;; ++f) {
            if (*f == '-') left = 1; else if (*f == '0') zero = 1; else if (*f == '+') plus = 1; else break;
        }
        while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        char buf[24], *s = buf;
        int len = 0;
        switch (*f) {
        case '%': buf[0] = '%'; len = 1; break;
        case 'c': buf[0] = (char)va_arg(ap, int); len = 1; break;
        case 's': s = va_arg(ap, char *); if (!s) s = "(null)"; len = (int)strlen(s); break;
        case 'd': case 'u': case 'x': {
            unsigned long v; int neg = 0;
            if (*f == 'd') { long x = va_arg(ap, int); if (x < 0) { neg = 1; v = (unsigned long)-x; } else v = (unsigned long)x; }
            else v = va_arg(ap, unsigned);
            unsigned base = *f == 'x' ? 16u : 10u;
            char tmp[24]; int t = 0;
            do { unsigned d = (unsigned)(v % base); tmp[t++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v /= base; } while (v);
            if (neg) buf[len++] = '-'; else if (plus) buf[len++] = '+';
            int pad = zero && !left ? width - len - t : 0;
            while (pad-- > 0) buf[len++] = '0';
            while (t) buf[len++] = tmp[--t];
            break;
        }
        default: buf[0] = '?'; len = 1; break;
        }
        int fill = width > len ? width - len : 0;
        if (!left) while (fill-- > 0) PUT(' ');
        for (int i = 0; i < len; ++i) PUT(s[i]);
        if (left) while (fill-- > 0) PUT(' ');
    }
    if (size) out[n < size ? n : size - 1] = 0;
#undef PUT
    return (int)n;
}

int snprintf(char *out, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(out, size, fmt, ap);
    va_end(ap);
    return r;
}
