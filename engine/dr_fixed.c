/* SPDX-License-Identifier: GPL-3.0-only */
/* The drum kit in fixed point (docs/adr/0022): the BEAT area's 808 parts on
 * Felucca's drum voices (third_party/felucca/drum_voice.c, Leo Kuroshita,
 * Hügelton Instruments, GPL-3.0-only; its dsp.c and tables MIT), their model
 * kit "80", the cymbal on Felucca's own. Only 32-bit integer products a
 * sample, envelopes and coefficients a block of 32: on the FM-1 our float
 * 808 took 67 % of a core (2026-11-06), Felucca's whole kit a few per cent.
 * ZAP, PULSE and the PO-12 kit stay as they are (tr808.c).
 *
 * Built only with TR808_FIXED (el_config.h: on the own platform); the host
 * and FM1 Open's SDK keep the float 808. */
#include "tr808.h"
#if TR808_FIXED
#include <string.h>
#include "../third_party/felucca/fe_tables.h"
typedef struct { int32_t amp0, amp1; } vmod_t;   /* (dsp.c's amplitude ramp: unused here) */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "../third_party/felucca/fe_dsp.c"
#include "../third_party/felucca/drum_voice.c"
#pragma clang diagnostic pop

enum { S_LEVEL, S_TUNE, S_DECAY, S_TONE, S_SNAPPY, S_SOUND, S_PAN, S_REV, S_DLY };   /* tr808.c P_* */
#define KIT80(lane) DV_KTYPE(1u, (lane))

/* Level against the float 808 (fitted by ear and RMS, host/hit_render.c). */
static const float fix_gain[D8_NUM] = {1.6f, 1.4f, 1.4f, 1.4f, 1.2f, 1.4f, 1.2f, 1.2f, 1.2f, 1.2f, 0, 0};

static struct {
    dv_param_t p[D8_NUM];
    dv_coef_t c[D8_NUM];
    dv_voice_t v[D8_NUM];
    dv_metal_t m[D8_NUM];
    uint8_t setup[D8_NUM];
    float vel[D8_NUM];
    uint8_t acc[D8_NUM];
    int32_t y[D8_NUM][CTL];   /* the block being played out */
    int32_t metal[CTL];
    uint8_t live[D8_NUM], out[D8_NUM];   /* sounding; in the current block */
    uint32_t pos;             /* samples of the block played (CTL: render the next) */
} drf;

int drf_part(const tr808_t *t, int part)
{
    (void)t;
    return part >= D8_BD && part <= D8_CH && !TR808_OWN_PART(part);   /* ours: tr808_metal_fixed.h, tr808_bd_fixed.h */
}

void drf_init(void)
{
    memset(&drf, 0, sizeof drf);
    drf.pos = CTL;
}
void drf_silence(void) { drf_init(); }

/* our knobs (64: the kit's design) as Felucca's parameters */
static void drf_params(const tr808_t *t, int part, dv_param_t *p)
{
    const uint8_t *k = t->pot[part];
    int tune = (k[S_TUNE] - 64) * 3 + t->shift_semis[part] * 16;   /* about +-12 semitones, in 1/16; the step's pitch lock */
    memset(p, 0, sizeof *p);
    p->decay = k[S_DECAY];
    p->tone = 64;
    p->extra = 64;
    p->level = 100;
    p->accent = drf.acc[part];
    switch (part) {
    case D8_BD: p->type = KIT80(DV_KICK); p->tone = k[S_TONE]; break;
    case D8_SD: p->type = KIT80(DV_SNARE); p->tone = k[S_TONE]; p->extra = k[S_SNAPPY]; break;
    case D8_LT:
    case D8_HT:
        p->type = k[S_SOUND] ? DVT_CONGA : KIT80(DV_TOM);
        tune += part == D8_HT ? 96 : -96;   /* the high one half an octave above, the low one below */
        p->extra = k[S_SNAPPY];             /* BEND */
        p->tone = k[S_TONE];                /* the stick's noise */
        break;
    case D8_RS: p->type = k[S_SOUND] ? DVT_CLAVE : KIT80(DV_RIM); break;
    case D8_CP: p->type = KIT80(DV_CLAP); p->tone = k[S_TONE]; break;
    case D8_CB: p->type = KIT80(DV_BELL); break;
    case D8_CY: p->type = DVT_CYM; p->tone = k[S_TONE]; break;
    case D8_OH: p->type = KIT80(DV_HATO); break;
    default: p->type = KIT80(DV_HATC); break;
    }
    p->tune = (int16_t)tune;
}

void drf_trigger(const tr808_t *t, int part, float velocity, float accent)
{
    if (!drf_part(t, part)) return;
    drf.acc[part] = (uint8_t)(accent * 127.0f);
    drf.vel[part] = velocity < 0.66f ? velocity / 0.66f : 1.0f;
    drf.setup[part] = 0;   /* (the accent is in the coefficients) */
    dv_trigger(&drf.v[part]);
    drf.live[part] = 1;
    /* closed hat chokes the open one (kit CHOKE), and the other way with BOTH */
    if (part == D8_CH && t->kit[2] && drf.live[D8_OH]) dv_choke(&drf.v[D8_OH]);
    if (part == D8_OH && t->kit[2] == 2 && drf.live[D8_CH]) dv_choke(&drf.v[D8_CH]);
}

/* the next block of every sounding part */
static void drf_block(tr808_t *t)
{
    for (int part = D8_BD; part <= D8_CH; ++part) {
        drf.out[part] = drf.live[part];
        if (!drf.live[part]) continue;
        dv_param_t p;
        drf_params(t, part, &p);
        if (!drf.setup[part] || memcmp(&p, &drf.p[part], sizeof p)) {
            drf.p[part] = p;
            dv_setup(&drf.c[part], &p);
            if (dv_uses_metal(drf.c[part].type)) dv_metal_tune(&drf.m[part], &drf.c[part]);
            drf.setup[part] = 1;
        }
        const int32_t *metal = 0;
        if (dv_uses_metal(drf.c[part].type)) {
            dv_metal_run(&drf.m[part], drf.metal, CTL);
            metal = drf.metal;
        }
        dv_run(&drf.c[part], &drf.v[part], metal, drf.y[part], CTL);
        if (!drf.v[part].live && !drf.v[part].trig) {
            drf.live[part] = 0;
            t->v[part].active = 0;
        }
    }
}

void drf_render(tr808_t *t, float *kick, float *left, float *right, float *rev, float *gate, float *dly, int frames)
{
    float level = t->kit[0] / 110.0f * (1.0f / 32768.0f);
    for (int n = 0; n < frames;) {
        if (drf.pos >= CTL) {
            int any = 0;
            for (int part = D8_BD; part <= D8_CH; ++part) any |= drf.live[part];
            if (!any) return;
            drf_block(t);
            drf.pos = 0;
        }
        int m = frames - n < (int)(CTL - drf.pos) ? frames - n : (int)(CTL - drf.pos);
        for (int part = D8_BD; part <= D8_CH; ++part) {
            if (!drf.out[part]) continue;
            const uint8_t *k = t->pot[part];
            float g = fix_gain[part] * k[S_LEVEL] / 100.0f * level * drf.vel[part];
            float sr = k[S_REV] / 127.0f, sd = k[S_DLY] / 127.0f;
            float pl = t->pan_l[part], pr = t->pan_r[part];
            int to_gate = part == D8_SD || part == D8_CP || part == D8_RS;
            const int32_t *y = drf.y[part] + drf.pos;
            for (int i = 0; i < m; ++i) {
                float s = (float)y[i] * g;
                if (part == D8_BD) kick[n + i] += s;
                else { left[n + i] += s * pl; right[n + i] += s * pr; }
                if (to_gate) gate[n + i] += s * sr; else rev[n + i] += s * sr;
                dly[n + i] += s * sd;
            }
        }
        drf.pos += (uint32_t)m;
        n += m;
    }
}
#endif
