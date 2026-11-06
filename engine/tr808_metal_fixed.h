/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * The 808's metal voices (cymbal, open and closed hat, cowbell) of tr808.c in fixed
 * point, for the own platform (docs/adr/0022, 0006: the calibrated model
 * stays the sound). Included by tr808.c (its helpers and calibrations).
 *
 * The same model, sample for sample: the six oscillators with PolyBLEP
 * edges and their wandering (the float code's random numbers, in the same
 * order), the cymbal's two band-passes, swing VCAs, envelopes and filters,
 * the hats of model 2 (twice differentiated sum, diode knee, Sallen-Key
 * high-passes, low-passes, DC block). Signals Q20, coefficients Q28,
 * envelopes Q30; products 32 x 32 -> 64 bit (one instruction on the FM-1,
 * like the shift); coefficients once per render call, worked out by the
 * float functions of tr808.c, so they are the float model's own values.
 * Floats only per call, at an oscillator's wrap (its new step) and near
 * an edge (PolyBLEP), a few thousand times a second. */
#if TR808_FIXED && TR808_METAL_FIXED

#define DM_ONE20 (1 << 20)
#define DM_ONE30 (1 << 30)
static inline EL_DSP_HELPER int32_t dm_mul(int32_t a, int32_t b, int s) { return (int32_t)(((int64_t)a * b) >> s); }
#define DM28(a, b) dm_mul((a), (b), 28)
#define DM30(a, b) dm_mul((a), (b), 30)
static inline EL_DSP_HELPER int32_t dm_q28(float x) { return (int32_t)(x * 268435456.0f); }
static inline EL_DSP_HELPER int32_t dm_q30(float x) { return (int32_t)(x * 1073741824.0f); }

/* tanh over +-8 (Q20 in and out): 1/128 steps */
static int32_t dr_tanh_tab[1025];
static inline EL_DSP_HELPER int32_t dr_tanh(int32_t x)
{
    int32_t a = x < 0 ? -x : x;
    if (a >= (8 << 20)) return x < 0 ? -DM_ONE20 : DM_ONE20;
    int32_t i = a >> 13, f = a & 0x1fff;
    int32_t y = dr_tanh_tab[i] + (((dr_tanh_tab[i + 1] - dr_tanh_tab[i]) * f) >> 13);
    return x < 0 ? -y : y;
}
/* the same as a float (DRIVE in tr808_render, the bass drum's) */
static inline EL_DSP_HELPER float dm_tanhf(float x)
{
    if (x >= 8.0f) return 1.0f;
    if (x <= -8.0f) return -1.0f;
    return (float)dr_tanh((int32_t)(x * 1048576.0f)) * (1.0f / DM_ONE20);
}

/* TPT state-variable filter: tr808.c's svf_set worked out in float, the
 * coefficients in Q28, the states Q20. */
typedef struct { int32_t ic1, ic2, a1, a2, a3, k; } dm_svf_t;
static EL_DSP_HELPER void dm_svf_set(dm_svf_t *f, float hz, float q)
{
    tr_svf_t s;
    svf_set(&s, hz, q);
    f->a1 = dm_q28(s.a1); f->a2 = dm_q28(s.a2); f->a3 = dm_q28(s.a3); f->k = dm_q28(s.k);
}
static inline EL_DSP_HELPER int32_t dm_bp(dm_svf_t *f, int32_t x)
{
    int32_t v3 = x - f->ic2, v1 = DM28(f->a1, f->ic1) + DM28(f->a2, v3), v2 = f->ic2 + DM28(f->a2, f->ic1) + DM28(f->a3, v3);
    f->ic1 = 2 * v1 - f->ic1;
    f->ic2 = 2 * v2 - f->ic2;
    return DM28(f->k, v1);
}
static inline EL_DSP_HELPER int32_t dm_hp(dm_svf_t *f, int32_t x)
{
    int32_t v3 = x - f->ic2, v1 = DM28(f->a1, f->ic1) + DM28(f->a2, v3), v2 = f->ic2 + DM28(f->a2, f->ic1) + DM28(f->a3, v3);
    f->ic1 = 2 * v1 - f->ic1;
    f->ic2 = 2 * v2 - f->ic2;
    return x - DM28(f->k, v1) - v2;
}

/* A bank of the six oscillators: phases and steps as 32-bit fractions of a
 * cycle (the float phase times 2^32). The shared bank, and one per hat
 * whose TUNE is off centre (copied from the shared one at its hit). */
/* quiet[i]: samples to come in which oscillator i meets no edge (it only
 * steps; its level stays yc[i]), worked out after each edge; 0: look. */
typedef struct { uint32_t ph[6], inc[6], quiet[6]; int32_t yc[6]; float rinc[6], tune; uint8_t on; } dm_bank_t;
#define DM_DUTY ((uint32_t)(METAL_DUTY * 4294967296.0))
enum { DM_CY, DM_OH, DM_CH, DM_CB, DM_VOICES };

/* Coefficients remembered with their argument (as tr808.c's m_decay /
 * m_exp / m_exp2): the transcendental functions run only when a knob
 * moves, not at every render call. DM_MEMOS a voice; x starts at -1e30. */
enum { DM_MEMOS = 10 };
static inline EL_DSP_HELPER float m_powf(tr_memo_t *m, float a, float b)
{
    if (a != m->x) { m->x = a; m->y = powf_approx(a, b); }
    return m->y;
}
static void dm_memo_init(tr_memo_t *m, int n) { for (int i = 0; i < n; ++i) m[i].x = -1e30f; }

typedef struct {
    int32_t env, env2, env3, att, lag1, lag2, lp1, hp1, out_lp, pink[3], dc_x, dc_y;
    dm_svf_t r1, r2, r3;
    tr_memo_t memo[DM_MEMOS];
    uint32_t age;
    uint8_t active;
} dm_voice_t;

static struct {
    dm_bank_t bank[3];          /* shared, OH's own, CH's own */
    dm_svf_t bp1, bp2;          /* the cymbal's 3.45 and 7.1 kHz band-passes */
    uint32_t cb_ph[2], cb_inc[2];   /* the cowbell's own two rectangles (TUNE off centre or a key) */
    float cb_rinc[2];
    dm_voice_t v[DM_VOICES];
    uint8_t init;
} dm;
static float dm_y[DM_VOICES][EL_BLOCK];

static void dm_reset(void)   /* with tr808_init: set up on the next render */
{
    memset(&dm, 0, sizeof dm);
    for (int k = 0; k < DM_VOICES; ++k) dm_memo_init(dm.v[k].memo, DM_MEMOS);
    for (int i = 0; i <= 1024; ++i) dr_tanh_tab[i] = (int32_t)(dsp_tanh((float)i * (1.0f / 128.0f)) * 1048576.0f);
}

static EL_DSP_HELPER void dm_step_set(tr808_t *t, dm_bank_t *b, int i)
{
    float dt = metal_hz[i] * metal_tol[i] * b->tune * t->metal_mod[i] * (1.0f / EL_SAMPLE_RATE);
    b->inc[i] = (uint32_t)(dt * 4294967296.0f);
    b->rinc[i] = b->inc[i] ? 1.0f / (float)b->inc[i] : 0.0f;
    b->quiet[i] = 0;
}
static EL_DSP_HELPER void dm_retune(tr808_t *t, int i)   /* an oscillator's wandering changed: every bank */
{
    for (int k = 0; k < 3; ++k) if (dm.bank[k].on) dm_step_set(t, &dm.bank[k], i);
}

/* tr808.c's blep, x = the distance from the edge in steps (float, rare) */
static inline EL_DSP_HELPER int32_t dm_half_blep(uint32_t since, uint32_t inc, float rinc)
{
    int32_t s = (int32_t)since;
    float x = (float)s * rinc, b;
    if (s >= 0) b = x + x - x * x - 1.0f;
    else b = x * x + x + x + 1.0f;
    return (int32_t)(b * (0.5f * DM_ONE20));
}

/* One sample of a bank: tr808.c's metal(), its sum / 3 - 1 in Q20. */
static EL_DSP_HELPER int32_t dm_bank_tick(tr808_t *t, dm_bank_t *b, int32_t *cowbell)
{
    int32_t sum = 0, cb = 0;
    for (int i = 0; i < 6; ++i) {
        uint32_t inc = b->inc[i];
        if (b->quiet[i]) {                      /* far from both edges: the same level, no wrap */
            --b->quiet[i];
            b->ph[i] += inc;
            sum += b->yc[i];
            if (i >= 4) cb += b->yc[i];
            continue;
        }
        uint32_t p = b->ph[i] + inc;
        float rinc = b->rinc[i];
        int wrapped = p < b->ph[i];
        b->ph[i] = p;
        if (wrapped && TR808_METAL_JITTER) {    /* this cycle's own small error (for the next sample on) */
            t->metal_jit[i] = 0.0052f * metal_rand(t);
            t->metal_mod[i] = 1.0f + t->metal_drift[i] + t->metal_jit[i];
            dm_retune(t, i);
        }
        int32_t y = p < DM_DUTY ? DM_ONE20 : 0;
        int near = 0;
        if (p < inc || p > 0u - inc) { y += dm_half_blep(p, inc, rinc); near = 1; }
        uint32_t down = p - DM_DUTY;
        if (down < inc || down > 0u - inc) { y -= dm_half_blep(down, inc, rinc); near = 1; }
        sum += y;
        if (i >= 4) cb += y;
        /* the samples until the next one within a step of an edge (with the step
         * from now on: the wandering may just have changed it) */
        uint32_t ni = b->inc[i];
        if (!near && ni) {
            uint32_t d0 = (0u - ni + 1u) - p, d1 = (DM_DUTY - ni + 1u) - p, d = d0 < d1 ? d0 : d1;
            b->quiet[i] = (d - 1u) / ni;
            b->yc[i] = p < DM_DUTY ? DM_ONE20 : 0;
        }
    }
    if (cowbell) *cowbell = 2 * cb - 2 * DM_ONE20;   /* 2 (r4 + r5) - 2 */
    return DM28(sum, dm_q28(1.0f / 3.0f)) - DM_ONE20;
}

/* tr808.c's cowbell_osc(): the 540 and 800 Hz rectangles alone, steady */
static EL_DSP_HELPER int32_t dm_cowbell_osc(void)
{
    int32_t sum = 0;
    for (int i = 0; i < 2; ++i) {
        uint32_t inc = dm.cb_inc[i], p = dm.cb_ph[i] + inc;
        dm.cb_ph[i] = p;
        int32_t y = p < DM_DUTY ? DM_ONE20 : 0;
        if (p < inc || p > 0u - inc) y += dm_half_blep(p, inc, dm.cb_rinc[i]);
        uint32_t down = p - DM_DUTY;
        if (down < inc || down > 0u - inc) y -= dm_half_blep(down, inc, dm.cb_rinc[i]);
        sum += y;
    }
    return 2 * sum - 2 * DM_ONE20;
}

/* tr808.c's swing_vca: r * env / (env + |r|), the division in 32 bits with
 * the denominator normalised to 16 bits (clz, one instruction). */
static inline EL_DSP_HELPER int32_t dm_swing(int32_t x, int32_t env20)
{
    int32_t r = x > 0 ? 4 * x : DM28(x, dm_q28(0.1f));
    int32_t den = env20 + (r > 0 ? r : -r) + 1;   /* + 1e-6 */
    if (env20 <= 0 || den <= 0) return 0;
    int sh = __builtin_clz((uint32_t)den) - 16;
    int32_t e, d;
    if (sh >= 0) { e = env20 << sh; d = den << sh; }
    else { e = env20 >> -sh; d = den >> -sh; }
    int32_t ratio = (e << 15) / d;                 /* Q15, e <= d < 2^16 */
    return dm_mul(r, ratio, 15);
}

/* The six oscillators run on while nothing metal sounds (as tr808_render). */
static EL_DSP_HELPER void dm_idle(tr808_t *t, int frames)
{
    dm_bank_t *b = &dm.bank[0];
    float tune = spread(t, D8_CY, P_TUNE) * (float)frames / EL_SAMPLE_RATE;
    for (int i = 0; i < 6; ++i) {
        float x = metal_hz[i] * metal_tol[i] * tune;
        x -= (float)(int)x;
        b->ph[i] += (uint32_t)(x * 4294967296.0f);
        b->quiet[i] = 0;
    }
}

static int dm_part(int k) { return k == DM_CY ? D8_CY : k == DM_OH ? D8_OH : k == DM_CH ? D8_CH : D8_CB; }

/* Renders the metal voices of this call into dm_y; returns which sound
 * (bit DM_*). tr808_render mixes them like the float voices. */
static EL_DSP_CODE unsigned dm_render(tr808_t *t, int frames)
{
    if (!dm.init) {
        for (int i = 0; i < 6; ++i) dm.bank[0].ph[i] = (uint32_t)(0.137f * (float)i * 4294967296.0f);
        dm_svf_set(&dm.bp1, 3450.0f, 6.0f);
        dm_svf_set(&dm.bp2, 7100.0f, 6.0f);
        dm.init = 1;
    }
    unsigned live = 0;
    for (int k = 0; k < DM_VOICES; ++k) {
        int part = dm_part(k);
        if (!po_engine(t, part) && t->v[part].active) live |= 1u << k;
        else dm.v[k].active = 0;
    }
    if (!live) { dm_idle(t, frames); return 0; }
    const float *ck = tr808_cy_cal;
    /* ---- per call: the shared bank, hits, coefficients */
    dm_bank_t *sb = &dm.bank[0];
    metal_wander(t, frames);
    sb->on = 1;
    sb->tune = spread(t, D8_CY, P_TUNE);
    for (int i = 0; i < 6; ++i) dm_step_set(t, sb, i);
    int32_t c_env[DM_VOICES] = {0}, c_env2 = 0, c_env3 = 0, c_lag1 = 0, c_lag2 = 0, c_out = 0, c_choke[DM_VOICES] = {0};
    int32_t g_low = 0, g_mid = 0, g_hi = 0, d_low = 0, d_high = 0, out_gain[DM_VOICES] = {0}, kv[DM_VOICES] = {0};
    int32_t oh_step = 0, c_att = 0, cb_env2 = 0, cb_att = 0, cb_mix1 = 0, cb_mix2 = 0, cb_edge = 0, cb_asym = 0;
    int cb_own = 0;
    uint32_t oh_start = 0;
    uint8_t own[2] = {0, 0}, fresh[DM_VOICES] = {0};
    for (int k = 0; k < DM_VOICES; ++k) {
        if (!(live & (1u << k))) continue;
        int part = dm_part(k);
        tr_voice_t *tv = &t->v[part];
        dm_voice_t *v = &dm.v[k];
        if (tv->age == 0 || !v->active) {       /* a hit: the envelopes from the top, the filters keep their state */
            v->active = 1;
            v->age = 0;
            v->env = v->env2 = v->env3 = DM_ONE30;
            v->att = DM_ONE30;
            v->lag1 = v->lag2 = 0;
            fresh[k] = 1;
            if (k == DM_CB) {
                /* (its filters: below, with TUNE / the key) */
            } else if (k == DM_CY) {
                dm_svf_set(&v->r1, 2500.0f, 0.97f);
                dm_svf_set(&v->r2, ck[15], 1.0f);
                dm_svf_set(&v->r3, ck[16], 5.64f);
            } else {
                float g = dsp_sqrt(spread(t, part, P_TUNE));   /* (at a hit only) */
                if (part == D8_CH) { const float *c = tr808_ch2_cal; dm_svf_set(&v->r2, c[0] * g, c[1]); dm_svf_set(&v->r3, c[2] * g, c[3]); }
                else { dm_svf_set(&v->r2, 6200.0f * g, 0.9f); dm_svf_set(&v->r3, 5200.0f * g, 0.8f); }
            }
        }
        float acc = tv->accent;
        if (k == DM_CB) {
            const float *cb = tr808_cb_cal;
            if (fresh[k]) {
                float tune = t->key_on ? t->key_r[part] : spread(t, part, P_TUNE);
                dm_svf_set(&v->r1, cb[0] * tune, cb[1]);
                dm_svf_set(&v->r2, cb[3] * tune, 0.7071f);
            }
            float decay = spread(t, part, P_DECAY);
            c_env[k] = dm_q30(m_decay(&v->memo[0], MS(cb[5]) * decay));
            cb_env2 = dm_q30(m_decay(&v->memo[1], MS(cb[6]) * decay));
            cb_att = dm_q30(1.0f / MS(0.7f));
            cb_mix1 = dm_q30(cb[4]); cb_mix2 = dm_q30(1.0f - cb[4]);
            cb_edge = dm_q28(cb[2]);
            kv[k] = dm_q28(0.45f + 0.55f * tv->volts * 14.0f / 4.0f / 3.5f);
            cb_asym = dm_q28(cb[7]);
            out_gain[k] = dm_q28(cb[8]);
            if (t->key_on || t->pot[part][P_TUNE] != 64) {   /* its own rectangles */
                float lo = t->key_on ? t->key_r[part] : spread(t, D8_CY, P_TUNE) * spread(t, part, P_TUNE);
                float hi = t->key_on ? t->key_cb_hi : lo;
                for (int i = 0; i < 2; ++i) {
                    float dt = metal_hz[4 + i] * (i ? hi : lo) * (1.0f / EL_SAMPLE_RATE);
                    dm.cb_inc[i] = (uint32_t)(dt * 4294967296.0f);
                    dm.cb_rinc[i] = dm.cb_inc[i] ? 1.0f / (float)dm.cb_inc[i] : 0.0f;
                }
                cb_own = 1;
            }
            continue;
        }
        if (k == DM_CY) {
            float decay = knob(t, part, P_DECAY), kt = knob(t, part, P_TONE);
            float tone = m_powf(&v->memo[0], kt, ck[14]);
            c_env[k] = dm_q30(m_decay(&v->memo[1], MS(ck[8] + ck[9] * decay)));
            c_env2 = dm_q30(m_decay(&v->memo[2], MS(ck[10] + ck[11] * decay)));
            c_env3 = dm_q30(m_decay(&v->memo[3], MS(ck[4])));
            c_lag1 = dm_q30(1.0f / MS(ck[12]));
            c_lag2 = dm_q30(1.0f / MS(ck[13]));
            d_low = dm_q28(ck[0]);
            d_high = dm_q28(ck[0] * 1.14f);
            g_low = dm_q28(ck[1] * m_exp2(&v->memo[4], -ck[6] * (tone - 0.5f)));
            g_mid = dm_q28(ck[2] * m_exp2(&v->memo[5], ck[7] * (tone - 0.5f)));
            g_hi = dm_q28(ck[3] * m_exp2(&v->memo[6], ck[5] * (tone - 0.5f)));
            c_out = dm_q30(1.0f - m_exp(&v->memo[7], -2.0f * DSP_PI * ck[17] / EL_SAMPLE_RATE));
            out_gain[k] = dm_q28(1.0f + 0.6f * acc);
            kv[k] = dm_q28(0.5f + 0.5f * acc);        /* the reservoirs' height (aux) */
        } else {
            if (part == D8_CH) c_env[k] = dm_q30(m_decay(&v->memo[0], MS(tr808_ch2_cal[5]) * spread(t, part, P_DECAY)));
            else {
                float len = MS(442.0f) * spread(t, part, P_DECAY);
                oh_start = (uint32_t)(0.3f * len);
                oh_step = dm_q30(1.0f / (0.7f * len));
                c_att = dm_q30(m_exp(&v->memo[1], -1.0f / MS(6.0f)));
            }
            if (tv->aux2 > 0) c_choke[k] = dm_q30(m_decay(&v->memo[2], MS(7.0f)));
            kv[k] = dm_q28(4.0f + 3.3f * acc);
            /* the chain's x8 / x5 and the calibration after the (linear) DC block */
            out_gain[k] = dm_q28(part == D8_CH ? 8.0f * tr808_hh2_cal[0] * tr808_ch2_cal[6] : 5.0f * tr808_hh2_cal[1]);
            int h = part == D8_CH;
            if (t->pot[part][P_TUNE] != 64) own[h] = 1;
        }
    }
    for (int h = 0; h < 2; ++h) {
        dm_bank_t *b = &dm.bank[1 + h];
        int part = h ? D8_CH : D8_OH;
        b->on = own[h];
        if (!own[h]) continue;
        b->tune = spread(t, D8_CY, P_TUNE) * spread(t, part, P_TUNE);
        for (int i = 0; i < 6; ++i) dm_step_set(t, b, i);
    }
    const int32_t c_lp1 = dm_q28(0.4513f), c_hp1 = dm_q28(0.5243f), c_hi = dm_q28(2.78f), c_raw = dm_q28(0.714f);
    const int32_t c_ch_lp = dm_q28(1.0f - m_exp(&dm.v[DM_CH].memo[3], -2.0f * DSP_PI * tr808_ch2_cal[4] / EL_SAMPLE_RATE)), c_oh_lp = dm_q28(0.5686f), c_dc = dm_q28(0.9719f), c_fade = dm_q30(0.9925f);
    const int32_t quiet = dm_q30(1e-4f), gone = dm_q30(1e-3f), knee0 = 1 << 24, k059 = (int32_t)(0.59f * (1 << 24));
    const int32_t k12 = (int32_t)(1.2f * (1 << 24)), k06 = (int32_t)(0.6f * (1 << 24)), r24 = dm_q28(1.0f / 2.4f);
    const uint32_t cy_end = (uint32_t)MS(300.0f);
    (void)knee0;
    for (int n = 0; n < frames; ++n) {
        /* ---- sources (tr808_render's order: shared bank, OH's, CH's) */
        int32_t cb_src = 0;
        int32_t m = dm_bank_tick(t, sb, &cb_src), low = 0, high = 0, hat_src[2];
        if (dm.v[DM_CY].active) { low = dm_bp(&dm.bp1, m); high = dm_bp(&dm.bp2, m); }
        hat_src[0] = hat_src[1] = DM28(m, c_raw);
        for (int h = 0; h < 2; ++h) {
            dm_bank_t *b = &dm.bank[1 + h];
            dm_voice_t *v = &dm.v[h ? DM_CH : DM_OH];
            if (!b->on || !v->active) continue;
            if (v->age == 0) { memcpy(b->ph, sb->ph, sizeof b->ph); memset(b->quiet, 0, sizeof b->quiet); }
            hat_src[h] = DM28(dm_bank_tick(t, b, 0), c_raw);
        }
        /* ---- cowbell: tr808.c's cowbell_fitted() (model 1) */
        {
            dm_voice_t *cv = &dm.v[DM_CB];
            if (cv->active) {
                if (cb_own) {
                    if (cv->age == 0) { dm.cb_ph[0] = sb->ph[4]; dm.cb_ph[1] = sb->ph[5]; }
                    cb_src = dm_cowbell_osc();
                }
                int32_t half = cb_src >> 1;
                int32_t y = dm_bp(&cv->r1, half) + DM28(dm_hp(&cv->r2, half), cb_edge);
                cv->env = DM30(cv->env, c_env[DM_CB]);
                cv->env2 = DM30(cv->env2, cb_env2);
                int64_t ar = (int64_t)cv->age * cb_att;
                int32_t attack = ar >= DM_ONE30 ? DM_ONE30 : (int32_t)ar;
                int32_t e = DM30(DM30(cv->env, cb_mix1) + DM30(cv->env2, cb_mix2), attack);
                if (cv->env2 < quiet) cv->active = 0;
                int32_t sv = DM28(DM30(y, e), kv[DM_CB]);
                sv += DM28(dm_mul(sv, sv, 20), cb_asym);                  /* the VCA's even harmonics */
                int32_t o = sv - cv->dc_x + DM30(cv->dc_y, dm_q30(0.9972f));   /* DC block ~20 Hz */
                cv->dc_x = sv; cv->dc_y = o;
                dm_y[DM_CB][n] = (float)DM28(o, out_gain[DM_CB]) * (1.0f / DM_ONE20);
                ++cv->age;
            } else dm_y[DM_CB][n] = 0;
        }
        /* ---- cymbal: tr808.c's cymbal() */
        dm_voice_t *v = &dm.v[DM_CY];
        if (v->active) {
            v->env = DM30(v->env, c_env[DM_CY]);
            v->env2 = DM30(v->env2, c_env2);
            v->env3 = DM30(v->env3, c_env3);
            v->lag1 += DM30(v->env - v->lag1, c_lag1);
            v->lag2 += DM30(v->env2 - v->lag2, c_lag2);
            int32_t peak = kv[DM_CY] << 2;            /* aux, Q28 -> Q30 */
            if (v->age < 5u) peak = dm_q30((0.5f + 0.5f * t->v[D8_CY].accent) * dsp_min(1.0f, (float)v->age / MS(0.1f)));
            int32_t lo = dm_swing(DM28(low, d_low), DM30(v->lag1, peak) >> 10);
            int32_t mid = dm_swing(DM28(high, d_high), DM30(v->lag2, peak) >> 10);
            int32_t hi = dm_swing(DM28(high, d_high), DM30(v->env3, peak) >> 10);
            lo = dm_hp(&v->r1, lo);
            v->lp1 += DM28(lo - v->lp1, c_lp1);
            mid = 2 * dm_hp(&v->r2, mid);
            hi = dm_hp(&v->r3, hi);
            v->hp1 += DM28(hi - v->hp1, c_hp1);
            hi = DM28(hi - v->hp1, c_hi);
            v->out_lp += DM30(DM28(v->lp1, g_low) + DM28(mid, g_mid) + DM28(hi, g_hi) - v->out_lp, c_out);
            dm_y[DM_CY][n] = (float)DM28(v->out_lp, out_gain[DM_CY]) * (1.0f / DM_ONE20);
            if (v->env < quiet && v->env2 < quiet && v->age > cy_end) v->active = 0;
            ++v->age;
        } else dm_y[DM_CY][n] = 0;
        /* ---- hats: tr808.c's hat() and hat_chain(), model 2 */
        for (int h = 0; h < 2; ++h) {
            int k = h ? DM_CH : DM_OH;
            v = &dm.v[k];
            if (!v->active) { dm_y[k][n] = 0; continue; }
            int32_t att;
            if (k == DM_CH) {
                v->env = DM30(v->env, c_env[k]);
                att = v->age < 5u ? dm_q30(dsp_min(1.0f, (float)v->age / MS(0.1f))) : DM_ONE30;
            } else {
                if (v->age >= oh_start) { v->env -= oh_step; if (v->env < 0) v->env = 0; }
                att = DM_ONE30 - v->att;
                v->att = DM30(v->att, c_att);
            }
            if (c_choke[k]) v->env = DM30(v->env, c_choke[k]);
            if (v->age == 0) { v->lag1 = hat_src[h]; v->lag2 = 0; }   /* the bank may have jumped while idle */
            int32_t e = dm_mul(DM30(v->env, att), kv[k], 34);       /* Q24 volts */
            int32_t over = e - k059, g;
            if (over <= 0) g = 0;
            else g = over < k12 ? DM28(dm_mul(over, over, 24), r24) : over - k06;
            int32_t t1 = hat_src[h] - v->lag1;
            v->lag1 = hat_src[h];
            int32_t d = (t1 - v->lag2) * 30;
            v->lag2 = t1;
            int32_t s = dm_hp(&v->r3, dm_hp(&v->r2, dm_mul(d, g, 24)));
            if (k == DM_CH) {
                v->lp1 += DM28(s - v->lp1, c_ch_lp);
                s = v->lp1;
            } else {
                v->lp1 += DM28(s - v->lp1, c_oh_lp); v->pink[0] += DM28(v->lp1 - v->pink[0], c_oh_lp);
                v->pink[1] += DM28(v->pink[0] - v->pink[1], c_oh_lp); v->pink[2] += DM28(v->pink[1] - v->pink[2], c_oh_lp);
                v->hp1 += DM28(v->pink[2] - v->hp1, c_oh_lp);
                s = v->hp1;
            }
            int32_t o = DM28(v->dc_y + s - v->dc_x, c_dc);           /* DC block 200 Hz */
            v->dc_x = s; v->dc_y = o;
            int32_t y = DM28(o, out_gain[k]);
            if (v->env < quiet) {                                      /* fade out, no step at the end */
                v->env3 = DM30(v->env3, c_fade);
                if (v->env3 < gone) v->active = 0;
            }
            dm_y[k][n] = (float)DM30(y, v->env3) * (1.0f + 0.6f * t->v[dm_part(k)].accent) * (1.0f / DM_ONE20);
            ++v->age;
        }
    }
    (void)fresh;
    /* the float voices' flags: the mixer reads dm_y for this call, then they are off */
    for (int k = 0; k < DM_VOICES; ++k)
        if ((live & (1u << k)) && !dm.v[k].active) t->v[dm_part(k)].active = 0;
    return live;
}
#endif
