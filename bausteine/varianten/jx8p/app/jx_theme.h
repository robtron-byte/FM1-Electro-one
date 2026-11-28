/* SPDX-License-Identifier: GPL-3.0-only
 * The look (docs/adr/0003): the colours of the user's JX-8P iOS app (github.com/robtron-byte/JX8P, branch
 * cursor/ios-jx8p-grundlage-76ee, commit 9687574, ios/Shared/UI/Controls.swift `Theme`): the JX-8P's black
 * panel, slightly blue, the PG-800's colours and the red of the LED display. The slot names are the MKS-50
 * firmware's (../../mks50/app/mk_theme.h), as the app keeps the MKS-50 app's, so the code ports unchanged:
 *   blue    the sound source (DCOs, mixer)         green   modulation (LFO, mod depths): the pale blue
 *   orange  the filter; the accent: LED red        white   envelopes, amp, playing: the button print
 *   violet  time and effects: the slider caps' grey
 * Change the colours here and nowhere else. */
#ifndef JX_THEME_H
#define JX_THEME_H
#define T_BG RGB(0, 0, 0)          /* #000000 body: pure black for the most contrast (wish of the user, 14.11.2026; the app: #0D0F12) */
#define T_INK RGB(232, 232, 227)   /* #E8E8E3 label */
#define T_DIM RGB(135, 143, 155)   /* #878F9B dim label */
#define T_LINE RGB(48, 53, 62)     /* #30353E hairline */
#define T_SEL RGB(230, 226, 214)   /* #E6E2D6 the button print: the selected part, the selected line */
#define T_BLUE RGB(61, 125, 216)   /* #3D7DD8 the parameter chart */
#define T_GREEN RGB(127, 196, 232) /* #7FC4E8 the edit map's headings */
#define T_ORANGE RGB(232, 56, 45)  /* #E8382D the LED display */
#define T_VIOLET RGB(142, 149, 158) /* #8E959E the PG-800's slider caps */
#define T_ACC T_ORANGE             /* accent: a held layer, unsaved, warnings */
#define T_WARN T_ORANGE
#define T_LIVE T_ORANGE            /* the scope, sounding voices: the LED red, as the app's readouts */
#endif
