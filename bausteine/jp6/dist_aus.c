/* SPDX-License-Identifier: GPL-3.0-only
 * jp6: die Distortion abgeschaltet (Schalter JP6_BS_DIST=0, jp6_bs.h). Wird statt engine/dist.c gebunden: kein Code,
 * keine Tabellen (DIST_FIXED: 49 KB), keine Rechenzeit. Der Klang geht unverändert durch; gegenüber dist.c mit DIST
 * OFF fehlt nur dessen kleine Verzögerung (DIST_LATENCY). JP-6 solo baut so (Wunsch des Nutzers 28.11.2026). */
#include "dist.h"

void dist_init_tables(void) {}
float dist_tight_hz(int mode) { (void)mode; return 0.0f; }
EL_DSP_CODE void dist_process(el_dist_t *d, float *left, float *right, int frames, int mode, int drive, int tone,
                              int level, int mix, int stereo)
{
    (void)d; (void)left; (void)right; (void)frames; (void)mode; (void)drive; (void)tone; (void)level; (void)mix; (void)stereo;
}
