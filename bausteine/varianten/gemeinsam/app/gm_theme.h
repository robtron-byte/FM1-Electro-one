/* SPDX-License-Identifier: GPL-3.0-only
 * Farben aller Solo- und Kombi-Firmwares (ADR 0044). Aus fm1-synths mks50/app/mk_theme.h (Commit 2a7b4fa): die Farben
 * der MKS-50-iOS-App des Nutzers. Fünf Farben sagen auf jeder Seite, was ein Regler tut:
 *   blau     Klangquelle (Oszillator)          grün     Modulation (LFO, Modulationstiefen), Oszilloskop
 *   orange   Filter; Akzent und „geändert“      weiß     Hüllkurve, Verstärker, Spielen
 *   violett  Zeit und Effekte
 * Eine Variante kann GM_ACCENT für ihre Firmenfarbe setzen (Kopfzeile). */
#ifndef GM_THEME_H
#define GM_THEME_H
#define T_BG RGB(0, 0, 0)
#define T_INK RGB(237, 237, 235)
#define T_DIM RGB(138, 138, 145)
#define T_LINE RGB(48, 48, 52)
#define T_SEL RGB(232, 232, 227)
#define T_BLUE RGB(51, 120, 255)
#define T_GREEN RGB(28, 204, 120)
#define T_ORANGE RGB(255, 94, 33)
#define T_VIOLET RGB(168, 120, 255)
#define T_ACC T_ORANGE
#define T_WARN T_ORANGE
#define T_LIVE T_GREEN
#ifndef GM_ACCENT
#define GM_ACCENT T_SEL
#endif
static const uint16_t gm_class_colour[5] = {T_BLUE, T_GREEN, T_ORANGE, T_SEL, T_VIOLET};
#endif
