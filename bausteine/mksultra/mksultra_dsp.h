/* SPDX-License-Identifier: GPL-3.0-only
 * mksultra: Bausteine der Klangerzeugung (nur von mksultra.c eingebunden). Übertragen aus engine/src/ der Vorlage:
 * fastmath.rs, rng.rs, filters.rs, blep.rs, hiir.rs, dco.rs, vcf.rs, env.rs, lfo.rs, chorus.rs, delay.rs, reverb.rs.
 * Regeln (docs/SOUND_ENGINE_PORTING.md §4): Typen wie dort (float = f32, double = f64), Klammerung wie dort, keine
 * FMA (-ffp-contract=off), ganzzahlige Überläufe über uint32_t, Float→Int sättigend wie Rusts `as`.
 */
#ifndef MKSULTRA_DSP_H
#define MKSULTRA_DSP_H
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define PI_F 3.14159265358979323846f

/* ---------- Hilfen für Rust-Semantik ---------- */
static inline int32_t wmul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static inline int32_t f2i(float x)
{
    if (x != x) return 0;
    if (x >= 2147483647.0f) return INT32_MAX;
    if (x <= -2147483648.0f) return INT32_MIN;
    return (int32_t)x;
}
static inline int32_t d2i(double x)
{
    if (x != x) return 0;
    if (x >= 2147483647.0) return INT32_MAX;
    if (x <= -2147483648.0) return INT32_MIN;
    return (int32_t)x;
}
static inline int64_t f2l(float x)
{
    if (x != x) return 0;
    if (x >= 9.2233720368547758e18f) return INT64_MAX;
    if (x <= -9.2233720368547758e18f) return INT64_MIN;
    return (int64_t)x;
}
static inline uint32_t f2u(float x)
{
    if (!(x > 0.0f)) return 0;
    if (x >= 4294967295.0f) return UINT32_MAX;
    return (uint32_t)x;
}
static inline float bits2f(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static inline uint32_t f2bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static inline float fmaxr(float a, float b) { return fmaxf(a, b); } /* Rust f32::max */
static inline float fminr(float a, float b) { return fminf(a, b); }
static inline int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ---------- fastmath.rs ---------- */
static inline float fastpow2(float p)
{
    float offset = p < 0.0f ? 1.0f : 0.0f;
    float c = p > -126.0f ? p : -126.0f;
    float z = (c - (float)f2i(c)) + offset;
    float a = c + 121.27405548095703f;
    float t = 27.728023529052734f / (4.842525482177734f - z);
    float r = (z * -1.4901291131973267f + a) + t;
    uint32_t bits = (uint32_t)f2l(r * 8388608.0f);
    return bits2f(bits);
}

/* ---------- rng.rs ---------- */
typedef struct { uint32_t state; } seeds_t;
static void seeds_init(seeds_t *s, uint32_t seed) { s->state = seed == 0 ? 12u : seed; }
static int32_t next_seed(seeds_t *s)
{
    uint64_t v = ((uint64_t)s->state * 16807u) % 0x7fffffffu;
    s->state = (uint32_t)v;
    if (v == 0) { s->state = 12; return 12; }
    return (int32_t)v;
}

/* ---------- filters.rs ---------- */
#define K44 2.2675737e-5f

typedef struct { float t, a, b, sr, y; } smoother_t;
static void sm_new(smoother_t *s, float t) { s->t = t; s->a = t; s->b = 1.0f / (t + 1.0f); s->sr = 44100.0f; s->y = 0.0f; }
static void sm_set_sr(smoother_t *s, float sr)
{
    s->sr = sr;
    s->y = 0.0f;
    s->a = (s->t * sr) * K44;
    s->b = 1.0f / (s->a + 1.0f);
}
static inline float sm_tick(smoother_t *s, float x) { s->y = (s->y * s->a + x) * s->b; return s->y; }

typedef struct { float sr, fc, g, s[2]; } tpt_t;
static void tpt_new(tpt_t *f, float fc) { f->sr = 44100.0f; f->fc = fc; f->g = 0.0f; f->s[0] = f->s[1] = 0.0f; }
static void tpt_set_sr(tpt_t *f, float sr)
{
    f->sr = sr;
    float inv = 1.0f / sr;
    float arg = ((float)((double)f->fc * 6.283185307179586)) * (0.5f * inv);
    float t = tanf(arg) * (inv * sr);
    f->g = (float)((double)t / ((double)t + 1.0));
}
static void tpt_set_engine_hpf(tpt_t *f, float fc, float pi_fc)
{
    f->fc = fc;
    float inv = 1.0f / f->sr;
    double d = (double)((inv * f->sr) * tanf(pi_fc * inv));
    f->g = (float)(d / (1.0 + d));
}
static inline float tpt_lp(tpt_t *f, int ch, float x)
{
    float v = (x - f->s[ch]) * f->g;
    float lp = f->s[ch] + v;
    f->s[ch] = v + lp;
    return lp;
}
static inline float tpt_hp(tpt_t *f, int ch, float x) { return x - tpt_lp(f, ch, x); }

static float fsqrt(float x)
{
    if (fabsf(x) < FLT_MIN) return 0.0f;
    float r_exact = 1.0f / sqrtf(x);
    float r = bits2f(f2bits(r_exact) & 0xfffff000u);
    float u = x * r;
    return (u * r + (-3.0f)) * u * (-0.5f);
}

enum { SVF_LP, SVF_HP, SVF_BELL, SVF_LSHELF };
typedef struct {
    float ic1, ic2, a1, a2, a3, m0, m1, m2, f, q;
    int ty;
    float sr, a_gain, sqrt_a;
    int enabled;
} svf_t;
static void svf_update(svf_t *s)
{
    float g = tanf((s->f * PI_F) / s->sr);
    float k = 1.0f / s->q;
    float a = s->a_gain;
    if (s->ty == SVF_LSHELF) g /= s->sqrt_a;
    else if (s->ty == SVF_BELL) k = 1.0f / (s->q * a);
    s->a1 = 1.0f / ((g + k) * g + 1.0f);
    s->a2 = s->a1 * g;
    s->a3 = s->a2 * g;
    switch (s->ty) {
    case SVF_LP: s->m0 = 0.0f; s->m1 = 0.0f; s->m2 = 1.0f; break;
    case SVF_HP: s->m0 = 1.0f; s->m1 = -k; s->m2 = -1.0f; break;
    case SVF_BELL: s->m0 = 1.0f; s->m1 = (a * a + (-1.0f)) * k; s->m2 = 0.0f; break;
    default: s->m0 = 1.0f; s->m1 = (a + (-1.0f)) * k; s->m2 = a * a + (-1.0f); break;
    }
}
static void svf_new(svf_t *s, float f, float q, int ty)
{
    memset(s, 0, sizeof *s);
    s->f = f; s->q = q; s->ty = ty; s->sr = 44100.0f; s->a_gain = 1.0f; s->sqrt_a = 1.0f; s->enabled = 1;
    svf_update(s);
}
static void svf_set_gain(svf_t *s, float a) { s->a_gain = a; s->sqrt_a = fsqrt(a); svf_update(s); }
static void svf_set_sr(svf_t *s, float sr) { s->sr = sr; svf_update(s); }
static void svf_set_gk(svf_t *s, float g, float k)
{
    s->a1 = 1.0f / ((g + k) * g + 1.0f);
    s->a2 = s->a1 * g;
    s->a3 = s->a2 * g;
    if (s->ty == SVF_LP) { s->m0 = 0.0f; s->m1 = 0.0f; s->m2 = 1.0f; }
    else { s->m0 = 1.0f; s->m1 = -k; s->m2 = -1.0f; }
}
static inline float svf_tick(svf_t *s, float x)
{
    float v3 = x - s->ic2;
    float v1 = s->a2 * v3 + s->a1 * s->ic1;
    float v2 = s->a3 * v3 + (s->a2 * s->ic1 + s->ic2);
    s->ic1 = (v1 + v1) - s->ic1;
    s->ic2 = (v2 + v2) - s->ic2;
    return s->m2 * v2 + (s->m1 * v1 + s->m0 * x);
}

typedef struct {
    int high;
    float sr, b0, b1, b2, a1, a2, x1, x2, x3, y1, y2, cfc, cq, cgain;
} biquad_t;
static void bq_new(biquad_t *b, int high, float sr) { memset(b, 0, sizeof *b); b->high = high; b->sr = sr; }
static void bq_set(biquad_t *b, float fc, float q, float gain)
{
    if (b->cq == q && b->cgain == gain && b->cfc == fc) return;
    b->cq = q; b->cgain = gain; b->cfc = fc;
    float w0 = fc * 6.2831855f / b->sr;
    float sn = sinf(w0), cs = cosf(w0);
    float alpha = sn / (q + q);
    float a0i = 1.0f / (alpha + 1.0f);
    if (b->high) {
        float v = a0i * (0.5f * cs + 0.5f);
        b->b0 = v; b->b2 = v; b->b1 = a0i * (-1.0f - cs);
    } else {
        float v = a0i * (0.5f - 0.5f * cs);
        b->b0 = v; b->b2 = v; b->b1 = a0i * (1.0f - cs);
    }
    b->a1 = a0i * (-2.0f * cs);
    b->a2 = a0i * (1.0f - alpha);
}
static inline float bq_tick(biquad_t *b, float in)
{
    float y = (b->b0 * b->x1 + (b->b1 * b->x2 + b->b2 * b->x3)) - (b->a2 * b->y2 + b->a1 * b->y1);
    b->x3 = b->x2; b->x2 = b->x1; b->x1 = in;
    b->y2 = b->y1; b->y1 = y;
    return y;
}

/* ---------- blep.rs ---------- */
#define BLEP_N 4096
#define BLEP_S 128
#define BLEP_LEN 4097

static void build_blep_table(float *out)
{
    static double c[BLEP_LEN];
    const double two_pi_n = 0.0015339807878856412;
    double acc = 0.0;
    for (int i = 0; i < BLEP_LEN; i++) {
        double x = (double)i * 0.0078125 + (-16.0);
        double s = x == 0.0 ? 1.0 : sin(x * 1.5707963267948966) / (x * 1.5707963267948966);
        s = (double)(float)s;
        double w = i < 4096 ? cos((double)i * two_pi_n * 2.0) * 0.08 + (cos((double)i * two_pi_n) * (-0.5) + 0.41999998688697815)
                            : -1.3113021848920603e-8;
        acc += s * w;
        c[i] = acc;
    }
    double last = c[BLEP_LEN - 1];
    double inv = 1.0 / last;
    for (int i = 0; i < BLEP_LEN - 1; i++) c[i] *= inv;
    c[BLEP_LEN - 1] = last / last;
    for (int i = 0; i < BLEP_LEN; i++) out[i] = (float)c[i];
}

typedef struct { float buf[32]; int32_t idx; } ring_t;

static inline void ring_insert(ring_t *r, const float *tab, float t, float s)
{
    float t1 = t < 1.0f ? t : 1.0f;
    t = t1 > 0.0f ? t1 : 0.0f;
    float x = (float)BLEP_S * t;
    int32_t i = f2i(floorf(x));
    if (i < BLEP_N) {
        float fr = x - (float)f2i(x);
        int32_t rr = r->idx;
        int32_t k = 0;
        while (i < BLEP_N) {
            float a = tab[i];
            float v = ((tab[i + 1] - a) * fr + a) * s;
            if (k >= 16) v -= s;
            r->buf[(rr + k) & 31] += v;
            k++;
            i += BLEP_S;
        }
    }
}
static inline float ring_advance_read(ring_t *r)
{
    r->buf[r->idx] = 0.0f;
    r->idx = r->idx < 31 ? r->idx + 1 : 0;
    return r->buf[r->idx];
}

typedef struct {
    float phase, state;
    int wrapped;
    float sync_reset, inc;
    int pw_done;
    float pw_latched;
} bosc_t;
static void bosc_new(bosc_t *o) { o->phase = 0.0f; o->state = 1.0f; o->wrapped = 0; o->sync_reset = 0.0f; o->inc = 0.0f; o->pw_done = 0; o->pw_latched = 0.5f; }

static inline void bosc_edge(bosc_t *o, ring_t *ring, const float *tab, float t, float amp)
{
    if (o->state == -1.0f && amp != 0.0f) ring_insert(ring, tab, t, amp);
    o->state = 1.0f;
    o->pw_done = 1;
}
static inline void bosc_wrap(bosc_t *o, ring_t *ring, const float *tab, float t, float pw, float amp)
{
    float ph = o->phase;
    for (;;) { ph += -1.0f; if (ph < 1.0f) break; }
    o->phase = ph;
    if (o->state == 1.0f) {
        if (amp != 0.0f) ring_insert(ring, tab, t, -amp);
        o->state = -1.0f;
    }
    o->wrapped = 1;
    o->pw_done = 0;
    o->pw_latched = pw;
}
static float bosc_pulse(bosc_t *o, ring_t *ring, const float *tab, float inc, float pw, float amp)
{
    o->wrapped = 0;
    o->inc = inc;
    o->phase += inc;
    float ii = 1.0f / inc;
    for (;;) {
        float ph = o->phase;
        float pl = o->pw_latched;
        int edge_pending = ph >= pl && !o->pw_done;
        if (edge_pending && ph < 1.0f) {
            float te = (ph - pl) * ii;
            bosc_edge(o, ring, tab, te, amp);
        } else if (edge_pending) {
            float te = (ph - pl) * ii;
            float tw = (ph + (-1.0f)) * ii;
            if (te > tw) bosc_edge(o, ring, tab, te, amp);
            else bosc_wrap(o, ring, tab, tw, pw, amp);
        } else if (ph < 1.0f) {
            return o->state * 0.5f;
        } else {
            float tw = (ph + (-1.0f)) * ii;
            bosc_wrap(o, ring, tab, tw, pw, amp);
        }
    }
}
static float bosc_pulse_sync(bosc_t *o, ring_t *ring, const float *tab, float inc, float pw, float m_phase, float m_inc, float amp)
{
    o->wrapped = 0;
    o->inc = inc;
    o->phase += inc;
    float m = m_phase + 1.0f;
    o->pw_latched = pw;
    float im = 1.0f / m_inc;
    float ii = 1.0f / inc;
    for (;;) {
        float ph = o->phase;
        float pl = o->pw_latched;
        float m1 = m + (-1.0f);
        float ts = m1 * im;
        int edge_pending = ph >= pl && !o->pw_done;
        enum { EV_SYNC, EV_EDGE, EV_WRAP, EV_DONE } ev;
        float t = 0.0f;
        if (edge_pending && ph < 1.0f) {
            float te = (ph - pl) * ii;
            if (ts > te) ev = EV_SYNC; else { ev = EV_EDGE; t = te; }
        } else if (edge_pending) {
            float te = (ph - pl) * ii;
            float tw = (ph + (-1.0f)) * ii;
            if (ts > te && ts > tw) ev = EV_SYNC;
            else if (te > tw) { ev = EV_EDGE; t = te; }
            else { ev = EV_WRAP; t = tw; }
        } else if (ph >= 1.0f) {
            float tw = (ph + (-1.0f)) * ii;
            if (ts > tw) ev = EV_SYNC; else { ev = EV_WRAP; t = tw; }
        } else if (ts < 0.0f) {
            ev = EV_DONE;
        } else {
            ev = EV_SYNC;
        }
        switch (ev) {
        case EV_DONE: return o->state * 0.5f;
        case EV_EDGE: bosc_edge(o, ring, tab, t, amp); break;
        case EV_WRAP: bosc_wrap(o, ring, tab, t, pw, amp); break;
        default:
            if (o->state == 1.0f) {
                if (amp != 0.0f) ring_insert(ring, tab, ts, -amp);
                o->state = -1.0f;
            }
            o->phase = ((o->sync_reset - ph) + ph) + o->inc * ts;
            o->pw_done = o->sync_reset > pl;
            m = m1;
            break;
        }
    }
}
static inline void bosc_silent(bosc_t *o, ring_t *ring, const float *tab, float inc, float pw)
{
    (void)bosc_pulse(o, ring, tab, inc, pw, 0.0f);
}

/* ---------- hiir.rs ---------- */
static double ipow_d(double b, uint64_t ex)
{
    double r = 1.0, base = b;
    while (ex > 0) {
        if (ex & 1) r *= base;
        base *= base;
        ex >>= 1;
    }
    return r;
}
static void hiir_design(int n, double tbw, float *out)
{
    const double PI = 3.141592653589793;
    int order = 2 * n + 1;
    double t0 = tan(tbw * (-PI / 2.0) + PI / 4.0);
    double k = t0 * t0;
    double kk = sqrt(sqrt(1.0 - k * k));
    double e = (0.5 - kk * 0.5) / (kk + 1.0);
    double e2 = e * e;
    double e4 = e2 * e2;
    double q = (((150.0 * e4 + 15.0) * e4 + 2.0) * e4 + 1.0) * e;
    for (int i = 0; i < n; i++) {
        double c = (double)(i + 1);
        double num = 0.0;
        uint64_t j = 0;
        for (;;) {
            double sign = j % 2 == 0 ? 1.0 : -1.0;
            double term = sign * ipow_d(q, j * (j + 1)) * sin((double)(2 * j + 1) * c * PI / (double)order);
            num += term;
            if (fabs(term) <= 1e-100 || j > 1000) break;
            j++;
        }
        double den = 0.0;
        j = 1;
        for (;;) {
            double sign = j % 2 == 0 ? 1.0 : -1.0;
            double term = sign * ipow_d(q, j * j) * cos((double)(2 * j) * c * PI / (double)order);
            den += term;
            if (fabs(term) <= 1e-100 || j > 1000) break;
            j++;
        }
        double t = num * sqrt(sqrt(q)) / (den + 0.5);
        double ww = t * t;
        double x = sqrt((1.0 - ww / k) * (1.0 - ww * k)) / (ww + 1.0);
        out[i] = (float)((1.0 - x) / (1.0 + x));
    }
}

#define HIIR_MAX 8
typedef struct { int n; float c[HIIR_MAX], x[HIIR_MAX], y[HIIR_MAX]; } chain_t;
static void chain_new(chain_t *ch, const float *c, int n)
{
    ch->n = n;
    for (int i = 0; i < n; i++) { ch->c[i] = c[i]; ch->x[i] = 0.0f; ch->y[i] = 0.0f; }
}
static void chain_clear(chain_t *ch) { for (int i = 0; i < ch->n; i++) ch->x[i] = ch->y[i] = 0.0f; }
static inline void chain_run(chain_t *ch, float a, float b, float *oa, float *ob)
{
    for (int j = 0; j < ch->n; j += 2) {
        float ta = (a - ch->y[j]) * ch->c[j] + ch->x[j];
        float tb = (b - ch->y[j + 1]) * ch->c[j + 1] + ch->x[j + 1];
        ch->x[j] = a; ch->x[j + 1] = b;
        ch->y[j] = ta; ch->y[j + 1] = tb;
        a = ta; b = tb;
    }
    *oa = a; *ob = b;
}
static inline void chain_up(chain_t *ch, float v, float *o0, float *o1) { chain_run(ch, v, v, o0, o1); }
static inline void chain_down_paths(chain_t *ch, float e0, float e1, float *a, float *b) { chain_run(ch, e1, e0, a, b); }
static inline float chain_down(chain_t *ch, float e0, float e1)
{
    float a, b;
    chain_down_paths(ch, e0, e1, &a, &b);
    return (b + a) * 0.5f;
}

/* ---------- vcf.rs ---------- */
typedef struct {
    float st[3][2];
    float g, r, sr;
    int32_t seed;
    float last_cut, fc, inv_sr;
    int vintage;
    smoother_t jitter;
} dsvf_t;

static inline float sat_factor(float s, int vintage)
{
    if (s == 0.0f || s != s) return 1.0f;
    float k = vintage ? 8.0f : 3.0f, c = vintage ? 0.8f : 0.75f;
    float ks = k * s;
    float u2 = ks * ks;
    float na = ((u2 + 378.0f) * u2 + 17325.0f) * u2 + 135135.0f;
    float da = (((28.0f * u2) + 3150.0f) * u2 + 62370.0f) * u2 + 135135.0f;
    float t1 = ((c * s) * na) / da;
    float ys = 31.400002f * s;
    float y2 = ys * ys;
    float nb = ((479249.0f * y2 + (-52785432.0f)) * y2 + 1640635904.0f) * y2 + (-11511340032.0f);
    float db = ((18361.0f * y2 + 3177720.0f) * y2 + 277920704.0f) * y2 + 11511340032.0f;
    float t2 = ((-0.01256f * s) * nb) / db;
    return (t2 + t1) / s;
}
static void dsvf_set_sr(dsvf_t *f, float sr)
{
    f->sr = sr;
    sm_set_sr(&f->jitter, sr);
    f->inv_sr = 1.0f / sr;
    for (int i = 0; i < 3; i++) f->st[i][0] = f->st[i][1] = 1e-4f;
}
static void dsvf_new(dsvf_t *f, int32_t seed)
{
    for (int i = 0; i < 3; i++) f->st[i][0] = f->st[i][1] = 1e-4f;
    f->g = 7.9176173f; f->r = 2.0f; f->sr = 44100.0f; f->seed = seed; f->last_cut = -1.0f; f->fc = -1.0f;
    f->inv_sr = 1.0f / 44100.0f; f->vintage = 0;
    sm_new(&f->jitter, 20.0f);
    dsvf_set_sr(f, 44100.0f);
}
static inline void dsvf_set_params(dsvf_t *f, float cut, float res, int vintage)
{
    f->vintage = vintage;
    float fc;
    if (cut != f->last_cut) {
        f->last_cut = cut;
        float p = fminr(fmaxr(cut, -0.3f), 1.0f);
        float x = (p * 127.0f + (-57.0f)) * 0.083333336f;
        float p2 = fastpow2(x);
        fc = (0.25f * 440.0f) * p2;
        if (fc <= 2.0f) fc = 2.0f;
        float half = f->sr * 0.5f;
        if (half < fc) fc = half;
        f->fc = fc;
    } else {
        fc = f->fc;
    }
    f->seed = wmul(f->seed, 16807);
    float n = (4.656613e-10f * fc) * (float)f->seed;
    float sm = sm_tick(&f->jitter, n);
    float w = (f->inv_sr * 3.1415927f) * ((sm * 0.1f) + fc);
    float w2 = w * w;
    float num = ((((-378.0f) + w2) * w2 + 17325.0f) * w2 + (-135135.0f)) * w;
    float den = (((28.0f * w2) + (-3150.0f)) * w2 + 62370.0f) * w2 + (-135135.0f);
    f->g = num / den;
    f->r = 2.0f - (res + res);
}
static inline float dsvf_step(dsvf_t *f, int ch, float x)
{
    float g = f->g, rr = f->r;
    float s1 = f->st[ch][0], s2 = f->st[ch][1];
    float fa = sat_factor(s1, f->vintage);
    float d = ((-2.0f + g) + (rr + rr)) * g + 1.0f;
    float y = (((s1 - s2 * g) + x * g) * fa) / (fa * d + (g + g));
    float lp = s2 + y * g;
    f->st[ch][0] = (y - s1) + y;
    f->st[ch][1] = lp + y * g;
    return lp;
}

/* ---------- env.rs ---------- */
static const double ENV_X[11] = {0.0, 0.1, 0.25, 0.35, 0.5, 0.6, 0.65, 0.75, 0.8, 0.9, 1.0};
static const double ENV_Y[11] = {0.0048, 0.0014, 0.0005, 0.0002, 5.5e-05, 2.6e-05, 1.58e-05, 6.18e-06,
                                 3.3999999999999996e-06, 1.1199999999999999e-06, 0.0};
static double calc_coef(float v)
{
    int32_t i = f2i(floorf(v * 127.0f + 0.5f));
    if (i == 0) return 4.0;
    double xs = (double)i * 0.007874015748031496;
    for (int j = 0; j < 10; j++)
        if (!(xs < ENV_X[j] || xs > ENV_X[j + 1]))
            return ((ENV_Y[j + 1] - ENV_Y[j]) * (xs - ENV_X[j])) / (ENV_X[j + 1] - ENV_X[j]) + ENV_Y[j];
    return 0.0;
}

#define ENV_K1 2.2675736961451248e-05
#define ENV_K2 3.779289343399742e-05
#define ENV_OFF 0.16
#define ENV_NOFF (-0.16)
#define ENV_R 26460.002f
#define ENV_EPS 1e-05

typedef struct {
    int32_t stage;
    double x, m, n, counter;
    int32_t idle;
    smoother_t out;
} env_t;
static void env_set_sr(env_t *e, float sr)
{
    e->m = (double)(44100.0f / sr);
    e->n = (double)sr / 243.0;
    sm_set_sr(&e->out, sr);
}
static void env_new(env_t *e)
{
    e->stage = 5; e->x = 0.0; e->m = 0.0; e->n = 1.0; e->counter = 0.0; e->idle = 10000000;
    sm_new(&e->out, 22.0f);
    env_set_sr(e, 44100.0f);
}
static inline int env_finished(const env_t *e) { return e->stage == 5 && (double)e->idle > e->n + e->n; }
static void env_reset(env_t *e) { e->counter = 0.0; e->stage = 0; e->out.y = 0.0f; }
static inline float env_tick(env_t *e, double t1, double l1, double t2, double l2, double t3, double l3, double t4, int gate, double kf)
{
    int32_t st = e->stage;
    if (!gate) {
        if (st == 5) e->idle = (int32_t)((uint32_t)e->idle + 1u);
        else { e->stage = 4; st = 4; }
    } else if (st == 5) {
        e->idle = (int32_t)((uint32_t)e->idle + 1u);
    }
    double cnt = e->counter + (-1.0);
    e->counter = cnt;
    float out_in;
    if (cnt > 0.0) {
        out_in = (float)e->x;
    } else {
        if (cnt < 0.0) e->counter = cnt + e->n;
        double c48 = e->m, c50 = e->n;
        switch (st) {
        case 0:
            e->x = (((c48 * kf) * t1) * c50 + e->x) + ((c48 * c50) * (ENV_K1 * kf)) / 33.3;
            if (e->x >= l1) { e->x = l1; e->stage = 1; }
            break;
        case 1: {
            double d = l2 - l1;
            double s = d >= 0.0 ? kf : -kf;
            double nm = c50 * c48;
            e->x = ((s * t2) * nm + e->x) + ((ENV_K1 * s) * nm) / 33.3;
            if ((d < 0.0 && e->x <= l2) || (d >= 0.0 && e->x >= l2)) { e->x = l2; e->stage = 2; }
            break;
        }
        case 2: {
            double d = l3 - l2;
            double a = fabs(e->x - l3);
            double s = d >= 0.0 ? kf : -kf;
            double nm = c50 * c48;
            e->x = (((s * t3) * nm) * a + e->x) + (((ENV_K2 * s) * nm) * (ENV_OFF + a)) / 33.3;
            if ((d < 0.0 && e->x <= l3) || (d >= 0.0 && e->x >= l3)) { e->x = l3; e->stage = 3; }
            break;
        }
        case 3: e->x = l3; break;
        case 4: {
            double dd = (double)(((float)33.3) * ENV_R);
            double pm = kf * c48;
            e->x = (e->x - (c50 * e->x) * (pm * t4)) + (((ENV_NOFF - e->x) * c50) * pm) / dd;
            if (e->x < ENV_EPS) { e->x = 0.0; e->idle = 0; e->stage = 5; }
            break;
        }
        case 5: e->x = 0.0; break;
        default: break;
        }
        out_in = (float)e->x;
    }
    return sm_tick(&e->out, out_in);
}

#endif
