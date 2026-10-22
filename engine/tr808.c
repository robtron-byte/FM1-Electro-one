/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Circuit-informed TR-808 voices; values and sources in docs/TR808.md. */
/* Device build: all code and constants of this file run in internal SRAM. */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "tr808.h"
#include "dsp_math.h"
#include <string.h>

#define MS(x) ((x) * 0.001f * EL_SAMPLE_RATE)
enum { P_LEVEL, P_TUNE, P_DECAY, P_TONE, P_SNAPPY, P_SOUND, P_PAN, P_REV, P_DLY };

/* ---- parameters ------------------------------------------------------------ */

static const char *const tom_names[] = {"TOM", "CONGA"}, *const rim_names[] = {"RIM", "CLAVE"};
static const char *const clap_names[] = {"CLAP", "MARAC"}, *const choke_names[] = {"OFF", "CH>OH"};
#define P(n, d) {n, 127, d, 0}
/* Every part has the same 9 slots; parts list only the ones they use. */
typedef struct { uint8_t count; uint8_t slot[TR_PARAMS]; fm1_param_t desc[TR_PARAMS]; } part_params_t;
static const part_params_t params[D8_NUM + 1] = {
    /* BD */ {8, {P_LEVEL, P_TUNE, P_DECAY, P_TONE, P_PAN, P_REV, P_DLY, P_SNAPPY},
              {P("Level", 100), P("Tune", 64), P("Decay", 64), P("Tone", 50), P("Pan", 64), P("Rev", 0), P("Dly", 0), P("Drive", 0)}},
    /* SD */ {8, {P_LEVEL, P_TUNE, P_TONE, P_SNAPPY, P_DECAY, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Tune", 64), P("Tone", 64), P("Snappy", 72), P("Decay", 64), P("Pan", 64), P("Rev", 0), P("Dly", 0)}},
    /* LT */ {7, {P_LEVEL, P_TUNE, P_DECAY, P_SOUND, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Tune", 64), P("Decay", 64), {"Sound", 1, 0, tom_names}, P("Pan", 45), P("Rev", 0), P("Dly", 0)}},
    /* MT */ {7, {P_LEVEL, P_TUNE, P_DECAY, P_SOUND, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Tune", 64), P("Decay", 64), {"Sound", 1, 0, tom_names}, P("Pan", 64), P("Rev", 0), P("Dly", 0)}},
    /* HT */ {7, {P_LEVEL, P_TUNE, P_DECAY, P_SOUND, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Tune", 64), P("Decay", 64), {"Sound", 1, 0, tom_names}, P("Pan", 83), P("Rev", 0), P("Dly", 0)}},
    /* RS */ {7, {P_LEVEL, P_TUNE, P_DECAY, P_SOUND, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Tune", 64), P("Decay", 64), {"Sound", 1, 0, rim_names}, P("Pan", 51), P("Rev", 0), P("Dly", 0)}},
    /* CP */ {8, {P_LEVEL, P_TONE, P_DECAY, P_SOUND, P_PAN, P_REV, P_DLY, P_TUNE},
              {P("Level", 100), P("Tone", 64), P("Decay", 64), {"Sound", 1, 0, clap_names}, P("Pan", 64), P("Rev", 0), P("Dly", 0), P("Tune", 64)}},
    /* CB */ {6, {P_LEVEL, P_TUNE, P_DECAY, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Tune", 64), P("Decay", 64), P("Pan", 77), P("Rev", 0), P("Dly", 0)}},
    /* CY */ {7, {P_LEVEL, P_TONE, P_DECAY, P_TUNE, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Tone", 64), P("Decay", 64), P("Tune", 64), P("Pan", 70), P("Rev", 0), P("Dly", 0)}},
    /* OH */ {6, {P_LEVEL, P_DECAY, P_TUNE, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Decay", 40), P("Tune", 64), P("Pan", 54), P("Rev", 0), P("Dly", 0)}},
    /* CH */ {6, {P_LEVEL, P_TUNE, P_DECAY, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Tune", 64), P("Decay", 64), P("Pan", 74), P("Rev", 0), P("Dly", 0)}},
    /* ZP: Tune = start pitch, Sweep = fall time, Depth = how far it falls,
     * Tone = drive from sine to a buzzy square */
    /* ZP */ {9, {P_LEVEL, P_TUNE, P_SNAPPY, P_SOUND, P_DECAY, P_TONE, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Tune", 64), P("Sweep", 50), P("Depth", 80), P("Decay", 50), P("Tone", 30),
               P("Pan", 76), P("Rev", 10), P("Dly", 30)}},
    /* PU: the TRIGGER OUT pulse (+15 V, ~20 ms on the spec sheet, ~10 ms
     * edge to edge on "Egypt, Egypt") into a mixer channel: Width = pulse
     * length, Tune = the channel EQ's boost frequency, Tone = how much EQ
     * (the ring that makes it a blip), Decay = input coupling (spike length) */
    /* PU */ {8, {P_LEVEL, P_SNAPPY, P_TUNE, P_TONE, P_DECAY, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Width", 64), P("Tune", 64), P("Tone", 70), P("Decay", 40), P("Pan", 52),
               P("Rev", 0), P("Dly", 0)}},
    /* KIT */ {3, {0, 1, 2}, {P("Level", 110), P("Accent", 100), {"Choke", 1, 1, choke_names}}},
};

int tr808_nparams(int part) { return (unsigned)part <= D8_KIT ? params[part].count : 0; }

const fm1_param_t *tr808_param(int part, int p)
{
    return (unsigned)part <= D8_KIT && (unsigned)p < params[part].count ? &params[part].desc[p] : 0;
}

static void set_pan(tr808_t *t, int part, int value)
{
    float a = (value - 64) * (DSP_PI / 256.0f);
    t->pan_l[part] = dsp_cos(a) - dsp_sin(a);
    t->pan_r[part] = dsp_cos(a) + dsp_sin(a);
}

EL_DSP_CODE void tr808_set(tr808_t *t, int part, int p, int value)
{
    const fm1_param_t *d = tr808_param(part, p);
    if (!d) return;
    value = value < 0 ? 0 : value > d->max ? d->max : value;
    if (part == D8_KIT) { t->kit[p] = (uint8_t)value; return; }
    unsigned slot = params[part].slot[p];
    t->pot[part][slot] = (uint8_t)value;
    if (slot == P_PAN) set_pan(t, part, value);
}

EL_DSP_CODE int tr808_get(const tr808_t *t, int part, int p)
{
    if (!tr808_param(part, p)) return 0;
    return part == D8_KIT ? t->kit[p] : t->pot[part][params[part].slot[p]];
}

/* Fixed per-unit component tolerance: the frequencies of a real 808 sit a
 * little off the nominal values, differently on every machine. */
static const float tolerance[D8_NUM] EL_DSP_DATA = {1.004f, 0.992f, 1.011f, 0.995f, 1.008f, 0.989f, 1.006f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
static const float metal_hz[6] EL_DSP_DATA = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};
static const float metal_tol[6] EL_DSP_DATA = {1.012f, 0.991f, 1.006f, 0.988f, 1.0f, 1.0f};

static inline void svf_set(tr_svf_t *f, float hz, float q);

void tr808_init(tr808_t *t)
{
    memset(t, 0, sizeof *t);
    for (int part = 0; part <= D8_KIT; ++part)
        for (int p = 0; p < params[part].count; ++p) tr808_set(t, part, p, params[part].desc[p].def);
    for (int part = 0; part < D8_NUM; ++part) t->v[part].rng = 0x9e3779b9u * (uint32_t)(part + 1);
    t->noise = 0x1234567u;
    for (int i = 0; i < 6; ++i) t->metal_phase[i] = 0.137f * (float)i;
    svf_set(&t->metal_bp1, 3450.0f, 6.0f);
    svf_set(&t->metal_bp2, 7100.0f, 6.0f);
}

/* ---- building blocks ------------------------------------------------------- */

/* Zero-delay-feedback state-variable filter (TPT). */
static inline void svf_set(tr_svf_t *f, float hz, float q)
{
    f->g = dsp_tan(DSP_PI * dsp_clamp(hz, 10.0f, 19000.0f) / EL_SAMPLE_RATE);
    f->k = 1.0f / dsp_max(q, 0.3f);
    f->a1 = 1.0f / (1.0f + f->g * (f->g + f->k));
    f->a2 = f->g * f->a1;
    f->a3 = f->g * f->a2;
}

/* Returns the band-pass output normalised to unity gain at the centre;
 * *low gets the low-pass output when needed. */
static inline float svf_bp(tr_svf_t *f, float x, float *low)
{
    float v3 = x - f->ic2, v1 = f->a1 * f->ic1 + f->a2 * v3, v2 = f->ic2 + f->a2 * f->ic1 + f->a3 * v3;
    f->ic1 = 2.0f * v1 - f->ic1;
    f->ic2 = 2.0f * v2 - f->ic2;
    if (low) *low = v2;
    return f->k * v1;
}

static inline float svf_hp(tr_svf_t *f, float x)
{
    float v3 = x - f->ic2, v1 = f->a1 * f->ic1 + f->a2 * v3, v2 = f->ic2 + f->a2 * f->ic1 + f->a3 * v3;
    f->ic1 = 2.0f * v1 - f->ic1;
    f->ic2 = 2.0f * v2 - f->ic2;
    return x - f->k * v1 - v2;
}

static inline float noise(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    *s = n;
    return (float)(int32_t)n * (1.0f / 2147483648.0f);
}

static inline float powf_approx(float a, float b) { return a <= 0 ? 0 : dsp_exp2(b * dsp_log2(a)); }
static inline float decay_coef(float samples) { return dsp_exp(-1.0f / dsp_max(samples, 1.0f)); }
static inline float knob(const tr808_t *t, int part, int slot) { return t->pot[part][slot] / 127.0f; }
/* Knob extension around the stock value: 64 = stock, 0 / 127 = /2 / x2. */
static inline float spread(const tr808_t *t, int part, int slot) { return dsp_exp2((t->pot[part][slot] - 64) / 64.0f); }

/* The 1 ms trigger pulse after the 808's differentiating pulse shaper
 * (high-pass, tau 0.1 ms, negative excursion clamped by a diode). */
static inline float trigger_pulse(tr_voice_t *v)
{
    float rect = v->age < (uint32_t)MS(1.0f) ? v->volts : 0.0f;
    v->pulse_lp += (rect - v->pulse_lp) * 0.2035f;
    float hp = rect - v->pulse_lp;
    return hp < -0.05f ? -0.05f : hp;
}

/* Transistor "swing" VCA: mild asymmetric gain. */
static inline float swing(float x) { return x > 0 ? x : x * (0.75f - 0.15f * x); }

/* ---- trigger ------------------------------------------------------------------- */

EL_DSP_CODE void tr808_trigger(tr808_t *t, int part, float velocity)
{
    if ((unsigned)part >= D8_NUM || velocity <= 0) return;
    tr_voice_t *v = &t->v[part];
    /* Accent: trigger height 4 V normal up to 14 V (scaled by the kit's
     * ACCENT knob); soft MIDI hits below the normal level are just quieter. */
    float a = dsp_clamp((velocity - 0.66f) / 0.34f, 0.0f, 1.0f) * (t->kit[1] / 127.0f);
    float volts = (4.0f + 10.0f * a) / 14.0f * dsp_min(1.0f, velocity / 0.66f);
    v->volts = volts;
    v->accent = a;
    v->age = 0;
    v->pulse_lp = 0;
    v->active = 1;
    v->env = 1.0f;
    v->env2 = 1.0f;
    v->env3 = 1.0f;
    v->aux = 0;
    v->aux2 = 0;
    v->pitch = 1.0f;
    /* Closed hat shuts the open hat off (fast, not instantly). */
    if (part == D8_CH && t->kit[2] && t->v[D8_OH].active) t->v[D8_OH].aux2 = 1.0f;
    /* Resonator and filter states are kept: no "machine gun" restart. */
}

EL_DSP_CODE void tr808_silence(tr808_t *t)
{
    for (int i = 0; i < D8_NUM; ++i) t->v[i].active = 0;
}

EL_DSP_CODE int tr808_active(const tr808_t *t)
{
    for (int i = 0; i < D8_NUM; ++i) if (t->v[i].active) return 1;
    return 0;
}

/* ---- voices ----------------------------------------------------------------------- */

/* Bass drum: shaped pulse into one resonator, attack switch to ~130 Hz for
 * 5 ms, retrigger kick, amplitude-dependent pitch sigh, tone low-pass with
 * the pulse leaking through (click), slight asymmetric non-linearity. */
static float bass_drum(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_BD;
    float f0 = 50.0f * tolerance[part] * dsp_exp2((t->pot[part][P_TUNE] - 64) / 64.0f);
    float q = 2.3f * dsp_exp2(5.19f * powf_approx(knob(t, part, P_DECAY), 0.8f));
    float pulse = trigger_pulse(v);
    uint32_t attack = (uint32_t)MS(5.0f);
    if (v->age == attack) v->aux = -0.8f * 0.2f * v->volts;  /* retrigger pulse */
    v->aux *= 0.99955f;                                      /* 50 ms */
    if ((v->age & 7u) == 0) {
        if (v->age < attack) svf_set(&v->r1, 130.0f * tolerance[part], 6.0f);
        else {
            /* Sigh: up to about a semitone at full accent, following the level. */
            float sigh = (0.02f + 0.04f * v->accent) * v->env;
            svf_set(&v->r1, f0 * (1.0f + sigh), q);
        }
    }
    v->env *= decay_coef(q / (DSP_PI * f0) * EL_SAMPLE_RATE);
    /* Raw resonator voltage (not normalised by Q): switching Q at the end of
     * the attack keeps the waveform continuous, as in the circuit. */
    float y = svf_bp(&v->r1, (pulse + v->aux) * 1.6f, 0) / v->r1.k;
    /* Feedback buffer non-linearity: 2nd harmonic about -44 dB, 3rd -53 dB. */
    float x = y * 1.4f;
    y = (x + 0.0126f * x * x - 0.009f * x * x * x) / 1.4f;
    float drive = knob(t, part, P_SNAPPY);
    if (drive > 0) { float d = 1.0f + 6.0f * drive; y = dsp_tanh(y * d) / dsp_sqrt(d); }
    /* Tone: 1-pole low-pass 305 Hz .. 7.2 kHz on the click path. */
    float tone = knob(t, part, P_TONE);
    float cutoff = 305.0f * dsp_exp2(4.56f * tone);
    float click = pulse * (0.04f + 0.18f * tone);
    v->out_lp += (y + click - v->out_lp) * dsp_min(1.0f, 2.0f * DSP_PI * cutoff / EL_SAMPLE_RATE);
    /* The resonator itself is far below the tone corner; pass it straight
     * and only filter the click edge. */
    float out = y + (v->out_lp - y);
    /* Output DC block (about 3.4 Hz). */
    float o = out - v->dc_x + 0.99952f * v->dc_y;
    v->dc_x = out; v->dc_y = o;
    if (v->age > (uint32_t)MS(30.0f) && v->env * v->volts < 1e-4f) v->active = 0;
    return o;
}

/* Snare: two resonators (173 / 336 Hz on later units) cross-faded by TONE,
 * plus noise set by SNAPPY. For weight and snap (where the circuit is
 * driven hard by the 808's trigger and output stages):
 *  - the body starts slightly sharp and settles within ~10 ms (knock),
 *  - the transistor summing stage saturates the body a little,
 *  - the noise covers 1.2 .. 9 kHz with a fast 30 ms decay plus a quieter
 *    90 ms tail (the measured envelope is longer than one RC alone),
 *  - the trigger edge leaks through as a short click. */
static float snare(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_SD;
    float pulse = trigger_pulse(v);
    float tune = tolerance[part] * spread(t, part, P_TUNE);
    if (v->age == 0) {
        svf_set(&v->r3, 1200.0f, 0.7f);   /* noise high-pass */
        v->aux = 0;
    }
    /* Knock: up to +8 % at full accent, settling with tau 6 ms. */
    v->pitch *= 0.99623f;
    if ((v->age & 7u) == 0) {
        float knock = 1.0f + (0.04f + 0.04f * v->accent) * v->pitch;
        svf_set(&v->r1, 173.0f * tune * knock, 20.0f);
        svf_set(&v->r2, 336.0f * tune * knock, 12.0f);
    }
    float tone = knob(t, part, P_TONE);
    /* Raw resonator voltages, as the circuit sums them. */
    float body = (1.0f - tone) * svf_bp(&v->r1, pulse * 3.0f, 0) / v->r1.k +
                 tone * 1.3f * svf_bp(&v->r2, pulse * 3.0f, 0) / v->r2.k;
    body = dsp_tanh(body * 0.9f) / 0.9f;
    float decay = spread(t, part, P_DECAY);
    v->env *= decay_coef(MS(30.0f) * decay);
    v->env2 *= decay_coef(MS(90.0f) * decay);
    float attack = dsp_min(1.0f, (float)v->age / MS(0.5f));
    float n = svf_hp(&v->r3, noise(&v->rng));
    v->aux += (n - v->aux) * 0.7277f;    /* 2-pole-ish low-pass at ~9 kHz */
    v->out_lp += (v->aux - v->out_lp) * 0.7277f;
    float noise_env = (v->env + 0.22f * v->env2) * attack * v->volts;
    float snap = (pulse > 0 ? pulse : 0) * 0.6f;
    float out = body * 0.55f + (v->out_lp * noise_env * 1.15f + snap) * knob(t, part, P_SNAPPY);
    if (v->age > (uint32_t)MS(500.0f) && v->env2 < 1e-4f) v->active = 0;
    return out;
}

/* Toms / congas: one resonator; diodes make the pitch start sharp and drop
 * (x1.06 normal, x1.14 accent). Toms add a little low-passed pink noise. */
static const float tom_hz[6][3] EL_DSP_DATA = {{80, 90, 100}, {120, 135, 160}, {165, 185, 220},
                                               {165, 185, 220}, {250, 280, 310}, {370, 400, 455}};
static float tom(tr808_t *t, tr_voice_t *v, int part)
{
    int conga = t->pot[part][P_SOUND] == 1, idx = (part - D8_LT) + (conga ? 3 : 0);
    float k = knob(t, part, P_TUNE), lo = tom_hz[idx][0], mid = tom_hz[idx][1], hi = tom_hz[idx][2];
    float f0 = (k < 0.5f ? lo * dsp_exp2(2.0f * k * dsp_log2(mid / lo)) : mid * dsp_exp2((2.0f * k - 1.0f) * dsp_log2(hi / mid))) * tolerance[part];
    float q = (conga ? 45.0f : 25.0f) * spread(t, part, P_DECAY);
    float pulse = trigger_pulse(v);
    float drop = (conga ? 0.3f : 1.0f) * (0.06f + 0.10f * v->accent);
    v->pitch *= decay_coef(MS(13.0f + 15.0f * v->accent));
    if ((v->age & 7u) == 0) svf_set(&v->r1, f0 * (1.0f + drop * v->pitch), q);
    float y = svf_bp(&v->r1, pulse * 2.4f, 0);
    v->env2 *= decay_coef(q / (DSP_PI * f0) * EL_SAMPLE_RATE);
    if (!conga) {
        v->env *= decay_coef(MS(85.0f) / 2.3f);
        v->out_lp += (noise(&v->rng) - v->out_lp) * 0.0554f;   /* ~400 Hz */
        y += v->out_lp * v->env * v->volts * 0.1f;
    }
    if (v->age > (uint32_t)MS(30.0f) && v->env2 * v->volts < 1e-4f) v->active = 0;
    return y;
}

/* Rim shot: 455 + 1667 Hz resonators into an asymmetric swing VCA (the
 * bright ring), 22 ms gate. Claves: one 2.5 kHz resonator ringing, 25 ms. */
static float rim(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_RS;
    int clave = t->pot[part][P_SOUND] == 1;
    float tune = tolerance[part] * spread(t, part, P_TUNE), pulse = trigger_pulse(v);
    if (v->age == 0) {
        svf_set(&v->r1, 455.0f * tune, 6.7f);
        svf_set(&v->r2, 1667.0f * tune, 13.5f);
        svf_set(&v->r3, 2500.0f * tune, 120.0f);
    }
    float gate_ms = (clave ? 25.0f : 22.0f) * spread(t, part, P_DECAY);
    float gate = v->age < (uint32_t)MS(gate_ms) ? 1.0f : 0.0f;
    v->env += (gate - v->env) * 0.0222f;   /* ~1 ms smoothing at the gate edges */
    float y;
    if (clave) y = svf_bp(&v->r3, pulse * 0.35f, 0) * 0.8f;
    else {
        float s = svf_bp(&v->r1, pulse * 2.0f, 0) + svf_bp(&v->r2, pulse * 2.0f, 0);
        y = s > 0 ? 4.0f * s : 0.1f * s;
        y = dsp_tanh(y * 0.5f) * 0.6f;
        v->out_lp += (y - v->out_lp) * 0.02f;  /* remove the thump (~150 Hz) */
        y -= v->out_lp;
    }
    y *= v->env;
    if (v->age > (uint32_t)MS(gate_ms + 10.0f)) v->active = 0;
    return y;
}

/* Hand clap: band-passed noise (~1.1 kHz, Q 2) through a sawtooth envelope
 * of three 10 ms pulses plus a 20 ms fall, and a quieter ~100 ms "reverb".
 * Maracas: high-passed noise with a 13 ms rise and 17 ms fall. */
static float clap(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_CP;
    float n = noise(&v->rng);
    if (t->pot[part][P_SOUND] == 1) {
        if (v->age == 0) svf_set(&v->r1, 6000.0f * spread(t, part, P_TUNE), 0.8f);
        float ms = (float)v->age / MS(1.0f), rise = 13.0f, fall = 17.0f * spread(t, part, P_DECAY);
        float e = ms < rise ? ms / rise : dsp_max(0.0f, 1.0f - (ms - rise) / fall);
        if (ms > rise + fall + 5.0f) v->active = 0;
        return svf_hp(&v->r1, n) * e * e * v->volts * 1.1f;
    }
    if (v->age == 0) svf_set(&v->r1, 1140.0f * spread(t, part, P_TONE) * tolerance[part], 1.95f);
    float bp = svf_bp(&v->r1, n, 0);
    float ms = (float)v->age / MS(1.0f), saw;
    if (ms < 30.0f) { float in = ms - 10.0f * (float)(int)(ms / 10.0f); saw = 1.0f - in / 10.0f; }
    else saw = dsp_exp(-(ms - 30.0f) / 8.7f);
    v->env2 *= decay_coef(MS(43.0f) * spread(t, part, P_DECAY));
    float e = saw + 0.38f * v->env2;
    if (ms > 30.0f && v->env2 < 1e-4f) v->active = 0;
    float y = bp * e * v->volts * 1.5f;
    return dsp_tanh(y * 1.4f) / 1.2f;   /* the BA662 VCA's distortion */
}

/* Shared metal source: six HD14584 Schmitt-trigger oscillators, 0/5 V
 * rectangles with 47.98 % duty (Werner), summed through equal resistors
 * into a 7-level staircase. Band-limited with PolyBLEP: the edges are what
 * strike the band-passes, aliased edges would add a grainy noise floor. */
#define METAL_DUTY 0.4798f
static inline float blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

static inline float rect(float phase, float dt)
{
    float y = phase < METAL_DUTY ? 1.0f : 0.0f;
    float down = phase - METAL_DUTY;
    if (down < 0) down += 1.0f;
    return y + 0.5f * blep(phase, dt) - 0.5f * blep(down, dt);
}

static float metal(tr808_t *t, float tune, float *cowbell)
{
    float sum = 0, r[6];
    for (int i = 0; i < 6; ++i) {
        float dt = metal_hz[i] * metal_tol[i] * tune * (1.0f / EL_SAMPLE_RATE);
        t->metal_phase[i] += dt;
        if (t->metal_phase[i] >= 1.0f) t->metal_phase[i] -= 1.0f;
        r[i] = rect(t->metal_phase[i], dt);
        sum += r[i];
    }
    *cowbell = 2.0f * (r[4] + r[5]) - 2.0f;
    return sum * (1.0f / 3.0f) - 1.0f;
}

static float cowbell(tr808_t *t, tr_voice_t *v, float src)
{
    const int part = D8_CB;
    if (v->age == 0) {
        float tune = spread(t, part, P_TUNE);
        svf_set(&v->r1, 880.0f * tune, 4.5f);
        svf_set(&v->r2, 880.0f * tune, 2.5f);
    }
    float y = svf_bp(&v->r2, svf_bp(&v->r1, src * 0.5f, 0), 0);
    /* Two-stage decay: down by half within 50 ms, then a ~500 ms tail. */
    v->env *= decay_coef(MS(15.0f));
    v->env2 *= decay_coef(MS(150.0f) * spread(t, part, P_DECAY));
    float attack = dsp_min(1.0f, (float)v->age / MS(0.7f));
    float e = (0.5f * v->env + 0.5f * v->env2) * attack;
    if (v->env2 < 1e-4f) v->active = 0;
    return swing(y * e * (0.45f + 0.55f * v->volts * 14.0f / 4.0f / 3.5f)) * 1.4f;
}

/* Swing-type VCA (one transistor, the envelope is its collector supply):
 * it passes mainly the positive half and clips against the envelope, so
 * the metal voices get rougher, not only quieter, as they decay (Werner's
 * "wild swing"). Linear for small signals at a high envelope. */
static inline float swing_vca(float x, float env)
{
    float r = x > 0 ? 4.0f * x : 0.1f * x;
    return r * env / (env + (r > 0 ? r : -r) + 1e-6f);
}

/* Cymbal calibration (fitted with host/analyze_cymbal.py against the band
 * balance and decay measurements of a real TR-808 at all 25 TONE / DECAY
 * settings, Fischer sample set). Host builds may change it. */
float tr808_cy_cal[CY_CAL] = {
    /* drive  low   decay short  tau_s  tone_s tone_l tone_d  tau_l0 tau_l1 tau_d0 tau_d1 lag_l lag_d  tone_p */
    1.2f,  1.0f,  1.0f,  1.0f,  30.0f, 3.0f,  1.0f,  0.5f,  300.0f, 800.0f, 100.0f, 380.0f, 60.0f, 6.0f, 2.0f,
    /* hh2 Hz  hh3 Hz   output low-pass Hz */
    8840.0f, 10320.0f, 18000.0f};

/* Cymbal after the circuit (Service Notes p.13, Werner ICMC-14): the
 * staircase through two band-passes (3.45 and 7.1 kHz, Q 6), three swing
 * VCAs with their own envelopes, three Sallen-Key high-passes, TONE.
 *   low band:   3.45 kHz -> VCA (reservoir behind a ~70 ms RC: it blooms and
 *               peaks ~120 ms after the hit) -> HP 2.5 kHz Q 0.97 -> the tone
 *               network's 4.2 kHz pole. Its decay follows DECAY too.
 *   DECAY band: 7.1 kHz -> VCA (DECAY, RC lag) -> HP 8.84 kHz Q 1, x2.
 *   short band: 7.1 kHz -> VCA (fixed, short) -> HP 10.32 kHz Q 5.64 plus a
 *               1-pole HP 5.2 kHz, +8.86 dB: the bright "tss" of the strike.
 * TONE mainly lifts the short band against the low one. */
static float cymbal(tr808_t *t, tr_voice_t *v, float low_src, float high_src)
{
    const int part = D8_CY;
    const float *k = tr808_cy_cal;
    if (v->age == 0) {
        svf_set(&v->r1, 2500.0f, 0.97f);
        svf_set(&v->r2, k[15], 1.0f);
        svf_set(&v->r3, k[16], 5.64f);
        /* The three reservoirs charge to the trigger height: accent +6 dB. */
        v->aux = 0.5f + 0.5f * v->accent;
    }
    float decay = knob(t, part, P_DECAY), tone = powf_approx(knob(t, part, P_TONE), k[14]);
    v->env *= decay_coef(MS(k[8] + k[9] * decay));       /* low band reservoir */
    v->env2 *= decay_coef(MS(k[10] + k[11] * decay));     /* DECAY band reservoir */
    v->env3 *= decay_coef(MS(k[4]));                      /* short band */
    v->lag1 += (v->env - v->lag1) * (1.0f / MS(k[12]));
    v->lag2 += (v->env2 - v->lag2) * (1.0f / MS(k[13]));
    float attack = dsp_min(1.0f, (float)v->age / MS(0.1f));
    float peak = v->aux * attack, d = k[0];
    float low = swing_vca(low_src * d, v->lag1 * peak);
    float mid = swing_vca(high_src * d * 1.14f, v->lag2 * peak);
    float hi = swing_vca(high_src * d * 1.14f, v->env3 * peak);
    low = svf_hp(&v->r1, low);
    v->lp1 += (low - v->lp1) * 0.4513f;                   /* 4.2 kHz pole */
    mid = 2.0f * svf_hp(&v->r2, mid);
    hi = svf_hp(&v->r3, hi);
    v->hp1 += (hi - v->hp1) * 0.5243f;                    /* 1-pole HP 5.2 kHz */
    hi = 2.78f * (hi - v->hp1);
    float g_low = k[1] * dsp_exp2(-k[6] * (tone - 0.5f)), g_mid = k[2] * dsp_exp2(k[7] * (tone - 0.5f));
    float g_hi = k[3] * dsp_exp2(k[5] * (tone - 0.5f));
    if (v->env < 1e-4f && v->env2 < 1e-4f && v->age > MS(300.0f)) v->active = 0;
    /* Tone network and LEVEL buffer: nearly flat, rolling off at the top. */
    v->out_lp += (g_low * v->lp1 + g_mid * mid + g_hi * hi - v->out_lp) * (1.0f - dsp_exp(-2.0f * DSP_PI * k[17] / EL_SAMPLE_RATE));
    return v->out_lp * (1.0f + 0.6f * v->accent);   /* accent +6 dB, see hat() */
}

/* Hats (Service Notes p.6/13): the 7.1 kHz band of the six oscillators
 * (no noise), a swing VCA, then a Sallen-Key high-pass, OH 7.8 kHz and
 * CH 11.7 kHz, both Q 2.5. Decay chart: OH 90 / 450 / 600 ms over its
 * DECAY knob, CH 50 ms. Accent +6 dB like the chart. */
static float hat(tr808_t *t, tr_voice_t *v, int part, float src)
{
    if (v->age == 0) {
        svf_set(&v->r1, part == D8_CH ? 11700.0f : 7800.0f, 2.5f);
        v->aux = 0.5f + 0.5f * v->accent;
    }
    float k = knob(t, part, P_DECAY);
    float decay = part == D8_CH ? 50.0f * spread(t, part, P_DECAY) : k < 0.5f ? 90.0f + 720.0f * k : 450.0f + 300.0f * (k - 0.5f);
    v->env *= decay_coef(MS(decay) / 2.3f);
    if (v->aux2 > 0) v->env *= decay_coef(MS(7.0f));    /* choked by the closed hat */
    float attack = dsp_min(1.0f, (float)v->age / MS(0.1f));
    float y = svf_hp(&v->r1, swing_vca(src * 1.2f, v->env * attack * v->aux));
    if (v->env < 1e-4f) v->active = 0;
    /* Accent: the higher reservoir alone gives ~+2 dB through the clipping
     * VCA; the chart's +6 dB needs the rest as gain. */
    return y * (1.0f + 0.6f * v->accent);
}

/* Zap: a sine whose pitch falls exponentially from Tune (300 Hz..6 kHz)
 * by Depth (up to 7 octaves) within Sweep (5..400 ms), the electro laser.
 * Tone drives it into a soft-clipped square; accent opens it further. */
static float zap(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_ZP;
    if (v->age == 0) {
        v->aux = 1340.0f * spread(t, part, P_TUNE) * spread(t, part, P_TUNE);   /* start Hz */
        v->pitch = 1.0f;
    }
    float sweep = 5.0f * dsp_exp2(6.3f * knob(t, part, P_SNAPPY));            /* ms */
    float depth = 7.0f * knob(t, part, P_SOUND);                               /* octaves */
    v->pitch *= decay_coef(MS(sweep));
    float hz = v->aux * dsp_exp2(-depth * (1.0f - v->pitch));
    v->out_lp += dsp_min(hz, 18000.0f) * (1.0f / EL_SAMPLE_RATE);
    if (v->out_lp >= 1.0f) v->out_lp -= 1.0f;
    float y = dsp_sin(2.0f * DSP_PI * v->out_lp);
    float drive = 1.0f + 12.0f * knob(t, part, P_TONE) * (0.6f + 0.4f * v->accent);
    y = dsp_tanh(y * drive) / dsp_tanh(drive);
    v->env *= decay_coef(MS(20.0f * dsp_exp2(5.3f * knob(t, part, P_DECAY))));
    float attack = dsp_min(1.0f, (float)v->age / MS(0.3f));
    if (v->env < 1e-4f) v->active = 0;
    return y * v->env * attack * (0.35f + 0.65f * v->volts);
}

/* Pulse: the 808's TRIGGER OUT (+15 V, a rectangle of ~10-20 ms) plugged
 * straight into a mixer channel, as Egyptian Lover did. The input coupling
 * turns the two edges into a sharp positive spike and, one pulse width
 * later, an equally sharp negative one; the hot 15 V clip the preamp; the
 * channel EQ boost makes each edge ring (the "zap"/blip). Width 2.5..40 ms,
 * Tune 400 Hz..6.4 kHz, Tone = EQ boost and Q, Decay = coupling 0.05..2 ms. */
static float pulse(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_PU;
    if (v->age == 0) {
        float tone = knob(t, part, P_TONE);
        svf_set(&v->r1, 1600.0f * spread(t, part, P_TUNE) * spread(t, part, P_TUNE), 1.5f + 8.0f * tone);
    }
    float width = MS(10.0f * dsp_exp2(4.0f * (knob(t, part, P_SNAPPY) - 0.5f)));   /* 2.5..40 ms, 64: 10 ms */
    float x = (float)v->age < width ? 1.0f : 0.0f;
    float coupling = MS(0.05f * dsp_exp2(5.3f * knob(t, part, P_DECAY)));
    v->dc_y = x - v->dc_x;                       /* 1-pole high-pass: the edges */
    v->dc_x += v->dc_y * (1.0f - decay_coef(coupling));
    float y = dsp_tanh(2.5f * v->dc_y) * 0.42f;  /* 15 V into a line input: clipped */
    float tone = knob(t, part, P_TONE);
    y += (0.6f + 7.0f * tone) * svf_bp(&v->r1, y, 0) * (0.15f + 0.85f * tone);
    v->out_lp += (y - v->out_lp) * 0.72f;        /* mixer bandwidth, ~9 kHz */
    if ((float)v->age > width + MS(60.0f)) v->active = 0;
    return v->out_lp * (0.6f + 0.4f * v->volts);
}

/* ---- render ------------------------------------------------------------------------- */

/* Output trims, calibrated with host/analyze_808.py: unaccented peaks follow
 * the Service Notes level chart (BD 3.5, SD 3, LT/HT 3.5, MT 3, RS 3 Vpp),
 * metal voices and clap set to a conventional mix level. */
static const float trim[D8_NUM] EL_DSP_DATA = {7.421f, 1.561f, 20.603f, 20.528f, 18.905f, 8.573f, 2.043f, 0.644f, 1.483f, 0.478f, 1.751f, 0.3f, 0.5f};

EL_DSP_CODE void tr808_render(tr808_t *t, float *kick, float *left, float *right,
                              float *rev, float *gate, float *dly, int frames)
{
    int metal_on = t->v[D8_CY].active || t->v[D8_OH].active || t->v[D8_CH].active || t->v[D8_CB].active;
    float level = t->kit[0] / 110.0f;
    for (int n = 0; n < frames; ++n) {
        float low_src = 0, high_src = 0, cb_src = 0;
        if (metal_on) {
            float m = metal(t, spread(t, D8_CY, P_TUNE), &cb_src);
            low_src = svf_bp(&t->metal_bp1, m, 0);
            high_src = svf_bp(&t->metal_bp2, m, 0);
        }
        for (int part = 0; part < D8_NUM; ++part) {
            tr_voice_t *v = &t->v[part];
            if (!v->active) continue;
            float y;
            switch (part) {
            case D8_BD: y = bass_drum(t, v); break;
            case D8_SD: y = snare(t, v); break;
            case D8_LT: case D8_MT: case D8_HT: y = tom(t, v, part); break;
            case D8_RS: y = rim(t, v); break;
            case D8_CP: y = clap(t, v); break;
            case D8_CB: y = cowbell(t, v, cb_src); break;
            case D8_CY: y = cymbal(t, v, low_src, high_src); break;
            case D8_ZP: y = zap(t, v); break;
            case D8_PU: y = pulse(t, v); break;
            default: y = hat(t, v, part, high_src * spread(t, part, P_TUNE) / spread(t, D8_CY, P_TUNE)); break;
            }
            ++v->age;
            y *= trim[part] * (t->pot[part][P_LEVEL] / 100.0f) * level;
            if (part == D8_BD) { kick[n] += y; }
            else { left[n] += y * t->pan_l[part]; right[n] += y * t->pan_r[part]; }
            float r = y * knob(t, part, P_REV), d = y * knob(t, part, P_DLY);
            if (part == D8_SD || part == D8_CP || part == D8_RS) gate[n] += r; else rev[n] += r;
            dly[n] += d;
        }
    }
}
