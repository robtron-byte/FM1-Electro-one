/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Circuit-informed TR-808 voices; values and sources in docs/TR808.md. */
/* Device build: the constants of this file sit in internal SRAM; its audio
 * code is marked EL_DSP_CODE / EL_DSP_HELPER (control code runs from Flash). */
#ifndef FM1_HOST_TEST
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "tr808.h"
#include "dsp_math.h"
#include "po12.h"
#include <string.h>

#define MS(x) ((x) * 0.001f * EL_SAMPLE_RATE)
enum { P_LEVEL, P_TUNE, P_DECAY, P_TONE, P_SNAPPY, P_SOUND, P_PAN, P_REV, P_DLY };

/* ---- parameters ------------------------------------------------------------ */

static const char *const tom_names[] = {"TOM", "CONGA"}, *const rim_names[] = {"RIM", "CLAVE"};
static const char *const sd_names[] = {"LATE", "EARLY"}, *const engine_names[] = {"TR-808", "PO-12"};
static const char *const key_names[] = {"OFF", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const follow_names[] = {"OFF", "BASS"};
static const char *const clap_names[] = {"CLAP", "MARAC"}, *const choke_names[] = {"OFF", "CH>OH", "BOTH"};
#define P(n, d) {n, 127, d, 0}
/* Every part has the same 9 slots; parts list only the ones they use.
 * Parameters added later go at the end of a part's list with 0 = the
 * sound as before (older saves hold 0 there): BD Punch, Drive on RS, CP,
 * CB, CY, OH, CH. */
typedef struct { uint8_t count; uint8_t slot[TR_PARAMS]; fm1_param_t desc[TR_PARAMS]; } part_params_t;
static const part_params_t params[D8_NUM + 1] = {
    /* BD */ {9, {P_LEVEL, P_TUNE, P_DECAY, P_TONE, P_PAN, P_REV, P_DLY, P_SNAPPY, P_SOUND},
              {P("Level", 100), P("Tune", 64), P("Decay", 64), P("Tone", 50), P("Pan", 64), P("Rev", 0), P("Dly", 0), P("Drive", 0),
               P("Punch", 0)}},
    /* SD: Sound = later units (172 / 339 Hz) or the first ones (238 / 476 Hz) */
    /* SD */ {9, {P_LEVEL, P_TUNE, P_TONE, P_SNAPPY, P_DECAY, P_PAN, P_REV, P_DLY, P_SOUND},
              {P("Level", 100), P("Tune", 64), P("Tone", 64), P("Snappy", 72), P("Decay", 64), P("Pan", 64), P("Rev", 0), P("Dly", 0),
               {"Sound", 1, 0, sd_names}}},
    /* Toms: the four knobs of the breakdown first (Pitch, Bend = how far
     * the pitch sags, Decay, Noise), then Level. Store version 15 moved
     * them here from Level, Tune, Decay, Sound, Pan, Rev, Dly. */
    /* LT */ {9, {P_TUNE, P_SNAPPY, P_DECAY, P_TONE, P_LEVEL, P_SOUND, P_PAN, P_REV, P_DLY},
              {P("Pitch", 64), P("Bend", 64), P("Decay", 64), P("Noise", 64), P("Level", 100), {"Sound", 1, 0, tom_names},
               P("Pan", 45), P("Rev", 0), P("Dly", 0)}},
    /* HT */ {9, {P_TUNE, P_SNAPPY, P_DECAY, P_TONE, P_LEVEL, P_SOUND, P_PAN, P_REV, P_DLY},
              {P("Pitch", 64), P("Bend", 64), P("Decay", 64), P("Noise", 64), P("Level", 100), {"Sound", 1, 0, tom_names},
               P("Pan", 83), P("Rev", 0), P("Dly", 0)}},
    /* RS */ {8, {P_LEVEL, P_TUNE, P_DECAY, P_SOUND, P_PAN, P_REV, P_DLY, P_SNAPPY},
              {P("Level", 100), P("Tune", 64), P("Decay", 64), {"Sound", 1, 0, rim_names}, P("Pan", 51), P("Rev", 0), P("Dly", 0),
               P("Drive", 0)}},
    /* CP */ {9, {P_LEVEL, P_TONE, P_DECAY, P_SOUND, P_PAN, P_REV, P_DLY, P_TUNE, P_SNAPPY},
              {P("Level", 100), P("Tone", 64), P("Decay", 64), {"Sound", 1, 0, clap_names}, P("Pan", 64), P("Rev", 0), P("Dly", 0), P("Tune", 64),
               P("Drive", 0)}},
    /* CB */ {7, {P_LEVEL, P_TUNE, P_DECAY, P_PAN, P_REV, P_DLY, P_SNAPPY},
              {P("Level", 100), P("Tune", 64), P("Decay", 64), P("Pan", 77), P("Rev", 0), P("Dly", 0), P("Drive", 0)}},
    /* CY */ {8, {P_LEVEL, P_TONE, P_DECAY, P_TUNE, P_PAN, P_REV, P_DLY, P_SNAPPY},
              {P("Level", 100), P("Tone", 64), P("Decay", 64), P("Tune", 64), P("Pan", 70), P("Rev", 0), P("Dly", 0), P("Drive", 0)}},
    /* OH */ {7, {P_LEVEL, P_DECAY, P_TUNE, P_PAN, P_REV, P_DLY, P_SNAPPY},
              {P("Level", 100), P("Decay", 40), P("Tune", 64), P("Pan", 54), P("Rev", 0), P("Dly", 0), P("Drive", 0)}},
    /* CH */ {7, {P_LEVEL, P_TUNE, P_DECAY, P_PAN, P_REV, P_DLY, P_SNAPPY},
              {P("Level", 100), P("Tune", 64), P("Decay", 64), P("Pan", 74), P("Rev", 0), P("Dly", 0), P("Drive", 0)}},
    /* ZP: Tune = start pitch, Sweep = fall time, Depth = how far it falls,
     * Tone = drive from sine to a buzzy square */
    /* ZP */ {9, {P_LEVEL, P_TUNE, P_SNAPPY, P_SOUND, P_DECAY, P_TONE, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Tune", 64), P("Sweep", 50), P("Depth", 80), P("Decay", 50), P("Tone", 30),
               P("Pan", 76), P("Rev", 10), P("Dly", 30)}},
    /* PU: the TRIGGER OUT into a mixer channel, fitted to a recording of a
     * real 808's trigger. Width = pulse length, Tune = pitch of the
     * coupling's thump, Ring = its resonance, Decay = spike length. */
    /* PU */ {8, {P_LEVEL, P_SNAPPY, P_TUNE, P_TONE, P_DECAY, P_PAN, P_REV, P_DLY},
              {P("Level", 100), P("Width", 64), P("Tune", 64), P("Ring", 64), P("Decay", 64), P("Pan", 52),
               P("Rev", 0), P("Dly", 0)}},
    /* KIT: Engine switches every part but PU to the PO-12 style voices */
    /* KIT: Choke CH>OH = the closed hat cuts the open one; BOTH = either cuts the other */
    /* KIT */ {6, {0, 1, 2, 3, 4, 5}, {P("Level", 110), P("Accent", 100), {"Choke", 2, 1, choke_names}, {"Engine", 1, 0, engine_names},
               {"Key", 12, 0, key_names}, {"Follow", 1, 0, follow_names}}},
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
static const float tolerance[D8_NUM] EL_DSP_DATA = {1.004f, 0.992f, 1.011f, 1.008f, 0.989f, 1.006f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
static const float metal_hz[6] EL_DSP_DATA = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};
static const float metal_tol[6] EL_DSP_DATA = {1.012f, 0.991f, 1.006f, 0.988f, 1.0f, 1.0f};

static inline void svf_set(tr_svf_t *f, float hz, float q);

static inline float decay_coef(float samples);
static float spread_tab[128];
static float tom_env3_c, tom_env2_c, tom_lag2_c, tom_bias;

void tr808_init(tr808_t *t)
{
    memset(t, 0, sizeof *t);
    for (int part = 0; part <= D8_KIT; ++part)
        for (int p = 0; p < params[part].count; ++p) tr808_set(t, part, p, params[part].desc[p].def);
    for (int i = 0; i < 128; ++i) spread_tab[i] = dsp_exp2((i - 64) / 64.0f);
    tom_env3_c = decay_coef(MS(0.19f));
    tom_env2_c = decay_coef(MS(10.0f));
    tom_lag2_c = decay_coef(MS(1.5f));
    tom_bias = dsp_tanh(0.76f * 0.0025f);
    for (int part = 0; part < D8_NUM; ++part) {
        t->v[part].rng = 0x9e3779b9u * (uint32_t)(part + 1);
        for (int i = 0; i < 8; ++i) t->v[part].memo[i].x = -1e30f;   /* nothing remembered yet */
    }
    t->noise = 0x1234567u;
    for (int i = 0; i < 6; ++i) { t->metal_phase[i] = 0.137f * (float)i; t->metal_mod[i] = 1.0f; }
    t->metal_rng = 0x2545f491u;
    svf_set(&t->metal_bp1, 3450.0f, 6.0f);
    svf_set(&t->metal_bp2, 7100.0f, 6.0f);
}

/* ---- building blocks ------------------------------------------------------- */

/* Zero-delay-feedback state-variable filter (TPT). */
static inline EL_DSP_HELPER void svf_set(tr_svf_t *f, float hz, float q)
{
    f->g = dsp_tan(DSP_PI * dsp_clamp(hz, 10.0f, 19000.0f) / EL_SAMPLE_RATE);
    f->k = 1.0f / dsp_max(q, 0.3f);
    f->a1 = 1.0f / (1.0f + f->g * (f->g + f->k));
    f->a2 = f->g * f->a1;
    f->a3 = f->g * f->a2;
}

/* Returns the band-pass output normalised to unity gain at the centre;
 * *low gets the low-pass output when needed. */
static inline EL_DSP_HELPER float svf_bp(tr_svf_t *f, float x, float *low)
{
    float v3 = x - f->ic2, v1 = f->a1 * f->ic1 + f->a2 * v3, v2 = f->ic2 + f->a2 * f->ic1 + f->a3 * v3;
    f->ic1 = 2.0f * v1 - f->ic1;
    f->ic2 = 2.0f * v2 - f->ic2;
    if (low) *low = v2;
    return f->k * v1;
}

static inline EL_DSP_HELPER float svf_hp(tr_svf_t *f, float x)
{
    float v3 = x - f->ic2, v1 = f->a1 * f->ic1 + f->a2 * v3, v2 = f->ic2 + f->a2 * f->ic1 + f->a3 * v3;
    f->ic1 = 2.0f * v1 - f->ic1;
    f->ic2 = 2.0f * v2 - f->ic2;
    return x - f->k * v1 - v2;
}

static inline EL_DSP_HELPER float noise(uint32_t *s)
{
    uint32_t n = *s;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    *s = n;
    return (float)(int32_t)n * (1.0f / 2147483648.0f);
}

static inline EL_DSP_HELPER float powf_approx(float a, float b) { return a <= 0 ? 0 : dsp_exp2(b * dsp_log2(a)); }
static inline EL_DSP_HELPER float decay_coef(float samples) { return dsp_exp(-1.0f / dsp_max(samples, 1.0f)); }
static inline EL_DSP_HELPER float knob(const tr808_t *t, int part, int slot) { return t->pot[part][slot] / 127.0f; }
/* Knob extension around the stock value: 64 = stock, 0 / 127 = /2 / x2
 * (spread_tab: dsp_exp2((pot - 64) / 64), filled by tr808_init). */
static inline EL_DSP_HELPER float spread(const tr808_t *t, int part, int slot) { return spread_tab[t->pot[part][slot] & 127u]; }
static inline EL_DSP_HELPER float m_decay(tr_memo_t *m, float samples)
{
    if (samples != m->x) { m->x = samples; m->y = decay_coef(samples); }
    return m->y;
}
static inline EL_DSP_HELPER float m_exp2(tr_memo_t *m, float x)
{
    if (x != m->x) { m->x = x; m->y = dsp_exp2(x); }
    return m->y;
}
static inline EL_DSP_HELPER float m_exp(tr_memo_t *m, float x)
{
    if (x != m->x) { m->x = x; m->y = dsp_exp(x); }
    return m->y;
}
/* Fixed coefficients (tom_env3_c, tom_env2_c, tom_lag2_c, tom_bias) are
 * set by tr808_init. */

/* The 1 ms trigger pulse after the 808's differentiating pulse shaper
 * (high-pass, tau 0.1 ms, negative excursion clamped by a diode). */
static inline EL_DSP_HELPER float trigger_pulse(tr_voice_t *v)
{
    float rect = v->age < (uint32_t)MS(1.0f) ? v->volts : 0.0f;
    v->pulse_lp += (rect - v->pulse_lp) * 0.2035f;
    float hp = rect - v->pulse_lp;
    return hp < -0.05f ? -0.05f : hp;
}

/* Transistor "swing" VCA: mild asymmetric gain. */
static inline EL_DSP_HELPER float swing(float x) { return x > 0 ? x : x * (0.75f - 0.15f * x); }

/* ---- PO-12 engine -------------------------------------------------------------- */

/* Parts that the PO-12 kit plays (PU stays the 808 trigger) and their
 * sounds; the Sound switch picks the second one. */
static inline EL_DSP_HELPER int po_engine(const tr808_t *t, int part) { return t->kit[3] && part != D8_PU; }

static inline EL_DSP_HELPER unsigned po_sound(const tr808_t *t, int part)
{
    static const uint8_t first[D8_NUM] = {PO_BD, PO_SD, PO_LOTOM, PO_HITOM, PO_STICKS, PO_CLAP, PO_COWBELL, PO_CY, PO_OH,
                                         PO_CH, PO_BLIP, 0};
    static const uint8_t second[D8_NUM] = {PO_BD, PO_SYNSD, PO_BASSTONE, PO_TONE, PO_CLICK, PO_NOISE, PO_COWBELL, PO_CY,
                                          PO_OH, PO_CH, PO_BLIP, 0};
    int alt = (part == D8_SD || part == D8_LT || part == D8_HT || part == D8_RS || part == D8_CP) && t->pot[part][P_SOUND];
    return alt ? second[part] : first[part];
}

/* ---- trigger ------------------------------------------------------------------- */

EL_DSP_CODE void tr808_trigger(tr808_t *t, int part, float velocity)
{
    if ((unsigned)part >= D8_NUM || velocity <= 0) return;
    if (po_engine(t, part)) {
        po_trigger(&t->po[part], po_sound(t, part), velocity, knob(t, part, P_TUNE), knob(t, part, P_DECAY));
        if (part == D8_CH && t->kit[2] && t->po[D8_OH].active) t->po[D8_OH].choke = 1;
        if (part == D8_OH && t->kit[2] == 2 && t->po[D8_CH].active) t->po[D8_CH].choke = 1;
        return;
    }
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
    if (part == D8_OH && t->kit[2] == 2 && t->v[D8_CH].active) t->v[D8_CH].aux2 = 1.0f;
    /* Resonator and filter states are kept: no "machine gun" restart. */
}

EL_DSP_CODE void tr808_silence(tr808_t *t)
{
    for (int i = 0; i < D8_NUM; ++i) t->v[i].active = t->po[i].active = 0;
}

EL_DSP_CODE int tr808_active(const tr808_t *t)
{
    for (int i = 0; i < D8_NUM; ++i) if (t->v[i].active || t->po[i].active) return 1;
    return 0;
}

/* ---- voices ----------------------------------------------------------------------- */

/* Bass drum: shaped pulse into one resonator, attack switch to ~130 Hz for
 * 5 ms, retrigger kick, amplitude-dependent pitch sigh, tone low-pass with
 * the pulse leaking through (click), slight asymmetric non-linearity. */
/* Bass drum calibration, fitted with host/hit_render.c and
 * host/calibration/fit_bd.py against recordings of a real TR-808 bass drum
 * at five DECAY settings: pitch glides from ~52 to 44 Hz within ~100 ms,
 * 2nd / 3rd harmonics at about -33..-43 / -35..-50 dB, decays within
 * ~2 dB rms of the envelopes. */
float tr808_bd_cal[BD_CAL] = {
    /* f0     sigh   sigh_acc att_hz att_q att_ms gain  retrig h2       h3       click q0    q_span sigh_ms drive */
    43.91f, 0.3451f, 0.03772f, 216.1f, 12.91f, 5.097f, 1.541f, 0.4058f, 0.09819f, -0.321f, 0.002613f, 2.91f, 5.743f, 37.45f, 4.777f};

/* Tail of the bass drum (A/B tested by ear):
 * BODY: a quiet octave that stays through the decay (rectified, 2 f0
 *   band, about -20 dB), so the tail is heard on small speakers too.
 *   0 = the pure fitted sine tail.
 * SINK: pitch that keeps sinking with the tail (0.06 = about 5 % over the
 *   decay, as in the FM1 Open SDK's kick); 0 = only the fitted sigh. */
#ifndef TR808_BD_BODY
#define TR808_BD_BODY 0.2f
#endif
#ifndef TR808_BD_SINK
#define TR808_BD_SINK 0.0f
#endif

static EL_DSP_VOICE float bass_drum(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_BD;
    const float *k = tr808_bd_cal;
    float f0 = k[0] * (t->key_on ? t->key_r[part] : tolerance[part] * spread(t, part, P_TUNE));
    float kd = knob(t, part, P_DECAY);
    if (kd != v->memo[0].x) { v->memo[0].x = kd; v->memo[0].y = k[11] * dsp_exp2(k[12] * powf_approx(kd, 0.8f)); }
    float q = v->memo[0].y;
    float pulse = trigger_pulse(v);
    /* PUNCH (0 = as calibrated): the attack phase higher (up to x2) and
     * longer (up to x2), a harder, clickier start as in the SDK's Attack. */
    float punch = knob(t, part, P_SOUND);
    uint32_t attack = (uint32_t)MS(k[5] * (1.0f + punch));
    if (v->age == attack) v->aux = -k[7] * 0.2f * v->volts;  /* retrigger pulse */
    v->aux *= 0.99955f;                                      /* 50 ms */
    if ((v->age & 7u) == 0) {
        if (v->age < attack) svf_set(&v->r1, k[3] * tolerance[part] * (1.0f + punch), k[4]);
        else {
            /* Sigh: the pitch starts high and glides down to f0 with its own
             * time constant (measured: ~52 -> 44 Hz within ~100 ms). */
            float sigh = (k[1] + k[2] * v->accent) * v->pitch + TR808_BD_SINK * v->env;
            svf_set(&v->r1, f0 * (1.0f + sigh), q);
        }
    }
    v->env *= m_decay(&v->memo[1], q / (DSP_PI * f0) * EL_SAMPLE_RATE);
    v->pitch *= m_decay(&v->memo[2], MS(k[13]));
    /* Raw resonator voltage (not normalised by Q): switching Q at the end of
     * the attack keeps the waveform continuous, as in the circuit. */
    float y = svf_bp(&v->r1, (pulse + v->aux) * k[6], 0) / v->r1.k;
    if (TR808_BD_BODY > 0) {
        if ((v->age & 7u) == 0) svf_set(&v->r2, 2.0f * f0, 0.85f);
        y += TR808_BD_BODY * svf_bp(&v->r2, dsp_abs(y), 0);
    }
    /* Feedback buffer non-linearity: 2nd and 3rd harmonics. */
    float x = y * k[14];
    y = (x + k[8] * x * x + k[9] * x * x * x) / k[14];
    float drive = knob(t, part, P_SNAPPY);
    if (drive > 0) { float d = 1.0f + 6.0f * drive; y = dsp_tanh(y * d) / dsp_sqrt(d); }
    /* Tone: 1-pole low-pass 305 Hz .. 7.2 kHz on the click path. */
    float tone = knob(t, part, P_TONE);
    float cutoff = 305.0f * m_exp2(&v->memo[3], 4.56f * tone);
    float click = pulse * (k[10] + 0.18f * tone);
    v->out_lp += (y + click - v->out_lp) * dsp_min(1.0f, 2.0f * DSP_PI * cutoff / EL_SAMPLE_RATE);
    /* The resonator itself is far below the tone corner; pass it straight
     * and only filter the click edge. */
    float out = y + (v->out_lp - y);
    /* Output DC block (about 3.4 Hz). */
    float o = out - v->dc_x + 0.99952f * v->dc_y;
    v->dc_x = out; v->dc_y = o;
    /* End of the sound: fade out over a few ms instead of stopping, so the
     * last ripple and the DC blocker's slow settling never end in a step. */
    if (v->age > (uint32_t)MS(30.0f) && v->env * v->volts < 1e-4f) {
        v->env3 *= 0.9925f;                  /* tau 3 ms */
        if (v->env3 < 1e-3f) v->active = 0;
    }
    return o * v->env3;
}

/* Snare: two resonators (173 / 336 Hz on later units) cross-faded by TONE,
 * plus noise set by SNAPPY. For weight and snap (where the circuit is
 * driven hard by the 808's trigger and output stages):
 *  - the body starts slightly sharp and settles within ~10 ms (knock),
 *  - the transistor summing stage saturates the body a little,
 *  - the noise covers 1.2 .. 9 kHz with a fast 30 ms decay plus a quieter
 *    90 ms tail (the measured envelope is longer than one RC alone),
 *  - the trigger edge leaks through as a short click. */
/* Snare calibration, fitted with host/snare_check.c + host/analyze_snare.py
 * against recordings of a real TR-808 at 16 TONE / SNAPPY settings: mode
 * levels within 0.8 dB rms, noise level within 0.4 dB, decays within ~6 %,
 * noise spectrum within 2.4 dB rms (1-12.7 kHz). */
float tr808_sd_cal[SD_CAL] = {
    /* low  high  q_low  q_high  noise  n_tau1 n_tau2 n_mix  n_hp    n_lp     snap */
    0.8591f, 0.1114f, 18.9f, 10.27f, 0.4709f, 28.89f, 39.58f, 0.1607f, 2098.7f, 7739.4f, 0.2575f};
/* Body lengthened by ear against the recording (808 Basics Kit A, snares
 * 13 / 14; docs/adr/0015): both Q x 1.2 (body falls 2.46 dB per 10 ms
 * instead of 2.93; the originals 2.78 / 2.37), body x 0.9 so noise to body
 * stays as there (-1.6 dB; originals -1.7 / -1.4). Fitted values before:
 * low 0.9546, high 0.1238, q_low 15.75, q_high 8.558. */

/* Snare (Service Notes, Werner): two bridged-T resonators struck by the
 * trigger pulse, 172 and 339 Hz (as measured on the recordings; the 1981
 * board had 238 / 476), the lower one ringing ~3x longer; TONE crossfades
 * them (the low one falls with the pot, the high one rises ~11 dB over
 * its travel). White noise through a high-pass and a low-pass, its own
 * two-slope envelope, mixed in by SNAPPY, which acts from ~1/4 of its
 * travel (nothing below, -9 dB at the middle, full at the end). */
/* Snare model 1 (A/B switch TR808_SD_MODEL, docs/adr/0015): the 8W8
 * engine's snare circuit (as ported in X0X-RMX), own implementation. The
 * two bridged-T resonators (173 / 336 Hz, Q 17.4 / 10.7, scaled by DECAY)
 * are struck so hard that the op-amp behind them clips (rail 40): a
 * punchy, compressed knock. TONE crossfades them at equal power. The noise
 * is half-wave rectified (the VCA transistor), its envelope one 75 ms
 * slope from SNAPPY x the trigger height, high-passed at 1.76 kHz and
 * open at the top. Output high-pass 30 Hz. */
#ifndef TR808_SD_MODEL
#define TR808_SD_MODEL 0
#endif
#ifndef TR808_SD1_GAIN
#define TR808_SD1_GAIN 0.01965f   /* as loud as model 0 over 0.1 s */
#endif
static EL_DSP_HELPER float snare_circuit(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_SD;
    float tune = t->key_on ? t->key_r[part] : tolerance[part] * spread(t, part, P_TUNE);
    float dq = dsp_clamp(1.3275f * spread(t, part, P_DECAY), 0.35f, 1.5f);
    float volts = 4.0f + 3.3f * v->accent;               /* 8W8's trigger height */
    if (v->age == 0) {
        svf_set(&v->r1, 173.3f * tune, 17.36f * dq);
        svf_set(&v->r2, 336.0f * tune, 10.66f * dq);
        svf_set(&v->r3, 1760.0f, 0.7071f);
        float tone = knob(t, part, P_TONE) * 0.5f * DSP_PI;
        v->lag1 = dsp_cos(tone);                          /* low resonator */
        v->lag2 = dsp_sin(tone);                          /* high resonator */
        v->env = knob(t, part, P_SNAPPY) * volts * 0.25f; /* noise */
    }
    float vp = trigger_pulse(v) / dsp_max(v->volts, 1e-3f) * volts;
    float s1 = svf_bp(&v->r1, vp * 603.9f, 0), s2 = svf_bp(&v->r2, vp * 228.3f, 0);
    float shells = 40.0f * dsp_tanh((s1 * v->lag1 + s2 * v->lag2) * (1.0f / 40.0f));
    v->env *= m_decay(&v->memo[0], 0.075f * EL_SAMPLE_RATE * dq / 1.3275f);
    float noise = svf_hp(&v->r3, dsp_max(t->noise_now, 0.0f) * v->env);
    float out = shells * 0.1491f + noise * 1.4908f;
    float o = out - v->dc_x + 0.99573f * v->dc_y;        /* high-pass 30 Hz */
    v->dc_x = out; v->dc_y = o;
    if (v->age > (uint32_t)MS(500.0f) && v->env < 1e-4f) v->active = 0;
    return o * TR808_SD1_GAIN * dsp_min(1.0f, v->volts * 14.0f / 4.0f);   /* soft MIDI hits quieter */
}

static EL_DSP_VOICE float snare(tr808_t *t, tr_voice_t *v)
{
    if (TR808_SD_MODEL) return snare_circuit(t, v);
    const int part = D8_SD;
    const float *k = tr808_sd_cal;
    float pulse = trigger_pulse(v);
    float tune = t->key_on ? t->key_r[part] : tolerance[part] * spread(t, part, P_TUNE), decay = spread(t, part, P_DECAY);
    if (v->age == 0) {
        svf_set(&v->r3, k[8], 0.7f);   /* noise high-pass */
        v->aux = 0;
    }
    /* Knock: up to +8 % at full accent, settling with tau 6 ms. */
    v->pitch *= 0.99623f;
    if ((v->age & 7u) == 0) {
        float knock = 1.0f + (0.04f + 0.04f * v->accent) * v->pitch;
        float early = t->pot[part][P_SOUND] ? 1.384f : 1.0f;   /* 238 / 476 Hz */
        svf_set(&v->r1, 172.0f * early * tune * knock, k[2] * decay);
        svf_set(&v->r2, 339.0f * early * 1.016f * tune * knock, k[3] * decay);
    }
    float tone = knob(t, part, P_TONE);
    /* Raw resonator voltages, as the circuit sums them. */
    float body = (1.0f - tone) * k[0] * svf_bp(&v->r1, pulse * 3.0f, 0) / v->r1.k +
                 k[1] * m_exp2(&v->memo[0], 1.79f * tone) * svf_bp(&v->r2, pulse * 3.0f, 0) / v->r2.k;
    body = dsp_tanh(body * 0.9f) / 0.9f;
    v->env *= m_decay(&v->memo[1], MS(k[5]) * decay);
    v->env2 *= m_decay(&v->memo[2], MS(k[6]) * decay);
    float attack = dsp_min(1.0f, (float)v->age / MS(0.5f));
    float n = svf_hp(&v->r3, t->noise_now);
    float lp = 1.0f - m_exp(&v->memo[3], -2.0f * DSP_PI * k[9] / EL_SAMPLE_RATE);
    v->aux += (n - v->aux) * lp;
    v->out_lp += (v->aux - v->out_lp) * lp;
    float noise_env = ((1.0f - k[7]) * v->env + k[7] * v->env2) * attack * v->volts;
    float snappy = dsp_clamp((knob(t, part, P_SNAPPY) - 0.26f) * (1.0f / 0.74f), 0.0f, 1.0f);
    float snap = (pulse > 0 ? pulse : 0) * k[10];
    float out = body * 0.55f + (v->out_lp * noise_env * k[4] + snap) * snappy;
    if (v->age > (uint32_t)MS(500.0f) && v->env2 < 1e-4f) v->active = 0;
    return out;
}

/* Toms / congas, rebuilt after tachyonbeats' "808 part 06: low tom"
 * breakdown and fitted to its audio (residual -31 dB): a sine that starts
 * on -cos with a 0.19 ms edge, its pitch sagging from f0 * (1 + bend) to
 * f0 with a 43 ms time constant (LT: 119 -> 93 Hz), an exponential fade
 * (75 ms), a little band-passed noise (1.4 %) and a click, all through a
 * gentle tanh drive (0.76). Accent pushes the body harder into the drive
 * and bends a little further. HT and the congas scale the times with the
 * pitch; congas bend less, decay faster and have no noise. */
static const float tom_hz[4] EL_DSP_DATA = {93.0f, 185.0f, 185.0f, 400.0f};   /* LT, HT, LC, HC */
static EL_DSP_VOICE float tom(tr808_t *t, tr_voice_t *v, int part)
{
    int conga = t->pot[part][P_SOUND] == 1, idx = (part - D8_LT) + (conga ? 2 : 0);
    float f0 = tom_hz[idx] * (t->key_on ? t->key_r[part] : m_exp2(&v->memo[0], 0.75f * (t->pot[part][P_TUNE] - 64) / 64.0f));   /* fitted: no tolerance */
    if (f0 != v->memo[1].x) { v->memo[1].x = f0; v->memo[1].y = dsp_sqrt(93.0f / f0); }
    float scale = v->memo[1].y;   /* times as fitted for LT, shorter for higher drums */
    if (v->age == 0) {
        v->lag2 = v->lag1;          /* what still rings fades out instead of clicking */
        v->aux = 0;                 /* phase: the hit starts on -cos */
        svf_set(&v->r1, 500.0f * f0 / tom_hz[idx], 1.0f);
    }
    float bend = 0.277f * knob(t, part, P_SNAPPY) * 2.0f * (conga ? 0.35f : 1.0f) * (1.0f + 0.4f * v->accent);
    v->pitch *= m_decay(&v->memo[2], MS(42.9f * scale));
    float hz = f0 * (1.0f + bend * v->pitch);
    v->aux += hz * (1.0f / EL_SAMPLE_RATE);
    if (v->aux >= 1.0f) v->aux -= 1.0f;
    float body = -dsp_cos(2.0f * DSP_PI * v->aux);
    v->env *= m_decay(&v->memo[4], MS(74.9f * scale * (conga ? 0.65f : 1.0f) * m_exp2(&v->memo[3], (t->pot[part][P_DECAY] - 64) / 48.0f)));
    v->env3 *= tom_env3_c;
    body *= v->env * (1.0f - v->env3);
    if (!conga) {
        /* Noise: band-passed around 500 Hz (the fit's residual), 10 ms. */
        float attack = dsp_min(1.0f, (float)v->age / MS(0.5f));
        v->env2 *= tom_env2_c;
        float amount = 0.014f * m_exp2(&v->memo[5], (t->pot[part][P_TONE] - 64) / 16.0f);
        body += svf_bp(&v->r1, t->noise_now, 0) * 2.0f * amount * v->env2 * attack;
        /* Click: a 0.3 ms spike, high-passed at 1 kHz. */
        float c = dsp_exp(-(float)v->age / MS(0.3f));
        v->out_lp += (c - v->out_lp) * 0.133f;
        body += 0.03f * (c - v->out_lp);
    }
    float soft = v->volts * 14.0f / (4.0f + 10.0f * v->accent);   /* MIDI hits below normal */
    float in = body * (1.0f + 1.2f * v->accent) * soft;
    float y = (dsp_tanh(0.76f * (in + 0.0025f)) - tom_bias) * (1.0f + 0.5f * v->accent);
    v->lag2 *= tom_lag2_c;
    y += v->lag2;
    v->lag1 = y;
    if (v->age > (uint32_t)MS(30.0f) && v->env < 1e-4f && dsp_abs(v->lag2) < 1e-5f) { v->lag1 = 0; v->active = 0; }
    return y;
}

/* Rim shot: 455 + 1667 Hz resonators into an asymmetric swing VCA (the
 * bright ring), 22 ms gate. Claves: one 2.5 kHz resonator ringing, 25 ms. */
static EL_DSP_VOICE float rim(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_RS;
    int clave = t->pot[part][P_SOUND] == 1;
    float tune = t->key_on ? t->key_r[part] : tolerance[part] * spread(t, part, P_TUNE), pulse = trigger_pulse(v);
    if (v->age == 0) {
        svf_set(&v->r1, 455.0f * tune, 6.7f * tr808_mx_cal[7]);
        svf_set(&v->r2, 1667.0f * tune, 13.5f * tr808_mx_cal[7]);
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

/* Hand clap after the circuit (A/B switch TR808_CP_MODEL, see
 * docs/adr/0008): 0 = as fitted (below); 1 = the circuit's envelope with
 * our filter and noise; 2 = also the circuit's filter and noise. Idea:
 * the 8W8 engine as ported in X0X-RMX; own implementation.
 * Envelope: three pulses 10 ms apart that jump to 0.8 and fall with
 * 4.5 ms (the sawtooth generator's capacitor), then at 28 ms the main
 * clap, opening over 3 ms and falling with 38.5 ms, under it a quiet
 * floor (-30 dB, 330 ms), the main and floor times scaled by DECAY.
 * Model 2: half-wave rectified noise (the VCA transistor conducts one
 * polarity), band-pass 874 Hz Q 1.29, high-pass 720 Hz. */
#ifndef TR808_CP_MODEL
#define TR808_CP_MODEL 3   /* chosen by ear (fitted model D), docs/adr/0008 */
#endif
#ifndef TR808_CP_GAIN1
#define TR808_CP_GAIN1 1.33f
#endif
#ifndef TR808_CP_GAIN2
#define TR808_CP_GAIN2 1.30f
#endif
/* Model 3: fitted to the recordings (808 Basics Kit A, hits 10 and 12,
 * host/calibration/fit_clap.py). The real clap's tail is NOT a loud
 * bright "reverb": after the main clap it is 43 dB down at 160 ms, then a
 * very quiet, dark floor rings on for over a second (-50 dB at 350 ms,
 * -60 dB at 900 ms, centre ~1 kHz, the highs 20-30 dB down). The highs of
 * the loud part come from the VCA driven hard; the quiet floor stays clean.
 * Paths: band-passed noise x (three teeth + main clap) -> VCA distortion,
 * plus the same band-passed noise x floor -> 1-pole low-pass. Fitted:
 * band-pass 1146 Hz Q 1.5; teeth 11.4 ms apart, falling with 5.5 ms; main
 * clap at 33.6 ms, falling with 32 ms; floor falling with 0.66 s through
 * a 1.2 kHz low-pass. Error against the recordings (rms dB, fine
 * envelope / tail / spectrum): 2.5-3.1 / 1.4-1.6 / 2.3-2.7, model 0:
 * 8.8 / 7-10 / 4-4.4. */
float tr808_cp_cal[CP_CAL] = {
    /* bp_hz  bp_q   tooth  tooth_ms gap_ms main_ms rise_ms main_tau floor  floor_tau floor_lp gain  drive */
    1146.0f, 1.499f, 0.7455f, 5.498f, 11.36f, 33.58f, 0.2133f, 31.8f, 0.0231f, 657.6f, 1186.0f, 1.5f, 1.557f};

static EL_DSP_HELPER float clap_fitted(tr808_t *t, tr_voice_t *v, float n)
{
    const int part = D8_CP;
    const float *k = tr808_cp_cal;
    float decay = spread(t, part, P_DECAY);
    if (v->age == 0) {
        svf_set(&v->r1, k[0] * spread(t, part, P_TONE) * tolerance[part], k[1]);
        v->lag1 = v->lag2 = v->out_lp = 0;   /* main, floor, floor's low-pass */
        v->pitch = 0;
    }
    uint32_t gap = (uint32_t)MS(k[4]), on = (uint32_t)MS(k[5]);
    if (v->age == 0 || v->age == gap || v->age == 2u * gap) v->env = k[2];
    if (v->age == on) { v->pitch = 1; v->lag2 = 1.0f; }
    float bp = svf_bp(&v->r1, n, 0);
    float loud = bp * (v->env + v->lag1) * v->volts * k[11];
    v->env *= m_decay(&v->memo[0], MS(k[3]));
    if (v->pitch > 0) {                      /* the main clap opens (linear) */
        v->lag1 = dsp_min(1.0f, v->lag1 + 1.0f / dsp_max(1.0f, MS(k[6])));
        if (v->lag1 >= 1.0f) v->pitch = 0;
    } else v->lag1 *= m_decay(&v->memo[2], MS(k[7]) * decay);
    v->lag2 *= m_decay(&v->memo[3], MS(k[9]) * decay);
    float lpc = 1.0f - m_exp(&v->memo[4], -2.0f * DSP_PI * k[10] / EL_SAMPLE_RATE);
    v->out_lp += (bp * v->lag2 * k[8] * v->volts * k[11] - v->out_lp) * lpc;
    if (v->age > on && v->pitch == 0 && v->lag1 < 1e-4f && v->lag2 * k[8] < 3e-5f) v->active = 0;
    return dsp_tanh(loud * k[12]) / (k[12] * (1.2f / 1.4f)) + v->out_lp;
}

static EL_DSP_HELPER float clap_circuit(tr808_t *t, tr_voice_t *v, float n)
{
    const int part = D8_CP;
    const int circuit = TR808_CP_MODEL == 2;
    float decay = spread(t, part, P_DECAY);
    if (v->age == 0) {
        float tone = spread(t, part, P_TONE) * tolerance[part];
        if (circuit) { svf_set(&v->r1, 874.0f * tone, 1.29f); svf_set(&v->r2, 720.0f, 0.7071f); }
        else svf_set(&v->r1, 1140.0f * tone, 1.95f);
        v->lag1 = v->lag2 = 0;   /* main, floor */
        v->pitch = 0;            /* 1 while the main clap opens */
    }
    if (v->age == 0 || v->age == (uint32_t)MS(10.0f) || v->age == (uint32_t)MS(20.0f)) v->env = 0.8f;
    if (v->age == (uint32_t)MS(28.0f)) { v->pitch = 1; v->lag2 = 0.03f; }
    float ctrl = v->env + v->lag1 + v->lag2;
    v->env *= 0.99497f;                                            /* 4.5 ms */
    if (v->pitch > 0) {
        v->lag1 += (1.0f - v->lag1) * 0.00753f;                    /* 3 ms */
        if (v->lag1 > 0.98f) v->pitch = 0;
    } else v->lag1 *= m_decay(&v->memo[0], MS(38.5f) * decay);
    v->lag2 *= m_decay(&v->memo[1], MS(330.0f) * decay);
    float src = circuit ? 2.0f * dsp_max(n, 0.0f) : n;
    float y = svf_bp(&v->r1, src * ctrl, 0);
    if (circuit) y = svf_hp(&v->r2, y);
    if (v->age > (uint32_t)MS(30.0f) && v->pitch == 0 && v->lag1 < 1e-4f && v->lag2 < 1e-5f) v->active = 0;
    y *= v->volts * (circuit ? TR808_CP_GAIN2 : TR808_CP_GAIN1);
    return dsp_tanh(y * 1.4f) / 1.2f;   /* the BA662 VCA's distortion */
}

/* Hand clap: band-passed noise (~1.1 kHz, Q 2) through a sawtooth envelope
 * of three 10 ms pulses plus a 20 ms fall, and a quieter ~100 ms "reverb".
 * Maracas: high-passed noise with a 13 ms rise and 17 ms fall. */
static EL_DSP_VOICE float clap(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_CP;
    float n = t->noise_now;
    if (t->pot[part][P_SOUND] == 1) {
        if (v->age == 0) svf_set(&v->r1, 6000.0f * spread(t, part, P_TUNE), 0.8f);
        float ms = (float)v->age / MS(1.0f), rise = 13.0f, fall = 17.0f * spread(t, part, P_DECAY);
        float e = ms < rise ? ms / rise : dsp_max(0.0f, 1.0f - (ms - rise) / fall);
        if (ms > rise + fall + 5.0f) v->active = 0;
        return svf_hp(&v->r1, n) * e * e * v->volts * 1.1f;
    }
    if (TR808_CP_MODEL == 3) return clap_fitted(t, v, n);
    if (TR808_CP_MODEL) return clap_circuit(t, v, n);
    if (v->age == 0) svf_set(&v->r1, 1140.0f * spread(t, part, P_TONE) * tolerance[part], 1.95f);
    float bp = svf_bp(&v->r1, n, 0);
    float ms = (float)v->age / MS(1.0f), saw;
    if (ms < 30.0f) { float in = ms - 10.0f * (float)(int)(ms / 10.0f); saw = 1.0f - in / 10.0f; }
    else saw = dsp_exp(-(ms - 30.0f) / 8.7f);
    v->env2 *= m_decay(&v->memo[0], MS(tr808_mx_cal[5]) * spread(t, part, P_DECAY));
    float e = saw + tr808_mx_cal[6] * v->env2;
    if (ms > 30.0f && v->env2 < 1e-4f) v->active = 0;
    float y = bp * e * v->volts * 1.5f;
    return dsp_tanh(y * 1.4f) / 1.2f;   /* the BA662 VCA's distortion */
}

/* Shared metal source: six HD14584 Schmitt-trigger oscillators, 0/5 V
 * rectangles with 47.98 % duty (Werner), summed through equal resistors
 * into a 7-level staircase. Band-limited with PolyBLEP: the edges are what
 * strike the band-passes, aliased edges would add a grainy noise floor. */
#define METAL_DUTY 0.4798f
static inline EL_DSP_HELPER float blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

static inline EL_DSP_HELPER float rect(float phase, float dt)
{
    float y = phase < METAL_DUTY ? 1.0f : 0.0f;
    float down = phase - METAL_DUTY;
    if (down < 0) down += 1.0f;
    return y + 0.5f * blep(phase, dt) - 0.5f * blep(down, dt);
}

/* Metal oscillators that wander (switch, 0 = perfectly steady): the
 * HD14584's threshold and the RC timing vary a little from cycle to cycle,
 * and slowly with temperature. Each oscillator gets a new random error of
 * up to +-0.5 % at every cycle and a slow random-walk drift of about
 * +-0.1 %. The beating pattern of the six then never repeats exactly, the
 * hats and the cymbal shimmer instead of sounding static. (Idea: the 8W8
 * engine as ported in X0X-RMX; own implementation.) */
#ifndef TR808_METAL_JITTER
#define TR808_METAL_JITTER 1   /* chosen by ear (metal-ab D), docs/adr/0009 */
#endif
static inline EL_DSP_HELPER float metal_rand(tr808_t *t)
{
    uint32_t n = t->metal_rng;
    n ^= n << 13; n ^= n >> 17; n ^= n << 5;
    t->metal_rng = n;
    return (float)(int32_t)n * (1.0f / 2147483648.0f);
}
static EL_DSP_HELPER void metal_wander(tr808_t *t, int frames)
{
    if (!TR808_METAL_JITTER) return;
    t->metal_count -= frames;
    while (t->metal_count <= 0) {               /* drift: every 256 samples */
        t->metal_count += 256;
        for (int i = 0; i < 6; ++i) t->metal_drift[i] = 0.98f * (t->metal_drift[i] + 2.5e-4f * metal_rand(t));
    }
    for (int i = 0; i < 6; ++i) t->metal_mod[i] = 1.0f + t->metal_drift[i] + t->metal_jit[i];
}

static EL_DSP_VOICE float metal(tr808_t *t, float *phase, float tune, float *cowbell)
{
    float sum = 0, r[6];
    for (int i = 0; i < 6; ++i) {
        float dt = metal_hz[i] * metal_tol[i] * tune * t->metal_mod[i] * (1.0f / EL_SAMPLE_RATE);
        phase[i] += dt;
        if (phase[i] >= 1.0f) {
            phase[i] -= 1.0f;
            if (TR808_METAL_JITTER) {           /* this cycle's own small error */
                t->metal_jit[i] = 0.0052f * metal_rand(t);
                t->metal_mod[i] = 1.0f + t->metal_drift[i] + t->metal_jit[i];
            }
        }
        r[i] = rect(phase[i], dt);
        sum += r[i];
    }
    if (cowbell) *cowbell = 2.0f * (r[4] + r[5]) - 2.0f;
    return sum * (1.0f / 3.0f) - 1.0f;
}

/* The cowbell's 540 and 800 Hz rectangles alone (its TUNE off centre). */
static EL_DSP_VOICE float cowbell_osc(float *phase, float tune_lo, float tune_hi)
{
    float sum = 0;
    for (int i = 0; i < 2; ++i) {
        float dt = metal_hz[4 + i] * (i ? tune_hi : tune_lo) * (1.0f / EL_SAMPLE_RATE);
        phase[i] += dt;
        if (phase[i] >= 1.0f) phase[i] -= 1.0f;
        sum += rect(phase[i], dt);
    }
    return 2.0f * sum - 2.0f;
}

/* Calibration of cowbell, clap tail, rim shot ring and the hats, fitted
 * against single-hit recordings of a real TR-808 (envelope and 1/3-octave
 * spectrum): cowbell, clap tail 98 ms, rim ring +19 %, closed hat 27 ms
 * to -20 dB, open hat a touch brighter. */
float tr808_mx_cal[MX_CAL] = {
    /* cb_hz  cb_q  cb_t1 cb_t2  cb_mix cp_tail cp_gain rs_q  ch_ms  oh_hf */
    2934.0f, 0.406f, 1.663f, 38.52f, 0.655f, 98.53f, 0.275f, 1.193f, 27.42f, 0.238f};

/* Cowbell (Service Notes, Werner "More Cowbell"): the 540 and 800 Hz
 * rectangles through their own gates into one broad band-pass, so their
 * harmonics up to a few kHz stay in (fitted: ~2.9 kHz, Q 0.4); a fast
 * click (1.7 ms) into a ~39 ms decay, as on the recording. */
/* Cowbell model 1 (A/B switch TR808_CB_MODEL, docs/adr/0012), fitted to
 * the cowbell of the recordings (808 Basics Kit A, hit 20; model 0 was
 * fitted to hit 9, which is not a cowbell). The real one is the two
 * rectangles through a narrow band-pass near the upper tone (the lower
 * one ~16 dB down, little above 2.5 kHz), a clank that falls ~13 dB
 * within 20 ms, then a long body (-40 dB only after ~230 ms). The VCA
 * adds even harmonics (2 x 573 Hz is clearly there). A little of the
 * rectangles' edges passes beside the band-pass (high-passed).
 * Fitted (host/calibration/fit_cowbell.py): band-pass 1033 Hz Q 7.1,
 * clank 89 % falling with 21 ms, body falling with 139 ms. Error against
 * the recording (rms dB; envelope 5 ms / 20 ms, spectrum, partials):
 * 1.35 / 1.4 / 4.3 / 6.7, model 0: 13.6 / 13.7 / 17.2 / 13.0. The rest of
 * the partial error: that unit's tones are exactly 3:2 (573 / 859 Hz), so
 * the VCA's products land on harmonics; 540 / 800 Hz (the 808's nominal
 * values, kept) are 1.48 apart. */
#ifndef TR808_CB_MODEL
#define TR808_CB_MODEL 1   /* fitted to the real cowbell, chosen by ear, docs/adr/0012 */
#endif
float tr808_cb_cal[CB_CAL] = {
    /* bp_hz  bp_q  edge     edge_hz clank   clank_ms body_ms asym     gain */
    1033.0f, 7.1f, 0.05648f, 901.6f, 0.8923f, 21.36f, 139.4f, 0.02784f, 1.64f};   /* gain: as loud as model 0 over 0.1 s */

static EL_DSP_HELPER float cowbell_fitted(tr808_t *t, tr_voice_t *v, float src)
{
    const int part = D8_CB;
    const float *k = tr808_cb_cal;
    if (v->age == 0) {
        float tune = t->key_on ? t->key_r[part] : spread(t, part, P_TUNE);
        svf_set(&v->r1, k[0] * tune, k[1]);
        svf_set(&v->r2, k[3] * tune, 0.7071f);
    }
    float y = svf_bp(&v->r1, src * 0.5f, 0) + k[2] * svf_hp(&v->r2, src * 0.5f);
    float decay = spread(t, part, P_DECAY);
    v->env *= m_decay(&v->memo[0], MS(k[5]) * decay);
    v->env2 *= m_decay(&v->memo[1], MS(k[6]) * decay);
    float attack = dsp_min(1.0f, (float)v->age / MS(0.7f));
    float e = (k[4] * v->env + (1.0f - k[4]) * v->env2) * attack;
    if (v->env2 < 1e-4f) v->active = 0;
    float s = y * e * (0.45f + 0.55f * v->volts * 14.0f / 4.0f / 3.5f);
    s += k[7] * s * s;                       /* the VCA's even harmonics */
    float o = s - v->dc_x + 0.9972f * v->dc_y;   /* DC block ~20 Hz */
    v->dc_x = s; v->dc_y = o;
    return o * k[8];
}

static EL_DSP_VOICE float cowbell(tr808_t *t, tr_voice_t *v, float src)
{
    const int part = D8_CB;
    const float *k = tr808_mx_cal;
    if (TR808_CB_MODEL) return cowbell_fitted(t, v, src);
    if (v->age == 0) svf_set(&v->r1, k[0] * (t->key_on ? t->key_r[part] : spread(t, part, P_TUNE)), k[1]);
    float y = svf_bp(&v->r1, src * 0.5f, 0);
    float decay = spread(t, part, P_DECAY);
    v->env *= m_decay(&v->memo[0], MS(k[2]) * decay);
    v->env2 *= m_decay(&v->memo[1], MS(k[3]) * decay);
    float attack = dsp_min(1.0f, (float)v->age / MS(0.7f));
    float e = (k[4] * v->env + (1.0f - k[4]) * v->env2) * attack;
    if (v->env2 < 1e-4f) v->active = 0;
    return swing(y * e * (0.45f + 0.55f * v->volts * 14.0f / 4.0f / 3.5f)) * 1.4f;
}

/* Swing-type VCA (one transistor, the envelope is its collector supply):
 * it passes mainly the positive half and clips against the envelope, so
 * the metal voices get rougher, not only quieter, as they decay (Werner's
 * "wild swing"). Linear for small signals at a high envelope. */
static inline EL_DSP_HELPER float swing_vca(float x, float env)
{
    float r = x > 0 ? 4.0f * x : 0.1f * x;
    return r * env / (env + (r > 0 ? r : -r) + 1e-6f);
}

/* Cymbal calibration, fitted with host/cymbal_check.c + host/analyze_cymbal.py
 * against band measurements of a real TR-808 at all 25 TONE / DECAY
 * settings (Fischer sample set): high/low band balance within 0.4 dB rms,
 * decay times within ~8 % (early) / ~10 % (late), 1/3 octaves within
 * 3.5 dB rms at TONE 5 / DECAY 2.5. Host builds may change it. */
float tr808_cy_cal[CY_CAL] = {
    /* drive  low   decay short  tau_s  tone_s tone_l tone_d  tau_l0 tau_l1 tau_d0 tau_d1 lag_l lag_d  tone_p */
    0.5476f, 0.5929f, 0.8818f, 1.658f, 47.71f, 1.884f, -0.4735f, 0.7804f, 159.7f, 469.2f, 102.8f, 173.6f, 58.48f, 4.648f, 1.996f,
    /* hh2 Hz  hh3 Hz   output low-pass Hz */
    10267.0f, 9089.0f, 11913.0f};

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
static EL_DSP_VOICE float cymbal(tr808_t *t, tr_voice_t *v, float low_src, float high_src)
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
    float decay = knob(t, part, P_DECAY), kt = knob(t, part, P_TONE);
    if (kt != v->memo[0].x) { v->memo[0].x = kt; v->memo[0].y = powf_approx(kt, k[14]); }
    float tone = v->memo[0].y;
    v->env *= m_decay(&v->memo[1], MS(k[8] + k[9] * decay));       /* low band reservoir */
    v->env2 *= m_decay(&v->memo[2], MS(k[10] + k[11] * decay));     /* DECAY band reservoir */
    v->env3 *= m_decay(&v->memo[3], MS(k[4]));                      /* short band */
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
    float g_low = k[1] * m_exp2(&v->memo[4], -k[6] * (tone - 0.5f)), g_mid = k[2] * m_exp2(&v->memo[5], k[7] * (tone - 0.5f));
    float g_hi = k[3] * m_exp2(&v->memo[6], k[5] * (tone - 0.5f));
    if (v->env < 1e-4f && v->env2 < 1e-4f && v->age > MS(300.0f)) v->active = 0;
    /* Tone network and LEVEL buffer: nearly flat, rolling off at the top. */
    v->out_lp += (g_low * v->lp1 + g_mid * mid + g_hi * hi - v->out_lp) * (1.0f - m_exp(&v->memo[7], -2.0f * DSP_PI * k[17] / EL_SAMPLE_RATE));
    return v->out_lp * (1.0f + 0.6f * v->accent);   /* accent +6 dB, see hat() */
}

/* Open hat with a linear discharge (switch, 0 = exponential as
 * fitted before). Idea: the 8W8 engine as ported in X0X-RMX; own implementation. */
#ifndef TR808_OH_LINEAR
#define TR808_OH_LINEAR 1      /* chosen by ear (metal-ab D), docs/adr/0009 */
#endif

/* Hats model 1 (A/B switch TR808_HAT_MODEL, docs/adr/0014): fitted to
 * the hats of the recordings (808 Basics Kit A, CH hits 17 / 22, OH 8 /
 * 18). Ours sounded too low: their strongest partials sat at 7.0 kHz
 * (the cymbal's 7.1 kHz band-pass), the originals' at 8.0-8.5 kHz, and the
 * originals have far more at 3-6 kHz and (open hat) above 9 kHz. The hats get their own
 * band-pass, a little of the 3.45 kHz band for their body (the originals
 * have 10-20 dB more at 3-6 kHz) and their own high-passes. The cymbal
 * keeps its calibration. Fitted (host/calibration/fit_hats.py): a broad
 * band-pass 6.4 kHz Q 3.2 plus 39 % of the 3.45 kHz band; CH high-pass
 * 10.9 kHz Q 1.24, OH 9.4 kHz Q 0.92. Error against the recordings (rms
 * dB; CH spectrum / CH envelope / OH spectrum / partials 6-10 kHz):
 * 2.6 / 2.6 / 3.0 / 4.4, model 0: 5.4 / 2.8 / 12.3 / 6.6. */
#ifndef TR808_HAT_MODEL
#define TR808_HAT_MODEL 2   /* chosen by ear (D, the X0X / 8W8 structure), docs/adr/0014 */
#endif
float tr808_hh_cal[HH_CAL] = {
    /* bp_hz  bp_q  low_mix ch_hz    ch_q  oh_hz   oh_q */
    6373.0f, 3.169f, 0.3871f, 10910.0f, 1.244f, 9418.0f, 0.9215f};

/* Hats model 2 (TR808_HAT_MODEL 2, docs/adr/0014): the structure of the
 * 8W8 engine's hats (as ported in X0X-RMX), own implementation. No band-
 * pass: the raw sum of the six oscillators is differentiated twice (+12 dB
 * per octave, every partial takes part), goes through the VCA with its
 * diode knee (nothing below ~0.6 V), two Sallen-Key high-passes (CH 5.2
 * and 2.7 kHz, OH 6.2 and 5.2 kHz), a low-pass (CH one pole ~10.5 kHz, OH
 * five poles ~5.9 kHz) and a 200 Hz DC block. Envelopes as ours. */
/* Times as 8W8 at its default knobs: CH falls with 42.5 ms (half its
 * 85 ms decay; the knee ends it), OH rises over ~6 ms, holds 30 % of
 * 0.44 s and falls linearly over the rest. DECAY scales them. Gains: as
 * loud as model 0 (CH over 50 ms, OH over 200 ms). */
float tr808_hh2_cal[HH2_CAL] = {0.000869221f, 0.0625397f};
static inline EL_DSP_HELPER float knee_gate(float e)
{
    float over = e - 0.59f;
    if (over <= 0.0f) return 0.0f;
    return over < 1.2f ? over * over * (1.0f / 2.4f) : over - 0.6f;
}
static EL_DSP_HELPER float hat_chain(tr_voice_t *v, int part, float bus, float env)
{
    float t1 = bus - v->lag1;
    v->lag1 = bus;
    float d = (t1 - v->lag2) * 30.0f;
    v->lag2 = t1;
    float s = svf_hp(&v->r3, svf_hp(&v->r2, d * knee_gate(env)));
    if (part == D8_CH) {
        v->lp1 += (s - v->lp1) * 0.776f;
        s = v->lp1;
    } else {
        v->lp1 += (s - v->lp1) * 0.5686f; v->pink[0] += (v->lp1 - v->pink[0]) * 0.5686f;
        v->pink[1] += (v->pink[0] - v->pink[1]) * 0.5686f; v->pink[2] += (v->pink[1] - v->pink[2]) * 0.5686f;
        v->hp1 += (v->pink[2] - v->hp1) * 0.5686f;
        s = v->hp1;
    }
    s *= part == D8_CH ? 8.0f : 5.0f;
    float o = 0.9719f * (v->dc_y + s - v->dc_x);   /* DC block 200 Hz */
    v->dc_x = s; v->dc_y = o;
    return o;
}

/* Hats (Service Notes p.6/13): the 7.1 kHz band of the six oscillators
 * (no noise), a swing VCA, then a Sallen-Key high-pass, OH 7.8 kHz and
 * CH 11.7 kHz, both Q 2.5. Decay chart: OH 90 / 450 / 600 ms over its
 * DECAY knob, CH 50 ms. Accent +6 dB like the chart. */
static EL_DSP_VOICE float hat(tr808_t *t, tr_voice_t *v, int part, float src)
{
    if (v->age == 0) {
        /* TUNE moves the high-pass with the oscillators, the closed hat's half
         * as far (it would leave the audio band); centred as on the 808 */
        float f = spread(t, part, P_TUNE);
        if (TR808_HAT_MODEL == 2) {
            float g = dsp_sqrt(f);   /* TUNE moves the filters with the oscillators, as model 0 */
            if (part == D8_CH) { svf_set(&v->r2, 5200.0f * g, 1.6f); svf_set(&v->r3, 2700.0f * g, 0.75f); }
            else { svf_set(&v->r2, 6200.0f * g, 0.9f); svf_set(&v->r3, 5200.0f * g, 0.8f); }
        } else if (TR808_HAT_MODEL) {
            const float *h = tr808_hh_cal;
            if (part == D8_CH) svf_set(&v->r1, h[3] * dsp_sqrt(f), h[4]);
            else svf_set(&v->r1, h[5] * f, h[6]);
        } else svf_set(&v->r1, part == D8_CH ? 11700.0f * dsp_sqrt(f) : 7800.0f * f, 2.5f);
        v->aux = 0.5f + 0.5f * v->accent;
    }
    float k = knob(t, part, P_DECAY);
    float decay = part == D8_CH ? tr808_mx_cal[8] * spread(t, part, P_DECAY) : k < 0.5f ? 90.0f + 720.0f * k : 450.0f + 300.0f * (k - 0.5f);
    if (TR808_OH_LINEAR && part == D8_OH) {
        /* The open hat's reservoir (C62) discharges through its DECAY pot
         * at a nearly constant current: the level holds for the first
         * third, then falls in a straight line. Length 1.8 x the chart
         * time: as long to -20 dB as the exponential decay. */
        float len = TR808_HAT_MODEL == 2 ? MS(442.0f) * spread(t, part, P_DECAY) : 1.8f * MS(decay);
        if (v->age >= (uint32_t)(0.3f * len)) {
            if (decay != v->memo[0].x) { v->memo[0].x = decay; v->memo[0].y = 1.0f / (0.7f * len); }
            v->env = dsp_max(0.0f, v->env - v->memo[0].y);
        }
    } else if (TR808_HAT_MODEL == 2) v->env *= m_decay(&v->memo[0], MS(42.5f) * spread(t, part, P_DECAY));
    else v->env *= m_decay(&v->memo[0], MS(decay) / 2.3f);
    if (v->aux2 > 0) v->env *= m_decay(&v->memo[1], MS(7.0f));    /* choked by the other hat */
    float attack = dsp_min(1.0f, (float)v->age / MS(0.1f));
    float y;
    if (TR808_HAT_MODEL == 2) {
        if (v->age == 0) { v->lag1 = src; v->lag2 = 0; }   /* the bank may have jumped while idle */
        /* trigger 4 V, accented up to 7.3 V (8W8's range) */
        if (part == D8_OH) attack = 1.0f - m_exp(&v->memo[2], -(float)v->age / MS(6.0f));
        y = hat_chain(v, part, src, (4.0f + 3.3f * v->accent) * v->env * attack) * tr808_hh2_cal[part == D8_OH];
    } else y = svf_hp(&v->r1, swing_vca(src * 1.2f, v->env * attack * v->aux));
    if (TR808_HAT_MODEL != 2 && part == D8_OH) { /* open hat: a little more top end */
        v->out_lp += (y - v->out_lp) * 0.7277f;  /* ~9 kHz */
        y += tr808_mx_cal[9] * (y - v->out_lp);
    }
    if (v->env < 1e-4f) {                        /* fade out, no step at the end */
        v->env3 *= 0.9925f;
        if (v->env3 < 1e-3f) v->active = 0;
    }
    /* Accent: the higher reservoir alone gives ~+2 dB through the clipping
     * VCA; the chart's +6 dB needs the rest as gain. */
    return y * v->env3 * (1.0f + 0.6f * v->accent);
}

/* Zap: a sine whose pitch falls exponentially from Tune (300 Hz..6 kHz)
 * by Depth (up to 7 octaves) within Sweep (5..400 ms), the electro laser.
 * Tone drives it into a soft-clipped square; accent opens it further. */
static EL_DSP_VOICE float zap(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_ZP;
    if (v->age == 0) {
        v->aux = 1340.0f * (t->key_on ? t->key_r[part] : spread(t, part, P_TUNE) * spread(t, part, P_TUNE));   /* start Hz */
        v->pitch = 1.0f;
    }
    float sweep = 5.0f * m_exp2(&v->memo[0], 6.3f * knob(t, part, P_SNAPPY));            /* ms */
    float depth = 7.0f * knob(t, part, P_SOUND);                               /* octaves */
    v->pitch *= m_decay(&v->memo[1], MS(sweep));
    float hz = v->aux * dsp_exp2(-depth * (1.0f - v->pitch));
    v->out_lp += dsp_min(hz, 18000.0f) * (1.0f / EL_SAMPLE_RATE);
    if (v->out_lp >= 1.0f) v->out_lp -= 1.0f;
    float y = dsp_sin(2.0f * DSP_PI * v->out_lp);
    float drive = 1.0f + 12.0f * knob(t, part, P_TONE) * (0.6f + 0.4f * v->accent);
    y = dsp_tanh(y * drive) / dsp_tanh(drive);
    v->env *= m_decay(&v->memo[3], MS(20.0f * m_exp2(&v->memo[2], 5.3f * knob(t, part, P_DECAY))));
    float attack = dsp_min(1.0f, (float)v->age / MS(0.3f));
    if (v->env < 1e-4f) v->active = 0;
    return y * v->env * attack * (0.35f + 0.65f * v->volts);
}

/* Pulse: the 808's TRIGGER OUT plugged straight into a mixer channel, as
 * Egyptian Lover did. Fitted to a recording of a real unit's trigger
 * (waveform residual -8.5 dB, spectrum within 4.5 dB rms over 40 Hz-8 kHz):
 * a 15.2 ms pulse rising with a 0.30 ms time constant and falling in
 * 0.02 ms, with a short positive glitch just before the fall; through the
 * input coupling (1-pole HP, 0.22 ms) and three 2-pole high-passes,
 * 341 Hz Q 0.65, 97 Hz Q 2.7, 92 Hz Q 4. Result: a soft positive spike,
 * then one pulse later a sharp negative crack about twice as loud, each
 * followed by a low, ringing "plop". Knobs at 64 = the recording, except
 * that the plop is taken out (see below): it clashed with the bass.
 *   Width 3.8..60 ms, Tune: the corners /4..x4, Ring: resonance of the
 *   250 Hz high-pass /2..x2, Decay: coupling 0.05..0.9 ms. */
static EL_DSP_VOICE float pulse(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_PU;
    if (v->age == 0) {
        float tune = t->key_on ? t->key_r[part] : spread(t, part, P_TUNE) * spread(t, part, P_TUNE), ring = spread(t, part, P_TONE);
        svf_set(&v->r1, 341.0f * tune, 0.65f);
        /* The recording rings at ~100 Hz through two resonant high-passes
         * (97 Hz Q 2.7, 92 Hz Q 4); that low "plop" fights the bass, so
         * here they sit at 250 Hz, flat (Ring adds resonance back). */
        svf_set(&v->r2, 250.0f * tune, 0.7f * ring);
        svf_set(&v->r3, 250.0f * tune, 0.55f * ring);
    }
    float width = MS(15.15f) * m_exp2(&v->memo[0], 4.0f * (knob(t, part, P_SNAPPY) - 0.5f));
    float age = (float)v->age;
    /* The trigger voltage: exponential rise, very fast fall, the glitch. */
    float target = age < width ? 1.0f : 0.0f;
    v->aux2 += (target - v->aux2) * (age < width ? 0.0719f : 0.6415f);  /* 0.30 / 0.022 ms at 44.1 kHz */
    float x = v->aux2 + (age < width && age >= width - MS(0.05f) ? 0.233f : 0.0f);
    float coupling = MS(0.216f) * spread(t, part, P_DECAY) * spread(t, part, P_DECAY);
    v->dc_y = x - v->dc_x;                       /* input coupling, 1-pole HP */
    v->dc_x += v->dc_y * (1.0f - m_decay(&v->memo[1], coupling));
    float y = svf_hp(&v->r3, svf_hp(&v->r2, svf_hp(&v->r1, v->dc_y)));
    v->lp1 += (y - v->lp1) * 0.0043f;            /* 30 Hz DC block */
    if (age > width + MS(50.0f)) v->active = 0;
    return (y - v->lp1) * 1.49f * (0.75f + 0.25f * v->volts);
}

/* ---- render ------------------------------------------------------------------------- */

/* Output trims, calibrated with host/analyze_808.py: unaccented peaks follow
 * the Service Notes level chart (BD 3.5, SD 3, LT/HT 3.5, MT 3, RS 3 Vpp),
 * metal voices and clap set to a conventional mix level. */
static const float trim[D8_NUM] EL_DSP_DATA = {4.365f, 5.87f, 0.0788f, 0.1085f, 9.314f, 2.043f, 1.448f, 0.727f, 0.4533f, 2.482f, 0.3f, 0.3f};

enum { DRIVE_PARTS = (1u << D8_RS) | (1u << D8_CP) | (1u << D8_CB) | (1u << D8_CY) | (1u << D8_OH) | (1u << D8_CH) };

/* PO-12 kit output: its own balance (po12.c), +4.6 dB to sit like the 808 kit in the mix. */
static const float po_trim[D8_NUM] EL_DSP_DATA = {1.7f, 1.7f, 1.7f, 1.7f, 1.7f, 1.7f, 1.7f, 1.7f, 1.7f, 1.7f, 1.7f, 1.7f};

/* Drum key (docs/adr/0016). KIT Key C..B tunes the tonal parts to a key,
 * Follow BASS adds the BASS area's transposition. Each part goes to the
 * note of its degree nearest to its stock pitch: BD, HT / high conga,
 * snare body, claves, cowbell, zap and pulse on the root, LT / low conga
 * and the rim shot on the fifth; the cowbell's upper tone a pure fifth
 * above its lower. TUNE then moves in semitones (+-12). Key OFF: as
 * before (TUNE stepless, the unit's tolerances). Ratios once per block. */
static EL_DSP_HELPER float key_note(int pc, float near_hz)
{
    float q = (12.0f * dsp_log2(near_hz * (1.0f / 440.0f)) + 69.0f - (float)pc) * (1.0f / 12.0f);
    int k = (int)(q + (q >= 0 ? 0.5f : -0.5f));
    return 440.0f * dsp_exp2((float)(pc + 12 * k - 69) * (1.0f / 12.0f));
}
static EL_DSP_HELPER void key_update(tr808_t *t)
{
    t->key_on = t->kit[4] > 0;
    if (!t->key_on) return;
    int pc = (t->kit[4] - 1 + (t->kit[5] ? t->key_shift : 0)) % 12;
    if (pc < 0) pc += 12;
    static const uint8_t deg[D8_NUM] = {0, 0, 7, 0, 7, 0, 0, 0, 0, 0, 0, 0};   /* semitones above the root */
    for (int part = 0; part < D8_NUM; ++part) {
        float base;
        int d = deg[part];
        switch (part) {
        case D8_BD: base = tr808_bd_cal[0]; break;
        case D8_SD: base = t->pot[D8_SD][P_SOUND] ? 172.0f * 1.384f : 172.0f; break;
        case D8_LT: base = tom_hz[t->pot[D8_LT][P_SOUND] == 1 ? 2 : 0]; break;
        case D8_HT: base = tom_hz[t->pot[D8_HT][P_SOUND] == 1 ? 3 : 1]; break;
        case D8_RS: if (t->pot[D8_RS][P_SOUND] == 1) { base = 2500.0f; d = 0; } else base = 455.0f; break;
        case D8_CB: base = 540.0f; break;
        case D8_ZP: base = 1340.0f; break;
        case D8_PU: base = 341.0f; break;
        default: t->key_r[part] = 1.0f; continue;
        }
        int semis = el_clampi((int)((t->pot[part][P_TUNE] - 64) * 12 + (t->pot[part][P_TUNE] >= 64 ? 32 : -32)) / 64, -12, 12);
        t->key_r[part] = key_note((pc + d) % 12, base) * dsp_exp2((float)semis * (1.0f / 12.0f)) / base;
    }
    t->key_cb_hi = t->key_r[D8_CB] * (1.4983f * 540.0f / 800.0f);   /* a pure fifth */
}

EL_DSP_CODE void tr808_render(tr808_t *t, float *kick, float *left, float *right,
                              float *rev, float *gate, float *dly, int frames)
{
    int metal_on = t->v[D8_CY].active || t->v[D8_OH].active || t->v[D8_CH].active || t->v[D8_CB].active;
    if (!metal_on) {
        /* The six oscillators never stop on the 808: they run on while
         * nothing metal sounds, so each hit starts at another phase. */
        float tune = spread(t, D8_CY, P_TUNE) * (float)frames / EL_SAMPLE_RATE;
        for (int i = 0; i < 6; ++i) {
            float ph = t->metal_phase[i] + metal_hz[i] * metal_tol[i] * tune;
            t->metal_phase[i] = ph - (float)(int)ph;
        }
    }
    key_update(t);
    float level = t->kit[0] / 110.0f;
    /* Per block (the knobs do not move within one): the parts that sound,
     * their level, sends and drive. */
    int list[D8_NUM], count = 0;
    float lv[D8_NUM], send_r[D8_NUM], send_d[D8_NUM], drive[D8_NUM], drive_norm[D8_NUM];
    for (int part = 0; part < D8_NUM; ++part) {
        if (po_engine(t, part) ? !t->po[part].active : !t->v[part].active) continue;
        list[count++] = part;
        lv[part] = t->pot[part][P_LEVEL] / 100.0f;
        send_r[part] = knob(t, part, P_REV);
        send_d[part] = knob(t, part, P_DLY);
        drive[part] = 0;
        if (((1u << part) & DRIVE_PARTS) && t->pot[part][P_SNAPPY] && !po_engine(t, part)) {
            drive[part] = 1.0f + 6.0f * knob(t, part, P_SNAPPY);
            drive_norm[part] = dsp_sqrt(drive[part]);
        }
    }
    if (metal_on) metal_wander(t, frames);
    for (int n = 0; n < frames; ++n) {
        float low_src = 0, high_src = 0, cb_src = 0, hat_band = 0, raw = 0;
        if (metal_on) {
            float m = metal(t, t->metal_phase, spread(t, D8_CY, P_TUNE), &cb_src);
            raw = m * 0.714f;   /* the six squares' sum at 8W8's scale (hats model 2) */
            low_src = svf_bp(&t->metal_bp1, m, 0);
            high_src = svf_bp(&t->metal_bp2, m, 0);
            if (TR808_HAT_MODEL == 1 && (t->v[D8_CH].active || t->v[D8_OH].active)) {
                const float *h = tr808_hh_cal;
                if (t->hat_band_hz != h[0]) { t->hat_band_hz = h[0]; svf_set(&t->hat_band, h[0], h[1]); }
                hat_band = svf_bp(&t->hat_band, m, 0) + h[2] * low_src;
            }
        }
        /* TUNE of the hats and the cowbell off centre: their oscillators
         * retuned (pitch, not level); they start where the shared bank is. */
        float hat_src[2] = {high_src, high_src};
        if (TR808_HAT_MODEL == 2) hat_src[0] = hat_src[1] = raw;
        else if (TR808_HAT_MODEL) hat_src[0] = hat_src[1] = hat_band;
        for (int h = 0; h < 2; ++h) {
            int part = h ? D8_CH : D8_OH;
            if (!t->v[part].active || t->pot[part][P_TUNE] == 64) continue;
            if (t->v[part].age == 0) {
                memcpy(t->hat_phase[h], t->metal_phase, sizeof t->metal_phase);
                t->hat_bp[h] = TR808_HAT_MODEL ? t->hat_band : t->metal_bp2;
                if (TR808_HAT_MODEL) svf_set(&t->hat_bp[h], dsp_min(tr808_hh_cal[0] * spread(t, part, P_TUNE), 16000.0f), tr808_hh_cal[1]);
                else svf_set(&t->hat_bp[h], dsp_min(7100.0f * spread(t, part, P_TUNE), 16000.0f), 6.0f);
            }
            float m = metal(t, t->hat_phase[h], spread(t, D8_CY, P_TUNE) * spread(t, part, P_TUNE), 0);
            hat_src[h] = TR808_HAT_MODEL == 2 ? m * 0.714f : svf_bp(&t->hat_bp[h], m, 0);
        }
        if (t->v[D8_CB].active && (t->key_on || t->pot[D8_CB][P_TUNE] != 64)) {
            if (t->v[D8_CB].age == 0) memcpy(t->cb_phase, t->metal_phase + 4, sizeof t->cb_phase);
            if (t->key_on) cb_src = cowbell_osc(t->cb_phase, t->key_r[D8_CB], t->key_cb_hi);
            else {
                float r = spread(t, D8_CY, P_TUNE) * spread(t, D8_CB, P_TUNE);
                cb_src = cowbell_osc(t->cb_phase, r, r);
            }
        }
        /* One white-noise source feeds snare, clap / maracas and toms, as
         * on the 808: hit together, their noise is the same. */
        t->noise_now = noise(&t->noise);
        for (int i = 0; i < count; ++i) {
            int part = list[i];
            tr_voice_t *v = &t->v[part];
            float y;
            if (po_engine(t, part)) {
                if (!t->po[part].active) continue;
                y = po_tick(&t->po[part]) * po_trim[part] * lv[part] * level;
                if (part == D8_BD) { kick[n] += y; }
                else { left[n] += y * t->pan_l[part]; right[n] += y * t->pan_r[part]; }
                float r = y * send_r[part], d = y * send_d[part];
                if (part == D8_SD || part == D8_CP || part == D8_RS) gate[n] += r; else rev[n] += r;
                dly[n] += d;
                continue;
            }
            if (!v->active) continue;
            switch (part) {
            case D8_BD: y = bass_drum(t, v); break;
            case D8_SD: y = snare(t, v); break;
            case D8_LT: case D8_HT: y = tom(t, v, part); break;
            case D8_RS: y = rim(t, v); break;
            case D8_CP: y = clap(t, v); break;
            case D8_CB: y = cowbell(t, v, cb_src); break;
            case D8_CY: y = cymbal(t, v, low_src, high_src); break;
            case D8_ZP: y = zap(t, v); break;
            case D8_PU: y = pulse(t, v); break;
            default: y = hat(t, v, part, hat_src[part == D8_CH]); break;
            }
            ++v->age;
            /* DRIVE on rim, clap, cowbell and the metal voices, as on the bass drum */
            if (drive[part] > 0) y = dsp_tanh(y * drive[part]) / drive_norm[part];
            y *= trim[part] * lv[part] * level;
            if (part == D8_BD) { kick[n] += y; }
            else { left[n] += y * t->pan_l[part]; right[n] += y * t->pan_r[part]; }
            float r = y * send_r[part], d = y * send_d[part];
            if (part == D8_SD || part == D8_CP || part == D8_RS) gate[n] += r; else rev[n] += r;
            dly[n] += d;
        }
    }
}
