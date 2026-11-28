/* SPDX-License-Identifier: GPL-3.0-only
 * dk808: Adapter für das Gerüst (dk808_bs.h, docs/adr/0050). */
#include <string.h>
#include "dk808_bs.h"

static const uint8_t def_knob[DK_COUNT][DK808_KNOBS] = {   /* = knob_default in dk808.c (Test prüft) */
    {50, 25, 50}, {50, 100, 100}, {25, 50, 50}, {50, 50, 0}, {50, 100, 0}, {50, 50, 0}, {50, 50, 0}, {50, 50, 0},
    {50, 50, 50}, {50, 50, 50}, {50, 50, 50}, {50, 50, 50}, {50, 50, 50}, {50, 100, 50}, {50, 50, 0}, {50, 0, 0},
    {50, 100, 0}};
#define K3(i) def_knob[i][0], def_knob[i][1], def_knob[i][2]
const uint8_t dk808_bs_init_kit[DK_BS_NP] = {
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    K3(0), K3(1), K3(2), K3(3), K3(4), K3(5), K3(6), K3(7), K3(8), K3(9), K3(10), K3(11), K3(12), K3(13), K3(14),
    K3(15), K3(16), 1, 0, 0, 0};
#undef K3

void dk808_bs_init(dk808_bs_t *d, double sr)
{
    dk808_init(&d->k, sr);
    d->dist = 0;
}
int dk808_bs_note_on(dk808_bs_t *d, int note, int vel)
{
    int i = dk808_note_instrument(note);
    if (i >= 0 && DK808_HAS(i) && vel > 0) dk808_trigger(&d->k, (unsigned)i, vel > 127 ? 127 : vel);
    return i >= 0 && DK808_HAS(i) ? i : -1;
}
void dk808_bs_all_off(dk808_bs_t *d)
{
    memset(d->k.active, 0, sizeof d->k.active);
    d->k.queued = 0;
}
int dk808_bs_param_max(unsigned p)
{
    if (p < DK_BS_CHOKE || p == DK_BS_DIST) return 100;
    return p < DK_BS_NP ? 1 : 0;
}
void dk808_bs_set_raw(dk808_bs_t *d, unsigned p, int v)
{
    int mx = dk808_bs_param_max(p);
    if (!mx) return;
    v = v < 0 ? 0 : v > mx ? mx : v;
    if (p < DK_BS_KNOB) dk808_set_level(&d->k, p, v / 100.0);
    else if (p < DK_BS_CHOKE) dk808_set_knob(&d->k, (p - DK_BS_KNOB) / DK808_KNOBS, (p - DK_BS_KNOB) % DK808_KNOBS, v / 100.0);
    else if (p == DK_BS_CHOKE) d->k.hat_choke = v;
    else if (p == DK_BS_CHOKE + 1) d->k.conga_choke = v;
    else if (p == DK_BS_CHOKE + 2) d->k.tom_choke = v;
    else d->dist = v / 100.0f;
}
void dk808_bs_load_values(dk808_bs_t *d, const uint8_t *v)
{
    for (unsigned p = 0; p < DK_BS_NP; p++) dk808_bs_set_raw(d, p, v[p]);
}
const char *dk808_bs_param_name(unsigned p, char *b)
{
    static const char *const sw[4] = {"HAT CHOKE", "CNG CHOKE", "TOM CHOKE", "DIST SEND"};
    const char *a = "", *n = "";
    if (p < DK_BS_KNOB) { a = dk808_instrument_abbr(p); n = "LEVEL"; }
    else if (p < DK_BS_CHOKE) { a = dk808_instrument_abbr((p - DK_BS_KNOB) / DK808_KNOBS);
                                n = dk808_knob_name((p - DK_BS_KNOB) / DK808_KNOBS, (p - DK_BS_KNOB) % DK808_KNOBS); }
    else if (p < DK_BS_NP) { strcpy(b, sw[p - DK_BS_CHOKE]); return b; }
    if (!*n) { b[0] = 0; return b; }
    unsigned k = 0;
    while (*a) b[k++] = *a++;
    b[k++] = ' ';
    while (*n && k < 15) b[k++] = *n++;
    b[k] = 0;
    return b;
}
int dk808_bs_voices(const dk808_bs_t *d)
{
    int n = 0;
    for (unsigned i = 0; i < DK_COUNT; i++) n += d->k.active[i] != 0;
    return n;
}
void dk808_bs_render(dk808_bs_t *d, float *l, float *r, float *dist, int n)
{
    float a = d->dist;
    for (int i = 0; i < n; ++i) {
        float s = (float)dk808_tick(&d->k);
        l[i] += s;
        r[i] += s;
        if (dist) dist[i] += s * a;
    }
}
