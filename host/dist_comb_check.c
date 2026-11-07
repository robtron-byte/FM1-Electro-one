/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Host check: a held STRINGS chord through the distortion send, written as
 * raw mono floats for tools/dist_comb.py (comb filtering of dry + wet).
 * Usage: dist_comb_check <out.f32> <mode 0..4> <drive> <send 0..127> [tone [master 0|1]]
 * (master 0: the master compressor and limiter off, so only the phase of dry and wet shows) */
#include "electro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 5) return 1;
    static el_engine_t e;
    static float out[44100 * 2 * 2], mono[44100 * 2];
    el_init(&e);
    e.fx.v[FX_DIST_MODE] = (uint8_t)atoi(argv[2]);
    e.fx.v[FX_DIST_DRIVE] = (uint8_t)atoi(argv[3]);
    e.fx.v[FX_DIST_TONE] = (uint8_t)(argc > 5 ? atoi(argv[5]) : 64);
    e.fx.v[FX_DIST_SEND_STRINGS] = (uint8_t)atoi(argv[4]);
    if (argc > 6 && !atoi(argv[6])) { e.option[EL_OPT_MASTER] = 0; e.mix.enabled = 0; }
    el_post(&e, EL_CMD_START, 0, 0);
    static const uint8_t notes[3] = {48, 52, 55};
    for (unsigned n = 0; n < 3; ++n) {
        uint8_t on[3] = {0x91, notes[n], 100};
        el_midi_input(&e, on, 3);
    }
    unsigned total = 44100u * 2u / EL_BLOCK * EL_BLOCK;
    for (unsigned pos = 0; pos < total; pos += EL_BLOCK) el_render_float(&e, out + pos * 2u, EL_BLOCK);
    for (unsigned i = 0; i < total; ++i) mono[i] = 0.5f * (out[2 * i] + out[2 * i + 1]);
    FILE *f = fopen(argv[1], "wb");
    if (!f) return 1;
    fwrite(mono, sizeof mono[0], total, f);
    fclose(f);
    return 0;
}
