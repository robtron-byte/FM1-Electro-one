/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "el_fmt.h"
#include <stdarg.h>

int el_snprintf(char *out, size_t size, const char *format, ...)
{
    va_list ap;
    size_t n = 0;
    va_start(ap, format);
#define PUT(c) do { char ch_ = (char)(c); if (n + 1 < size) out[n] = ch_; ++n; } while (0)
    for (const char *f = format; *f; ++f) {
        if (*f != '%') { PUT(*f); continue; }
        ++f;
        int plus = 0, zero = 0, width = 0;
        for (;; ++f) {
            if (*f == '+') plus = 1;
            else if (*f == '0') zero = 1;
            else break;
        }
        while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        if (*f == 's') {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "";
            int len = 0;
            while (s[len]) ++len;
            for (; width > len; --width) PUT(' ');
            while (*s) PUT(*s++);
        } else if (*f == 'c') {
            PUT((char)va_arg(ap, int));
        } else if (*f == 'd' || *f == 'u') {
            unsigned long v;
            char sign = 0, digits[12];
            if (*f == 'd') {
                long d = va_arg(ap, int);
                if (d < 0) { sign = '-'; v = (unsigned long)(-d); }
                else { v = (unsigned long)d; if (plus) sign = '+'; }
            } else v = va_arg(ap, unsigned);
            int len = 0;
            do { digits[len++] = (char)('0' + v % 10u); v /= 10u; } while (v);
            int pad = width - len - (sign ? 1 : 0);
            if (!zero) for (; pad > 0; --pad) PUT(' ');
            if (sign) PUT(sign);
            if (zero) for (; pad > 0; --pad) PUT('0');
            while (len) PUT(digits[--len]);
        } else if (*f == '%') {
            PUT('%');
        } else {
            if (!*f) break;
            PUT('%'); PUT(*f);
        }
    }
#undef PUT
    if (size) out[n < size ? n : size - 1] = 0;
    va_end(ap);
    return (int)n;
}
