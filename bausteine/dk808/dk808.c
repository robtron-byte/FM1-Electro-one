/* SPDX-License-Identifier: GPL-3.0-only
 * dk808: Port des DrumKern (Swift) nach C, siehe dk808.h und README.md. Jede Funktion entspricht der gleichnamigen
 * Stelle in Voices.swift / DSPMath.swift; die Rechenreihenfolge ist dieselbe (wichtig für den Vergleich mit den
 * Referenz-Renders, darum auch -ffp-contract=off im Makefile). */
#include "dk808.h"
#if DK808_FLOAT
#define DK_SIN sinf
#define DK_COS cosf
#define DK_EXP expf
#define DK_POW powf
#define DK_TANH tanhf
#define DK_FABS fabsf
#define DK_TAN tanf
#define DK_EXPM1 expm1f
#else
#define DK_SIN sin
#define DK_COS cos
#define DK_EXP exp
#define DK_POW pow
#define DK_TANH tanh
#define DK_FABS fabs
#define DK_TAN tan
#define DK_EXPM1 expm1
#endif
#include <math.h>
#include <string.h>

static const dk_real twoPi = 2.0 * M_PI;
static const dk_real trigGain = 2.6449999809265137;
static const dk_real voiceOutputGain = 0.22334179607059307;
static const dk_real trigGainBig = 4.170000076293945;
#define LEVEL voiceOutputGain

/* ---- DSPMath ---- */
static dk_real dmin(dk_real a, dk_real b) { return b < a ? b : a; }   /* Swift min: bei Gleichheit a */
static dk_real dmax(dk_real a, dk_real b) { return b > a ? b : a; }
static dk_real clampd(dk_real v, dk_real lo, dk_real hi) { return dmin(dmax(v, lo), hi); }
static dk_real pole(dk_real hz, dk_real rate) { return DK_EXP(-twoPi * hz / rate); }

static void lp_set(dk_lp_t *f, dk_real hz, dk_real rate) { f->p = pole(dmin(hz, rate * 0.49), rate); }
static dk_real lp_run(dk_lp_t *f, dk_real x) { f->y = x * (1.0 - f->p) + f->p * f->y; return f->y; }
static void hp_set(dk_hp_t *f, dk_real hz, dk_real rate) { f->p = pole(hz, rate); f->b = (f->p + 1.0) * 0.5; }
static void hp_pole(dk_hp_t *f, dk_real p) { f->p = p; f->b = (f->p + 1.0) * 0.5; }
static dk_real hp_run(dk_hp_t *f, dk_real x)
{
    dk_real y = f->b * (x - f->x1) + f->p * f->y1;
    f->x1 = x; f->y1 = y;
    return y;
}

enum { BQ_LP, BQ_HP, BQ_BP };
static void bq_setq(dk_bq_t *b, int kind, dk_real hz, dk_real q, dk_real rate)
{
    dk_real f = clampd(hz, 10.0, rate * 0.49);
    dk_real w0 = twoPi * f / rate;
    dk_real cw = DK_COS(w0), sw = DK_SIN(w0);
    dk_real alpha = sw / (2 * dmax(q, 0.05));
    dk_real nb0, nb1, nb2;
    dk_real na0 = 1 + alpha, na1 = -2 * cw, na2 = 1 - alpha;
    if (kind == BQ_HP) { nb0 = (1 + cw) / 2; nb1 = -(1 + cw); nb2 = (1 + cw) / 2; }
    else if (kind == BQ_BP) { nb0 = alpha; nb1 = 0; nb2 = -alpha; }
    else { nb0 = (1 - cw) / 2; nb1 = 1 - cw; nb2 = (1 - cw) / 2; }
    b->b0 = nb0 / na0; b->b1 = nb1 / na0; b->b2 = nb2 / na0; b->a1 = na1 / na0; b->a2 = na2 / na0;
}
static dk_real bq_run(dk_bq_t *b, dk_real x)
{
    dk_real y = b->b0 * x + b->b1 * b->x1 + b->b2 * b->x2 - b->a1 * b->y1 - b->a2 * b->y2;
    if (DK_FABS(y) < 1e-20) y = 0;
    b->x2 = b->x1; b->x1 = x; b->y2 = b->y1; b->y1 = y;
    return y;
}

/* ---- Bausteine aus Voices.swift ---- */
static dk_real svf_tick(dk_svf_t *s, dk_real input, dk_real f, dk_real q)
{
    dk_real hi = input - q * s->bp;
    s->lo = f * s->bp + s->lo;
    hi -= s->lo;
    s->bp = f * hi + s->bp;
    return s->bp;
}
static dk_real tune_poly(dk_real x) { return 0.072 * x * x + 0.154 * x + 0.905; }
static dk_real follow(dk_real e, dk_real target, dk_real k) { return e - (e - target) * k; }
static dk_real vca(dk_real m, dk_real c, dk_real K)
{
    dk_real X = c * m;
#if DK808_FLOAT
    /* float: 1 - X / expm1(X) löscht sich bei kleinem X aus; dort die Reihe X/2 - X^2/12 + X^4/720 (docs/adr/0050) */
    if (DK_FABS(X) < 0.5f) { dk_real X2 = X * X; return (X * 0.5f - X2 * (1.0f / 12.0f) + X2 * X2 * (1.0f / 720.0f)) / K; }
#endif
    dk_real ratio = DK_FABS(X) < 1e-9 ? 1.0 - X * 0.5 : X / DK_EXPM1(X);
    return (1.0 - ratio) / K;
}
static dk_real ap_run(dk_ap_t *a, dk_real x) { dk_real y = a->c * (x - a->y1) + a->x1; a->x1 = x; a->y1 = y; return y; }
static void hb_init(dk_hb_t *h)
{
    memset(h, 0, sizeof *h);
    h->upA.c = h->dnA.c = 0.138024994073021;
    h->upB.c = h->dnB.c = 0.5846832574873193;
}
static dk_real hb_down(dk_hb_t *h, dk_real x0, dk_real x1)
{
    dk_real a = ap_run(&h->dnA, x0);
    dk_real b = ap_run(&h->dnB, h->dnHold);
    h->dnHold = x1;
    return 0.5 * (a + b);
}
static void metal_filters(dk_bq_t *bp1, dk_bq_t *bp2, dk_real t, dk_real rate)
{
    bq_setq(bp1, BQ_BP, 7117.0 * (t + 0.5), 4.0, rate);
    bq_setq(bp2, BQ_BP, 3553.0 * (t + 0.5), 2.5, rate);
}
static int alive(dk_gate_t *g, dk_real out)
{
    if (DK_FABS(out) > 1.52587e-05) { g->quiet = 0; return 1; }
    g->quiet += 1;
    return g->quiet < g->limit;
}

#if DK808_NEEDS_NOISE
static void noise_init(dk_noise_t *n, dk_real rate)
{
    memset(n, 0, sizeof *n);
    n->w258 = 0x67452301u; n->w25c = 0xEFCDAB89u;
    lp_set(&n->lp2000, 2000, rate);
    bq_setq(&n->bq, BQ_LP, 501, 0.7071, rate);
    hp_set(&n->hp1, 170, rate);
    lp_set(&n->lp1, 353, rate);
}
static void noise_tick(dk_noise_t *n)
{
    n->raw = (dk_real)(int32_t)n->w25c * 2.3283064365386963e-10;
    n->w258 ^= n->w25c;
    n->w25c = n->w25c + n->w258;
    n->out10 = 1.5 * lp_run(&n->lp2000, n->raw);
    n->out138 = lp_run(&n->lp1, hp_run(&n->hp1, bq_run(&n->bq, n->raw) * 4.0));
}
#endif

#if DK808_NEEDS_METAL
static const dk_real metal_freq[6] = {543.499286, 806.49998682, 510.1999067, 292.399998022, 361.69998711, 210.1};
static const dk_real metal_duty[6] = {0.4375, 0.4354, 0.4336, 0.4289, 0.4339, 0.4197};
static dk_real blep(dk_real t, dk_real dt)
{
    if (t < dt) { dk_real x = t / dt; return x + x - x * x - 1; }
    if (t > 1 - dt) { dk_real x = (t - 1) / dt; return x * x + x + x + 1; }
    return 0;
}
static void metal_tick(dk_metal_t *m)
{
    dk_real s = 0.0;
    for (int i = 0; i < 6; ++i) {
        dk_real dt = m->inc[i], d = metal_duty[i];
#if DK808_FLOAT
        dk_real p = (dk_real)m->ph[i] * (1.0f / 4294967296.0f);
#else
        dk_real p = m->phase[i];
#endif
        dk_real v = p < d ? 0.5 : -0.5;
        v += 0.5 * blep(p, dt);
        dk_real q = p - d;
        if (q < 0) q += 1;
        v -= 0.5 * blep(q, dt);
        s += v;
#if DK808_FLOAT
        m->ph[i] += m->ph_inc[i];
        m->phase[i] = (dk_real)m->ph[i] * (1.0f / 4294967296.0f);   /* (für dk808_init: die Phase läuft über ein init weiter) */
#else
        p += dt;
        if (p >= 1) p -= 1;
        m->phase[i] = p;
#endif
    }
    m->out = 0.5 * s;
}
#endif

/* ---- Bass Drum ---- */
#if DK808_HAS(DK_BD)
static void bd_update(dk808_t *d, const dk_real *c)
{
    dk_bd_t *v = &d->bd;
    dk_real x = c[0], y = c[1], z = c[2];
    v->tonePole = pole(2000.0 * x * x + 200.0, d->R);
    v->damping = DK_EXP(y * -4.903508884975746) * 0.2921680327867917 + 0.007831967213314962;
    v->sweep = DK_EXP(11.553458178450429 / (z + -2.160964047443686)) * 1049.296197933348;
}
static void bd_init(dk808_t *d)
{
    dk_bd_t *v = &d->bd;
    memset(v, 0, sizeof *v);
    v->damping = 0.033; v->sweep = 1.0; v->excB = 1.0; v->envDone = 1;
    hp_set(&v->outHP, 50.0, d->R);
}
static void bd_trigger(dk808_t *d, dk_real u)
{
    dk_bd_t *v = &d->bd;
    dk_real R = d->R, H = d->H;
    v->A = u * trigGainBig;
    v->c1 = 0.0043 * R; v->c2 = 0.00097 * R;
    v->f1 = 1; v->f2 = 1;
    v->r = 0; v->E = 0;
    v->rRate = H * 0.1;
    v->a = 0.5; v->h = H * 0.5;
    v->e1 = 0; v->e2 = 0; v->e3 = 0; v->envDone = 0;
    v->excP = pole(1000.0, R); v->excB = (v->excP + 1.0) * 0.5;
}
static dk_real bd_render(dk808_t *d)
{
    dk_bd_t *v = &d->bd;
    dk_real R = d->R, H = d->H, J = H * H * H;
    if (v->f1) {
        if ((int)v->c1 == 0) {
            v->f1 = 0;
            v->rRate = H * 0.06;
            dk_real t = 1.0 / dmax(v->A, 1e-9);
            t *= 10.0;
            t = v->sweep * t;
            t /= R;
            v->excP = DK_EXP(t * -6.283185307179586); v->excB = (v->excP + 1.0) * 0.5;
        }
        v->c1 -= 1.0;
    }
    if (v->f2) {
        if ((int)v->c2 == 0) { v->f2 = 0; v->a = -0.03; v->h = H * 0.05; }
        v->c2 -= 1.0;
    }
    dk_real hp = v->negEPrev * -v->excB;
    hp = hp - v->E * v->excB;
    hp = v->hpPrev * v->excP + hp;
    v->hpPrev = hp;
    v->negEPrev = -v->E;
    dk_real pitch = v->E * 45.0 + hp * 35.0;
    dk_real drive = v->a * 50.0 + hp * 0.1;
    dk_real step = DK_SIN((pitch + 47.5) / R * M_PI) * 2.0;
    dk_real oldBand = v->band;
    dk_real high = drive - v->damping * oldBand;
    v->low = step * oldBand + v->low;
    v->band = step * (high - v->low) + oldBand;
    dk_real shape = DK_EXP(-(v->band + 0.1));
    shape += -1.0;
    shape += 0.09516258196404048;
    dk_real raw = v->a * -2.0 + shape;
    v->toneState = raw * (1.0 - v->tonePole) + v->tonePole * v->toneState;
    dk_real out = hp_run(&v->outHP, v->toneState);
    out = out * LEVEL * v->A * 1.3;
    v->a = v->a - v->a * v->h;
    if (v->f1) { v->r = v->r - (v->r - 1.0) * v->rRate; v->E = v->r; }
    else if (!v->envDone) {
        v->e1 = J * 0.0008 + v->e1; v->e2 = v->e1 + v->e2; v->e3 = v->e2 + v->e3;
        v->E = v->e3 * -0.1 + 1.0;
        if (!(v->E > 0)) { v->E = 0; v->envDone = 1; }
    }
    if (v->envDone && !alive(&d->gate[DK_BD], out)) d->active[DK_BD] = 0;
    return out;
}
#endif

/* ---- Laser Gun ---- */
#if DK808_HAS(DK_LG)
static void lg_update(dk808_t *d, const dk_real *c)
{
    dk_lg_t *v = &d->lg;
    dk_real H = d->H;
    v->depth = 0.012499999999932523 * DK_EXP(4.394449154676355 * c[0]) - 0.012499999998941072;
    v->adec = 0.1 * H * (0.0011717612809592545 + 0.14882823871913825 * DK_EXP(-6.480156065054237 * c[1]));
    v->pdec = 0.1 * H * (0.0011999999984554088 + 0.2888000000010188 * DK_EXP(-2.145273604520082 * c[2]));
}
static void lg_trigger(dk808_t *d, dk_real u)
{
    dk_lg_t *v = &d->lg;
    v->tail = d->active[DK_LG] ? v->aenv * v->A : 0;
    v->A = u * trigGain;
    v->penv = 1; v->aenv = 1; v->phase = 0;
    v->timer = 0.00097 * d->R;
}
static dk_real lg_render(dk808_t *d)
{
    dk_lg_t *v = &d->lg;
    dk_real out = 0.0;
    if (v->timer > 0) {
        v->timer -= 1;
        v->tail -= v->tail * 0.11 * d->H;
        out += 0.5 * DK_SIN(v->tailPhase) * v->tail * LEVEL;
    }
    dk_real f = 40.0 + 35050.0 * v->penv * v->depth;
    v->phase += 6.283185307179586 * f / d->R;
    if (v->phase > 6.283185307179586) v->phase -= 6.283185307179586;
    v->tailPhase = v->phase;
    out += 0.5 * DK_SIN(v->phase) * v->A * v->aenv * LEVEL;
    v->penv -= v->penv * v->pdec;
    v->aenv -= v->aenv * v->adec;
    if (!alive(&d->gate[DK_LG], out)) d->active[DK_LG] = 0;
    return out;
}
#endif

/* ---- Snare Drum ---- */
#if DK808_HAS(DK_SD)
static void sd_init(dk808_t *d)
{
    dk_sd_t *v = &d->sd;
    dk_real R = d->R;
    memset(v, 0, sizeof *v);
    v->bodyLevel = 1.0; v->snapDecay = 1.0; v->snapLevel = 0.5; v->bodyLevelS = 1.0;
    v->f1 = 2.0 * DK_SIN(534.0707511102648 / R);
    v->f2 = 2.0 * DK_SIN(M_PI * 340.0 / R);
    v->f3 = 21362.83 / R;
    hp_set(&v->hpA, 12000, R);
    hp_pole(&v->hpB, pole(3400, R)); v->hpB.b = v->hpA.b;   /* Eigenheit des Originals */
}
static void sd_update(dk808_t *d, const dk_real *c)
{
    dk_sd_t *v = &d->sd;
    dk_real y = c[0], z = c[1];
    v->toneMix = 0.03214285714290635 * DK_EXP(3.4692021107718554 * y) - 0.03214285714214475;
    v->bodyLevel = 1.6666666666188559 * DK_EXP(-0.7133498879091811 * y) - 0.6666666666160388;
    v->snapDecay = 0.13650287544670278 * DK_EXP(3.6008475114694884 - 3.218875824868201 * z);
    v->snapLevel = c[2];
}
static void sd_trigger(dk808_t *d, dk_real u)
{
    dk_sd_t *v = &d->sd;
    dk_real a = u * trigGain;
    v->A2 = a * a;
    v->t1 = (int)(0.00097 * d->R); v->t2 = (int)(0.004 * d->R);
    v->eRate = 0.99 * d->H;
    v->nTarget = 1; v->nRate = 0.015 * d->H;
}
static dk_real sd_render(dk808_t *d)
{
    dk_sd_t *v = &d->sd;
    dk_real H = d->H;
    dk_real pulse = v->t1 > 0 ? v->A2 : 0.0;
    if (v->t1 > 0) v->t1 -= 1;
    if (v->t2 > 0) { v->t2 -= 1; if (v->t2 == 0) { v->nTarget = 0; v->nRate = 0.0005 * H * v->snapDecay; } }
    v->toneMixS = follow(v->toneMixS, v->toneMix, 0.002 * H);
    v->bodyLevelS = follow(v->bodyLevelS, v->bodyLevel, 0.002 * H);
    v->e = follow(v->e, pulse, v->eRate);
    dk_real hp = hp_run(&v->hpA, v->e);
    dk_real in1 = v->e + 20.0 * hp;
    dk_real t = svf_tick(&v->svf1, in1, v->f1, 0.065) + 0.5 * hp;
    dk_real band2 = svf_tick(&v->svf2, pulse, v->f2, 0.11);
    dk_real n = d->noise.out10;
    v->g = n < 0 ? 0 : v->g - 0.2 * (v->g - 1.0);
    v->nenv = follow(v->nenv, v->nTarget, v->nRate);
    dk_real band3 = svf_tick(&v->svf3, hp_run(&v->hpB, v->g * v->nenv) * v->A2, v->f3, 0.9);
    dk_real out = ((t + 5.0 * band2 * v->toneMixS) * v->bodyLevelS * 0.06 - 2.0 * band3 * v->snapLevel) * LEVEL;
    if (v->t1 == 0 && v->t2 == 0 && v->nenv < 1e-4 && !alive(&d->gate[DK_SD], out)) d->active[DK_SD] = 0;
    return out;
}
#endif

/* ---- Rim Shot ---- */
#if DK808_HAS(DK_RS)
static void rs_init(dk808_t *d)
{
    dk_rs_t *v = &d->rs;
    memset(v, 0, sizeof *v);
    hp_set(&v->hp10k, 10000, d->R);
    bq_setq(&v->hp570, BQ_HP, 570, 3, d->R);
}
static void rs_update(dk808_t *d, const dk_real *c)
{
    dk_rs_t *v = &d->rs;
    dk_real P = tune_poly(c[0]);
    v->f1 = 2.0 * DK_SIN(442.0 * M_PI * P / d->R);
    v->f2 = 2.0 * DK_SIN(1745.0 * M_PI * P / d->R);
    dk_real G = 4.0 * DK_POW(2.0, -4.0 * c[1]);
    v->q1 = 0.15 * G; v->q2 = 0.1 * G;
}
static void rs_trigger(dk808_t *d, dk_real u)
{
    dk_rs_t *v = &d->rs;
    dk_real H = d->H;
    v->A = u * trigGain;
    v->timer = 0.00097 * d->R;
    v->p1 = 1; v->p2 = 1; v->p1Rate = 0.04 * H; v->p2Rate = 0.5 * H;
    v->eTarget = 1; v->eRate = 0.09 * H;
}
static dk_real rs_render(dk808_t *d)
{
    dk_rs_t *v = &d->rs;
    dk_real H = d->H;
    if (v->timer > 0) { v->timer -= 1; if (v->timer <= 0) { v->p1Rate = 0.4 * H; v->eTarget = 0; v->eRate = 0.005 * H; } }
    dk_real c = clampd(2.5 * svf_tick(&v->svf1, 0.5 * v->p1, v->f1, v->q1), -1, 1);
    dk_real s2 = 1.25 * svf_tick(&v->svf2, v->p2, v->f2, v->q2);
    dk_real vv = dmin(0.2 * (0.5 * c - 4.5 * s2), 0);
    dk_real w = (DK_EXP(50.0 * vv) - 1.0) * v->e * v->A;
    if (w < -0.5) w += hp_run(&v->hp10k, w);
    dk_real out = -0.5 * LEVEL * bq_run(&v->hp570, w);
    v->p1 -= v->p1 * v->p1Rate;
    v->p2 -= v->p2 * v->p2Rate;
    v->e = follow(v->e, v->eTarget, v->eRate);
    if (v->timer <= 0 && v->e < 1e-4 && !alive(&d->gate[DK_RS], out)) d->active[DK_RS] = 0;
    return out;
}
#endif

/* ---- Hand Clap ---- */
#if DK808_HAS(DK_CP)
static const dk_real clap_burst[4] = {0.006, 0.0054, 0.0053, 0.0008};
static void cp_init(dk808_t *d)
{
    dk_cp_t *v = &d->cp;
    memset(v, 0, sizeof *v);
    v->done = 1;
    hp_set(&v->hp, 7300, d->R); lp_set(&v->lp, 700, d->R);
}
static void cp_update(dk808_t *d, const dk_real *c)
{
    dk_cp_t *v = &d->cp;
    v->f = 2.0 * DK_SIN(M_PI * 930.0 * (c[0] + 0.5) / d->R);
    v->tailDecay = 0.01 * (0.004999999999567403 + 0.04500000000040698 * DK_EXP(-2.1972245772713475 * c[1]));
}
static void cp_trigger(dk808_t *d, dk_real u)
{
    dk_cp_t *v = &d->cp;
    v->A = u * trigGain;
    v->a = 0.4; v->k = 0; v->done = 0;
    v->e1 = 0;
}
static dk_real cp_render(dk808_t *d)
{
    dk_cp_t *v = &d->cp;
    dk_real H = d->H;
    dk_real s = v->done ? 0 : 0.5 * (DK_TANH(6.0 * (v->a - 0.2)) + 0.833654607);
    dk_real bA = svf_tick(&v->svfA, 8.0 * hp_run(&v->hp, d->noise.out10), v->f, 0.9);
    dk_real bB = svf_tick(&v->svfB, bA, v->f, 0.9);
    dk_real filt = 0.8 * (bA + bB);
    dk_real l = lp_run(&v->lp, filt);
    if (!v->done && v->k < 3) v->e1 = follow(v->e1, 1.0, 0.0022 * H);
    else v->e1 -= v->e1 * v->tailDecay * H;
    v->e2 = follow(v->e2, v->e1, 0.4 * H);
    dk_real out = (2.0 * s * filt + 0.15 * l * v->e2) * LEVEL * v->A;
    if (!v->done) {
        v->a *= (1.0 - clap_burst[v->k] * H);
        dk_real next = v->k == 2 ? 0.1 : 0.02;
        if (v->a <= next) {
            if (v->k < 3) { v->k += 1; v->a = v->k == 3 ? 0.25 : 0.4; }
            else v->done = 1;
        }
    }
    if (v->done && v->e2 < 1e-5 && !alive(&d->gate[DK_CP], out)) d->active[DK_CP] = 0;
    return out;
}
#endif

/* ---- Congas und Toms ---- */
#if DK808_HAS_CONGA || DK808_HAS_TOM
static void drum_init(dk808_t *d, dk_drum_t *v, dk_real f0, dk_real q0)
{
    memset(v, 0, sizeof *v);
    v->f0 = f0; v->q0 = q0; v->noiseMix = 0.5; v->dampMul = 1.0;
    hp_set(&v->hp, 900, d->R);
}
static dk_real drum_env(dk808_t *d, dk_drum_t *v)
{
    if (v->timer > 0) { v->timer -= 1; if (v->timer == 0) { v->target = 0; v->rate = 0.5 * d->H; } }
    v->e = follow(v->e, v->target, v->rate);
    dk_real h = hp_run(&v->hp, -v->e);
    return h > 0 ? 0.1 * h : h;
}
#endif
#if DK808_HAS_CONGA
static void conga_update(dk808_t *d, dk_drum_t *v, const dk_real *c)
{
    v->f = 2.0 * DK_SIN(M_PI * v->f0 * tune_poly(c[0]) / d->R);
    v->q = v->q0 * 4.0 * DK_POW(2.0, -4.0 * c[1]);
}
static void conga_trigger(dk808_t *d, dk_drum_t *v, dk_real u)
{
    v->A = u * trigGain;
    v->dampMul = 1; v->target = 1;
    v->timer = (int)(0.00097 * d->R);
    v->rate = 0.99 * d->H;
}
static dk_real conga_render(dk808_t *d, dk_drum_t *v, unsigned inst)
{
    dk_real s = drum_env(d, v);
    dk_real bp = svf_tick(&v->svf, v->A * s, v->f, v->q * v->dampMul);
    dk_real out = bp * 3.0 * LEVEL * 0.763;
    if (v->timer == 0 && !alive(&d->gate[inst], out)) d->active[inst] = 0;
    return out;
}
#endif
#if DK808_HAS_TOM
static void tom_update(dk808_t *d, dk_drum_t *v, const dk_real *c)
{
    v->f = 2.0 * DK_SIN(M_PI * tune_poly(c[0]) * v->f0 / d->R);
    v->q = v->q0 * 4.0 * DK_POW(2.0, -4.0 * c[1]);
    v->noiseMix = c[2];
}
static void tom_trigger(dk808_t *d, dk_drum_t *v, dk_real u)
{
    v->A = u * trigGainBig;
    v->target = v->A; v->nenv = v->A; v->dampMul = 1;
    v->timer = (int)(0.00097 * d->R);
    v->rate = 0.99 * d->H;
}
static dk_real tom_render(dk808_t *d, dk_drum_t *v, unsigned inst)
{
    dk_real s = drum_env(d, v);
    dk_real bp = svf_tick(&v->svf, s, v->f, v->q * v->dampMul);
    dk_real out = (bp + d->noise.out138 * v->nenv * 0.15 * v->noiseMix) * 3.2 * 0.916 * LEVEL;
    v->nenv *= (1.0 - 0.0002 * d->H);
    if (v->timer == 0 && !alive(&d->gate[inst], out)) d->active[inst] = 0;
    return out;
}
#endif

/* ---- Hi-Hats (2× überabgetastet) ---- */
#if DK808_HAS_HAT
static void hat_init(dk808_t *d, dk_hat_t *v, int open)
{
    (void)d;
    memset(v, 0, sizeof *v);
    v->open = open;
    hb_init(&v->hb);
}
static void hat_update(dk808_t *d, dk_hat_t *v, const dk_real *c)
{
    dk_real vv = c[0], y = c[1], t = c[2];
    if (v->open) {
        bq_setq(&v->hpf, BQ_HP, 8840.0 * (vv + 0.5), 1.5, d->R2);
        v->dec = 0.001 * (0.2688888888889135 * DK_EXP(-3.409496184475471 * y) + 0.05111111111139488);
    } else {
        bq_setq(&v->hpf, BQ_HP, 64088.49013323178 / twoPi * (vv + 0.5), 1.5, d->R2);
        v->dec = 0.01 * (0.423837209300238 * DK_EXP(-2.0269089606884894 * y) - 0.023837209300276788);
    }
    metal_filters(&v->bp1, &v->bp2, t, d->R);
}
static void hat_trigger(dk808_t *d, dk_hat_t *v, dk_real u)
{
    v->A = u * trigGain;
    v->choked = 0;
    v->timer = 0.00097 * d->R2;
    v->target = v->A; v->e = 0; v->rate = 0.2 * d->S40_2;
}
static dk_real hat_sub(dk808_t *d, dk_hat_t *v, dk_real m)
{
    dk_real S40 = d->S40_2;
    if (v->timer > 0) v->timer -= 1;
    else { v->target = 0; v->rate = v->choked ? 0.01 * S40 : v->dec * S40; }
    dk_real vv = vca(m, 1000, 500) * v->e;
    v->p = 1.7 * vv + v->p * (1.0 - 0.3 * S40) + 0.05 * v->e + 0.01 * v->e * v->e * v->e * v->e;
    if (v->p < 0) v->p *= 0.01;
    dk_real y = bq_run(&v->hpf, v->p * 192000.0 / d->R2);
    v->e = follow(v->e, v->target, v->rate);
    return (v->open ? 0.22 : 0.44) * y;
}
static dk_real hat_render(dk808_t *d, dk_hat_t *v, unsigned inst)
{
    dk_real band = bq_run(&v->bp1, d->metal.out);
    dk_real u0 = ap_run(&v->hb.upA, band), u1 = ap_run(&v->hb.upB, band);
    dk_real o0 = hat_sub(d, v, u0);
    dk_real o1 = hat_sub(d, v, u1);
    dk_real out = hb_down(&v->hb, o0, o1) * LEVEL;
    if (v->target == 0 && v->e < 1e-5 && !alive(&d->gate[inst], out)) d->active[inst] = 0;
    return out;
}
#endif

/* ---- Cymbal (2× überabgetastet) ---- */
#if DK808_HAS(DK_CY)
static void cy_init(dk808_t *d)
{
    dk_cy_t *v = &d->cy;
    memset(v, 0, sizeof *v);
    v->tone = 0.5;
    hb_init(&v->hbA); hb_init(&v->hbB); hb_init(&v->hbOut);
    bq_setq(&v->hpA, BQ_HP, 8840, 1.5, d->R2);
    bq_setq(&v->hpB, BQ_HP, 8840, 7.5, d->R2);
    bq_setq(&v->hpLow, BQ_HP, 2800, 2, d->R2);
    lp_set(&v->lpLow, 5000, d->R2);
}
static void cy_update(dk808_t *d, const dk_real *k)
{
    dk_cy_t *v = &d->cy;
    dk_real S40 = d->S40_2;
    v->tone = k[0];
    dk_real y = k[1];
    v->d40 = (0.00035 - 0.0001 * y) * S40;
    v->d60 = 0.01 * S40 * (0.0245 * DK_EXP(-2.5055259369629344 * y) + 0.0029999999999678127);
    v->d78 = 0.001 * S40 * (0.081 * DK_EXP(-4.394449154698994 * y) + 0.019);
    metal_filters(&v->bp1, &v->bp2, k[2], d->R);
}
static void cy_trigger(dk808_t *d, dk_real u)
{
    dk_cy_t *v = &d->cy;
    dk_real S40 = d->S40_2;
    v->A = u * trigGain;
    v->timer = 0.00097 * d->R2;
    v->c = 1; v->n = 1;
    v->r1 = 0.2 * S40; v->r2 = 0.003 * S40; v->rB = 0.2 * S40; v->rN = 0.00025 * S40; v->tB = 1;
}
static dk_real cy_sub(dk808_t *d, dk_real a, dk_real b)
{
    dk_cy_t *v = &d->cy;
    dk_real S40 = d->S40_2;
    if (v->timer > 0) { v->timer -= 1; if (v->timer <= 0) { v->r1 = v->d60; v->r2 = v->d78; v->tB = 0; v->rB = 0.0007 * S40; } }
    v->c *= (1.0 - v->d40);
    v->m1 = follow(v->m1, v->c, v->r1);
    v->m2 = follow(v->m2, v->m1, v->r2);
    dk_real envA = v->m1 + v->m2;
    v->envB = follow(v->envB, v->tB, v->rB);
    v->n -= v->n * v->rN;
    dk_real env3 = dmax(v->n - 0.01, 0);
    dk_real vA = vca(a, 1000, 500) * envA;
    v->pA = 1.7 * vA + v->pA * (1.0 - 0.3 * S40);
    dk_real hA = bq_run(&v->hpA, v->pA) * 2.4;
    dk_real vB = vca(a, 1000, 500) * v->envB;
    v->pB = 1.7 * vB + v->pB * (1.0 - 0.3 * S40);
    dk_real hB = bq_run(&v->hpB, v->pB);
    dk_real vL = vca(b, 300, 100) * env3;
    v->pL = 0.7 * vL + v->pL * (1.0 - 0.1 * S40);
    dk_real lo = lp_run(&v->lpLow, bq_run(&v->hpLow, v->pL * 0.1 * 0.2));
    return (hA * (1 - v->tone / 2) + 2 * hB * v->tone + lo * (1 - v->tone / 2)) * 0.14 * LEVEL * v->A;
}
static dk_real cy_render(dk808_t *d)
{
    dk_cy_t *v = &d->cy;
    dk_real m = d->metal.out;
    dk_real xa = bq_run(&v->bp1, m);
    dk_real a0 = ap_run(&v->hbA.upA, xa), a1 = ap_run(&v->hbA.upB, xa);
    dk_real xb = bq_run(&v->bp2, m);
    dk_real b0 = ap_run(&v->hbB.upA, xb), b1 = ap_run(&v->hbB.upB, xb);
    dk_real o0 = cy_sub(d, a0, b0);
    dk_real o1 = cy_sub(d, a1, b1);
    dk_real out = hb_down(&v->hbOut, o0, o1);
    if (v->timer <= 0 && v->m1 + v->m2 < 1e-5 && v->n < 0.011 && v->envB < 1e-5 && !alive(&d->gate[DK_CY], out))
        d->active[DK_CY] = 0;
    return out;
}
#endif

/* ---- Claves ---- */
#if DK808_HAS(DK_CL)
static void cl_update(dk808_t *d, const dk_real *c)
{
    dk_cl_t *v = &d->cl;
    v->f = 2.0 * DK_SIN(tune_poly(c[0]) * 7615.220592301658 / d->R);
    v->q = 4.0 * DK_POW(2.0, -4.0 * c[1]) * 0.015;
}
static void cl_trigger(dk808_t *d, dk_real u) { d->cl.e = u * trigGain; d->cl.timer = 0.00097 * d->R; }
static dk_real cl_render(dk808_t *d)
{
    dk_cl_t *v = &d->cl;
    if (v->timer > 0) v->timer -= 1;
    dk_real bp = svf_tick(&v->svf, 4.3 * v->e, v->f, v->q);
    dk_real out = bp * 0.25 * LEVEL * 0.724;
    v->e -= v->e * 0.7 * d->H;
    if (!alive(&d->gate[DK_CL], out)) d->active[DK_CL] = 0;
    return out;
}
#endif

/* ---- Maracas ---- */
#if DK808_HAS(DK_MA)
static void ma_update(dk808_t *d, const dk_real *c)
{
    dk_bq_t *hp = &d->ma.hp;
    dk_real K = DK_TAN(M_PI * 10009.0 * (c[0] + 0.5) / d->R);
    dk_real a0 = 1.0 + K / 1.5 + K * K;
    hp->b0 = 1.0 / a0; hp->b1 = -2.0 / a0; hp->b2 = 1.0 / a0;
    hp->a1 = 2.0 * (K * K - 1.0) / a0;
    hp->a2 = (1.0 - K / 1.5 + K * K) / a0;
}
static void ma_trigger(dk808_t *d, dk_real u)
{
    dk_ma_t *v = &d->ma;
    v->A = u * trigGain;
    v->timer = 0.02 * d->R;
    v->env1Target = 1; v->env1Rate = 0.003 * d->H;
}
static dk_real ma_render(dk808_t *d)
{
    dk_ma_t *v = &d->ma;
    if (v->timer > 0) { v->timer -= 1; if (v->timer <= 0) { v->env1Target = 0; v->env1Rate = 0.02 * (d->R / 44100.0); } }
    dk_real n = d->noise.out10;
    if (n > 0) { dk_real t = n + 1.0; n = 1.5 * t - 0.5 * t * t * t - 1.0; }
    dk_real x = n * v->A * (-1.1 * v->e);
    dk_real out = bq_run(&v->hp, x) * -5.0 * LEVEL * 0.77;
    v->env1 = follow(v->env1, v->env1Target, v->env1Rate);
    v->e = follow(v->e, v->env1, 0.01 * d->H);
    if (v->timer <= 0 && v->e < 1e-5 && !alive(&d->gate[DK_MA], out)) d->active[DK_MA] = 0;
    return out;
}
#endif

/* ---- Cow Bell (2× überabgetastet) ---- */
#if DK808_HAS(DK_CB)
static void cb_init(dk808_t *d)
{
    dk_cb_t *v = &d->cb;
    memset(v, 0, sizeof *v);
    v->D = 1.0;
    hb_init(&v->hb);
    bq_setq(&v->hpf, BQ_HP, 806, 3, d->R2);
}
static void cb_update(dk808_t *d, const dk_real *c)
{
    dk_cb_t *v = &d->cb;
    dk_real f0 = 210.1 * (c[0] + 0.5);
    v->period1 = d->R2 / (2.58686 * f0); v->thr1 = (dk_real)(long)(v->period1 * 0.4375);
    v->period2 = d->R2 / (3.8386482 * f0); v->thr2 = (dk_real)(long)(v->period2 * 0.4354);
    v->D = 0.8571428571313702 + 0.14285714286308415 * DK_EXP(4.158883083319521 * (1.0 - c[1]));
}
static void cb_trigger(dk808_t *d, dk_real u)
{
    dk_cb_t *v = &d->cb;
    dk_real K = d->S40_2;
    v->A = u * trigGain;
    v->timer = 0.00097 * d->R2;
    v->e1 = 0; v->e2 = 0; v->t = 1; v->r1 = 0.025 * K; v->r2 = 0.025 * K;
}
static dk_real cb_pass(dk808_t *d)
{
    dk_cb_t *v = &d->cb;
    dk_real K = d->S40_2;
    if (v->timer > 0) { v->timer -= 1; if (v->timer <= 0) { v->t = 0; v->r1 = 0.003 * K * v->D; v->r2 = 0.00022 * K * v->D; } }
    v->ph1 += 1; if (v->ph1 >= v->period1) v->ph1 -= v->period1;
    v->ph2 += 1; if (v->ph2 >= v->period2) v->ph2 -= v->period2;
    dk_real sq1 = v->ph1 > v->thr1 ? 0.5 : -0.5;
    dk_real sq2 = v->ph2 > v->thr2 ? 0.5 : -0.5;
    dk_real env = v->e1 + 0.15 * v->e2;
    dk_real s1 = v->A * sq1 * env; if (s1 < 0) s1 *= 0.077;
    dk_real s2 = v->A * sq2 * env; if (s2 < 0) s2 *= 0.077;
    v->lp1 = v->lp1 - (v->lp1 - s1) * (s1 > v->lp1 ? 0.1 : 0.7) * K;
    v->lp2 = v->lp2 - (v->lp2 - s2) * (s2 > v->lp2 ? 0.12 : 0.7) * K;
    v->e1 = follow(v->e1, v->t, v->r1);
    v->e2 = follow(v->e2, v->t, v->r2);
    return bq_run(&v->hpf, v->lp1 + v->lp2) * LEVEL * 0.4;
}
static dk_real cb_render(dk808_t *d)
{
    dk_real o0 = cb_pass(d);
    dk_real o1 = cb_pass(d);
    dk_real out = hb_down(&d->cb.hb, o0, o1);
    if (d->cb.t == 0 && d->cb.e2 < 1e-5 && !alive(&d->gate[DK_CB], out)) d->active[DK_CB] = 0;
    return out;
}
#endif

/* ---- Regler, Namen, Drum-Map (Model.swift) ---- */
static const char *const knob_names[DK_COUNT][DK808_KNOBS] = {
    {"TONE", "DECAY", "SWEEP"}, {"DEPTH", "DECAY", "SWEEP"}, {"TONE", "DECAY", "SNAPPY"}, {"TUNE", "DECAY", ""},
    {"TONE", "REVERB", ""}, {"TUNE", "DECAY", ""}, {"TUNE", "DECAY", ""}, {"TUNE", "DECAY", ""},
    {"TUNE", "DECAY", "SNAPPY"}, {"TUNE", "DECAY", "SNAPPY"}, {"TUNE", "DECAY", "SNAPPY"},
    {"TONE", "DECAY", "BP FREQ"}, {"TONE", "DECAY", "BP FREQ"}, {"TONE", "DECAY", "BP FREQ"},
    {"TUNE", "DECAY", ""}, {"TONE", "", ""}, {"TUNE", "DECAY", ""}};
static const dk_real knob_default[DK_COUNT][DK808_KNOBS] = {
    {50, 25, 50}, {50, 100, 100}, {25, 50, 50}, {50, 50, 0}, {50, 100, 0}, {50, 50, 0}, {50, 50, 0}, {50, 50, 0},
    {50, 50, 50}, {50, 50, 50}, {50, 50, 50}, {50, 50, 50}, {50, 50, 50}, {50, 100, 50}, {50, 50, 0}, {50, 0, 0},
    {50, 100, 0}};
static const char *const abbr[DK_COUNT] = {"BD", "LG", "SD", "RS", "CP", "LC", "MC", "HC", "LT", "MT", "HT",
                                           "CH", "OH", "CY", "CL", "MA", "CB"};

const char *dk808_instrument_abbr(unsigned i) { return i < DK_COUNT ? abbr[i] : ""; }
const char *dk808_knob_name(unsigned i, unsigned k) { return i < DK_COUNT && k < DK808_KNOBS ? knob_names[i][k] : ""; }

int dk808_note_instrument(int note)
{
    switch (note) {
    case 35: case 36: return DK_BD;
    case 37: return DK_RS;
    case 38: case 40: return DK_SD;
    case 39: return DK_CP;
    case 41: case 45: return DK_LT;
    case 42: case 44: return DK_CH;
    case 43: case 50: return DK_HT;
    case 46: return DK_OH;
    case 47: case 48: return DK_MT;
    case 49: case 51: case 52: case 57: return DK_CY;
    case 56: return DK_CB;
    case 58: return DK_LG;
    case 60: case 62: return DK_HC;
    case 61: case 64: return DK_LC;
    case 63: return DK_MC;
    case 70: return DK_MA;
    case 75: return DK_CL;
    default: return -1;
    }
}

static const dk_real tom_f0[3] = {86, 140, 186}, tom_q0[3] = {0.04, 0.045, 0.045};
static const dk_real conga_f0[3] = {190, 285, 400}, conga_q0[3] = {0.023, 0.028, 0.03};

static void update(dk808_t *d, unsigned i)
{
    const dk_real *c = d->knob[i];
    switch (i) {
#if DK808_HAS(DK_BD)
    case DK_BD: bd_update(d, c); break;
#endif
#if DK808_HAS(DK_LG)
    case DK_LG: lg_update(d, c); break;
#endif
#if DK808_HAS(DK_SD)
    case DK_SD: sd_update(d, c); break;
#endif
#if DK808_HAS(DK_RS)
    case DK_RS: rs_update(d, c); break;
#endif
#if DK808_HAS(DK_CP)
    case DK_CP: cp_update(d, c); break;
#endif
#if DK808_HAS_CONGA
    case DK_LC: case DK_MC: case DK_HC: conga_update(d, &d->conga[i - DK_LC], c); break;
#endif
#if DK808_HAS_TOM
    case DK_LT: case DK_MT: case DK_HT: tom_update(d, &d->tom[i - DK_LT], c); break;
#endif
#if DK808_HAS_HAT
    case DK_CH: case DK_OH: hat_update(d, &d->hat[i - DK_CH], c); break;
#endif
#if DK808_HAS(DK_CY)
    case DK_CY: cy_update(d, c); break;
#endif
#if DK808_HAS(DK_CL)
    case DK_CL: cl_update(d, c); break;
#endif
#if DK808_HAS(DK_MA)
    case DK_MA: ma_update(d, c); break;
#endif
#if DK808_HAS(DK_CB)
    case DK_CB: cb_update(d, c); break;
#endif
    default: (void)c; break;
    }
}

void dk808_init(dk808_t *d, double rate)
{
    memset(d, 0, sizeof *d);
    d->R = rate; d->H = 2.0 * (1.0 - DK_POW(2.0, -44100.0 / rate)); d->S40 = 44100.0 / rate;
    d->R2 = rate * 2; d->S40_2 = 44100.0 / d->R2;
    d->hat_choke = 1;
    for (unsigned i = 0; i < DK_COUNT; ++i) {
        d->gate[i].limit = (int)(rate * 0.05);
        for (unsigned k = 0; k < DK808_KNOBS; ++k) d->knob[i][k] = knob_default[i][k] / 100.0;
        d->level[i] = 1;
    }
#if DK808_NEEDS_NOISE
    noise_init(&d->noise, rate);
#endif
#if DK808_NEEDS_METAL
    for (int i = 0; i < 6; ++i) d->metal.inc[i] = metal_freq[i] / rate;
#if DK808_FLOAT
    for (int i = 0; i < 6; ++i) {   /* (Startphase und Schritt aus double, wie die Referenz) */
        d->metal.ph[i] = (uint32_t)((double)d->metal.phase[i] * 4294967296.0);
        d->metal.ph_inc[i] = (uint32_t)((double)metal_freq[i] / rate * 4294967296.0 + 0.5);
    }
#endif
#endif
#if DK808_HAS(DK_BD)
    bd_init(d);
#endif
#if DK808_HAS(DK_SD)
    sd_init(d);
#endif
#if DK808_HAS(DK_RS)
    rs_init(d);
#endif
#if DK808_HAS(DK_CP)
    cp_init(d);
#endif
#if DK808_HAS_CONGA
    for (int i = 0; i < 3; ++i) drum_init(d, &d->conga[i], conga_f0[i], conga_q0[i]);
#endif
#if DK808_HAS_TOM
    for (int i = 0; i < 3; ++i) drum_init(d, &d->tom[i], tom_f0[i], tom_q0[i]);
#endif
#if DK808_HAS_HAT
    hat_init(d, &d->hat[0], 0); hat_init(d, &d->hat[1], 1);
#endif
#if DK808_HAS(DK_CY)
    cy_init(d);
#endif
#if DK808_HAS(DK_CB)
    cb_init(d);
#endif
    (void)tom_f0; (void)tom_q0; (void)conga_f0; (void)conga_q0;
    for (unsigned i = 0; i < DK_COUNT; ++i)
        if (DK808_INSTRUMENTS >> i & 1u) update(d, i);
}

void dk808_set_level(dk808_t *d, unsigned i, double level)
{
    if (i < DK_COUNT) d->level[i] = (dk_real)(level < 0 ? 0 : level > 1 ? 1 : level);
}
void dk808_set_knob(dk808_t *d, unsigned i, unsigned k, double value)
{
    if (i >= DK_COUNT || k >= DK808_KNOBS) return;
    d->knob[i][k] = value < 0 ? 0 : value > 1 ? 1 : value;
    if (DK808_INSTRUMENTS >> i & 1u) update(d, i);
}

void dk808_trigger(dk808_t *d, unsigned i, int velocity)
{
    if (i >= DK_COUNT || !(DK808_INSTRUMENTS >> i & 1u) || velocity <= 0 || d->queued >= DK808_QUEUE) return;
    d->queue[d->queued].inst = (uint8_t)i;
    d->queue[d->queued].vel = (uint8_t)(velocity > 127 ? 127 : velocity);
    ++d->queued;
}

static void choke(dk808_t *d, unsigned i)
{
    switch (i) {
#if DK808_HAS(DK_OH)
    case DK_OH: d->hat[1].choked = 1; break;
#endif
#if DK808_HAS_CONGA
    case DK_LC: case DK_MC: case DK_HC: d->conga[i - DK_LC].dampMul = 12; break;
#endif
#if DK808_HAS_TOM
    case DK_LT: case DK_MT: case DK_HT: d->tom[i - DK_LT].dampMul = 12; d->tom[i - DK_LT].nenv = 0; break;
#endif
    default: (void)d; break;
    }
}

/* Engine.fire */
static void fire(dk808_t *d, unsigned i, int velocity)
{
    float v32 = (float)velocity * 0.007874015718698502f;
    dk_real u = (dk_real)v32 * 1.377;
    if ((i == DK_CH || i == DK_OH) && d->hat_choke) choke(d, DK_OH);
    if (i >= DK_LC && i <= DK_HC && d->conga_choke)
        for (unsigned c = DK_LC; c <= DK_HC; ++c) if (c != i) choke(d, c);
    if (i >= DK_LT && i <= DK_HT && d->tom_choke)
        for (unsigned c = DK_LT; c <= DK_HT; ++c) if (c != i) choke(d, c);
    switch (i) {
#if DK808_HAS(DK_BD)
    case DK_BD: bd_trigger(d, u); break;
#endif
#if DK808_HAS(DK_LG)
    case DK_LG: lg_trigger(d, u); break;
#endif
#if DK808_HAS(DK_SD)
    case DK_SD: sd_trigger(d, u); break;
#endif
#if DK808_HAS(DK_RS)
    case DK_RS: rs_trigger(d, u); break;
#endif
#if DK808_HAS(DK_CP)
    case DK_CP: cp_trigger(d, u); break;
#endif
#if DK808_HAS_CONGA
    case DK_LC: case DK_MC: case DK_HC: conga_trigger(d, &d->conga[i - DK_LC], u); break;
#endif
#if DK808_HAS_TOM
    case DK_LT: case DK_MT: case DK_HT: tom_trigger(d, &d->tom[i - DK_LT], u); break;
#endif
#if DK808_HAS_HAT
    case DK_CH: case DK_OH: hat_trigger(d, &d->hat[i - DK_CH], u); break;
#endif
#if DK808_HAS(DK_CY)
    case DK_CY: cy_trigger(d, u); break;
#endif
#if DK808_HAS(DK_CL)
    case DK_CL: cl_trigger(d, u); break;
#endif
#if DK808_HAS(DK_MA)
    case DK_MA: ma_trigger(d, u); break;
#endif
#if DK808_HAS(DK_CB)
    case DK_CB: cb_trigger(d, u); break;
#endif
    default: (void)u; return;
    }
    d->active[i] = 1;
    d->gate[i].quiet = 0;
}

/* Engine.tick: erst wartende Trigger, dann Rauschen und Metallbank, dann jede aktive Stimme. */
dk_real dk808_tick(dk808_t *d)
{
    for (unsigned q = 0; q < d->queued; ++q) fire(d, d->queue[q].inst, d->queue[q].vel);
    d->queued = 0;
#if DK808_NEEDS_NOISE
    noise_tick(&d->noise);
#endif
#if DK808_NEEDS_METAL
    metal_tick(&d->metal);
#endif
    dk_real out = 0.0;
#if DK808_HAS(DK_BD)
    if (d->active[DK_BD]) out += d->level[DK_BD] * bd_render(d);
#endif
#if DK808_HAS(DK_LG)
    if (d->active[DK_LG]) out += d->level[DK_LG] * lg_render(d);
#endif
#if DK808_HAS(DK_SD)
    if (d->active[DK_SD]) out += d->level[DK_SD] * sd_render(d);
#endif
#if DK808_HAS(DK_RS)
    if (d->active[DK_RS]) out += d->level[DK_RS] * rs_render(d);
#endif
#if DK808_HAS(DK_CP)
    if (d->active[DK_CP]) out += d->level[DK_CP] * cp_render(d);
#endif
#if DK808_HAS_CONGA
    for (unsigned i = DK_LC; i <= DK_HC; ++i)
        if (DK808_HAS(i) && d->active[i]) out += d->level[i] * conga_render(d, &d->conga[i - DK_LC], i);
#endif
#if DK808_HAS_TOM
    for (unsigned i = DK_LT; i <= DK_HT; ++i)
        if (DK808_HAS(i) && d->active[i]) out += d->level[i] * tom_render(d, &d->tom[i - DK_LT], i);
#endif
#if DK808_HAS_HAT
    for (unsigned i = DK_CH; i <= DK_OH; ++i)
        if (DK808_HAS(i) && d->active[i]) out += d->level[i] * hat_render(d, &d->hat[i - DK_CH], i);
#endif
#if DK808_HAS(DK_CY)
    if (d->active[DK_CY]) out += d->level[DK_CY] * cy_render(d);
#endif
#if DK808_HAS(DK_CL)
    if (d->active[DK_CL]) out += d->level[DK_CL] * cl_render(d);
#endif
#if DK808_HAS(DK_MA)
    if (d->active[DK_MA]) out += d->level[DK_MA] * ma_render(d);
#endif
#if DK808_HAS(DK_CB)
    if (d->active[DK_CB]) out += d->level[DK_CB] * cb_render(d);
#endif
    return out;
}

void dk808_render(dk808_t *d, float *left, float *right, int n)
{
    for (int i = 0; i < n; ++i) {
        float s = (float)dk808_tick(d);
        left[i] += s;
        right[i] += s;
    }
}

/* ---- baustein_t ---- */
static const char *b_param_name(unsigned p)
{
    static const char *const sw[3] = {"HAT CHOKE", "CONGA CHOKE", "TOM CHOKE"};
    if (p < DK808_P_HAT_CHOKE) return knob_names[p / DK808_KNOBS][p % DK808_KNOBS];
    return p < DK808_PARAMS ? sw[p - DK808_P_HAT_CHOKE] : "";
}
static void b_init(void *s, double rate) { dk808_init(s, rate); }
static void b_note_on(void *s, int note, int vel)
{
    int i = dk808_note_instrument(note);
    if (i >= 0) dk808_trigger(s, (unsigned)i, vel);
}
static void b_note_off(void *s, int note) { (void)s; (void)note; }
static void b_set(void *s, unsigned p, double v)
{
    dk808_t *d = s;
    if (p < DK808_P_HAT_CHOKE) dk808_set_knob(d, p / DK808_KNOBS, p % DK808_KNOBS, v);
    else if (p == DK808_P_HAT_CHOKE) d->hat_choke = v >= 0.5;
    else if (p == DK808_P_CONGA_CHOKE) d->conga_choke = v >= 0.5;
    else if (p == DK808_P_TOM_CHOKE) d->tom_choke = v >= 0.5;
}
static void b_render(void *s, float *l, float *r, int n) { dk808_render(s, l, r, n); }
static int b_active(const void *s)
{
    const dk808_t *d = s;
    if (d->queued) return 1;
    for (unsigned i = 0; i < DK_COUNT; ++i) if (d->active[i]) return 1;
    return 0;
}

const baustein_t dk808_baustein = {"dk808", "TR-808 (DrumKern)", sizeof(dk808_t), DK808_PARAMS, b_param_name,
                                   b_init, b_note_on, b_note_off, b_set, b_render, b_active};
