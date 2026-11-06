/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * The 808's snare, toms / congas, rim shot / claves and hand clap /
 * maracas of tr808.c in fixed point, for the own platform (docs/adr/0022,
 * 0006: the calibrated model stays the sound). Included by tr808.c after
 * tr808_metal_fixed.h (its helpers). The default models only (snare 0,
 * clap 3).
 *
 * The same models, sample for sample, with the float code's white noise
 * (the same generator and state, so the noise is the same sample for
 * sample). Signals Q20, coefficients Q28, envelopes Q30; tanh and cos from
 * tables with linear interpolation (errors below -100 dB). Floats per
 * render call and, where the float code sets a filter every 8 samples, at
 * those points (the float svf_set). */
#if TR808_FIXED && TR808_REST_FIXED
#if TR808_SD_MODEL != 0 || TR808_CP_MODEL != 3
#error "tr808_rest_fixed.h: only the default snare (0) and clap (3) models"
#endif

#define DR_ONE20 (1 << 20)
static inline EL_DSP_HELPER int32_t dr_q20(float x) { return (int32_t)(x * 1048576.0f); }
static inline EL_DSP_HELPER float dr_f20(int32_t x) { return (float)x * (1.0f / DR_ONE20); }

/* tanh over +-8 (Q20 in and out): 1/128 steps */
static int32_t dr_tanh_tab[1025];
static inline EL_DSP_HELPER int32_t dr_tanh(int32_t x)
{
    int32_t a = x < 0 ? -x : x;
    if (a >= (8 << 20)) return x < 0 ? -DR_ONE20 : DR_ONE20;
    int32_t i = a >> 13, f = a & 0x1fff;
    int32_t y = dr_tanh_tab[i] + (((dr_tanh_tab[i + 1] - dr_tanh_tab[i]) * f) >> 13);
    return x < 0 ? -y : y;
}
/* sin of a 32-bit phase (Q20): 1024 steps a cycle */
static int32_t dr_sin_tab[1025];
static inline EL_DSP_HELPER int32_t dr_sin(uint32_t ph)
{
    uint32_t i = ph >> 22, f = (ph >> 10) & 0xfffu;
    return dr_sin_tab[i] + (((dr_sin_tab[i + 1] - dr_sin_tab[i]) * (int32_t)f) >> 12);
}

typedef struct {
    dm_svf_t r1, r2, r3;
    int32_t env, env2, env3, pitch, aux, out_lp, lag1, lag2, pulse_lp, dc_x, dc_y, click;
    uint32_t age, phase;
    uint8_t active, opening;
} dr_voice_t;

static struct {
    dr_voice_t v[D8_CP + 1];   /* SD, LT, HT, RS, CP (index = part) */
    uint8_t init;
} dr;
static float dr_y[D8_CP + 1][EL_BLOCK];

static void dr_reset(void)
{
    memset(&dr, 0, sizeof dr);
    for (int i = 0; i <= 1024; ++i) {
        dr_tanh_tab[i] = dr_q20(dsp_tanh((float)i * (1.0f / 128.0f)));
        dr_sin_tab[i] = dr_q20(dsp_sin(2.0f * DSP_PI * (float)i * (1.0f / 1024.0f)));
    }
}

/* the band-pass's raw voltage v1 (svf_bp(..) / k) */
static inline EL_DSP_HELPER int32_t dr_bp_raw(dm_svf_t *f, int32_t x)
{
    int32_t v3 = x - f->ic2, v1 = DM28(f->a1, f->ic1) + DM28(f->a2, v3), v2 = f->ic2 + DM28(f->a2, f->ic1) + DM28(f->a3, v3);
    f->ic1 = 2 * v1 - f->ic1;
    f->ic2 = 2 * v2 - f->ic2;
    return v1;
}
/* the first whole sample at or after x (for age < x and age > x tests) */
static inline uint32_t dr_ceil(float x) { uint32_t i = (uint32_t)x; return (float)i < x ? i + 1u : i; }

/* tr808.c's trigger_pulse() in Q20 */
static inline EL_DSP_HELPER int32_t dr_pulse(dr_voice_t *v, int32_t volts, uint32_t len, int32_t c_plp, int32_t clamp)
{
    int32_t rect = v->age < len ? volts : 0;
    v->pulse_lp += DM28(rect - v->pulse_lp, c_plp);
    int32_t hp = rect - v->pulse_lp;
    return hp < clamp ? clamp : hp;
}
/* the float noise() of tr808.c, Q20 */
static inline EL_DSP_HELPER int32_t dr_noise(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    *s = n;
    return (int32_t)n >> 11;
}
/* (float)age * scale as Q30, at most 1 (attack ramps) */
static inline EL_DSP_HELPER int32_t dr_ramp(uint32_t age, int32_t step_q30)
{
    int64_t r = (int64_t)age * step_q30;
    return r >= DM_ONE30 ? DM_ONE30 : (int32_t)r;
}

static EL_DSP_CODE unsigned dr_render(tr808_t *t, int frames)
{
    static const uint8_t parts[] = {D8_SD, D8_LT, D8_HT, D8_RS, D8_CP};
    unsigned live = 0;
    for (unsigned i = 0; i < sizeof parts; ++i) {
        int part = parts[i];
        if (!po_engine(t, part) && t->v[part].active) live |= 1u << part;
        else dr.v[part].active = 0;
    }
    if (!live) return 0;
    const uint32_t pulse_len = (uint32_t)MS(1.0f);
    const int32_t c_plp = dm_q28(0.2035f), clamp = dr_q20(-0.05f), quiet = dm_q30(1e-4f);
    for (unsigned pi = 0; pi < sizeof parts; ++pi) {
        int part = parts[pi];
        if (!(live & (1u << part))) continue;
        tr_voice_t *tv = &t->v[part];
        dr_voice_t *v = &dr.v[part];
        float *out = dr_y[part];
        float acc = tv->accent;
        int32_t volts = dr_q20(tv->volts);
        uint32_t ns = t->noise;                  /* the main loop's noise, sample for sample */
        int fresh = tv->age == 0 || !v->active;
        if (fresh) {                             /* tr808_trigger(): envelopes from the top, filters keep their state */
            v->active = 1;
            v->age = 0;
            v->pulse_lp = 0;
            v->env = v->env2 = v->env3 = v->pitch = DM_ONE30;
            v->aux = 0;
        }
        if (part == D8_SD) {
            /* ---- snare(): model 0 */
            const float *k = tr808_sd_cal;
            float tune = t->key_on ? t->key_r[part] : tolerance[part] * spread(t, part, P_TUNE), decay = spread(t, part, P_DECAY);
            if (fresh) dm_svf_set(&v->r3, k[8], 0.7f);
            float tone = knob(t, part, P_TONE);
            float early = t->pot[part][P_SOUND] ? 1.384f : 1.0f;
            float knock0 = 0.04f + 0.04f * acc;
            const int32_t g_low = dm_q28((1.0f - tone) * k[0]), g_high = dm_q28(k[1] * dsp_exp2(1.79f * tone));
            const int32_t c_env = dm_q30(decay_coef(MS(k[5]) * decay)), c_env2 = dm_q30(decay_coef(MS(k[6]) * decay));
            const int32_t c_pitch = dm_q30(0.99623f), att_step = dm_q30(1.0f / MS(0.5f));
            const int32_t c_lp = dm_q30(1.0f - dsp_exp(-2.0f * DSP_PI * k[9] / EL_SAMPLE_RATE));
            const int32_t mix1 = dm_q30(1.0f - k[7]), mix2 = dm_q30(k[7]), g_n = dm_q28(k[4]), g_snap = dm_q28(k[10]);
            float snappy_f = dsp_clamp((knob(t, part, P_SNAPPY) - 0.26f) * (1.0f / 0.74f), 0.0f, 1.0f);
            const int32_t snappy = dm_q30(snappy_f), c09 = dm_q28(0.9f), c09i = dm_q28(1.0f / 0.9f), c055 = dm_q28(0.55f);
            const uint32_t end = (uint32_t)MS(500.0f);
            for (int n = 0; n < frames; ++n) {
                int32_t noise = dr_noise(&ns);
                if (!v->active) { out[n] = 0; continue; }
                int32_t pulse = dr_pulse(v, volts, pulse_len, c_plp, clamp);
                v->pitch = DM30(v->pitch, c_pitch);
                if ((v->age & 7u) == 0) {
                    float knock = 1.0f + knock0 * ((float)v->pitch * (1.0f / DM_ONE30));
                    dm_svf_set(&v->r1, 172.0f * early * tune * knock, k[2] * decay);
                    dm_svf_set(&v->r2, 339.0f * early * 1.016f * tune * knock, k[3] * decay);
                }
                int32_t in = pulse * 3;
                int32_t body = DM28(dr_bp_raw(&v->r1, in), g_low) + DM28(dr_bp_raw(&v->r2, in), g_high);
                body = DM28(dr_tanh(DM28(body, c09)), c09i);
                v->env = DM30(v->env, c_env);
                v->env2 = DM30(v->env2, c_env2);
                int32_t attack = dr_ramp(v->age, att_step);
                int32_t nh = dm_hp(&v->r3, noise);
                v->aux += DM30(nh - v->aux, c_lp);
                v->out_lp += DM30(v->aux - v->out_lp, c_lp);
                int32_t nenv = DM30(DM30(DM30(v->env, mix1) + DM30(v->env2, mix2), attack), volts << 10);
                int32_t snap = DM28(pulse > 0 ? pulse : 0, g_snap);
                int32_t y = DM28(body, c055) + DM30(DM28(DM30(v->out_lp, nenv), g_n) + snap, snappy);
                if (v->age > end && v->env2 < quiet) v->active = 0;
                out[n] = dr_f20(y);
                ++v->age;
            }
        } else if (part == D8_LT || part == D8_HT) {
            /* ---- tom(): toms and congas */
            int conga = t->pot[part][P_SOUND] == 1, idx = (part - D8_LT) + (conga ? 2 : 0);
            float f0 = tom_hz[idx] * (t->key_on ? t->key_r[part] : dsp_exp2(0.75f * (t->pot[part][P_TUNE] - 64) / 64.0f));
            float scale = dsp_sqrt(93.0f / f0);
            if (fresh) {
                v->lag2 = v->lag1;               /* what still rings fades out instead of clicking */
                v->phase = 0;                    /* the hit starts on -cos */
                v->click = DM_ONE30;
                dm_svf_set(&v->r1, 500.0f * f0 / tom_hz[idx], 1.0f);
            }
            float bend = 0.277f * knob(t, part, P_SNAPPY) * 2.0f * (conga ? 0.35f : 1.0f) * (1.0f + 0.4f * acc);
            float base_f = f0 * (1.0f / EL_SAMPLE_RATE) * 4294967296.0f;
            const uint32_t base = (uint32_t)base_f;
            const int32_t bend_inc = (int32_t)(base_f * bend * (1.0f / 4.0f));   /* /4: Q30 pitch, shift 28 */
            const int32_t c_pitch = dm_q30(decay_coef(MS(42.9f * scale)));
            const int32_t c_env = dm_q30(decay_coef(MS(74.9f * scale * (conga ? 0.65f : 1.0f) * dsp_exp2((t->pot[part][P_DECAY] - 64) / 48.0f))));
            const int32_t c_env3 = dm_q30(tom_env3_c), c_env2 = dm_q30(tom_env2_c), c_lag2 = dm_q30(tom_lag2_c);
            const int32_t att_step = dm_q30(1.0f / MS(0.5f));
            const int32_t amount = dm_q28(2.0f * 0.014f * dsp_exp2((t->pot[part][P_TONE] - 64) / 16.0f));
            const int32_t c_click = dm_q30(dsp_exp(-1.0f / MS(0.3f))), c_clp = dm_q28(0.133f), g_click = dm_q28(0.03f);
            float soft = tv->volts * 14.0f / (4.0f + 10.0f * acc);
            const int32_t g_in = dm_q28(0.76f * (1.0f + 1.2f * acc) * soft), bias_in = dr_q20(0.76f * 0.0025f);
            const int32_t bias = dr_q20(tom_bias), g_out = dm_q28(1.0f + 0.5f * acc), tiny = dr_q20(1e-5f);
            const uint32_t settle = (uint32_t)MS(30.0f);
            for (int n = 0; n < frames; ++n) {
                int32_t noise = dr_noise(&ns);
                if (!v->active) { out[n] = 0; continue; }
                v->pitch = DM30(v->pitch, c_pitch);
                v->phase += base + (uint32_t)dm_mul(bend_inc, v->pitch, 28);
                int32_t body = -dr_sin(v->phase + 0x40000000u);   /* -cos */
                v->env = DM30(v->env, c_env);
                v->env3 = DM30(v->env3, c_env3);
                body = DM30(body, DM30(v->env, DM_ONE30 - v->env3));
                if (!conga) {
                    int32_t attack = dr_ramp(v->age, att_step);
                    v->env2 = DM30(v->env2, c_env2);
                    body += DM30(DM30(DM28(dm_bp(&v->r1, noise), amount), v->env2), attack);
                    int32_t c = v->click >> 10;            /* exp(-age / 0.3 ms), Q20 */
                    v->click = DM30(v->click, c_click);
                    v->out_lp += DM28(c - v->out_lp, c_clp);
                    body += DM28(c - v->out_lp, g_click);
                }
                int32_t y = DM28(dr_tanh(DM28(body, g_in) + bias_in) - bias, g_out);
                v->lag2 = DM30(v->lag2, c_lag2);
                y += v->lag2;
                v->lag1 = y;
                if (v->age > settle && v->env < quiet && (v->lag2 < 0 ? -v->lag2 : v->lag2) < tiny) { v->lag1 = 0; v->active = 0; }
                out[n] = dr_f20(y);
                ++v->age;
            }
        } else if (part == D8_RS) {
            /* ---- rim(): rim shot and claves */
            int clave = t->pot[part][P_SOUND] == 1;
            float tune = t->key_on ? t->key_r[part] : tolerance[part] * spread(t, part, P_TUNE);
            if (fresh) {
                dm_svf_set(&v->r1, 455.0f * tune, 6.7f * tr808_mx_cal[7]);
                dm_svf_set(&v->r2, 1667.0f * tune, 13.5f * tr808_mx_cal[7]);
                dm_svf_set(&v->r3, 2500.0f * tune, 120.0f);
            }
            float gate_ms = (clave ? 25.0f : 22.0f) * spread(t, part, P_DECAY);
            const uint32_t gate_end = (uint32_t)MS(gate_ms), end = (uint32_t)MS(gate_ms + 10.0f);
            const int32_t c_gate = dm_q30(0.0222f), c01 = dm_q28(0.1f), c05 = dm_q28(0.5f), c06 = dm_q28(0.6f), c_lp = dm_q30(0.02f);
            const int32_t c035 = dm_q28(0.35f), c08 = dm_q28(0.8f);
            for (int n = 0; n < frames; ++n) {
                dr_noise(&ns);
                if (!v->active) { out[n] = 0; continue; }
                int32_t pulse = dr_pulse(v, volts, pulse_len, c_plp, clamp);
                int32_t gate = v->age < gate_end ? DM_ONE30 : 0;
                v->env += DM30(gate - v->env, c_gate);
                int32_t y;
                if (clave) y = DM28(dm_bp(&v->r3, DM28(pulse, c035)), c08);
                else {
                    int32_t s = dm_bp(&v->r1, pulse * 2) + dm_bp(&v->r2, pulse * 2);
                    y = s > 0 ? 4 * s : DM28(s, c01);
                    y = DM28(dr_tanh(DM28(y, c05)), c06);
                    v->out_lp += DM30(y - v->out_lp, c_lp);   /* remove the thump (~150 Hz) */
                    y -= v->out_lp;
                }
                y = DM30(y, v->env);
                if (v->age > end) v->active = 0;
                out[n] = dr_f20(y);
                ++v->age;
            }
        } else {
            /* ---- clap(): maracas, and the fitted clap (model 3) */
            if (t->pot[part][P_SOUND] == 1) {
                if (fresh) dm_svf_set(&v->r1, 6000.0f * spread(t, part, P_TUNE), 0.8f);
                float rise = 13.0f, fall = 17.0f * spread(t, part, P_DECAY);
                const uint32_t rise_lim = dr_ceil(MS(rise)), end = (uint32_t)MS(rise + fall + 5.0f);
                const int32_t up = dm_q30(1.0f / MS(rise)), down = dm_q30(1.0f / MS(fall)), g = dm_q28(1.1f * tv->volts);
                const int64_t off = (int64_t)(MS(rise) / MS(fall) * 1073741824.0f);
                for (int n = 0; n < frames; ++n) {
                    int32_t noise = dr_noise(&ns);
                    if (!v->active) { out[n] = 0; continue; }
                    /* e = ms < rise ? ms / rise : max(0, 1 - (ms - rise) / fall) */
                    int32_t e;
                    if (v->age < rise_lim) e = dr_ramp(v->age, up);
                    else { int64_t d = (int64_t)v->age * down - off; e = d >= DM_ONE30 ? 0 : DM_ONE30 - (int32_t)d; }
                    if (v->age > end) v->active = 0;
                    int32_t y = DM28(DM30(DM30(dm_hp(&v->r1, noise), e), e), g);
                    out[n] = dr_f20(y);
                    ++v->age;
                }
            } else {
                const float *k = tr808_cp_cal;
                float decay = spread(t, part, P_DECAY);
                if (fresh) {
                    dm_svf_set(&v->r1, k[0] * spread(t, part, P_TONE) * tolerance[part], k[1]);
                    v->lag1 = v->lag2 = v->out_lp = 0;   /* main, floor, floor's low-pass */
                    v->opening = 0;
                }
                const uint32_t gap = (uint32_t)MS(k[4]), on = (uint32_t)MS(k[5]);
                const int32_t tooth = dm_q30(k[2]), c_tooth = dm_q30(decay_coef(MS(k[3])));
                const int32_t rise = dm_q30(1.0f / dsp_max(1.0f, MS(k[6])));
                const int32_t c_main = dm_q30(decay_coef(MS(k[7]) * decay)), c_floor = dm_q30(decay_coef(MS(k[9]) * decay));
                const int32_t c_lp = dm_q30(1.0f - dsp_exp(-2.0f * DSP_PI * k[10] / EL_SAMPLE_RATE));
                const int32_t g_loud = dm_q28(tv->volts * k[11] * k[12]), g_floor = dm_q28(k[8] * tv->volts * k[11]);
                const int32_t g_out = dm_q28(1.0f / (k[12] * (1.2f / 1.4f)));
                const int32_t floor_quiet = dm_q30(3e-5f / k[8]);
                for (int n = 0; n < frames; ++n) {
                    int32_t noise = dr_noise(&ns);
                    if (!v->active) { out[n] = 0; continue; }
                    uint32_t age = v->age;
                    if (age == 0 || age == gap || age == 2u * gap) v->env = tooth;
                    if (age == on) { v->opening = 1; v->lag2 = DM_ONE30; }
                    int32_t bp = dm_bp(&v->r1, noise);
                    int32_t loud = DM28(DM30(bp, v->env + v->lag1), g_loud);   /* x drive (k12) already */
                    v->env = DM30(v->env, c_tooth);
                    if (v->opening) {                    /* the main clap opens (linear) */
                        v->lag1 += rise;
                        if (v->lag1 >= DM_ONE30) { v->lag1 = DM_ONE30; v->opening = 0; }
                    } else v->lag1 = DM30(v->lag1, c_main);
                    v->lag2 = DM30(v->lag2, c_floor);
                    v->out_lp += DM30(DM28(DM30(bp, v->lag2), g_floor) - v->out_lp, c_lp);
                    if (age > on && !v->opening && v->lag1 < quiet && v->lag2 < floor_quiet) v->active = 0;
                    int32_t y = DM28(dr_tanh(loud), g_out) + v->out_lp;
                    out[n] = dr_f20(y);
                    ++v->age;
                }
            }
        }
        if (!v->active) tv->active = 0;
    }
    return live;
}
#endif
