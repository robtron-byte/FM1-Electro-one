/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Small snprintf for the panel texts: the FM1 Open libc has none. Knows
 * %s %c %d %u %%, the flags + and 0 and a width (as in "%+d", "%02u").
 * Always terminates; returns the length written (without the 0). */
#ifndef EL_FMT_H
#define EL_FMT_H
#include <stddef.h>
int el_snprintf(char *out, size_t size, const char *format, ...);
#endif
