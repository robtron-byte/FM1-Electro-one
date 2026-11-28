/* SPDX-License-Identifier: GPL-3.0-only
 * JP-6 solo: Pool, Parts und Kerne um den JP-6-Kern von FM1-Electro (engine/jpp.h: was es ist). engine/jp6.c wird hier
 * eingebunden, nicht kopiert: Hüllkurven, VCOs, VCF, HPF, derive, control, Distortion und Chorus sind seine. Eigen ist
 * nur render_slice: render_voice aus jp6.c, das die Werte eines Steuerschritts aus jpp_slice_t liest statt aus dem
 * Part, damit beide Kerne die Stimmen eines Parts zugleich rechnen können (jp6.c rechnet Steuerschritt und Stimmen
 * abwechselnd). */
#include "jp6.c"
#include "jpp.h"

static derived_t dv[JPP_PARTS];                 /* derive je Part, einmal je Block (wie jp6_render) */

void jpp_init(jpp_t *s, unsigned pool, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    for (unsigned p = 0; p < JPP_PARTS; ++p) jp6_part_init(&s->part[p], JP6_MAX_VOICES, seed + p);
    s->pool = (uint8_t)(pool < 1 ? 1 : pool > JP6_MAX_VOICES * JPP_PARTS ? JP6_MAX_VOICES * JPP_PARTS : pool);
    s->cores = 1;
}

EL_DSP_CODE void jpp_part_load(jpp_t *s, unsigned pi, const uint8_t v[JP_COUNT]) { if (pi < JPP_PARTS) jp6_part_load(&s->part[pi], v); }
EL_DSP_CODE void jpp_part_set(jpp_t *s, unsigned pi, unsigned param, int value) { if (pi < JPP_PARTS) jp6_part_set(&s->part[pi], param, value); }
EL_DSP_CODE void jpp_all_off(jpp_t *s, unsigned pi) { if (pi < JPP_PARTS) jp6_all_off(&s->part[pi]); }
EL_DSP_CODE int jpp_active(const jpp_t *s, unsigned pi) { return pi < JPP_PARTS && jp6_active(&s->part[pi]); }
EL_DSP_CODE unsigned jpp_part_voices(const jpp_t *s, unsigned pi)
{
    unsigned n = 0;
    if (pi < JPP_PARTS) for (unsigned i = 0; i < JP6_MAX_VOICES; ++i) n += s->part[pi].voice[i].active;
    return n;
}

/* Der Pool: hält ein Part mehr, als frei ist, geht die älteste Stimme der anderen Parts (erst losgelassene). */
static EL_DSP_HELPER void pool_make_room(jpp_t *s, unsigned pi, unsigned want)
{
    for (;;) {
        unsigned others = 0, mine = jpp_part_voices(s, pi);
        for (unsigned p = 0; p < JPP_PARTS; ++p) if (p != pi) others += jpp_part_voices(s, p);
        if (others + (want > mine ? want : mine) <= s->pool || !others) return;
        jp6_voice_t *best = 0; unsigned bp = 0, bi = 0;
        for (int pass = 0; pass < 2 && !best; ++pass)
            for (unsigned p = 0; p < JPP_PARTS; ++p) {
                if (p == pi) continue;
                for (unsigned i = 0; i < JP6_MAX_VOICES; ++i) {
                    jp6_voice_t *v = &s->part[p].voice[i];
                    if (!v->active || (pass == 0 && v->gate)) continue;
                    if (!best || s->stamp[p][i] < s->stamp[bp][bi]) { best = v; bp = p; bi = i; }
                }
            }
        if (!best) return;
        best->active = 0; best->gate = 0; best->key = -1;
        best->env1.stage = best->env2.stage = ENV_IDLE;
        best->env1.value = best->env2.value = 0;
    }
}

EL_DSP_CODE void jpp_note_on(jpp_t *s, unsigned pi, int note, float velocity, int legato)
{
    if (pi >= JPP_PARTS) return;
    jp6_part_t *p = &s->part[pi];
    int mode = p->v[JP_KEY_MODE];
    unsigned mine = jpp_part_voices(s, pi), want = mode == 3 ? JP6_MAX_VOICES : mode == 2 ? 1u : mine + 1u;
    pool_make_room(s, pi, want > JP6_MAX_VOICES ? JP6_MAX_VOICES : want);
    uint32_t before[JP6_MAX_VOICES];
    for (unsigned i = 0; i < JP6_MAX_VOICES; ++i) before[i] = p->voice[i].age;
    jp6_note_on(p, note, velocity, legato);
    for (unsigned i = 0; i < JP6_MAX_VOICES; ++i) if (p->voice[i].age != before[i]) s->stamp[pi][i] = ++s->clock;
}

EL_DSP_CODE void jpp_note_off(jpp_t *s, unsigned pi, int note) { if (pi < JPP_PARTS) jp6_note_off(&s->part[pi], note); }

/* ---- ein Block ------------------------------------------------------------------------------------------------- */

/* render_voice aus engine/jp6.c, die Werte des Steuerschritts aus sl (dort: p->lfo_value, p->hpf_*). */
static EL_DSP_CODE void render_slice(const jp6_part_t *p, const jpp_slice_t *sl, jp6_voice_t *v, const derived_t *d,
                                     float *left, float *right, int n)
{
    float cutoff_oct = sl->cut;
    if (d->porta) v->pitch += (v->target_pitch - v->pitch) * d->porta_coef;
    else v->pitch = v->target_pitch;
    float pitch = v->pitch, drift_semis = 0, drift_oct = 0;
    if (d->drift > 0) {
        v->wander += xorshift(&v->rng) * 0.02f * (float)n / 16.0f - v->wander * 0.0005f * (float)n;
        v->wander = dsp_clamp(v->wander, -1, 1);
        drift_semis = d->drift * (v->drift_pitch * 0.08f + v->wander * 0.04f);
        drift_oct = d->drift * v->drift_cutoff * 0.15f;
    }
    if (v->lfo_fade < 1.0f) v->lfo_fade = dsp_min(1, v->lfo_fade + d->lfo_fade_inc);
    float lfo = sl->lfo * v->lfo_fade;
    float velocity = v->velocity;
    float env1 = v->env1.value * d->env1_sign, env2 = v->env2.value;
    float mod = lfo * d->lfo_semis + env1 * d->env1_semis;
    float base = pitch + drift_semis + v->unison_spread * d->unison_semis;
    float dt1 = note_hz(base + d->offset1 + (d->mod1 ? mod : 0)) * (1.0f / EL_SAMPLE_RATE);
    float dt2 = dsp_clamp(note_hz(base + d->offset2 + (d->mod2 ? mod : 0)) * (1.0f / EL_SAMPLE_RATE), 0.0f, 0.45f);
    float pw_amount = d->pw_depth;
    if (d->pwm_source == 0) pw_amount = d->pw_depth * (0.5f + 0.5f * lfo);
    else if (d->pwm_source == 2) pw_amount = d->pw_depth * dsp_abs(env1);
    float pw = 0.5f - 0.45f * dsp_clamp(pw_amount, 0, 1);
    float vcf_env = (d->vcf_env2 ? env2 : env1) * (1.0f - d->vcf_velocity + d->vcf_velocity * velocity);
    float octaves = cutoff_oct + d->key_follow * (pitch - 60.0f) + vcf_env * d->vcf_env_oct + lfo * d->vcf_lfo_oct + drift_oct;
    vcf_t f;
    vcf_set(&f, 20.0f * dsp_exp2(dsp_clamp(octaves, -2, 11)), d);
    float xmod = dsp_clamp(d->crossmod + d->crossmod_env * v->env1.value, 0, 1);
    xmod = xmod < 0.004f ? 0.0f : 4.0f * xmod * dsp_sqrt(xmod);
    float dynamics = 1.0f - d->vca_velocity + d->vca_velocity * velocity;
    float tremolo = 1.0f - d->tremolo * (0.5f - 0.5f * lfo);
    float amp = d->amp * dynamics * tremolo * (v->unison ? 0.45f : 1.0f);
    float pan = v->unison ? v->unison_spread * 0.85f : 0.0f;
    float gl = amp * dsp_min(1, 1 - pan), gr = amp * dsp_min(1, 1 + pan);
    env_settings_t s1, s2;
    env_settings(p, v, 0, pitch, &d->env[0], &s1);
    env_settings(p, v, 1, pitch, &d->env[1], &s2);
    float b0 = sl->hb0, b1 = sl->hb1, b2 = sl->hb2, a1 = sl->ha1, a2 = sl->ha2;
    int hpf_on = sl->hpf_on;
    float fc = 0.85f * dsp_min(dt1, dt2) * EL_SAMPLE_RATE;
    if (hpf_on && fc < hpf_hz(dsp_max(sl->hpf_s, 0.5f))) {
        if (dsp_abs(fc - v->hpf_fc) > 0.005f * fc) { hpf_coefs(fc, v->hpf_c); v->hpf_fc = fc; }
        b0 = v->hpf_c[0]; b1 = v->hpf_c[1]; b2 = v->hpf_c[2]; a1 = v->hpf_c[3]; a2 = v->hpf_c[4];
    }
    for (int k = 0; k < n; ++k) {
        env_next(&v->env1, &s1);
        float e = env_next(&v->env2, &s2);
        float x = vco_tick(v, d, dt1, dt2, pw, xmod) * 0.3f;
        if (hpf_on) {
            float y = b0 * x + v->hz1;
            v->hz1 = b1 * x - a1 * y + v->hz2;
            v->hz2 = b2 * x - a2 * y;
            x = y;
        }
        x = vcf(v, &f, x) * e;
        left[k] += x * gl;
        right[k] += x * gr;
    }
    if (v->env2.stage == ENV_IDLE) { v->active = 0; v->key = -1; }
}

/* Welche Stimmen und Parts dieser Block wo rechnet: Stimmen der Reihe nach, Kern 1 bekommt die Hälfte (weniger, wenn
 * dort Delay und Hall laufen); Parts mit Nachklang abwechselnd, Kern 0 zuerst. */
EL_DSP_CODE void jpp_plan(jpp_t *s)
{
    unsigned n = 0, k = 0, two = s->cores > 1;
    for (unsigned p = 0; p < JPP_PARTS; ++p) n += jpp_part_voices(s, p);
    unsigned n1 = two ? n / 2u : 0u;
    if (two && s->extra1 && n1) n1 -= n1 > 1 ? 2u : 1u;
    if (two && s->extra1 && n1 > n) n1 = 0;
    for (unsigned c = 0; c < JPP_CORES; ++c) s->voices_on[c] = s->parts_on[c] = s->ctl_on[c] = 0;
    unsigned lp = 0;
    for (unsigned p = 0; p < JPP_PARTS; ++p) {
        jp6_part_t *pt = &s->part[p];
        int any = 0;
        for (unsigned i = 0; i < JP6_MAX_VOICES; ++i) {
            s->vcore[p][i] = 0;
            if (!pt->voice[i].active) continue;
            any = 1;
            s->vcore[p][i] = (uint8_t)(k++ >= n - n1 ? 1 : 0);
            s->voices_on[s->vcore[p][i]]++;
        }
        s->anyv[p] = (uint8_t)any;
        s->live[p] = (uint8_t)(any || pt->silent_blocks < SILENT_BLOCKS);
        s->pcore[p] = (uint8_t)(two && s->live[p] && (lp++ & 1u) && !s->extra1 ? 1 : 0);
        if (s->live[p]) s->parts_on[s->pcore[p]]++;
    }
    s->ctl_on[0] = 1;
}

/* Kern 0 legt für alle klingenden Parts die Steuerschritte des Blocks fest (derive, control aus jp6.c) und leert die
 * Puffer; Kern 1 hat hier nichts zu tun. */
EL_DSP_CODE void jpp_control(jpp_t *s, unsigned core, float bpm)
{
    if (core) return;
    for (unsigned p = 0; p < JPP_PARTS; ++p) {
        if (!s->live[p]) continue;
        jp6_part_t *pt = &s->part[p];
        derive(pt, &dv[p]);
        for (unsigned k = 0; k < JPP_SLICES; ++k) {
            jpp_slice_t *sl = &s->sl[p][k];
            sl->cut = control(pt, bpm);
            sl->lfo = pt->lfo_value; sl->hpf_on = pt->hpf_on; sl->hpf_s = pt->hpf_smoothed;
            sl->hb0 = pt->hpf_b0; sl->hb1 = pt->hpf_b1; sl->hb2 = pt->hpf_b2; sl->ha1 = pt->hpf_a1; sl->ha2 = pt->hpf_a2;
        }
        for (unsigned c = 0; c < JPP_CORES; ++c)
            for (unsigned i = 0; i < JPP_BLOCK; ++i) s->vout[c][p][0][i] = s->vout[c][p][1][i] = 0;
    }
}

EL_DSP_CODE void jpp_voices(jpp_t *s, unsigned core)
{
    for (unsigned p = 0; p < JPP_PARTS; ++p) {
        if (!s->live[p]) continue;
        jp6_part_t *pt = &s->part[p];
        float *l = s->vout[core][p][0], *r = s->vout[core][p][1];
        for (unsigned i = 0; i < JP6_MAX_VOICES; ++i) {
            jp6_voice_t *v = &pt->voice[i];
            if (s->vcore[p][i] != core) continue;
            for (unsigned k = 0; k < JPP_SLICES && v->active; ++k)
                render_slice(pt, &s->sl[p][k], v, &dv[p], l + k * CONTROL, r + k * CONTROL, CONTROL);
        }
    }
}

/* Die Stimmen beider Kerne zusammen, dann wie jp6_render: DC-Sperre und Lautstärke, Distortion, Chorus, Pegel, Sends. */
EL_DSP_CODE void jpp_finish(jpp_t *s, unsigned core)
{
    for (unsigned pi = 0; pi < JPP_PARTS; ++pi) {
        if (s->pcore[pi] != core) continue;
        float *ol = s->pout[pi][0], *orr = s->pout[pi][1], *dl = s->psend[pi][0], *rl = s->psend[pi][1];
        for (unsigned i = 0; i < JPP_BLOCK; ++i) ol[i] = orr[i] = dl[i] = rl[i] = 0;
        if (!s->live[pi]) continue;
        jp6_part_t *p = &s->part[pi];
        float l[JPP_BLOCK], r[JPP_BLOCK];
        int any = s->anyv[pi];
        for (unsigned i = 0; i < JPP_BLOCK; ++i) {
            l[i] = s->vout[0][pi][0][i] + s->vout[1][pi][0][i];
            r[i] = s->vout[0][pi][1][i] + s->vout[1][pi][1][i];
        }
        const int frames = JPP_BLOCK;
        float g = p->volume_smoothed * 0.5f;
        for (int i = 0; i < frames; ++i) {
            float yl = l[i] - p->dc_xl + 0.9995f * p->dc_yl, yr = r[i] - p->dc_xr + 0.9995f * p->dc_yr;
            p->dc_xl = l[i]; p->dc_yl = yl;
            p->dc_xr = r[i]; p->dc_yr = yr;
            l[i] = yl * g;
            r[i] = yr * g;
        }
        dist_process(&p->dist, l, r, frames, p->v[JP_DIST_MODE], p->v[JP_DIST_DRIVE], p->v[JP_DIST_TONE],
                     p->v[JP_DIST_LEVEL], p->v[JP_DIST_MIX], dv[pi].stereo);
        chorus(&p->chorus, l, r, frames, p->v[JP_CHORUS], p->v[JP_CHORUS_MODE], curve_exp(curve_chorus, p->v[JP_CHORUS_RATE]));
        float level = p->level;
        float ds = powf_fast(p->v[JP_DELAY_SEND] / 127.0f, 1.5f) * level, rs = powf_fast(p->v[JP_REVERB_SEND] / 127.0f, 1.5f) * level;
        for (int i = 0; i < frames; ++i) {
            ol[i] += l[i] * level;
            orr[i] += r[i] * level;
            float m = 0.5f * (l[i] + r[i]);
            dl[i] += m * ds;
            rl[i] += m * rs;
        }
        if (any) p->silent_blocks = 0;
        else if (++p->silent_blocks >= SILENT_BLOCKS) p->dc_xl = p->dc_yl = p->dc_xr = p->dc_yr = 0;
    }
}

EL_DSP_CODE void jpp_mix(jpp_t *s, float *left, float *right)
{
    for (unsigned i = 0; i < JPP_BLOCK; ++i) s->send_sum[0][i] = s->send_sum[1][i] = 0;
    for (unsigned p = 0; p < JPP_PARTS; ++p) {
        if (!s->live[p]) continue;
        for (unsigned i = 0; i < JPP_BLOCK; ++i) {
            left[i] += s->pout[p][0][i]; right[i] += s->pout[p][1][i];
            s->send_sum[0][i] += s->psend[p][0][i]; s->send_sum[1][i] += s->psend[p][1][i];
        }
    }
}

EL_DSP_CODE void jpp_render(jpp_t *s, float *left, float *right, float bpm)
{
    s->cores = 1;
    jpp_plan(s);
    jpp_control(s, 0, bpm);
    jpp_voices(s, 0);
    jpp_voices(s, 1);
    jpp_finish(s, 0);
    jpp_finish(s, 1);
    jpp_mix(s, left, right);
}

float jpp_lfo_hz(int rate) { return lfo_hz((float)el_clampi(rate, 0, 127)); }
