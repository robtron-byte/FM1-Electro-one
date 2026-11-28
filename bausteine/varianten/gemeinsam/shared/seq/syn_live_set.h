/* SPDX-License-Identifier: GPL-3.0-only
 * Live-Aufnahme (syn_live.h, docs/adr/0053): die Einstellungen einer Schleife, für die Seiten der Apps. */
#ifndef SYN_LIVE_SET_H
#define SYN_LIVE_SET_H
enum { LV_SET_ON, LV_SET_BARS, LV_SET_GROW, LV_SET_CUT, LV_SET_N };
enum { LV_GROW_REPEAT, LV_GROW_EMPTY };            /* verlängern: Inhalt wiederholen oder leer */
enum { LV_CUT_START, LV_CUT_END };                 /* kürzen: Anfang oder Ende behalten */
#endif
