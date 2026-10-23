/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM. */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "po12.h"
#include "dsp_math.h"

const char *const po_sound_names[PO_SOUNDS] = {"PO BD", "PO SD", "PO CH", "PO OH", "SYN SD", "STICKS", "PO CY", "NOISE",
                                               "PO CLAP", "CLICK", "LO TOM", "HI TOM", "PO COWB", "BLIP", "TONE", "BASSTONE"};

/* Fitted to a recording of the 16 sounds at default knobs (envelope and
 * 1/3-octave spectra, early and late; pitch track for the tonal ones).
 * Gains keep the recording's balance, with the bass drum as loud as the
 * 808 bass drum. */
float po_sounds[PO_SOUNDS][POP_PARAMS] = {
    /* wave fs fe pms olev oatt ohold odec nlev nmode nhz nq natt ndec bursts bms fmr fmi drive gate crush pw outlp gain ofilt */
    {0.0f, 272.8f, 33.41f, 23.34f, 0.8324f, 0.3233f, 0.0f, 143.7f, 0.0f, 0.0f, 1027.0f, 0.6809f, 0.1065f, 11.03f, 0.0f, 0.0f, 1.0f, 0.01943f, 1.361f, 0.0f, 0.0f, 0.5551f, 1.999e+04f, 0.2944f, 0.1911f}, /* BD */
    {0.0f, 153.1f, 241.5f, 16.29f, 0.7322f, 0.341f, 0.0f, 26.32f, 2.785f, 1.0f, 1118.0f, 0.2632f, 0.08941f, 31.5f, 0.0f, 0.0f, 1.0f, 0.3067f, 0.6233f, 0.0f, 0.0f, 0.7433f, 6108.0f, 0.3142f, 0.6289f}, /* SD */
    {0.0f, 149.3f, 84.25f, 11.99f, 0.0f, 0.1027f, 0.0f, 5.565f, 0.6475f, 2.0f, 6238.0f, 0.9892f, 0.1338f, 7.173f, 0.0f, 0.0f, 1.0f, 0.037f, 0.05483f, 0.0f, 0.0f, 0.4808f, 1.178e+04f, 0.2325f, 0.03979f}, /* CH */
    {0.0f, 124.4f, 127.2f, 10.81f, 0.0f, 0.1043f, 0.0f, 5.05f, 1.016f, 1.0f, 1704.0f, 0.2897f, 0.1112f, 45.35f, 0.0f, 0.0f, 1.0f, 0.1757f, 0.1725f, 0.0f, 0.0f, 0.478f, 2.394e+04f, 0.3535f, 0.1564f}, /* OH */
    {2.0f, 81.0f, 81.0f, 50.0f, 0.9f, 0.1f, 380.0f, 60.0f, 0.08f, 2.0f, 3000.0f, 0.7f, 0.1f, 400.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 520.0f, 0.0f, 0.5f, 2500.0f, 0.7577f, 0.6f}, /* SYN SD */
    {0.0f, 12.25f, 634.1f, 6.069f, 1.129f, 0.1357f, 0.0f, 4.828f, 0.3541f, 1.0f, 1743.0f, 0.9888f, 0.1449f, 5.168f, 0.0f, 0.0f, 1.0f, 0.3758f, 0.253f, 0.0f, 0.0f, 0.6311f, 1.44e+05f, 0.5198f, 0.268f}, /* STICKS */
    {0.0f, 64.57f, 49.75f, 3.978f, 0.0f, 0.5616f, 0.0f, 6.731f, 0.09781f, 2.0f, 94.17f, 1.281f, 0.2269f, 130.9f, 0.0f, 0.0f, 1.0f, 0.4917f, 0.1276f, 592.4f, 0.0f, 0.5144f, 1.575e+05f, 2.712f, 0.5233f}, /* CY */
    {0.0f, 100.0f, 100.0f, 10.01f, 0.0f, 0.1f, 0.0f, 5.0f, 1.426f, 0.0f, 1.5e+04f, 0.7f, 114.9f, 30.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 120.0f, 0.0f, 0.5f, 1.8e+04f, 0.1114f, 0.0f}, /* NOISE */
    {0.0f, 675.1f, 42.3f, 2.915f, 0.0f, 0.1053f, 0.0f, 4.607f, 2.395f, 1.0f, 2280.0f, 0.6469f, 0.01832f, 20.07f, 2.0f, 11.5f, 1.0f, 0.7753f, 1.186f, 0.0f, 0.0f, 0.4524f, 3538.0f, 0.4374f, 0.9675f}, /* CLAP */
    {0.0f, 14.72f, 400.8f, 29.69f, 0.4218f, 0.2041f, 0.0f, 3.712f, 4.72f, 1.0f, 915.0f, 0.9828f, 0.07184f, 4.6f, 0.0f, 0.0f, 1.0f, 0.1156f, 0.1251f, 0.0f, 0.0f, 0.8378f, 4.026e+04f, 0.165f, 0.338f}, /* CLICK */
    {2.0f, 161.7f, 113.7f, 101.0f, 8.337f, 0.4437f, 44.01f, 66.68f, 0.0f, 0.0f, 324.6f, 0.793f, 0.1227f, 15.23f, 0.0f, 0.0f, 1.0f, 0.07163f, 0.04878f, 0.0f, 0.0f, 0.5004f, 956.1f, 0.1143f, 0.3629f}, /* LO TOM */
    {0.0f, 609.5f, 91.98f, 43.18f, 2.125f, 15.88f, 19.62f, 54.43f, 0.0f, 0.0f, 1184.0f, 0.971f, 0.06898f, 12.81f, 0.0f, 0.0f, 1.0f, 3.702e-05f, 0.09732f, 0.0f, 0.0f, 0.5608f, 1.674e+04f, 0.1391f, 0.07115f}, /* HI TOM */
    {2.0f, 795.3f, 545.4f, 10.3f, 0.7261f, 0.1003f, 0.0f, 15.99f, 0.3066f, 1.0f, 3000.0f, 0.9003f, 0.1143f, 6.267f, 0.0f, 0.0f, 1.0f, 0.09037f, 0.07627f, 0.0f, 0.0f, 0.2738f, 2.162e+04f, 0.3571f, 0.03631f}, /* COWBELL */
    {2.0f, 547.4f, 544.0f, 10.09f, 0.844f, 0.08794f, 0.0f, 79.87f, 0.0f, 0.0f, 951.6f, 0.684f, 0.1065f, 10.77f, 0.0f, 0.0f, 1.0f, 0.04689f, 0.0385f, 0.0f, 1.306e+04f, 0.5294f, 2.302e+04f, 0.2772f, 0.08011f}, /* BLIP */
    {3.0f, 227.1f, 220.1f, 10.32f, 0.5237f, 0.1044f, 62.32f, 20.26f, 0.0f, 0.0f, 1026.0f, 0.7315f, 0.09835f, 10.05f, 0.0f, 0.0f, 1.0f, 0.1763f, 0.03066f, 95.07f, 0.0f, 0.5089f, 1.855e+04f, 0.1962f, 0.02908f}, /* TONE */
    {3.0f, 129.1f, 49.51f, 12.06f, 0.353f, 1.082f, 61.43f, 19.87f, 0.0f, 0.0f, 1154.0f, 35.71f, 0.04474f, 8.948f, 0.0f, 0.0f, 0.9056f, 0.3288f, 0.1135f, 98.25f, 0.0f, 0.3199f, 1.412e+04f, 0.5504f, 0.8841f}, /* BASSTONE */
};

static inline float rnd(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    *s = n;
    return (float)(int32_t)n * (1.0f / 2147483648.0f);
}

static inline float coef_ms(float ms) { return ms <= 0.0f ? 0.0f : dsp_exp(-1.0f / (ms * 0.001f * EL_SAMPLE_RATE)); }

EL_DSP_CODE void po_trigger(po_voice_t *v, unsigned sound, float velocity, float a, float b)
{
    uint32_t rng = v->rng ? v->rng : 0x6a09e667u;
    v->active = 1;
    v->sound = (uint8_t)(sound < PO_SOUNDS ? sound : 0);
    v->age = 0;
    v->rng = rng;
    v->phase = v->mphase = 0;
    v->pitch = 1.0f;
    v->a_env = 0;
    v->n_env = 0;
    v->n_rise = 0;
    v->fade = 1.0f;
    v->crush_phase = 1.0f;
    v->a = dsp_exp2(2.0f * (a - 0.5f));          /* pitch x0.5 .. x2 */
    v->b = dsp_exp2(4.0f * (b - 0.5f));          /* length x0.25 .. x4 */
    v->velocity = velocity;
    v->choke = 0;
}

EL_DSP_CODE float po_tick(po_voice_t *v)
{
    if (!v->active) return 0.0f;
    const float *p = po_sounds[v->sound];
    float ms = (float)v->age * (1000.0f / EL_SAMPLE_RATE), len = v->b;
    /* Oscillator: exponential glide from f_start to f_end, optional FM. */
    float f = (p[POP_F_END] + (p[POP_F_START] - p[POP_F_END]) * v->pitch) * v->a;
    v->pitch *= coef_ms(p[POP_PITCH_MS] * len);
    float dt = dsp_min(f, 18000.0f) * (1.0f / EL_SAMPLE_RATE);
    float mod = 0;
    if (p[POP_FM_INDEX] > 0) {
        v->mphase += dt * p[POP_FM_RATIO];
        if (v->mphase >= 1.0f) v->mphase -= 1.0f;
        mod = p[POP_FM_INDEX] * dsp_sin(2.0f * DSP_PI * v->mphase);
    }
    v->phase += dt;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    float ph = v->phase + mod * 0.159155f;
    ph -= (float)(int)ph;
    if (ph < 0) ph += 1.0f;
    float o;
    switch ((int)p[POP_WAVE]) {
    case 1: o = 4.0f * dsp_abs(ph - 0.5f) - 1.0f; break;
    case 2: o = ph < p[POP_PULSE_WIDTH] ? 1.0f : -1.0f; break;
    case 3: o = 1.0f - 2.0f * ph; break;
    default: o = dsp_sin(2.0f * DSP_PI * ph); break;
    }
    /* Oscillator envelope: linear attack, hold, exponential decay. */
    float att = p[POP_OSC_ATTACK], hold = p[POP_OSC_HOLD] * len;
    if (ms < att) v->a_env = ms / att;
    else if (ms < att + hold) v->a_env = 1.0f;
    else v->a_env *= coef_ms(p[POP_OSC_DECAY] * len);
    /* Noise: state-variable filter, rise then decay, optional clap bursts. */
    float n = rnd(&v->rng), ne = v->n_env, osc = p[POP_OSC_LEVEL] * o * v->a_env;
    /* Noise envelope first: rise, then decay; optional clap bursts. */
    float natt = p[POP_NOISE_ATTACK];
    if (ms < natt) v->n_env = ms / natt;
    else {
        if (v->n_env < 1.0f && ms - natt < 1.0f) v->n_env = 1.0f;
        v->n_env *= coef_ms(p[POP_NOISE_DECAY] * len);
    }
    ne = v->n_env;
    int bursts = (int)p[POP_BURSTS];
    if (bursts > 0 && p[POP_BURST_MS] > 0) {
        float span = p[POP_BURST_MS] * (float)bursts;
        if (ms < span) { float in = ms - p[POP_BURST_MS] * (float)(int)(ms / p[POP_BURST_MS]); ne = dsp_max(ne, 1.0f - in / p[POP_BURST_MS]); }
    }
    float into = p[POP_NOISE_LEVEL] * n * ne + p[POP_OSC_FILTER] * osc;
    float g = dsp_tan(DSP_PI * dsp_min(p[POP_NOISE_HZ] * v->a, 19000.0f) / EL_SAMPLE_RATE), k = 1.0f / dsp_max(p[POP_NOISE_Q], 0.3f);
    float hp = (into - (k + g) * v->s1 - v->s2) / (1.0f + g * (k + g)), bp = g * hp + v->s1, lpo = g * bp + v->s2;
    v->s1 = g * hp + bp; v->s2 = g * bp + lpo;
    int mode = (int)p[POP_NOISE_MODE];
    float nf = mode == 0 ? lpo : mode == 1 ? bp * k : hp;
    float y = nf + (1.0f - p[POP_OSC_FILTER]) * osc;
    if (p[POP_DRIVE] > 0) { float d = 1.0f + p[POP_DRIVE]; y = dsp_tanh(y * d) / dsp_tanh(d); }
    /* Lo-fi: sample-and-hold at a low rate. */
    if (p[POP_CRUSH_HZ] > 0) {
        v->crush_phase += p[POP_CRUSH_HZ] * (1.0f / EL_SAMPLE_RATE);
        if (v->crush_phase >= 1.0f) { v->crush_phase -= 1.0f; v->crush_val = y; }
        y = v->crush_val;
    }
    float c = 1.0f - dsp_exp(-2.0f * DSP_PI * dsp_min(p[POP_OUT_LP_HZ], 20000.0f) / EL_SAMPLE_RATE);
    v->lp += (y - v->lp) * c;
    y = v->lp * p[POP_GAIN] * (0.6f + 0.4f * v->velocity);
    /* End: the gate (hard, as on the recordings, but over 1 ms), or fade
     * out once both envelopes are down. */
    float gate = p[POP_GATE_MS] * len;
    int done = v->choke || (gate > 0 && ms >= gate) || (ms > att + hold + 5.0f && ms > natt + 5.0f && v->a_env < 1e-4f && v->n_env < 1e-4f);
    if (done) {
        v->fade -= 1.0f / (0.001f * EL_SAMPLE_RATE);
        if (v->fade <= 0) { v->fade = 0; v->active = 0; }
    }
    ++v->age;
    return y * v->fade;
}
