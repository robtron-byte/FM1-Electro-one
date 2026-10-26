/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Circuit-informed TR-808 voices; values and sources in docs/TR808.md. */
/* Device build: all code and constants of this file run in internal SRAM. */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
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
    /* KIT */ {4, {0, 1, 2, 3}, {P("Level", 110), P("Accent", 100), {"Choke", 2, 1, choke_names}, {"Engine", 1, 0, engine_names}}},
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

/* ---- PO-12 engine -------------------------------------------------------------- */

/* Parts that the PO-12 kit plays (PU stays the 808 trigger) and their
 * sounds; the Sound switch picks the second one. */
static inline int po_engine(const tr808_t *t, int part) { return t->kit[3] && part != D8_PU; }

static inline unsigned po_sound(const tr808_t *t, int part)
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

static float bass_drum(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_BD;
    const float *k = tr808_bd_cal;
    float f0 = k[0] * tolerance[part] * dsp_exp2((t->pot[part][P_TUNE] - 64) / 64.0f);
    float q = k[11] * dsp_exp2(k[12] * powf_approx(knob(t, part, P_DECAY), 0.8f));
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
    v->env *= decay_coef(q / (DSP_PI * f0) * EL_SAMPLE_RATE);
    v->pitch *= decay_coef(MS(k[13]));
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
    float cutoff = 305.0f * dsp_exp2(4.56f * tone);
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
    0.9546f, 0.1238f, 15.75f, 8.558f, 0.4709f, 28.89f, 39.58f, 0.1607f, 2098.7f, 7739.4f, 0.2575f};

/* Snare (Service Notes, Werner): two bridged-T resonators struck by the
 * trigger pulse, 172 and 339 Hz (as measured on the recordings; the 1981
 * board had 238 / 476), the lower one ringing ~3x longer; TONE crossfades
 * them (the low one falls with the pot, the high one rises ~11 dB over
 * its travel). White noise through a high-pass and a low-pass, its own
 * two-slope envelope, mixed in by SNAPPY, which acts from ~1/4 of its
 * travel (nothing below, -9 dB at the middle, full at the end). */
static float snare(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_SD;
    const float *k = tr808_sd_cal;
    float pulse = trigger_pulse(v);
    float tune = tolerance[part] * spread(t, part, P_TUNE), decay = spread(t, part, P_DECAY);
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
                 k[1] * dsp_exp2(1.79f * tone) * svf_bp(&v->r2, pulse * 3.0f, 0) / v->r2.k;
    body = dsp_tanh(body * 0.9f) / 0.9f;
    v->env *= decay_coef(MS(k[5]) * decay);
    v->env2 *= decay_coef(MS(k[6]) * decay);
    float attack = dsp_min(1.0f, (float)v->age / MS(0.5f));
    float n = svf_hp(&v->r3, t->noise_now);
    float lp = 1.0f - dsp_exp(-2.0f * DSP_PI * k[9] / EL_SAMPLE_RATE);
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
static float tom(tr808_t *t, tr_voice_t *v, int part)
{
    int conga = t->pot[part][P_SOUND] == 1, idx = (part - D8_LT) + (conga ? 2 : 0);
    float f0 = tom_hz[idx] * dsp_exp2(0.75f * (t->pot[part][P_TUNE] - 64) / 64.0f);   /* fitted: no tolerance */
    float scale = dsp_sqrt(93.0f / f0);   /* times as fitted for LT, shorter for higher drums */
    if (v->age == 0) {
        v->lag2 = v->lag1;          /* what still rings fades out instead of clicking */
        v->aux = 0;                 /* phase: the hit starts on -cos */
        svf_set(&v->r1, 500.0f * f0 / tom_hz[idx], 1.0f);
    }
    float bend = 0.277f * knob(t, part, P_SNAPPY) * 2.0f * (conga ? 0.35f : 1.0f) * (1.0f + 0.4f * v->accent);
    v->pitch *= decay_coef(MS(42.9f * scale));
    float hz = f0 * (1.0f + bend * v->pitch);
    v->aux += hz * (1.0f / EL_SAMPLE_RATE);
    if (v->aux >= 1.0f) v->aux -= 1.0f;
    float body = -dsp_cos(2.0f * DSP_PI * v->aux);
    v->env *= decay_coef(MS(74.9f * scale * (conga ? 0.65f : 1.0f) * dsp_exp2((t->pot[part][P_DECAY] - 64) / 48.0f)));
    v->env3 *= decay_coef(MS(0.19f));
    body *= v->env * (1.0f - v->env3);
    if (!conga) {
        /* Noise: band-passed around 500 Hz (the fit's residual), 10 ms. */
        float attack = dsp_min(1.0f, (float)v->age / MS(0.5f));
        v->env2 *= decay_coef(MS(10.0f));
        float amount = 0.014f * dsp_exp2((t->pot[part][P_TONE] - 64) / 16.0f);
        body += svf_bp(&v->r1, t->noise_now, 0) * 2.0f * amount * v->env2 * attack;
        /* Click: a 0.3 ms spike, high-passed at 1 kHz. */
        float c = dsp_exp(-(float)v->age / MS(0.3f));
        v->out_lp += (c - v->out_lp) * 0.133f;
        body += 0.03f * (c - v->out_lp);
    }
    float soft = v->volts * 14.0f / (4.0f + 10.0f * v->accent);   /* MIDI hits below normal */
    float in = body * (1.0f + 1.2f * v->accent) * soft;
    float y = (dsp_tanh(0.76f * (in + 0.0025f)) - dsp_tanh(0.76f * 0.0025f)) * (1.0f + 0.5f * v->accent);
    v->lag2 *= decay_coef(MS(1.5f));
    y += v->lag2;
    v->lag1 = y;
    if (v->age > (uint32_t)MS(30.0f) && v->env < 1e-4f && dsp_abs(v->lag2) < 1e-5f) { v->lag1 = 0; v->active = 0; }
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

/* Hand clap: band-passed noise (~1.1 kHz, Q 2) through a sawtooth envelope
 * of three 10 ms pulses plus a 20 ms fall, and a quieter ~100 ms "reverb".
 * Maracas: high-passed noise with a 13 ms rise and 17 ms fall. */
static float clap(tr808_t *t, tr_voice_t *v)
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
    if (v->age == 0) svf_set(&v->r1, 1140.0f * spread(t, part, P_TONE) * tolerance[part], 1.95f);
    float bp = svf_bp(&v->r1, n, 0);
    float ms = (float)v->age / MS(1.0f), saw;
    if (ms < 30.0f) { float in = ms - 10.0f * (float)(int)(ms / 10.0f); saw = 1.0f - in / 10.0f; }
    else saw = dsp_exp(-(ms - 30.0f) / 8.7f);
    v->env2 *= decay_coef(MS(tr808_mx_cal[5]) * spread(t, part, P_DECAY));
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

static float metal(float *phase, float tune, float *cowbell)
{
    float sum = 0, r[6];
    for (int i = 0; i < 6; ++i) {
        float dt = metal_hz[i] * metal_tol[i] * tune * (1.0f / EL_SAMPLE_RATE);
        phase[i] += dt;
        if (phase[i] >= 1.0f) phase[i] -= 1.0f;
        r[i] = rect(phase[i], dt);
        sum += r[i];
    }
    if (cowbell) *cowbell = 2.0f * (r[4] + r[5]) - 2.0f;
    return sum * (1.0f / 3.0f) - 1.0f;
}

/* The cowbell's 540 and 800 Hz rectangles alone (its TUNE off centre). */
static float cowbell_osc(float *phase, float tune)
{
    float sum = 0;
    for (int i = 0; i < 2; ++i) {
        float dt = metal_hz[4 + i] * tune * (1.0f / EL_SAMPLE_RATE);
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
static float cowbell(tr808_t *t, tr_voice_t *v, float src)
{
    const int part = D8_CB;
    const float *k = tr808_mx_cal;
    if (v->age == 0) svf_set(&v->r1, k[0] * spread(t, part, P_TUNE), k[1]);
    float y = svf_bp(&v->r1, src * 0.5f, 0);
    float decay = spread(t, part, P_DECAY);
    v->env *= decay_coef(MS(k[2]) * decay);
    v->env2 *= decay_coef(MS(k[3]) * decay);
    float attack = dsp_min(1.0f, (float)v->age / MS(0.7f));
    float e = (k[4] * v->env + (1.0f - k[4]) * v->env2) * attack;
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
        /* TUNE moves the high-pass with the oscillators, the closed hat's half
         * as far (it would leave the audio band); centred as on the 808 */
        float f = spread(t, part, P_TUNE);
        svf_set(&v->r1, part == D8_CH ? 11700.0f * dsp_sqrt(f) : 7800.0f * f, 2.5f);
        v->aux = 0.5f + 0.5f * v->accent;
    }
    float k = knob(t, part, P_DECAY);
    float decay = part == D8_CH ? tr808_mx_cal[8] * spread(t, part, P_DECAY) : k < 0.5f ? 90.0f + 720.0f * k : 450.0f + 300.0f * (k - 0.5f);
    v->env *= decay_coef(MS(decay) / 2.3f);
    if (v->aux2 > 0) v->env *= decay_coef(MS(7.0f));    /* choked by the other hat */
    float attack = dsp_min(1.0f, (float)v->age / MS(0.1f));
    float y = svf_hp(&v->r1, swing_vca(src * 1.2f, v->env * attack * v->aux));
    if (part == D8_OH) {                         /* open hat: a little more top end */
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
static float pulse(tr808_t *t, tr_voice_t *v)
{
    const int part = D8_PU;
    if (v->age == 0) {
        float tune = spread(t, part, P_TUNE) * spread(t, part, P_TUNE), ring = spread(t, part, P_TONE);
        svf_set(&v->r1, 341.0f * tune, 0.65f);
        /* The recording rings at ~100 Hz through two resonant high-passes
         * (97 Hz Q 2.7, 92 Hz Q 4); that low "plop" fights the bass, so
         * here they sit at 250 Hz, flat (Ring adds resonance back). */
        svf_set(&v->r2, 250.0f * tune, 0.7f * ring);
        svf_set(&v->r3, 250.0f * tune, 0.55f * ring);
    }
    float width = MS(15.15f) * dsp_exp2(4.0f * (knob(t, part, P_SNAPPY) - 0.5f));
    float age = (float)v->age;
    /* The trigger voltage: exponential rise, very fast fall, the glitch. */
    float target = age < width ? 1.0f : 0.0f;
    v->aux2 += (target - v->aux2) * (age < width ? 0.0719f : 0.6415f);  /* 0.30 / 0.022 ms at 44.1 kHz */
    float x = v->aux2 + (age < width && age >= width - MS(0.05f) ? 0.233f : 0.0f);
    float coupling = MS(0.216f) * spread(t, part, P_DECAY) * spread(t, part, P_DECAY);
    v->dc_y = x - v->dc_x;                       /* input coupling, 1-pole HP */
    v->dc_x += v->dc_y * (1.0f - decay_coef(coupling));
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
    float level = t->kit[0] / 110.0f;
    for (int n = 0; n < frames; ++n) {
        float low_src = 0, high_src = 0, cb_src = 0;
        if (metal_on) {
            float m = metal(t->metal_phase, spread(t, D8_CY, P_TUNE), &cb_src);
            low_src = svf_bp(&t->metal_bp1, m, 0);
            high_src = svf_bp(&t->metal_bp2, m, 0);
        }
        /* TUNE of the hats and the cowbell off centre: their oscillators
         * retuned (pitch, not level); they start where the shared bank is. */
        float hat_src[2] = {high_src, high_src};
        for (int h = 0; h < 2; ++h) {
            int part = h ? D8_CH : D8_OH;
            if (!t->v[part].active || t->pot[part][P_TUNE] == 64) continue;
            if (t->v[part].age == 0) {
                memcpy(t->hat_phase[h], t->metal_phase, sizeof t->metal_phase);
                t->hat_bp[h] = t->metal_bp2;
                svf_set(&t->hat_bp[h], dsp_min(7100.0f * spread(t, part, P_TUNE), 16000.0f), 6.0f);
            }
            float m = metal(t->hat_phase[h], spread(t, D8_CY, P_TUNE) * spread(t, part, P_TUNE), 0);
            hat_src[h] = svf_bp(&t->hat_bp[h], m, 0);
        }
        if (t->v[D8_CB].active && t->pot[D8_CB][P_TUNE] != 64) {
            if (t->v[D8_CB].age == 0) memcpy(t->cb_phase, t->metal_phase + 4, sizeof t->cb_phase);
            cb_src = cowbell_osc(t->cb_phase, spread(t, D8_CY, P_TUNE) * spread(t, D8_CB, P_TUNE));
        }
        /* One white-noise source feeds snare, clap / maracas and toms, as
         * on the 808: hit together, their noise is the same. */
        t->noise_now = noise(&t->noise);
        for (int part = 0; part < D8_NUM; ++part) {
            tr_voice_t *v = &t->v[part];
            float y;
            if (po_engine(t, part)) {
                if (!t->po[part].active) continue;
                y = po_tick(&t->po[part]) * po_trim[part] * (t->pot[part][P_LEVEL] / 100.0f) * level;
                if (part == D8_BD) { kick[n] += y; }
                else { left[n] += y * t->pan_l[part]; right[n] += y * t->pan_r[part]; }
                float r = y * knob(t, part, P_REV), d = y * knob(t, part, P_DLY);
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
            if (((1u << part) & DRIVE_PARTS) && t->pot[part][P_SNAPPY]) {
                float d = 1.0f + 6.0f * knob(t, part, P_SNAPPY);
                y = dsp_tanh(y * d) / dsp_sqrt(d);
            }
            y *= trim[part] * (t->pot[part][P_LEVEL] / 100.0f) * level;
            if (part == D8_BD) { kick[n] += y; }
            else { left[n] += y * t->pan_l[part]; right[n] += y * t->pan_r[part]; }
            float r = y * knob(t, part, P_REV), d = y * knob(t, part, P_DLY);
            if (part == D8_SD || part == D8_CP || part == D8_RS) gate[n] += r; else rev[n] += r;
            dly[n] += d;
        }
    }
}
