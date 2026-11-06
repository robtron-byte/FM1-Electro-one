/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Every MKS-50 sound, a three-note chord low and high (2 s each), raw
 * float32 stereo to stdout: built with MKS_CHOP_GRID 0 and 1, compared by
 * host/fixed_cmp.py (make chop-grid-check, docs/adr/0019). */
#include <stdio.h>
#include "mks.h"

int main(void)
{
    static mks_part_t p;
    static float l[EL_BLOCK], r[EL_BLOCK], d[EL_BLOCK], v[EL_BLOCK], out[2 * EL_BLOCK];
    for (unsigned s = 0; s < mks_preset_count; ++s) {
        mks_part_init(&p, 3, 0x1234567u);
        mks_part_load(&p, mks_presets[s].v);
        p.level = 1.0f;
        static const int roots[2] = {41, 65};
        for (int o = 0; o < 2; ++o) {
            for (int n = 0; n < 3; ++n) mks_note_on(&p, roots[o] + 4 * n - (n == 2), 0.8f, 0);
            for (int b = 0; b < 2 * 44100 / EL_BLOCK; ++b) {
                if (b == 44100 / EL_BLOCK) for (int n = 0; n < 3; ++n) mks_note_off(&p, roots[o] + 4 * n - (n == 2));
                for (int i = 0; i < EL_BLOCK; ++i) l[i] = r[i] = d[i] = v[i] = 0;
                mks_render(&p, l, r, d, v, EL_BLOCK, 120.0f);
                for (int i = 0; i < EL_BLOCK; ++i) { out[2 * i] = l[i]; out[2 * i + 1] = r[i]; }
                fwrite(out, sizeof out, 1, stdout);
            }
        }
    }
    return 0;
}
