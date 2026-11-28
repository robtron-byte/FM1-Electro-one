/* SPDX-License-Identifier: GPL-3.0-only
 * Distortion-Send des Gerüsts (gm_dist.c, aus FM1-Electros engine/dist.c). Modus 0..3 = TUBE, OD, DIST, RAT;
 * drive, tone, level 0..127 (level 64 = ±0 dB zur Ausgleichsverstärkung des Modus). In place, nur nass (Mix 127). */
#ifndef GM_DIST_H
#define GM_DIST_H
#ifndef GM_BLOCK_MAX
#define GM_BLOCK_MAX 64                         /* größter Block, den gm_dist_process bekommt */
#endif
enum { GMD_MODES = 4 };
void gm_dist_init(void);
void gm_dist_process(float *send, int frames, int mode, int drive, int tone, int level);
#endif
