/* SPDX-License-Identifier: GPL-3.0-only
 * sh101: Port von MonoBassCore (Swift) nach C99. Gleiche Formeln, Konstanten und Reihenfolge wie die Vorlage;
 * Abschnittsnummern beziehen sich auf docs/SOUND-ENGINE.md der Vorlage. Kommentare nennen die Swift-Stelle. */
#include "sh101.h"
#include <math.h>
#include <string.h>

/* Swift Float.pi ist zur Null hin gerundet (0x1.921fb4p+1), nicht zum nächsten Wert */
#define PI_F 0x1.921fb4p+1f
#define BLEP_TAPS 16
#define BLEP_OS 128
#define BLEP_LEN (BLEP_TAPS * BLEP_OS)

/* ---------------- Tabellen (einmal berechnet, für alle Instanzen gleich) ---------------- */

static float blep_table[BLEP_LEN + 1];
static float sine_table[257], tri_table[257], square_table[257];
static float hb01_steep[8], hb01_mid[4], hb04_steep[8], hb04_mid[4], hb04_wide[4];
static int tables_ready;

/* HalfbandDesign.coefficients (Halfband.swift) */
static void halfband_design(float *out, int count, double transition)
{
    double k = tan((1.0 - transition * 2.0) * M_PI / 4.0);
    k *= k;
    double kk = sqrt(sqrt(1.0 - k * k));
    double e = 0.5 * (1.0 - kk) / (1.0 + kk);
    double e4 = e * e * e * e;
    double q = e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));
    double order = (double)(count * 2 + 1);
    for (int index = 0; index < count; ++index) {
        double c = (double)(index + 1);
        double num = 0.0, sign = 1.0;
        int i = 0;
        for (;;) {
            double acc = pow(q, (double)(i * (i + 1))) * sin((double)(i * 2 + 1) * c * M_PI / order) * sign;
            num += acc; sign = -sign; i += 1;
            if (fabs(acc) <= 1e-100) break;
        }
        double den = 0.0;
        i = 1; sign = -1.0;
        for (;;) {
            double acc = pow(q, (double)(i * i)) * cos((double)(i * 2) * c * M_PI / order) * sign;
            den += acc; sign = -sign; i += 1;
            if (fabs(acc) <= 1e-100) break;
        }
        double ww = num * pow(q, 0.25) / (den + 0.5);
        double wwsq = ww * ww;
        double x = sqrt((1.0 - wwsq * k) * (1.0 - wwsq / k)) / (1.0 + wwsq);
        out[index] = (float)((1.0 - x) / (1.0 + x));
    }
}

static void make_tables(void)
{
    if (tables_ready) return;
    /* BlepTable (Oscillator.swift) */
    static double impulse[BLEP_LEN];
    const int n = BLEP_LEN;
    double center = (double)n / 2.0, total = 0.0;
    for (int i = 0; i < n; ++i) {
        double x = ((double)i - center) / (double)BLEP_OS;
        double s = fabs(x) < 1e-12 ? 1.0 : sin(M_PI * x) / (M_PI * x);
        double w = (double)i / (double)(n - 1);
        double win = 0.42 - 0.5 * cos(2 * M_PI * w) + 0.08 * cos(4 * M_PI * w);
        impulse[i] = s * win;
    }
    for (int i = 0; i < n; ++i) total += impulse[i];
    double acc = 0.0;
    for (int i = 0; i < n; ++i) { acc += impulse[i]; blep_table[i] = (float)(acc / total); }
    blep_table[n] = 1;
    /* LFO-Tabellen (Engine.swift) */
    for (int i = 0; i <= 256; ++i) {
        sine_table[i] = i == 256 ? 0 : sinf((float)i * 0.024543693f);
        if (i < 64) tri_table[i] = (float)i / 64;
        else if (i < 128) tri_table[i] = (float)(128 - i) / 64;
        else if (i < 192) tri_table[i] = -(float)(i - 128) / 64;
        else if (i < 256) tri_table[i] = (float)(i - 256) / 64;
        else tri_table[i] = 0;
        square_table[i] = i < 128 ? 1.0f : (i == 256 ? 1.0f : -1.0f);
    }
    /* HalfbandSet(transition: 0.01) für die Ladder, 0.04 für den Clipper */
    halfband_design(hb01_steep, 8, 0.01);
    halfband_design(hb01_mid, 4, 0.01 * 0.5 + 0.25);
    halfband_design(hb04_steep, 8, 0.04);
    halfband_design(hb04_mid, 4, 0.04 * 0.5 + 0.25);
    halfband_design(hb04_wide, 4, 0.04 * 0.25 + 0.375);
    tables_ready = 1;
}

/* ---------------- Halbband-Ketten (HalfbandChains) ---------------- */

static void hb_init(sh101_halfband_t *h, const float *coefs, int count)
{
    h->n0 = (count + 1) / 2; h->n1 = count / 2;
    for (int i = 0; i < h->n0; ++i) h->c[i] = coefs[i * 2];
    for (int i = 0; i < h->n1; ++i) h->c[h->n0 + i] = coefs[i * 2 + 1];
    memset(h->m, 0, sizeof h->m);
}
static void hb_reset(sh101_halfband_t *h) { memset(h->m, 0, sizeof h->m); }

static inline void hb_run(sh101_halfband_t *h, float a, float b, float *o0, float *o1)
{
    float s0 = a, s1 = b;
    float *m = h->m;
    const float *c = h->c;
    for (int i = 0; i < h->n0; ++i) {
        float y = m[i * 2] + c[i] * (s0 - m[i * 2 + 1]);
        m[i * 2] = s0; m[i * 2 + 1] = y; s0 = y;
    }
    for (int i = 0; i < h->n1; ++i) {
        int j = (h->n0 + i) * 2;
        float y = m[j] + c[h->n0 + i] * (s1 - m[j + 1]);
        m[j] = s1; m[j + 1] = y; s1 = y;
    }
    *o0 = s0; *o1 = s1;
}
static inline void hb_up(sh101_halfband_t *h, float x, float *a, float *b) { hb_run(h, x, x, a, b); }
static inline float hb_down(sh101_halfband_t *h, float first, float second)
{
    float a, b;
    hb_run(h, second, first, &a, &b);
    return (a + b) * 0.5f;
}

/* ---------------- Glätter (Smoother) ---------------- */

static void sm_make(sh101_smoother_t *s, float base) { s->base = base; s->coef = 0; s->g = 1; s->value = 0; }
static void sm_rate(sh101_smoother_t *s, float sr)
{
    s->coef = s->base * sr / 44100; s->g = 1 / (1 + s->coef); s->value = 0;
}
static inline float sm_tick(sh101_smoother_t *s, float x)
{
    if (fabsf(x - s->value) > 1e-7f) s->value = (x + s->value * s->coef) * s->g;
    return s->value;
}

/* ---------------- Oszillator (Oscillator.swift) ---------------- */

static inline void blep_add(sh101_blep_t *bb, float t, float scale)
{
    float frac = t - floorf(t);
    int idx = (int)t;
    for (int i = 0; i < BLEP_TAPS; ++i) {
        if (idx + 1 < BLEP_LEN) {
            float a = blep_table[idx];
            bb->buf[(i + bb->pos) & 15] += scale * (1 - (a + frac * (blep_table[idx + 1] - a)));
        }
        idx += BLEP_OS;
    }
}
static inline float blep_advance(sh101_blep_t *bb)
{
    bb->buf[bb->pos] = 0;
    bb->pos = (bb->pos + 1) & 15;
    return bb->buf[bb->pos];
}

static inline float pulse_next(sh101_pulse_t *o, float input, float freq, float width)
{
    float p = input;
    if (p >= 1) {
        p -= 1;
        o->sign = 1;
        float t = (input * o->sr - o->sr) * (float)BLEP_OS / freq;
        blep_add(&o->blep, t, o->sign);
    }
    o->phase = p;
    float shrink = 1 - 2 * freq / o->sr;
    float threshold = 0.5f + shrink * (width - 0.5f);
    if (threshold <= p && o->sign == 1) {
        o->sign = -1;
        float since = fminf(o->sr * (p - threshold) / freq, 1);
        blep_add(&o->blep, since * (float)BLEP_OS, o->sign);
    }
    float b = blep_advance(&o->blep);
    return b - 0.5f * o->sign;
}

static void vco_rate(sh101_vco_t *v, float rate)
{
    v->sr = rate; v->inv_sr = 1 / rate;
    v->pulse.sr = rate; v->sub.sr = rate; v->sub2.sr = rate;
    v->noise_a = 11025 / rate;
    v->wander_coef = 3 * rate / 44100; v->wander_g = 1 / (1 + v->wander_coef);
    v->n1 = 0; v->n2 = 0;
}

static void vco_make(sh101_vco_t *v)
{
    memset(v, 0, sizeof *v);
    v->sr = 44100; v->inv_sr = 1.0f / 44100; v->freq = 440;
    v->pulse.sr = v->sub.sr = v->sub2.sr = 44100;
    v->pulse.sign = v->sub.sign = v->sub2.sign = 1;
    v->wander_coef = 3; v->wander_g = 0.25f;
    v->seed = 1; v->noise_a = 0.25f;
}

static inline int32_t lcg(int32_t seed) { return (int32_t)((uint32_t)seed * 16807u); }

static void vco_align(sh101_vco_t *v)
{
    if (fabsf(v->p0 - 2 * v->p1) < 0.6f) v->p1 = v->p0 * 0.5f;
    if (fabsf(v->p0 - 4 * v->p2) < 0.6f) v->p2 = v->p0 * 0.25f;
}

static inline float vco_process(sh101_vco_t *v, float note, float env, float drift, const sh101_shared_t *s)
{
    v->freq = 440 * exp2f((drift + note + v->wander) / 12 - 5.75f);
    float inc = fminf(v->freq * v->inv_sr, 0.5f);
    if (inc <= 1e-7f) inc = 1e-7f;
    v->p0 += inc; v->p1 += inc * 0.5f; v->p2 += inc * 0.25f;

    float width = 0.496f;
    if (s->pwm > 0) {
        float base;
        switch (s->pwm_mode) {
        case 1: base = 0.47f; break;
        case 2: base = s->lfo_tri * 0.245f + 0.245f; break;
        default: base = env * 0.49f; break;
        }
        width = base * s->pwm * 0.992f + 0.496496f;
    }
    float pulse_out = pulse_next(&v->pulse, v->p0, v->freq, width);
    v->out_pulse = pulse_out;

    float sp = v->p0;
    if (sp >= 1) {
        sp -= 1;
        float t = (v->p0 * v->sr - v->sr) * (float)BLEP_OS / v->freq;
        blep_add(&v->saw, t, 1);
    }
    v->saw_phase = sp;
    float saw_out = inc * (float)BLEP_TAPS * -0.45f - 0.5f + v->saw_phase;
    saw_out = saw_out + blep_advance(&v->saw);
    v->out_saw = saw_out;

    float sub_out;
    switch (s->sub_mode) {
    case 1: sub_out = pulse_next(&v->sub, v->p2, v->freq * 0.25f, 0.50007f); break;
    case 2: sub_out = pulse_next(&v->sub, v->p1, v->freq * 0.5f, 0.5001f); break;
    default: {
        float a = pulse_next(&v->sub, v->p2, v->freq * 0.25f, 0.25007f) * -0.25f;
        sub_out = a - pulse_next(&v->sub2, v->p2, v->freq * 0.25f, 0.7507f);
    } break;
    }
    v->out_sub = sub_out;

    if (v->p0 >= 1) {
        v->seed = lcg(v->seed);
        float target = (float)v->seed * 4.6566128e-12f;
        if (fabsf(target - v->wander) > 1e-7f) v->wander = (target + v->wander * v->wander_coef) * v->wander_g;
        v->p0 -= 1;
    }
    if (v->p1 >= 1) v->p1 -= 1;
    if (v->p2 >= 1) v->p2 -= 1;

    v->seed = lcg(v->seed);
    float noise = (float)v->seed * 4.656613e-10f;
    if (!s->white_noise) {
        v->n1 += v->noise_a * (noise - v->n1);
        v->n2 += v->noise_a * (v->n1 - v->n2);
        noise = v->n2;
    }
    noise += noise;
    v->out_noise = noise;
    float floor_ = s->noise_floor ? 1e-8f : 1e-9f;
    return (floor_ + s->noise_vol) * noise + pulse_out * s->pulse_vol + s->saw_vol * saw_out * 1.2f
           + s->sub_vol * sub_out * 0.65f;
}

/* ---------------- Hüllkurve (Envelope, double) ---------------- */

static void env_make(sh101_env_t *e)
{
    memset(e, 0, sizeof *e);
    e->state = 4; e->k = 44100 * 3.5; e->fall = 20;
    e->last_a = e->last_d = e->last_r = -1.0;
}
static void env_rate(sh101_env_t *e, double sr)
{
    e->k = sr * 3.5; e->fall = sr / 2205;
    e->last_a = -1; e->last_d = -1; e->last_r = -1;
}
static void env_trigger(sh101_env_t *e)
{
    e->start_level = e->value;
    e->state = (e->value <= 0 || e->state > 2) ? 0 : -1;
}
static inline double env_tick(sh101_env_t *e, int gate, double a, double d, double s, double r)
{
    if (e->last_a != a) { e->last_a = a; e->c_a = (a * 1.4 + 0.00028) * e->k; }
    if (e->last_d != d) { e->last_d = d; e->c_d = (d * 2.5 + 0.0005) * e->k; }
    if (e->last_r != r) { e->last_r = r; e->c_r = (r * 2.8 + 0.000224) * e->k; }
    if (!gate) e->state = 3;
    switch (e->state) {
    case 0:
        e->value = (e->c_a * 0.8 * e->value + 1.6) / (e->c_a * 0.8 + 1);
        if (e->value >= 1) { e->state = 1; e->value = 1; }
        break;
    case 1:
    case 2: {
        double f = e->value <= 0.8 ? 0.8 : 4.84 - 4.8 * e->value;
        double y = (s - 1e-6 + e->value * f * e->c_d) / (e->c_d * f + 1);
        e->value = s > y ? s : y;
    } break;
    case 3: {
        double g = 1 - e->value;
        double f = 0.6 - 0.4 * g * g * g;
        e->value = (e->value * f * e->c_r - 0.0001) / (e->c_r * f + 1);
        if (e->value <= 0) { e->state = 4; e->value = 0; }
    } break;
    case -1:
        e->value = e->fall * e->value / (e->fall + 1);
        if (e->value <= 0.4 || e->skip_fall) e->state = 0;
        break;
    default: break;
    }
    return e->value;
}

/* ---------------- Filter (Ladder, 4× überabgetastet) ---------------- */

static void ladder_make(sh101_ladder_t *l)
{
    memset(l, 0, sizeof *l);
    l->sr = 44100;
    sm_make(&l->note_s, 4);
    hb_init(&l->up1, hb01_mid, 4); hb_init(&l->up2, hb01_steep, 8);
    hb_init(&l->down1, hb01_steep, 8); hb_init(&l->down2, hb01_mid, 4);
}
static void ladder_rate(sh101_ladder_t *l, float rate)
{
    l->sr = rate;
    l->limit = (rate * 1.5f + 1000) / 4;
    l->s0 = l->s1 = l->s2 = l->s3 = l->u_prev = 0;
    sm_rate(&l->note_s, rate);
    hb_reset(&l->up1); hb_reset(&l->up2); hb_reset(&l->down1); hb_reset(&l->down2);
}
static inline float conductance(float m)
{
    float m2 = m * m, m4 = m2 * m2, m8 = m4 * m4;
    return (m8 + 105 * m4 + 200) / (6 * m8 + 220 * m4 + 200);
}
static inline float ladder_step(sh101_ladder_t *l, float u, float g, float k, int twelve)
{
    float t = (l->u_prev + u) * 2 + 0.14f + 28 * k * l->s3;
    float a0 = (t * t + 50) / (t * t * 15 + 50);
    float g0 = conductance(l->s0 - 0.6f * a0 + 0.14f);
    float g1 = conductance(l->s1 - 0.6f * l->s0 + 0.14f);
    float g2 = conductance(l->s2 - 0.6f * l->s1 + 0.14f);
    float g3 = conductance(l->s3 - 0.6f * l->s2 + 0.14f);
    float h0 = 1 / (g * g0 + 1), h1 = 1 / (g * g1 + 1), h2 = 1 / (g * g2 + 1), h3 = 1 / (g * g3 + 1);
    float A = h3 * g * g3 * h2;
    float B = A * g * g2 * h1;
    float C = B * g * g1 * h0;
    float D = C * a0 * g * g0;
    float y3 = (h3 * l->s3 + A * l->s2 + B * l->s1 + C * l->s0 + D * u) / (k * D + 1);
    float x0 = (u - k * y3) * a0;
    float v0 = (l->s0 + g * g0 * x0) * h0;
    float v1 = (l->s1 + g * g1 * v0) * h1;
    float v2 = (l->s2 + g * g2 * v1) * h2;
    float g2x = g + g;
    l->s0 += (x0 - v0) * g0 * g2x;
    l->s1 += (v0 - v1) * g1 * g2x;
    l->s2 += (v1 - v2) * g2 * g2x;
    l->s3 += (v2 - y3) * g3 * g2x;
    l->u_prev = u;
    return twelve ? v1 : y3;
}
static inline float ladder_process(sh101_ladder_t *l, float x, float cutoff_mod, float pitch, float fm,
                                   const sh101_shared_t *s)
{
    l->fm_smoothed = fm == 0 ? 0 : (l->fm_smoothed + fm * 3) * 0.25f;
    float note = sm_tick(&l->note_s, pitch - 12);
    float fc = exp2f(12.425205f * (cutoff_mod + 0.15f)) / 4;
    fc *= exp2f(4 * l->fm_smoothed + s->keyboard_s * (note - 36) / 12);
    float ratio = fc / l->limit;
    float comp = 0.21f;
    if (ratio <= 0.7f) comp = ratio * 0.6f - 0.21f;
    if (ratio <= 0.35f) comp = 0;
    fc = fminf(fc, l->limit);
    float g = tanf(PI_F * fc / l->sr);
    float res = s->resonance;
    float res_eff = res - res * comp;
    float k = res_eff * 5.714286f;
    float one_minus = 1 - res;
    float om4 = one_minus * one_minus * one_minus * one_minus;
    float gain = res * 0.05525f + 0.03575f;
    if (s->volume_correction) gain *= fabsf(res - 0.5f) * -1.5f + 1.75f;

    float e, o, a, b, c, d;
    hb_up(&l->up1, x, &e, &o);
    hb_up(&l->up2, e, &a, &b);
    hb_up(&l->up2, o, &c, &d);
    float first = (a * 0.25f + a * 0.25f * ((om4 - om4 * res) * 0.37f - 0.37f) * comp) * gain;
    float y0 = ladder_step(l, first, g, k, s->filter12);
    float y1 = ladder_step(l, b * 0.25f * gain, g, k, s->filter12);
    float y2 = ladder_step(l, c * 0.25f * gain, g, k, s->filter12);
    float y3 = ladder_step(l, d * 0.25f * gain, g, k, s->filter12);
    float h0 = hb_down(&l->down1, y0, y1);
    float h1 = hb_down(&l->down1, y2, y3);
    return hb_down(&l->down2, h0, h1);
}

/* ---------------- Stimme (Voice) ---------------- */

static void voice_make(sh101_voice_t *v)
{
    memset(v, 0, sizeof *v);
    v->sr = 44100;
    vco_make(&v->vco);
    ladder_make(&v->filter);
    env_make(&v->vca_env); env_make(&v->filter_env);
    sm_make(&v->cutoff_s, 1000); sm_make(&v->vel_s, 4); sm_make(&v->bend_s, 1000);
    v->porta_current = 42; v->porta_unit = 1; v->porta_last_i = -1;
    v->porta_active = 1;
    v->note = 60; v->note_pitch = 60; v->pitch = 60;
    v->tail_length = 10000;
}
static void voice_rate(sh101_voice_t *v, float rate)
{
    v->sr = rate;
    vco_rate(&v->vco, rate);
    ladder_rate(&v->filter, rate);
    env_rate(&v->vca_env, (double)rate); env_rate(&v->filter_env, (double)rate);
    sm_rate(&v->cutoff_s, rate); sm_rate(&v->vel_s, rate); sm_rate(&v->bend_s, rate);
    v->porta_unit = (double)rate / 44100;
    v->porta_last_i = -1;
    float g = tanf(PI_F * 6.2f / rate);
    v->hp_a = g / (g + 1); v->hp_s = 0;
    v->tail_length = (int)((double)rate * 0.22675736961451248);
}
static int voice_sounding(const sh101_voice_t *v) { return v->active || v->vca_env.state != 4; }

static void voice_note_on(sh101_voice_t *v, int n, int legato, float vel, int no_retrig, const sh101_shared_t *s)
{
    v->tail = 0;
    if (v->vca_env.state == 4) {
        vco_align(&v->vco);
        v->bend_s.value = s->bender_dco * 12 * s->pitch_wheel;
        v->no_retrigger = 0;
        v->snap_velocity = 1;
        v->legato_from_release = 0;
        v->vca_env.start_level = v->vca_env.value;
        v->vca_env.state = 0;
        env_trigger(&v->filter_env);
        v->velocity = vel;
    } else if (!v->no_retrigger) {
        env_trigger(&v->vca_env);
        env_trigger(&v->filter_env);
        v->velocity = vel;
    }
    if (!v->legato_from_release) v->cutoff_s.value = s->cutoff + s->bender_filter * s->pitch_wheel;
    v->note_pitch = (n >= 0 && n <= 128) ? (float)n : 60;
    v->porta_active = s->porta_mode;
    switch (s->porta_mode) {
    case 2:
        if (!(v->active || v->legato_from_release)) v->velocity = vel;
        break;
    case 1:
        v->porta_current = (double)v->note_pitch; v->velocity = vel;
        break;
    default:
        if (!(v->active || v->legato_from_release)) { v->porta_current = (double)v->note_pitch; v->velocity = vel; }
        break;
    }
    v->active = 1;
    v->note = n;
    v->released = 0;
    v->legato_from_release = legato;
    v->no_retrigger = no_retrig;
}

static inline float voice_process(sh101_voice_t *v, const sh101_shared_t *s)
{
    if (v->vca_env.state == 4) {
        v->filter_env.value = 0;
        v->tail += 1;
        if (v->tail < v->tail_length) (void)vco_process(&v->vco, v->note_pitch, 1, s->drift, s);
        return 0;
    }
    int vca_gate = s->vca_env_mode;
    float fenv = 0;
    if (v->active || v->filter_env.state != 4) {
        int gate = v->active && !v->released;
        float r_add = vca_gate ? 0.00014f : 0.00028f;
        fenv = (float)env_tick(&v->filter_env, gate, (double)(s->attack_s + 0.00017f), (double)(0.8f * s->decay_s),
                               (double)s->sustain_s, (double)(s->release + r_add));
    }
    double target = (double)v->note_pitch;
    if (v->porta_active == 1 || v->porta_current == target) {
        v->porta_current = target;
    } else {
        double i = (double)s->porta_intensity;
        if (v->porta_last_linear != s->porta_linear) { v->porta_last_i = -1; v->porta_last_linear = s->porta_linear; }
        if (!s->porta_linear) {
            if (v->porta_last_i != i) { v->porta_last_i = i; v->porta_exp = i * i * 40000 * i * v->porta_unit; }
            v->porta_current = (target + v->porta_current * v->porta_exp) / (v->porta_exp + 1);
        } else {
            if (v->porta_last_i != i) {
                v->porta_last_i = i;
                double b = i * -0.1 + 0.11;
                b *= b;
                v->porta_lin = (b * b * b * 10000 + 9.99999993922529e-05) / v->porta_unit;
            }
            if (v->porta_current <= target) {
                double y = v->porta_current + v->porta_lin;
                v->porta_current = target < y ? target : y;
            } else {
                double y = v->porta_current - v->porta_lin;
                v->porta_current = target > y ? target : y;
            }
        }
    }
    v->pitch = s->octave_semis + (float)v->porta_current * 0.999955f + s->mod_wheel * s->lfo_sine * s->bender_mod * 12;
    float bend = sm_tick(&v->bend_s, s->bender_dco * 12 * s->pitch_wheel);
    float vco_note = s->fine_tune - 0.5f + s->dco_lfo * s->lfo + v->pitch + bend + s->range_semis;
    float osc = vco_process(&v->vco, vco_note, fenv, s->drift, s);

    float cut = sm_tick(&v->cutoff_s, s->cutoff + s->bender_filter * s->pitch_wheel);
    float env_amt = s->env_amount + v->velocity * s->vel_envelope;
    float fm = 0;
    if (s->fm_intensity_s > 0) {
        float src;
        switch (s->fm_source) {
        case 1: src = v->vco.out_pulse; break;
        case 2: src = v->vco.out_saw; break;
        case 3: src = v->vco.out_sub; break;
        case 4: src = v->vco.out_noise; break;
        default: src = 0; break;
        }
        if (src != 0) fm = s->fm_intensity_s * s->fm_intensity_s * src;
    }
    float filtered = ladder_process(&v->filter, osc, cut + s->lfo * s->filter_lfo + fenv * env_amt, v->pitch, fm, s);
    float dv = (filtered - v->hp_s) * v->hp_a;
    float lp = v->hp_s + dv;
    v->hp_s = lp + dv;
    float hp = filtered - lp;

    float vel_gain = (1 - s->vel_volume) + v->velocity * s->vel_volume;
    if (v->snap_velocity) { v->snap_velocity = 0; v->vel_s.value = vel_gain; }
    v->vca_env.skip_fall = !vca_gate || s->declick;
    float dc = s->declick ? 0.0015f : 0;
    int gate = v->active && !v->released;
    if (vca_gate) {
        float extra = dc + 0.0001f;
        (void)env_tick(&v->vca_env, gate, (double)(extra + s->attack_s), (double)(0.8f * s->decay_s),
                       (double)s->sustain_s, (double)(s->release + extra));
    } else {
        (void)env_tick(&v->vca_env, gate, (double)dc, (double)(0.8f * s->decay_s), 1, (double)(dc * 0.5f + 5e-5f));
    }
    float vg = sm_tick(&v->vel_s, vel_gain);
    double level = v->vca_env.value - 0.02;
    if (level < 0) level = 0;
    return (float)level * hp * vg;
}

/* ---------------- Parameter (Params.swift) ---------------- */

static const char *const keys[SH101_PARAM_COUNT] = {
    "volume", "masterfinetune", "octavetranspose", "portamentomode", "portamentointensity", "portamentolinear",
    "polymode", "lforate", "lfowaveform", "lfotrigger", "lfosync", "lfoinverted", "dcolfovalue", "dcolfovaluesnap",
    "dcopwmvalue", "dcopwmmode", "dcorange", "pulsevolume", "sawvolume", "suboscvolume", "suboscmode", "noisevolume",
    "whitenoiseenabled", "noisefloor", "filtercutoff", "filterresonance", "filterenvelopevalue",
    "filtermodulationvalue", "filterkeyboardvalue", "filterenvelopevaluefullrange", "filtervolumecorrection",
    "filter12db", "vcamode", "adsrmode", "adsrattack", "adsrdecay", "adsrsustain", "adsrrelease", "adsrdecklick",
    "controlbenderfilter", "controlbenderdco", "controlbendermodulation", "controlvelocityvolume",
    "controlvelocityenvelope", "fmpulse", "fmsaw", "fmsubosc", "fmnoise", "fmintensity", "distortionamount",
    "arpseqsyncenabled", "arpseqtempo", "arpseqlength", "arpenabled", "arpmode", "arpseqsyncmode", "arpseqhold",
    "seqenabled", "arpseqshuffle", "pitchwheel", "modulationwheel"};

const char *sh101_param_key(unsigned p) { return p < SH101_PARAM_COUNT ? keys[p] : ""; }

float sh101_param_max(unsigned p)
{
    if (p == SH101_ARP_MODE) return 6;
    if (p == SH101_ARP_SEQ_SYNC_MODE) return 3;
    return 1;
}

float sh101_param_default(unsigned p)
{
    switch (p) {
    case SH101_VOLUME: case SH101_MASTER_FINE_TUNE: case SH101_LFO_RATE: case SH101_ADSR_DECAY: case SH101_PITCH_WHEEL:
        return 0.5f;
    case SH101_OCTAVE_TRANSPOSE: case SH101_PORTAMENTO_MODE: return 0.6f;
    case SH101_DCO_RANGE: case SH101_ARP_SEQ_TEMPO: return 0.3f;
    case SH101_SAW_VOLUME: case SH101_SUB_OSC_VOLUME: case SH101_FILTER_CUTOFF: case SH101_FILTER_KEYBOARD_VALUE:
    case SH101_ADSR_MODE: case SH101_ARP_SEQ_SYNC_ENABLED:
        return 1;
    case SH101_ARP_SEQ_LENGTH: return 16.0f / 96.0f;
    default: return 0;
    }
}

static uint32_t rnd(sh101_t *e)
{
    e->rng ^= e->rng << 13; e->rng ^= e->rng >> 17; e->rng ^= e->rng << 5;
    return e->rng;
}
/* Ersatz für Float(Int32.random(in: 0...Int32.max)) * 9.313226e-10 - 1 */
static double rnd_bipolar(sh101_t *e) { return (double)((float)(int32_t)(rnd(e) >> 1) * 9.313226e-10f - 1); }

static void update_lfo_rate(sh101_t *e)
{
    static const double sync[32] = {
        1.0 / 3840, 1.0 / 3600, 1.0 / 3360, 1.0 / 3120, 1.0 / 2880, 1.0 / 2640, 1.0 / 2400, 1.0 / 2160,
        1.0 / 1920, 1.0 / 1680, 1.0 / 1440, 1.0 / 1200, 1.0 / 960, 1.0 / 720, 1.0 / 480, 0.002777777777708333,
        0.003125, 1.0 / 240, 0.005555555555416666, 0.00625, 1.0 / 120, 0.011111111110833332, 0.0125, 1.0 / 60,
        0.022222222221666664, 0.025, 1.0 / 30, 0.04444444444333333, 0.05, 1.0 / 15, 0.08888888888666666, 0.1};
    if (e->lfo_sync) {
        int i = (int)(e->lfo_rate_param * 31 + 0.5f);
        if (i > 31) i = 31;
        if (i < 0) i = 0;
        e->lfo_hz = e->tempo * sync[i];
    } else {
        e->lfo_hz = exp2((double)e->lfo_rate_param * 4.906890595608519) * 1.033793103448276 - 1.013793103448276;
    }
    e->lfo_inc = e->lfo_hz * 255 / (double)e->sample_rate;
}

#if SH101_WITH_ARPSEQ
static void update_clock_rate(sh101_t *e)
{
    static const double f[12] = {0.5, 0.6666667, 0.75, 1, 1.3333334, 1.5, 2, 2.6666667, 3, 3, 4, 4.5};
    if (e->values[SH101_ARP_SEQ_SYNC_ENABLED] > 0.5f) {
        int i = (int)(e->arp_rate_param * 11 + 0.5f);
        if (i > 11) i = 11;
        if (i < 0) i = 0;
        e->clock_inc = f[i] * 4 * e->tempo / ((double)e->sample_rate * 60);
    } else {
        double per_second = (exp((double)e->arp_rate_param * 2.995732273553991) * 0.05263157894736842
                             - 0.05263157894736842) * 24 + 1;
        e->clock_inc = per_second / (double)e->sample_rate;
    }
}

static int cmp_up(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }
static void sort_ints(int *x, int n, int down)
{
    for (int i = 1; i < n; ++i) { /* Einfügesortierung: wenige Noten */
        int v = x[i], j = i - 1;
        while (j >= 0 && (down ? x[j] < v : cmp_up(&x[j], &v) > 0)) { x[j + 1] = x[j]; --j; }
        x[j + 1] = v;
    }
}

static void sort_arp(sh101_t *e)
{
    int mode = (int)(e->values[SH101_ARP_MODE] + 0.5f);
    int *list = e->arp_sorted;
    int n = e->arp_n;
    memcpy(list, e->arp_notes, sizeof(int) * (size_t)n);
    switch (mode) {
    case 0: case 1: case 2: sort_ints(list, n, 0); break;
    case 3: sort_ints(list, n, 1); break;
    case 6:
        for (int i = 0; i < n / 2; ++i) { int t = list[i]; list[i] = list[n - 1 - i]; list[n - 1 - i] = t; }
        break;
    default: break;
    }
    int m = n;
    if (mode == 1 && n > 2) for (int i = n - 2; i >= 1; --i) list[m++] = list[i];
    if (mode == 2 && n >= 1) for (int i = n - 1; i >= 0; --i) list[m++] = list[i];
    e->arp_sorted_n = m;
}
#endif

static int arp_or_seq(const sh101_t *e)
{
#if SH101_WITH_ARPSEQ
    return (e->values[SH101_ARP_ENABLED] > 0.5f || e->values[SH101_SEQ_ENABLED] > 0.5f) && !e->seq_record;
#else
    (void)e;
    return 0;
#endif
}

static void all_inactive(sh101_t *e)
{
    for (int i = 0; i < SH101_VOICES; ++i) e->voices[i].active = 0;
}

/* SynthEngine.apply (parameterChanged) */
static void apply(sh101_t *e, unsigned p, float v)
{
    sh101_shared_t *s = &e->s;
    switch (p) {
    case SH101_VOLUME: e->volume = (exp2f(v * 4.321928094887363f) - 1) * 7.2f; break;
    case SH101_FILTER_CUTOFF: s->cutoff = v; break;
    case SH101_FILTER_RESONANCE: s->resonance = exp2f(v) - 1; break;
    case SH101_FILTER_MODULATION_VALUE:
        s->filter_lfo = (exp2f(v * 4.321928f) * 0.05263158f - 0.05263158f) * 0.5f;
        break;
    case SH101_FILTER_ENVELOPE_VALUE:
    case SH101_FILTER_ENVELOPE_VALUE_FULL_RANGE: {
        float en = exp2f(e->values[SH101_FILTER_ENVELOPE_VALUE]) - 1;
        s->env_amount = e->values[SH101_FILTER_ENVELOPE_VALUE_FULL_RANGE] >= 0.5f ? en : en * 0.63f;
    } break;
    case SH101_ADSR_ATTACK: s->attack = v * 0.1f + v * v * v * 0.9f; break;
    case SH101_ADSR_DECAY: s->decay = v * 0.1f + v * v * v * 0.9f; break;
    case SH101_ADSR_SUSTAIN: s->sustain = v; break;
    case SH101_ADSR_RELEASE: s->release = v * v * 0.9f + v * v * v * 0.1f; break;
    case SH101_LFO_RATE: e->lfo_rate_param = v; update_lfo_rate(e); break;
    case SH101_LFO_SYNC: e->lfo_sync = v > 0.5f; update_lfo_rate(e); break;
    case SH101_LFO_TRIGGER: e->lfo_trigger = v > 0.5f; break;
    case SH101_LFO_INVERTED: e->lfo_invert = v > 0.5f ? -1 : 1; break;
    case SH101_LFO_WAVEFORM: e->lfo_wave = (int)(v * 3 + 0.5f); break;
    case SH101_PORTAMENTO_MODE: s->porta_mode = (int)(v * 2 + 0.5f); break;
    case SH101_PORTAMENTO_INTENSITY: s->porta_intensity = v; break;
    case SH101_PORTAMENTO_LINEAR: s->porta_linear = v > 0.5f; break;
    case SH101_OCTAVE_TRANSPOSE: {
        static const float o[3] = {-12, 0, 12};
        int i = (int)(v * 2 + 0.5f);
        if (i < 3) s->octave_semis = o[i];
    } break;
    case SH101_FILTER_KEYBOARD_VALUE: s->keyboard = v; break;
    case SH101_FILTER_VOLUME_CORRECTION: s->volume_correction = v > 0.5f; break;
    case SH101_POLYMODE: {
#if SH101_WITH_POLY
        int n = v <= 0 ? 1 : 6;
#else
        int n = 1;
#endif
        if (n != e->voice_count) { e->voice_count = n; all_inactive(e); }
    } break;
    case SH101_DCO_PWM_VALUE: s->pwm = v < 0.25f ? v * v * 4 : v; break;
    case SH101_SUB_OSC_MODE: s->sub_mode = (int)(v * 2 + 0.5f); break;
    case SH101_DCO_RANGE: {
        static const float r[4] = {-24, -12, 0, 12};
        int i = (int)(v * 3 + 0.5f);
        if (i < 4) s->range_semis = r[i];
    } break;
    case SH101_FM_INTENSITY: s->fm_intensity = v; break;
    case SH101_FM_PULSE: case SH101_FM_SAW: case SH101_FM_SUB_OSC: case SH101_FM_NOISE:
        if (e->values[SH101_FM_PULSE] > 0.5f) s->fm_source = 1;
        else if (e->values[SH101_FM_SAW] > 0.5f) s->fm_source = 2;
        else if (e->values[SH101_FM_SUB_OSC] > 0.5f) s->fm_source = 3;
        else if (e->values[SH101_FM_NOISE] > 0.5f) s->fm_source = 4;
        else s->fm_source = 0;
        break;
    case SH101_CONTROL_BENDER_FILTER: s->bender_filter = v; break;
    case SH101_CONTROL_BENDER_DCO: s->bender_dco = v; break;
    case SH101_CONTROL_BENDER_MODULATION: s->bender_mod = v; break;
    case SH101_MASTER_FINE_TUNE: s->fine_tune = v; break;
    case SH101_DCO_PWM_MODE: s->pwm_mode = (int)(v * 2 + 0.5f); break;
    case SH101_WHITE_NOISE_ENABLED: s->white_noise = v > 0.5f; break;
    case SH101_VCA_MODE: s->vca_env_mode = v > 0.5f; break;
    case SH101_ADSR_MODE: s->adsr_mode = (int)(v * 2 + 0.5f); break;
    case SH101_CONTROL_VELOCITY_VOLUME: s->vel_volume = v; break;
    case SH101_CONTROL_VELOCITY_ENVELOPE: s->vel_envelope = v; break;
    case SH101_ADSR_DECLICK: s->declick = v > 0.5f; break;
    case SH101_PULSE_VOLUME: s->pulse_vol = v; break;
    case SH101_SAW_VOLUME: s->saw_vol = v; break;
    case SH101_SUB_OSC_VOLUME: s->sub_vol = v; break;
    case SH101_NOISE_VOLUME: s->noise_vol = v; break;
    case SH101_NOISE_FLOOR: s->noise_floor = v > 0.5f; break;
    case SH101_DISTORTION_AMOUNT: e->drive = v; break;
    case SH101_FILTER_12DB: s->filter12 = v >= 0.5f; break;
    case SH101_DCO_LFO_VALUE:
    case SH101_DCO_LFO_VALUE_SNAP: {
        float f = e->values[SH101_DCO_LFO_VALUE];
        if (f < 0.2f) f = f * 5 * f * 5 * 0.2f;
        if (f < 0.5f) f = (float)((pow(10.0, (double)(f + f)) * (1.0 / 9.0) - 1.0 / 9.0) * 0.5);
        float semis = (exp2f(f) - 1) * 24;
        if (e->values[SH101_DCO_LFO_VALUE_SNAP] > 0.5f) semis = (float)(int)(semis + 0.5f);
        s->dco_lfo = semis;
    } break;
    case SH101_PITCH_WHEEL: s->pitch_wheel = v * 2 - 1; break;
    case SH101_MODULATION_WHEEL: s->mod_wheel = v; break;
#if SH101_WITH_ARPSEQ
    case SH101_ARP_SEQ_TEMPO: e->arp_rate_param = v; update_clock_rate(e); break;
    case SH101_ARP_SEQ_SYNC_ENABLED: update_clock_rate(e); break;
    case SH101_ARP_SEQ_LENGTH: { int l = (int)(v * 96); e->seq_length = l < 1 ? 1 : l; } break;
    case SH101_ARP_MODE: sort_arp(e); break;
    case SH101_ARP_ENABLED: case SH101_SEQ_ENABLED: case SH101_ARP_SEQ_HOLD:
        if (v <= 0.5f || p != SH101_ARP_SEQ_HOLD) {
            memcpy(e->arp_notes, e->held, sizeof(int) * (size_t)e->held_n);
            e->arp_n = e->held_n;
            sort_arp(e);
            if (e->held_n == 0) all_inactive(e);
            e->current_step_display = -1;
        }
        break;
#endif
    default: break;
    }
}

void sh101_set(sh101_t *e, unsigned p, float raw)
{
    if (p >= SH101_PARAM_COUNT) return;
    float mx = sh101_param_max(p);
    e->values[p] = raw < 0 ? 0 : raw > mx ? mx : raw;
    e->dirty[p] = 1;
    e->any_dirty = 1;
}
float sh101_get(const sh101_t *e, unsigned p) { return p < SH101_PARAM_COUNT ? e->values[p] : 0; }

void sh101_prepare(sh101_t *e)
{
    if (e->any_dirty) {
        for (unsigned p = 0; p < SH101_PARAM_COUNT; ++p)
            if (e->dirty[p]) { e->dirty[p] = 0; apply(e, p, e->values[p]); }
        e->any_dirty = 0;
    }
#if SH101_WITH_ARPSEQ
    if (e->values[SH101_LFO_SYNC] > 0.5f || e->values[SH101_ARP_SEQ_SYNC_ENABLED] > 0.5f) {
        update_lfo_rate(e); update_clock_rate(e);
    }
#else
    if (e->values[SH101_LFO_SYNC] > 0.5f) update_lfo_rate(e);
#endif
}

void sh101_init(sh101_t *e, double rate)
{
    make_tables();
    memset(e, 0, sizeof *e);
    e->tempo = 120;
    for (unsigned p = 0; p < SH101_PARAM_COUNT; ++p) { e->values[p] = sh101_param_default(p); e->dirty[p] = 1; }
    e->any_dirty = 1;
    e->voice_count = 1;
    sm_make(&e->sm_a, 500); sm_make(&e->sm_d, 500); sm_make(&e->sm_s, 500);
    sm_make(&e->sm_fm, 500); sm_make(&e->sm_kbd, 500); sm_make(&e->sm_vol, 1000);
    e->volume = 25;
    e->lfo_invert = 1; e->lfo_hz = 1;
    e->lfo_seed = 1; e->lfo_noise_a = 0.25f; e->lfo_rate_param = 0.5f;
    e->rng = 0x2545F491u;
    hb_init(&e->dist_up1, hb04_steep, 8); hb_init(&e->dist_up2, hb04_mid, 4); hb_init(&e->dist_up3, hb04_wide, 4);
    hb_init(&e->dist_dn1, hb04_wide, 4); hb_init(&e->dist_dn2, hb04_mid, 4); hb_init(&e->dist_dn3, hb04_steep, 8);
#if SH101_WITH_ARPSEQ
    for (int i = 0; i < SH101_STEPS; ++i) {
        e->steps[i].velocity = 0.5f; e->steps[i].length = 1;
    }
    e->seq_length = 16;
    e->arp_rate_param = 0.3f;
    e->current_step_display = -1;
#endif
    /* Shared-Vorgaben */
    sh101_shared_t *s = &e->s;
    s->saw_vol = 1; s->sub_vol = 1; s->range_semis = -12;
    s->cutoff = 1; s->keyboard = 1; s->keyboard_s = 1; s->adsr_mode = 1; s->porta_mode = 1;
    for (int i = 0; i < SH101_VOICES; ++i) voice_make(&e->voices[i]);
    /* setSampleRate */
    float sr = (float)(rate > 0 ? rate : 44100);
    e->sample_rate = sr;
    for (int i = 0; i < SH101_VOICES; ++i) voice_rate(&e->voices[i], sr);
    sm_rate(&e->sm_a, sr); sm_rate(&e->sm_d, sr); sm_rate(&e->sm_s, sr);
    sm_rate(&e->sm_fm, sr); sm_rate(&e->sm_kbd, sr); sm_rate(&e->sm_vol, sr);
    e->d_rate = 261.6159f / sr;
    e->lfo_noise_a = 11025 / sr;
    update_lfo_rate(e);
#if SH101_WITH_ARPSEQ
    update_clock_rate(e);
#endif
}

/* ---------------- Tasten (VoiceController) ---------------- */

static void list_remove(int *x, int *n, int v)
{
    int m = 0;
    for (int i = 0; i < *n; ++i) if (x[i] != v) x[m++] = x[i];
    *n = m;
}
static void list_front(int *x, int *n, int v, int cap)
{
    if (*n >= cap) *n = cap - 1;
    memmove(x + 1, x, sizeof(int) * (size_t)*n);
    x[0] = v; ++*n;
}

#if SH101_WITH_ARPSEQ
static void record_step(sh101_t *e, int n, float vel)
{
    sh101_step_t st = {n - 60, 0, (uint8_t)(e->held_n == 2), 0, vel, 1};
    e->steps[e->record_index] = st;
    e->record_index = (e->record_index + 1) % SH101_STEPS;
    int len = e->record_index > 1 ? e->record_index : 1;
    sh101_set(e, SH101_ARP_SEQ_LENGTH, (float)len / 96);
}
#endif

void sh101_note_on(sh101_t *e, int n, float vel)
{
    if (vel <= 0) { sh101_note_off(e, n); return; }
    if (e->lfo_trigger) { e->lfo_phase = 0; e->sh_value = rnd_bipolar(e); }
    int was_empty = e->held_n == 0;
    list_remove(e->held, &e->held_n, n);
    list_front(e->held, &e->held_n, n, 128);
#if SH101_WITH_ARPSEQ
    if (e->seq_record) record_step(e, n, vel);
    if (arp_or_seq(e)) {
        int hold = e->values[SH101_ARP_SEQ_HOLD] > 0.5f;
        if (hold && was_empty) e->arp_n = 0;
        int found = 0;
        for (int i = 0; i < e->arp_n; ++i) if (e->arp_notes[i] == n) found = 1;
        if (!found) list_front(e->arp_notes, &e->arp_n, n, 128);
        sort_arp(e);
        e->seq_first = 1;
        if (was_empty || e->arp_n == 1) {
            e->step_index = 0; e->arp_index = 0;
            e->clock_phase = 1; e->step_time = 0; e->shuffle_pending = 0; e->step_parity = 0;
        }
        return;
    }
#endif
    e->counter += 1;
    if (e->voice_count == 1) {
        sh101_voice_t *v = &e->voices[0];
        v->last_on = e->counter;
        voice_note_on(v, n, 0, vel, e->s.adsr_mode == 1 && !was_empty, &e->s);
    } else {
        sh101_voice_t *v = 0;
        for (int i = 0; i < e->voice_count && !v; ++i)
            if (e->voices[i].note == n && voice_sounding(&e->voices[i])) v = &e->voices[i];
        for (int i = 0; i < e->voice_count && !v; ++i)
            if (!voice_sounding(&e->voices[i])) v = &e->voices[i];
        if (!v) {
            v = &e->voices[0];
            for (int i = 1; i < e->voice_count; ++i) if (e->voices[i].last_on < v->last_on) v = &e->voices[i];
        }
        v->last_on = e->counter;
        voice_note_on(v, n, 0, vel, 0, &e->s);
    }
}

void sh101_note_off(sh101_t *e, int n)
{
    list_remove(e->held, &e->held_n, n);
#if SH101_WITH_ARPSEQ
    if (arp_or_seq(e)) {
        if (e->values[SH101_ARP_SEQ_HOLD] <= 0.5f) {
            list_remove(e->arp_notes, &e->arp_n, n);
            sort_arp(e);
            if (e->arp_n == 0) { e->voices[0].active = 0; e->current_step_display = -1; }
        }
        return;
    }
#endif
    if (e->voice_count == 1) {
        sh101_voice_t *v = &e->voices[0];
        if (e->held_n > 0) {
            if (v->note != e->held[0]) voice_note_on(v, e->held[0], 1, 0, e->s.adsr_mode == 1, &e->s);
        } else {
            v->active = 0;
        }
    } else {
        for (int i = 0; i < e->voice_count; ++i) if (e->voices[i].note == n) e->voices[i].active = 0;
    }
}

void sh101_all_notes_off(sh101_t *e)
{
    e->held_n = 0;
#if SH101_WITH_ARPSEQ
    e->arp_n = 0; e->arp_sorted_n = 0;
#endif
    all_inactive(e);
}
void sh101_pitch_bend(sh101_t *e, float b) { e->s.pitch_wheel = b; }
void sh101_mod_wheel(sh101_t *e, float m) { e->s.mod_wheel = m; }

#if SH101_WITH_ARPSEQ
void sh101_set_step(sh101_t *e, int i, const sh101_step_t *st)
{
    if (i >= 0 && i < SH101_STEPS) e->steps[i] = *st;
}

static void trigger_step(sh101_t *e)
{
    int si = e->step_index < 95 ? e->step_index : 95;
    const sh101_step_t *step = &e->steps[si];
    e->current_step = e->step_index;
    e->current_step_display = e->step_index;
    sh101_voice_t *voice = &e->voices[0];
    if (e->lfo_trigger) { e->lfo_phase = 0; e->sh_value = rnd_bipolar(e); }
    if (e->values[SH101_SEQ_ENABLED] > 0.5f) {
        if (e->arp_n > 0) {
            int base = e->arp_notes[0];
            voice->active = 0;
            if (!step->pause) {
                int no_retrig = (step->slide_adsr || e->s.adsr_mode == 1) ? !e->seq_first : 0;
                voice_note_on(voice, base + step->note_offset, step->slide, step->velocity, no_retrig, &e->s);
                e->seq_first = 0;
            }
        }
    } else {
        int n = e->arp_sorted_n;
        if (n >= 1) {
            if (e->arp_index >= n) e->arp_index = 0;
            if (!step->pause) {
                int note = e->arp_sorted[e->arp_index];
                if ((int)(e->values[SH101_ARP_MODE] + 0.5f) == 5) {
                    float u = (float)(rnd(e) >> 8) * (1.0f / 16777216.0f);
                    int r = (int)(u * (float)(n - 1) + 0.5f);
                    if (r == e->arp_index) r += 1;
                    e->arp_index = r;
                } else {
                    e->arp_index += 1;
                }
                if (n < 2) {
                    if (!voice->active || note != voice->note) voice_note_on(voice, note, 0, step->velocity, 0, &e->s);
                    e->arp_index = 1;
                } else {
                    voice->active = 0;
                    int no_retrig = step->slide_adsr ? 1 : e->s.adsr_mode == 1;
                    voice_note_on(voice, note, step->slide, step->velocity, no_retrig, &e->s);
                }
            } else {
                voice->active = 0;
            }
        }
    }
    e->step_index = e->step_index + 1 < e->seq_length ? e->step_index + 1 : 0;
}

static void clock_tick(sh101_t *e)
{
    if (e->arp_n == 0) return;
    e->clock_phase += e->clock_inc;
    e->step_time += e->clock_inc;
    double swing = (double)e->values[SH101_ARP_SEQ_SHUFFLE];
    double delay = swing * swing * 0.5;
    if (e->clock_phase >= 1) {
        e->clock_phase -= 1;
        e->step_parity = !e->step_parity;
        if (!e->step_parity && delay > 0) {
            e->shuffle_pending = 1; e->shuffle_time = 0;
        } else {
            e->step_time = 0; e->gate_closed = 0;
            trigger_step(e);
        }
    } else if (e->shuffle_pending) {
        e->shuffle_time += e->clock_inc;
        if (e->shuffle_time >= delay) {
            e->shuffle_pending = 0; e->step_time = 0; e->gate_closed = 0;
            trigger_step(e);
        }
    }
    double len = (double)e->steps[e->current_step < 95 ? e->current_step : 95].length;
    if (!e->gate_closed && len < 0.95 && e->step_time > len) {
        e->gate_closed = 1;
        e->voices[0].active = 0;
    }
}
#endif

/* ---------------- Ausgang (Clipper mit ADAA, double) ---------------- */

static inline double antiderivative(double x) { return fabs(x) <= 1 ? x * x * 0.5 : fabs(x) - 0.5; }
static inline float clip(sh101_t *e, float xf)
{
    double x = (double)xf, xp = e->adaa_prev;
    e->adaa_prev = x;
    if (fabs(x - xp) < 1e-6) {
        double y = (x + xp) * 0.5;
        y = y > 1 ? 1 : y;
        y = y < -1 ? -1 : y;
        return (float)y;
    }
    return (float)((antiderivative(x) - antiderivative(xp)) / (x - xp));
}

static inline double lerp(const float *t, int i, double f)
{
    double a = (double)t[i];
    return a + ((double)t[i + 1] - a) * f;
}

/* SynthEngine.renderSample */
float sh101_sample(sh101_t *e)
{
    sh101_shared_t *s = &e->s;
    s->attack_s = sm_tick(&e->sm_a, s->attack);
    s->decay_s = sm_tick(&e->sm_d, s->decay);
    s->sustain_s = sm_tick(&e->sm_s, s->sustain);
    s->fm_intensity_s = sm_tick(&e->sm_fm, s->fm_intensity);
    s->keyboard_s = sm_tick(&e->sm_kbd, s->keyboard);

    const float two_pi = 6.2831855f;
    e->d_ph0 += e->d_rate * 0.3f;
    float d = 0.0005f * sinf(two_pi * e->d_ph0);
    if (e->d_ph0 > 1) e->d_ph0 -= 1;
    e->d_ph1 += e->d_rate * 1.46f + d * 0.1f;
    d += 0.002f * sinf(two_pi * e->d_ph1);
    if (e->d_ph1 > 1) e->d_ph1 -= 1;
    e->d_ph2 += e->d_rate * 0.7f + d * 0.1f;
    d += 0.0004f * sinf(two_pi * e->d_ph2);
    if (e->d_ph2 > 1) e->d_ph2 -= 1;
    s->drift = d;

    if (s->adsr_mode == 0) {
        if (s->lfo_sine <= 0 || e->lfo_gate_armed) {
            if (s->lfo_sine <= 0) e->lfo_gate_armed = 0; else e->lfo_gate_done = 1;
        } else {
            e->lfo_gate_done = 0; e->lfo_gate_armed = 1;
        }
    }

    float sum = 0;
    if (arp_or_seq(e)) {
#if SH101_WITH_ARPSEQ
        clock_tick(e);
#endif
        sum = voice_process(&e->voices[0], s);
    } else {
        for (int vi = 0; vi < e->voice_count; ++vi) {
            sh101_voice_t *v = &e->voices[vi];
            if (v->active && s->adsr_mode == 0) {
                if (s->lfo_sine <= 0 || e->lfo_gate_done) {
                    if (s->lfo_sine <= 0) v->released = 1;
                } else {
                    voice_note_on(v, v->note, 0, v->velocity, 0, s);
                }
            }
            sum += voice_process(v, s);
        }
    }

    float x = sum * 300;
    if (e->drive != 0) {
        float dr = e->drive;
        x += x * dr * dr * dr * dr * 60;
        float a, b, a0, a1, b0, b1, c[8], y[8];
        hb_up(&e->dist_up1, x, &a, &b);
        hb_up(&e->dist_up2, a, &a0, &a1);
        hb_up(&e->dist_up2, b, &b0, &b1);
        hb_up(&e->dist_up3, a0, &c[0], &c[1]);
        hb_up(&e->dist_up3, a1, &c[2], &c[3]);
        hb_up(&e->dist_up3, b0, &c[4], &c[5]);
        hb_up(&e->dist_up3, b1, &c[6], &c[7]);
        for (int i = 0; i < 8; ++i) y[i] = clip(e, c[i]);
        float z0 = hb_down(&e->dist_dn1, y[0], y[1]), z1 = hb_down(&e->dist_dn1, y[2], y[3]);
        float z2 = hb_down(&e->dist_dn1, y[4], y[5]), z3 = hb_down(&e->dist_dn1, y[6], y[7]);
        float w0 = hb_down(&e->dist_dn2, z0, z1), w1 = hb_down(&e->dist_dn2, z2, z3);
        float clipped = hb_down(&e->dist_dn3, w0, w1);
        float wet = fminf(dr / 0.2f, 1);
        x += wet * (clipped - x);
    }
    float out = x * 0.0033333334f * sm_tick(&e->sm_vol, e->volume);

    double ph = e->lfo_phase;
    int wrapped = 0;
    if (ph > 255) { ph -= 255; wrapped = 1; }
    int idx = (int)ph;
    double frac = ph - (double)idx;
    double tri = lerp(tri_table, idx, frac);
    double sine = lerp(sine_table, idx, frac);
    double value;
    switch (e->lfo_wave) {
    case 1: value = lerp(square_table, idx, frac); break;
    case 2:
        if (wrapped) e->sh_value = rnd_bipolar(e);
        value = e->sh_value;
        break;
    case 3: {
        e->lfo_seed = lcg(e->lfo_seed);
        float white = (float)e->lfo_seed * -4.656613e-10f;
        e->lfo_n1 += e->lfo_noise_a * (white - e->lfo_n1);
        e->lfo_n2 += e->lfo_noise_a * (e->lfo_n1 - e->lfo_n2);
        value = (double)e->lfo_n2 * 1.7;
    } break;
    default: value = tri; break;
    }
    e->lfo_phase = ph + e->lfo_inc;
    s->lfo = (float)value * e->lfo_invert;
    s->lfo_tri = (float)tri;
    s->lfo_sine = (float)sine;
    s->lfo_wrapped = wrapped;
    return out;
}

/* ---------------- baustein_t ---------------- */

static const char *b_pname(unsigned p) { return sh101_param_key(p); }
static void b_init(void *st, double rate) { sh101_init((sh101_t *)st, rate); }
static void b_note_on(void *st, int note, int vel) { sh101_note_on((sh101_t *)st, note, (float)vel / 127.0f); }
static void b_note_off(void *st, int note) { sh101_note_off((sh101_t *)st, note); }
static void b_set_param(void *st, unsigned p, double v) { sh101_set((sh101_t *)st, p, (float)v * sh101_param_max(p)); }
static void b_render(void *st, float *l, float *r, int n)
{
    sh101_t *e = (sh101_t *)st;
    sh101_prepare(e);
    for (int i = 0; i < n; ++i) {
        float y = sh101_sample(e);
        l[i] += y; r[i] += y;
    }
}
static int b_active(const void *st)
{
    const sh101_t *e = (const sh101_t *)st;
    for (int i = 0; i < SH101_VOICES; ++i) if (voice_sounding(&e->voices[i])) return 1;
    return 0;
}

const baustein_t sh101_baustein = {"sh101", "SH-101 (MonoBass)", sizeof(sh101_t), SH101_PARAM_COUNT, b_pname, b_init,
                                   b_note_on, b_note_off, b_set_param, b_render, b_active};
