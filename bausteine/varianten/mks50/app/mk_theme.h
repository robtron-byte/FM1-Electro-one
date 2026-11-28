/* SPDX-License-Identifier: GPL-3.0-only
 * The look (docs/adr/0008): the colours of the user's MKS-50 iOS app (github.com/robtron-byte/Mks50-iOS-,
 * commit 080db22, Shared/UI/Controls.swift `Theme`): a near-black body, flat surfaces parted by hairlines,
 * white for what is selected, and five colours that say what a control does, the same on every page:
 *   blue    the sound source (oscillator)      green   modulation (LFO, mod depths), the display's readout
 *   orange  the filter; also the accent        white   envelope, amp, playing
 *   violet  time and effects (chorus, delay, reverb)
 * Change the colours here and nowhere else. */
#ifndef MK_THEME_H
#define MK_THEME_H
#define T_BG RGB(0, 0, 0)          /* #000000 body: pure black for the most contrast (wish of the user, 14.11.2026; the app: #0B0B0C) */
#define T_INK RGB(237, 237, 235)   /* #EDEDEB label */
#define T_DIM RGB(138, 138, 145)   /* #8A8A91 dim label */
#define T_LINE RGB(48, 48, 52)     /* #303034 hairline */
#define T_SEL RGB(232, 232, 227)   /* #E8E8E3 white: the selected part, the selected line */
#define T_BLUE RGB(51, 120, 255)   /* #3378FF */
#define T_GREEN RGB(28, 204, 120)  /* #1CCC78 */
#define T_ORANGE RGB(255, 94, 33)  /* #FF5E21 */
#define T_VIOLET RGB(168, 120, 255) /* #A878FF */
#define T_ACC T_ORANGE             /* accent: a held layer, unsaved, warnings (the app has no red) */
#define T_WARN T_ORANGE
#define T_LIVE T_GREEN             /* the scope, sounding voices */
#endif
