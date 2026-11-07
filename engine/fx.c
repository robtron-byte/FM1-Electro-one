/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#if defined(EL_PLATFORM)   /* own platform: the constants in SRAM with the audio code */
#pragma const_seg(".dsp_rodata")
#elif !defined(FM1_HOST_TEST)   /* FM1 Open SDK build */
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "fx.h"
#include "dsp_math.h"
#include <string.h>

/* Delay note values in quarter notes (DelayDivision in the iOS apps). */
static const float quarters[12] EL_DSP_DATA = {0.125f, 1.0f / 6, 0.25f, 0.375f, 1.0f / 3, 0.5f, 0.75f, 2.0f / 3,
                                               1, 1.5f, 2, 4};
static const char *const division_names[12] = {"1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.",
                                               "1/4T", "1/4", "1/4.", "1/2", "1/1"};
static const char *const names[FX_PARAMS] = {"TIME", "FEEDBACK", "TONE", "DLY RET", "SIZE", "DECAY", "DAMP", "REV RET",
                                             "DIST", "DRIVE", "D TONE", "DST BEAT", "DST RHYT", "DST STRG", "DST BASS"};
static const char *const dist_names[DIST_MODES] = {"OFF", "OD", "DIST", "RAT", "TUBE"};
static const float base_lengths[4] EL_DSP_DATA = {1116, 1277, 1422, 1557};
static const unsigned diff_lengths[2] EL_DSP_DATA = {379, 277};

void fx_init(el_fx_t *fx)
{
    static const uint8_t defaults[FX_PARAMS] = {6, 52, 70, 100, 70, 72, 50, 100, DIST_RAT, 60, 70, 0, 0, 0, 0};
    memset(fx, 0, sizeof *fx);
    memcpy(fx->v, defaults, sizeof defaults);
    fx->lfo[1] = 0.25f; fx->lfo[2] = 0.5f; fx->lfo[3] = 0.75f;
    fx->dist.gain = 1;
    fx->dist.mono = 1;
}

int fx_param_max(unsigned param)
{
    return param == FX_DELAY_DIV ? 11 : param == FX_DIST_MODE ? DIST_MODES - 1 : param < FX_PARAMS ? 127 : 0;
}
const char *fx_dist_mode_name(int value) { return dist_names[el_clampi(value, 0, DIST_MODES - 1)]; }
const char *fx_param_name(unsigned param) { return param < FX_PARAMS ? names[param] : ""; }
const char *fx_delay_division_name(int value) { return division_names[el_clampi(value, 0, 11)]; }

static inline __attribute__((always_inline)) float soft(float x)
{
    x = dsp_clamp(x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

/* Linear read from a third-rate 16-bit line, position in third-rate frames. */
static inline __attribute__((always_inline)) float delay_read(const int16_t *line, unsigned write, float back)
{
    float pos = (float)write - back;
    int i = (int)pos;
    if ((float)i > pos) --i;
    float frac = pos - (float)i;
    if (i < 0) i += FX_DELAY_CAP;   /* write < CAP and back < CAP: at most one turn (no division) */
    float a = line[i], b = line[i + 1 < FX_DELAY_CAP ? i + 1 : 0];
    return (a + (b - a) * frac) * (1.0f / 32767.0f);
}

/* Non-linear reverb taps: 16 echoes from 20 to 330 ms at nearly flat level,
 * then nothing - the classic gated 80s / electro snare and clap. Positions
 * in half-rate frames, alternating sides. */
#if FX_GATE_DENSE == 0
static const uint16_t gate_taps[16] EL_DSP_DATA = {441, 783, 1102, 1433, 1764, 2117, 2448, 2800,
                                                    3131, 3484, 3815, 4167, 4498, 4851, 5182, 7277};
static const float gate_gains[16] EL_DSP_DATA = {0.62f, 0.58f, 0.6f, 0.55f, 0.57f, 0.53f, 0.55f, 0.5f,
                                                  0.52f, 0.47f, 0.48f, 0.42f, 0.4f, 0.32f, 0.22f, 0.0f};
#elif FX_GATE_DENSE == 1
/* Dense version: 24 taps from 20 to 322 ms at irregular distances (5-22 ms,
 * no periodic flutter), the sides mixed, about the same total energy. It
 * swells in over the first three taps (no distinct first echo that sounds
 * like a second clap) and falls over the last fifth, then the gate. */
enum { GATE_DENSE_TAPS = 24 };
static const uint16_t gate_dense_taps[GATE_DENSE_TAPS] EL_DSP_DATA = {
    441, 701, 1053, 1358, 1586, 1985, 2120, 2435, 2754, 2991, 3344, 3688,
    3808, 4110, 4552, 4683, 4987, 5461, 5589, 5892, 6246, 6428, 6830, 7100};
static const float gate_dense_gains[GATE_DENSE_TAPS] EL_DSP_DATA = {
    0.171f, 0.29f, 0.404f, 0.5f, 0.495f, 0.485f, 0.482f, 0.475f, 0.466f, 0.461f, 0.451f, 0.442f,
    0.439f, 0.432f, 0.421f, 0.418f, 0.409f, 0.397f, 0.394f, 0.361f, 0.278f, 0.235f, 0.141f, 0.078f};
static const uint8_t gate_dense_right[GATE_DENSE_TAPS] EL_DSP_DATA = {
    0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};
#else
/* Gated reverb 2: four lines in the gate buffer (2048 half-rate frames
 * each; 27.8 / 35.7 / 44.0 / 54.1 ms), Hadamard feedback, a gentle
 * low-pass in the loop. A hit (fast envelope 3 x above the slow one)
 * opens the gate for 300 ms (again only after 100 ms); then it closes
 * over ~13 ms and the loop's feedback goes with it: no tail. */
enum { GATE_LINE = 2048, GATE_HOLD = 6615, GATE_REFRACT = 2205 };
static const uint16_t gate_len[4] EL_DSP_DATA = {613, 787, 971, 1193};
#ifndef FX_GATE_FB
#define FX_GATE_FB 0.94f
#endif
#ifndef FX_GATE_OUT
#define FX_GATE_OUT 0.57f   /* as loud as variants 0 and 1 */
#endif
#endif

/* Reverb lines and diffusers in 16 bit (truncated: symmetric, no DC in
 * the loops), with headroom. */
static inline EL_DSP_HELPER int16_t line_store(float x)
{
    return (int16_t)(dsp_clamp(x, -3.9998f, 3.9998f) * FX_LINE_SCALE);
}

#if FX_GATE_DENSE != 2
static inline EL_DSP_HELPER float gate_read(const int16_t *line, unsigned write, unsigned back)
{
    return line[(write + FX_GATE_CAP - back) % FX_GATE_CAP] * (1.0f / 32767.0f);
}
#endif

/* ---- punch-ins borrowing the buffers --------------------------------------- */

static const struct { uint32_t off, len; } regions[3] = {
    {0, FX_HALL_REGION}, {FX_HALL_REGION, FX_TAPE_REGION}, {FX_HALL_REGION + FX_TAPE_REGION, FX_DELAY_REGION}};

static EL_DSP_HELPER unsigned region_of(unsigned use) { return use == FX_USE_HALL ? 0u : use == FX_USE_TAPE ? 1u : 2u; }

EL_DSP_HELPER int16_t *fx_region(el_fx_t *fx, unsigned use) { return fx->mem.hole + regions[region_of(use)].off; }

/* The first user stops delay, hall and gate and clears everything; each
 * user starts with its own region empty. */
EL_DSP_HELPER void fx_borrow(el_fx_t *fx, unsigned use)
{
    if (!fx->users) memset(&fx->mem, 0, sizeof fx->mem);
    else memset(fx_region(fx, use), 0, regions[region_of(use)].len * sizeof(int16_t));
    fx->users |= (uint8_t)use;
}

/* The last user gives the buffers back, empty: delay and hall start clean. */
EL_DSP_HELPER void fx_release(el_fx_t *fx, unsigned use)
{
    fx->users &= (uint8_t)~use;
    if (!fx->users) {
        memset(&fx->mem, 0, sizeof fx->mem);
        fx->delay_sleep = fx->reverb_sleep = fx->gate_sleep = 0;
    }
}

/* ---- the big hall of the reverb throw ------------------------------------ */

/* Dattorro's tank lengths (at 29761 Hz), scaled to the half rate and made
 * 1.3 times larger: each half of the tank loops in about half a second.
 * (It shares the buffers with the tape stop and the repeat.) */
#define HOLE_SCALE (0.5f * EL_SAMPLE_RATE / 29761.0f * 1.3f)
enum { HOLE_EXCURSION = 24 };   /* modulation of the first tank allpasses, samples */
static const uint16_t hole_in_len[4] = {142, 107, 379, 277};
static const uint16_t hole_tank[2][4] = {{672, 4453, 1800, 3720}, {908, 4217, 2656, 3163}};
/* Output taps (Dattorro's table): tank, line (0 first delay, 1 second
 * allpass, 2 second delay), samples back, sign; left then right. */
static const int16_t hole_taps[2][7][4] = {
    {{1, 0, 266, 1}, {1, 0, 2974, 1}, {1, 1, 1913, -1}, {1, 2, 1996, 1}, {0, 0, 1990, -1}, {0, 1, 187, -1}, {0, 2, 1066, -1}},
    {{0, 0, 353, 1}, {0, 0, 3627, 1}, {0, 1, 1228, -1}, {0, 2, 2673, 1}, {1, 0, 2111, -1}, {1, 1, 335, -1}, {1, 2, 121, -1}}};

static void hole_line(fx_line_t *l, uint32_t *off, float len)
{
    l->off = *off;
    l->len = (uint32_t)len + 2u;
    l->pos = 0;
    *off += l->len;
}

EL_DSP_HELPER void hole_init(fx_hole_t *h)
{
    memset(h, 0, sizeof *h);
    uint32_t off = 0;
    for (int i = 0; i < 4; ++i) hole_line(&h->in[i], &off, hole_in_len[i] * HOLE_SCALE);
    for (int t = 0; t < 2; ++t) {
        hole_line(&h->ap1[t], &off, hole_tank[t][0] * HOLE_SCALE + HOLE_EXCURSION + 2);
        hole_line(&h->dl1[t], &off, hole_tank[t][1] * HOLE_SCALE);
        hole_line(&h->ap2[t], &off, hole_tank[t][2] * HOLE_SCALE);
        hole_line(&h->dl2[t], &off, hole_tank[t][3] * HOLE_SCALE);
    }
    if (off > FX_HALL_REGION) memset(h, 0, sizeof *h);   /* never: the lines fit their region */
    /* The taps sit at fixed points: their whole and fractional parts once
     * here, not in float at every step (hole_tap). */
    for (int s = 0; s < 2; ++s)
        for (int k = 0; k < 7; ++k) {
            float back = (float)hole_taps[s][k][2] * HOLE_SCALE;
            unsigned ib = (unsigned)back;
            float fb = back - (float)ib;
            h->tap_i[s][k] = (uint16_t)(fb > 0.0f ? ib + 1u : ib);   /* the older of the two samples */
            h->tap_f[s][k] = fb > 0.0f ? 1.0f - fb : 0.0f;          /* towards the newer one */
        }
}

/* 16-bit lines at a quarter of full scale: room for the tank's peaks. */
static inline __attribute__((always_inline)) float hole_get(const int16_t *m, const fx_line_t *l, float back)
{
    float p = (float)l->pos - back;
    while (p < 0.0f) p += (float)l->len;
    unsigned i = (unsigned)p, len = l->len;
    /* i is below 2 len: a subtraction instead of the two divisions of a
     * modulo (26 reads a tank step: over a million divisions a second) */
    if (i >= len) i -= len;
    unsigned j = i + 1u >= len ? i + 1u - len : i + 1u;
    float f = p - (float)(unsigned)p, a = m[l->off + i], b = m[l->off + j];
    return (a + (b - a) * f) * (4.0f / 32767.0f);
}

static inline __attribute__((always_inline)) void hole_put(int16_t *m, fx_line_t *l, float x)
{
    m[l->off + l->pos] = (int16_t)(dsp_clamp(x * 0.25f, -1.0f, 1.0f) * 32767.0f);
    l->pos = l->pos + 1u < l->len ? l->pos + 1u : 0u;
}

static inline __attribute__((always_inline)) float hole_allpass(int16_t *m, fx_line_t *l, float x, float g, float back)
{
    float d = hole_get(m, l, back), w = x + g * d;
    hole_put(m, l, w);
    return d - g * w;
}

/* tap k of side s: the line's sample tap_i back, interpolated by tap_f
 * towards the newer one (as hole_get with the same back) */
static inline __attribute__((always_inline)) float hole_tap(const int16_t *m, const fx_hole_t *h, int s, int k)
{
    int t = hole_taps[s][k][0], line = hole_taps[s][k][1];
    const fx_line_t *l = line == 0 ? &h->dl1[t] : line == 1 ? &h->ap2[t] : &h->dl2[t];
    unsigned len = l->len, back = h->tap_i[s][k];
    unsigned i = l->pos >= back ? l->pos - back : l->pos + len - back;
    unsigned j = i + 1u < len ? i + 1u : 0u;
    float a = m[l->off + i], b = m[l->off + j];
    return (a + (b - a) * h->tap_f[s][k]) * (4.0f / 32767.0f);
}

EL_DSP_CODE void hole_render(el_fx_t *fx, const float *in, float *left, float *right, int frames)
{
    fx_hole_t *h = &fx->hole;
    int16_t *m = fx_region(fx, FX_USE_HALL);
    /* RT60 about 22 s (the tank low-pass shortens it to around 12): per
     * half-loop gain from the tank's loop time. */
    float loop = 0;
    for (int t = 0; t < 2; ++t) for (int k = 0; k < 4; ++k) loop += hole_tank[t][k] * HOLE_SCALE;
    const float decay = dsp_exp2(-3.0f * 3.321928f * (loop / 2.0f) / (22.0f * 0.5f * EL_SAMPLE_RATE));
    const float damp = 0.26f;   /* tank low-pass at the half rate: about 1.1 kHz, dark and warm */
    for (int n = 0; n < frames; ++n) {
        if (fx->throw_on) fx->throw_mix += (1.0f - fx->throw_mix) * 0.0045f;   /* 5 ms in */
        else if ((fx->throw_mix *= 0.99987f) < 0.001f) fx->throw_mix = 0.0f;   /* let go: -60 dB in 1.2 s */
        h->acc += fx->throw_on ? in[n] : 0.0f;
        h->half ^= 1u;
        if (!h->half) {
            /* One step of the tank at half rate. */
            float x = h->acc * 0.5f;
            h->acc = 0;
            static const float ig[4] = {0.75f, 0.75f, 0.625f, 0.625f};
            for (int i = 0; i < 4; ++i) x = hole_allpass(m, &h->in[i], x, ig[i], (float)(h->in[i].len - 2u));
            h->phase += 0.6f / (0.5f * EL_SAMPLE_RATE);
            if (h->phase >= 1.0f) h->phase -= 1.0f;
            for (int t = 0; t < 2; ++t) {
                float lfo = dsp_sin(2.0f * DSP_PI * (h->phase + 0.25f * (float)t));
                float a = x + h->feedback[1 - t] * decay;
                float back = (float)(h->ap1[t].len - 2u) - HOLE_EXCURSION * (0.5f + 0.5f * lfo) - 1.0f;
                a = hole_allpass(m, &h->ap1[t], a, -0.7f, back);
                hole_put(m, &h->dl1[t], a);
                float d = hole_get(m, &h->dl1[t], (float)(h->dl1[t].len - 2u));
                h->lp[t] += (d - h->lp[t]) * damp;
                d = hole_allpass(m, &h->ap2[t], h->lp[t] * decay, 0.5f, (float)(h->ap2[t].len - 2u));
                hole_put(m, &h->dl2[t], d);
                h->feedback[t] = hole_get(m, &h->dl2[t], (float)(h->dl2[t].len - 2u));
            }
            for (int s = 0; s < 2; ++s) {
                float y = 0;
                for (int k = 0; k < 7; ++k)
                    y += hole_tap(m, h, s, k) * (float)hole_taps[s][k][3];
                h->prev[s] = h->out[s];
                h->out[s] = y * 0.6f;
            }
        }
        /* Back to the full rate: halfway between the last two outputs. */
        float l = h->half ? 0.5f * (h->prev[0] + h->out[0]) : h->out[0];
        float r = h->half ? 0.5f * (h->prev[1] + h->out[1]) : h->out[1];
        left[n] += l * fx->throw_mix;
        right[n] += r * fx->throw_mix;
    }
}

/* FX_REVERB_HALF 1 (default, docs/adr/0019): the hall's network runs on
 * every second frame (half the cost); its lines, modulation and damping are
 * scaled so size and decay stay, the output is interpolated between. 0: on
 * every frame as before. */
#ifndef FX_REVERB_HALF
#define FX_REVERB_HALF 1
#endif
/* FX_REVERB_LFO_EVERY n (docs/adr/0019): the hall's four line modulations
 * (LFO, triangle, read position) worked out every n-th of its frames and
 * held in between (the position then moves one frame a frame: the same
 * whole offset and fraction). 1: every frame, as before. */
#ifndef FX_REVERB_LFO_EVERY
#define FX_REVERB_LFO_EVERY 1
#endif

EL_DSP_CODE void fx_render(el_fx_t *fx, const float *dly, const float *rev, const float *gate,
                           float *left, float *right, int frames, float bpm)
{
    const uint8_t *v = fx->v;
    /* Delay: tempo-synced time, glided so tempo changes bend instead of click. */
    float seconds = 60.0f / (bpm < 30 ? 30 : bpm) * quarters[v[FX_DELAY_DIV] < 12 ? v[FX_DELAY_DIV] : 11];
    float target = dsp_clamp(seconds * EL_SAMPLE_RATE * (1.0f / 3.0f), 8.0f, FX_DELAY_CAP - 4.0f);
    if (fx->time <= 0) fx->time = target;
    float fb = v[FX_DELAY_FB] / 127.0f * 1.05f;
    float tone = 0.05f + 0.9f * (v[FX_DELAY_TONE] / 127.0f) * (v[FX_DELAY_TONE] / 127.0f);
    float dret = v[FX_DELAY_RETURN] / 127.0f;
    /* Reverb: size scales the line lengths, decay is RT60 0.3 .. 12 s. */
    float size = v[FX_REVERB_SIZE] / 127.0f, scale = 0.35f + 0.95f * size;
    float t60 = 0.3f * dsp_exp2(5.32f * v[FX_REVERB_DECAY] / 127.0f);
    float rret = v[FX_REVERB_RETURN] / 127.0f, lengths[4], gains[4];
    for (int i = 0; i < 4; ++i) {
        lengths[i] = dsp_min(base_lengths[i] * scale, FX_REVERB_CAP - 24.0f);
        gains[i] = dsp_exp2(-3.0f * 3.321928f * lengths[i] / (t60 * EL_SAMPLE_RATE));
    }
    float damp_hz = 14000.0f - v[FX_REVERB_DAMP] / 127.0f * 12000.0f;
    float diffusion = 0.55f + 0.17f * size, mod = 1.5f + 8.0f * size;
    unsigned dsize[2] = {(unsigned)(diff_lengths[0] * (0.5f + size)), (unsigned)(diff_lengths[1] * (0.5f + size))};
#if FX_REVERB_HALF
    /* the same times at half the rate: half the frames (the gains stay:
     * they are per pass of a line, which takes as long as before) */
    const float rev_rate = EL_SAMPLE_RATE * 0.5f;
    for (int i = 0; i < 4; ++i) lengths[i] *= 0.5f;
    dsize[0] >>= 1; dsize[1] >>= 1;
    mod *= 0.5f;
    damp_hz = dsp_min(damp_hz, rev_rate * 0.45f);
#else
    const float rev_rate = EL_SAMPLE_RATE;
#endif
    float damp = 1.0f - dsp_exp(-2.0f * DSP_PI * damp_hz / rev_rate);
    int delay_on = dret > 0.0f, reverb_on = rret > 0.0f;
    /* Reverb throw: the big hall owns the buffers while it sounds; delay,
     * hall and gated reverb rest. The memory is cleared when it starts and
     * when it has rung out, so both sides begin in silence. */
    if (fx->throw_on && !(fx->users & FX_USE_HALL)) {
        fx_borrow(fx, FX_USE_HALL);
        hole_init(&fx->hole);
        fx->throw_mix = 0.0f;
    }
    if (fx->users) {
        if (fx->users & FX_USE_HALL) {
            hole_render(fx, rev, left, right, frames);
            if (!fx->throw_on && fx->throw_mix <= 0.0f) fx_release(fx, FX_USE_HALL);
        }
        return;
    }

    /* Sleep: an effect with silent input whose output has died away is
     * cleared once and skipped until input comes back. */
    int dly_in = 0, rev_in = 0, gate_in = 0;
    for (int n = 0; n < frames; ++n) {
        dly_in |= dly[n] != 0.0f;
        rev_in |= rev[n] != 0.0f;
        gate_in |= gate[n] != 0.0f;
    }
    if (dly_in) fx->delay_sleep = 0;
    if (rev_in) fx->reverb_sleep = 0;
    if (gate_in) fx->gate_sleep = 0;
    delay_on &= !fx->delay_sleep;
    reverb_on &= !fx->reverb_sleep;
    /* Asleep, the delay time and the reverb's modulation still run on. */
    if (fx->delay_sleep) fx->time = target;
    if (fx->reverb_sleep && rret > 0.0f)
        for (int i = 0; i < 4; ++i) {
            fx->lfo[i] += (0.23f + 0.17f * (float)i) / EL_SAMPLE_RATE * (float)frames;
            fx->lfo[i] -= (float)(int)fx->lfo[i];
        }
    int gate_on = !fx->gate_sleep;
    float delay_peak = 0, reverb_peak = 0, gate_peak = 0;
    /* One loop per effect (each only when it runs): no tests between them
     * every frame. Each adds to left / right in the same order as before. */
    /* (the states in locals during the loops: left / right are floats, so the
     * compiler would reload every fx field each frame; the same arithmetic) */
    if (delay_on) {
        float time = fx->time, hp = fx->dly_hp[0], lp0 = fx->lp[0], lp1 = fx->lp[1], acc0 = fx->acc[0], acc1 = fx->acc[1];
        unsigned half = fx->half, dw = fx->dw;
        int16_t *line0 = fx->mem.b.delay[0], *line1 = fx->mem.b.delay[1];
        for (int n = 0; n < frames; ++n) {
            time += (target - time) * 0.0002f;
            /* Read between the stored frames by the step within the current
             * one, so the echoes are interpolated, not held. */
            float back = time - (float)half * (1.0f / 3.0f);
            float yl = delay_read(line0, dw, back), yr = delay_read(line1, dw, back);
            /* Ping-pong: the input enters left, each side feeds the other.
             * The send is high-passed (about 200 Hz) to keep the echoes off
             * the kick and bass. */
            hp += (dly[n] - hp) * 0.0281f;
            float in = dly[n] - hp;
            float wl = soft(in + fb * yr), wr = soft(fb * yl);
            lp0 += (wl - lp0) * tone;
            lp1 += (wr - lp1) * tone;
            acc0 += lp0;
            acc1 += lp1;
            if (++half >= 3u) { /* every third frame: store the averaged three */
                half = 0;
                line0[dw] = (int16_t)(dsp_clamp(acc0 * (1.0f / 3.0f), -1, 1) * 32767.0f);
                line1[dw] = (int16_t)(dsp_clamp(acc1 * (1.0f / 3.0f), -1, 1) * 32767.0f);
                acc0 = acc1 = 0;
                dw = dw + 1u < FX_DELAY_CAP ? dw + 1u : 0u;
            }
            left[n] += yl * dret;
            right[n] += yr * dret;
            delay_peak = dsp_max(delay_peak, dsp_max(dsp_abs(yl), dsp_abs(yr)));
        }
        fx->time = time; fx->dly_hp[0] = hp; fx->lp[0] = lp0; fx->lp[1] = lp1; fx->acc[0] = acc0; fx->acc[1] = acc1;
        fx->half = half; fx->dw = dw;
    }
    if (reverb_on) {
        float hp = fx->rev_hp[0], lfo[4], dmp[4], inc[4];
        unsigned rw = fx->rw, fw = fx->fw;
        for (int i = 0; i < 4; ++i) { lfo[i] = fx->lfo[i]; dmp[i] = fx->damp[i]; inc[i] = (0.23f + 0.17f * (float)i) / rev_rate; }
#if FX_REVERB_LFO_EVERY > 1
        unsigned lfo_cnt = fx->lfo_cnt, back[4];
        float hfrac[4];
        for (int i = 0; i < 4; ++i) { back[i] = fx->lfo_back[i]; hfrac[i] = fx->lfo_frac[i]; }
#endif
#if FX_REVERB_HALF
        float acc = fx->rev_acc, o00 = fx->rev_out[0][0], o01 = fx->rev_out[0][1], o10 = fx->rev_out[1][0], o11 = fx->rev_out[1][1];
        unsigned half = fx->rev_half;
#endif
        for (int n = 0; n < frames; ++n) {
            /* High-passed send (about 300 Hz): a clean low end under the hall. */
            hp += (rev[n] - hp) * 0.0418f;
            float x = rev[n] - hp;
#if FX_REVERB_HALF
            acc += x;
            half ^= 1u;
            if (half) {   /* the frame between: halfway to the last output */
                left[n] += 0.5f * (o00 + o01) * rret;
                right[n] += 0.5f * (o10 + o11) * rret;
                continue;
            }
            x = acc * 0.5f;
            acc = 0.0f;
#endif
EL_UNROLL
            for (int d = 0; d < 2; ++d) {
                float delayed = fx->mem.b.diff[d][(fw - dsize[d]) & (FX_DIFF_CAP - 1)] * (1.0f / FX_LINE_SCALE);
                float w = x + diffusion * delayed;
                fx->mem.b.diff[d][fw & (FX_DIFF_CAP - 1)] = line_store(w);
                x = delayed - diffusion * w;
            }
            float y[4], sum = 0;
#if FX_REVERB_LFO_EVERY > 1
            int lfo_now = lfo_cnt == 0;
            lfo_cnt = lfo_cnt + 1u < FX_REVERB_LFO_EVERY ? lfo_cnt + 1u : 0u;
#endif
EL_UNROLL
            for (int i = 0; i < 4; ++i) {
#if FX_REVERB_LFO_EVERY > 1
                int k;
                float frac;
                if (lfo_now) {
                    lfo[i] += inc[i] * (float)FX_REVERB_LFO_EVERY;
                    if (lfo[i] >= 1.0f) lfo[i] -= 1.0f;
                    float p = lfo[i], tri = p < 0.5f ? 4 * p - 1 : 3 - 4 * p;
                    float pos = (float)rw - (lengths[i] + mod * tri);
                    k = (int)pos;
                    if ((float)k > pos) --k;
                    frac = pos - (float)k;
                    back[i] = rw - (unsigned)k;   /* (held: the line read this far behind the write) */
                    hfrac[i] = frac;
                } else {
                    k = (int)(rw - back[i]);
                    frac = hfrac[i];
                }
#else
                lfo[i] += inc[i];
                if (lfo[i] >= 1.0f) lfo[i] -= 1.0f;
                float p = lfo[i], tri = p < 0.5f ? 4 * p - 1 : 3 - 4 * p;
                float pos = (float)rw - (lengths[i] + mod * tri);
                int k = (int)pos;
                if ((float)k > pos) --k;
                float frac = pos - (float)k;
#endif
                float a = fx->mem.b.lines[i][(unsigned)k & (FX_REVERB_CAP - 1)] * (1.0f / FX_LINE_SCALE);
                float b = fx->mem.b.lines[i][(unsigned)(k + 1) & (FX_REVERB_CAP - 1)] * (1.0f / FX_LINE_SCALE);
                dmp[i] += (a + (b - a) * frac - dmp[i]) * damp;
                y[i] = dmp[i];
                sum += y[i] * gains[i];
            }
            sum *= 0.5f; /* Householder: 2 / N */
EL_UNROLL
            for (int i = 0; i < 4; ++i)
                fx->mem.b.lines[i][rw & (FX_REVERB_CAP - 1)] = line_store(y[i] * gains[i] - sum + x * ((i & 1) ? -0.5f : 0.5f));
            rw = (rw + 1u) & (FX_REVERB_CAP - 1);
            fw = (fw + 1u) & (FX_DIFF_CAP - 1);
#if FX_REVERB_HALF
            o00 = o01;
            o10 = o11;
            o01 = 0.6f * (y[0] - y[2] + 0.5f * y[1]);
            o11 = 0.6f * (y[1] - y[3] + 0.5f * y[0]);
            left[n] += o01 * rret;
            right[n] += o11 * rret;
#else
            left[n] += 0.6f * (y[0] - y[2] + 0.5f * y[1]) * rret;
            right[n] += 0.6f * (y[1] - y[3] + 0.5f * y[0]) * rret;
#endif
            reverb_peak = dsp_max(reverb_peak, dsp_abs(y[0]) + dsp_abs(y[1]) + dsp_abs(y[2]) + dsp_abs(y[3]));
        }
        fx->rev_hp[0] = hp; fx->rw = rw; fx->fw = fw;
#if FX_REVERB_LFO_EVERY > 1
        fx->lfo_cnt = (uint8_t)lfo_cnt;
        for (int i = 0; i < 4; ++i) { fx->lfo_back[i] = back[i]; fx->lfo_frac[i] = hfrac[i]; }
#endif
        for (int i = 0; i < 4; ++i) { fx->lfo[i] = lfo[i]; fx->damp[i] = dmp[i]; }
#if FX_REVERB_HALF
        fx->rev_acc = acc; fx->rev_half = (uint8_t)half;
        fx->rev_out[0][0] = o00; fx->rev_out[0][1] = o01; fx->rev_out[1][0] = o10; fx->rev_out[1][1] = o11;
#endif
    }
#if FX_GATE_DENSE == 2
    /* (as delay and hall: the states in locals; the same arithmetic) */
    if (gate_on) {
        float hp = fx->gate_hp[0], acc = fx->gate_acc, lp0 = fx->gate_lp[0], lp1 = fx->gate_lp[1];
        float ef = fx->gate_ef, es = fx->gate_es, gain = fx->gate_gain, dmp[4];
        unsigned half = fx->gate_half, hold = fx->gate_hold, gw = fx->gw, gfw = fx->gfw;
        for (int i = 0; i < 4; ++i) dmp[i] = fx->gate_damp[i];
        int16_t *line = fx->mem.b.gate;
        for (int n = 0; n < frames; ++n) {
            /* Gated reverb: diffused input at half rate, flat tap cluster. */
            hp += (gate[n] - hp) * 0.0418f;
            acc += gate[n] - hp;
            half ^= 1u;
            if (!half) {
                float x = acc * 0.5f;
                acc = 0;
                float in = x;   /* before the diffusers: the gate's onset detector */
                static const unsigned sizes[2] = {142, 107};
                const float k = 0.6f;
EL_UNROLL
                for (int d = 0; d < FX_GATE_DIFFS; ++d) {
                    float delayed = fx->gate_diff[d][(gfw - sizes[d]) & 255u] * (1.0f / FX_LINE_SCALE);
                    float w = x + k * delayed;
                    fx->gate_diff[d][gfw & 255u] = line_store(w);
                    x = delayed - k * w;
                }
                gfw = (gfw + 1u) & 255u;
                float a = dsp_abs(in);
                ef += (a - ef) * 0.045f;    /* ~1 ms */
                es += (a - es) * 0.0009f;   /* ~50 ms */
                if (ef > 3.0f * es + 2e-3f && hold + GATE_REFRACT < GATE_HOLD) hold = GATE_HOLD;
                float target = hold ? 1.0f : 0.0f;
                if (hold) --hold;
                gain += (target - gain) * (target > gain ? 0.045f : 0.0035f);
                unsigned w = gw & (GATE_LINE - 1u);
                float y[4];
EL_UNROLL
                for (int i = 0; i < 4; ++i) {
                    float r = line[i * GATE_LINE + ((w - gate_len[i]) & (GATE_LINE - 1u))] * (1.0f / 8191.0f);
                    dmp[i] += (r - dmp[i]) * 0.8f;   /* ~5.6 kHz per pass */
                    y[i] = dmp[i];
                }
                float g = 0.5f * FX_GATE_FB * gain;
                float h[4] = {y[0] + y[1] + y[2] + y[3], y[0] - y[1] + y[2] - y[3], y[0] + y[1] - y[2] - y[3], y[0] - y[1] - y[2] + y[3]};
EL_UNROLL
                for (int i = 0; i < 4; ++i)
                    line[i * GATE_LINE + w] = (int16_t)(dsp_clamp((i & 1 ? -x : x) + g * h[i], -3.9998f, 3.9998f) * 8191.0f);
                float gl = (y[0] - y[2]) * gain * FX_GATE_OUT;
                float gr = (y[1] - y[3]) * gain * FX_GATE_OUT;
                gw = (gw + 1u) % FX_GATE_CAP;
                /* Gentle low-pass (about 9 kHz at the half rate) on the return. */
                lp0 += (gl - lp0) * 0.7f;
                lp1 += (gr - lp1) * 0.7f;
                gate_peak = dsp_max(gate_peak, dsp_abs(gl) + dsp_abs(gr) + dsp_abs(x));
            }
            left[n] += lp0 * 0.5f;
            right[n] += lp1 * 0.5f;
        }
        fx->gate_hp[0] = hp; fx->gate_acc = acc; fx->gate_lp[0] = lp0; fx->gate_lp[1] = lp1;
        fx->gate_ef = ef; fx->gate_es = es; fx->gate_gain = gain;
        fx->gate_half = (uint8_t)half; fx->gate_hold = (uint16_t)hold; fx->gw = gw; fx->gfw = gfw;
        for (int i = 0; i < 4; ++i) fx->gate_damp[i] = dmp[i];
    }
#else
    if (gate_on)
        for (int n = 0; n < frames; ++n) {
            /* Gated reverb: diffused input at half rate, flat tap cluster. */
            fx->gate_hp[0] += (gate[n] - fx->gate_hp[0]) * 0.0418f;
            fx->gate_acc += gate[n] - fx->gate_hp[0];
            fx->gate_half ^= 1u;
            if (!fx->gate_half) {
                float x = fx->gate_acc * 0.5f;
                fx->gate_acc = 0;
    #if FX_GATE_DENSE == 2
                float in = x;   /* before the diffusers: the gate's onset detector */
    #endif
    #if FX_GATE_DENSE == 1
                /* 10.9 / 9.0 / 7.1 / 5.1 ms: the taps smear into one surface */
                static const unsigned sizes[4] = {241, 199, 157, 113};
                const float k = FX_GATE_K;
    #else
                static const unsigned sizes[2] = {142, 107};
                const float k = 0.6f;
    #endif
                for (int d = 0; d < FX_GATE_DIFFS; ++d) {
                    float delayed = fx->gate_diff[d][(fx->gfw - sizes[d]) & 255u] * (1.0f / FX_LINE_SCALE);
                    float w = x + k * delayed;
                    fx->gate_diff[d][fx->gfw & 255u] = line_store(w);
                    x = delayed - k * w;
                }
                fx->gfw = (fx->gfw + 1u) & 255u;
                float gl = 0, gr = 0;
    #if FX_GATE_DENSE == 2
                {
                    float a = dsp_abs(in);
                    fx->gate_ef += (a - fx->gate_ef) * 0.045f;    /* ~1 ms */
                    fx->gate_es += (a - fx->gate_es) * 0.0009f;   /* ~50 ms */
                    if (fx->gate_ef > 3.0f * fx->gate_es + 2e-3f && fx->gate_hold + GATE_REFRACT < GATE_HOLD) fx->gate_hold = GATE_HOLD;
                    float target = fx->gate_hold ? 1.0f : 0.0f;
                    if (fx->gate_hold) --fx->gate_hold;
                    fx->gate_gain += (target - fx->gate_gain) * (target > fx->gate_gain ? 0.045f : 0.0035f);
                    int16_t *line = fx->mem.b.gate;
                    unsigned w = fx->gw & (GATE_LINE - 1u);
                    float y[4];
                    for (int i = 0; i < 4; ++i) {
                        float r = line[i * GATE_LINE + ((w - gate_len[i]) & (GATE_LINE - 1u))] * (1.0f / 8191.0f);
                        fx->gate_damp[i] += (r - fx->gate_damp[i]) * 0.8f;   /* ~5.6 kHz per pass */
                        y[i] = fx->gate_damp[i];
                    }
                    float g = 0.5f * FX_GATE_FB * fx->gate_gain;
                    float h[4] = {y[0] + y[1] + y[2] + y[3], y[0] - y[1] + y[2] - y[3], y[0] + y[1] - y[2] - y[3], y[0] - y[1] - y[2] + y[3]};
                    for (int i = 0; i < 4; ++i)
                        line[i * GATE_LINE + w] = (int16_t)(dsp_clamp((i & 1 ? -x : x) + g * h[i], -3.9998f, 3.9998f) * 8191.0f);
                    gl = (y[0] - y[2]) * fx->gate_gain * FX_GATE_OUT;
                    gr = (y[1] - y[3]) * fx->gate_gain * FX_GATE_OUT;
                }
    #elif FX_GATE_DENSE == 1
                fx->mem.b.gate[fx->gw] = (int16_t)(dsp_clamp(x, -1, 1) * 32767.0f);
                for (int t = 0; t < GATE_DENSE_TAPS; ++t) {
                    float s = gate_read(fx->mem.b.gate, fx->gw, gate_dense_taps[t]) * gate_dense_gains[t];
                    if (gate_dense_right[t]) gr += s; else gl += s;
                }
    #else
                fx->mem.b.gate[fx->gw] = (int16_t)(dsp_clamp(x, -1, 1) * 32767.0f);
                for (int t = 0; t < 15; ++t) {
                    float s = gate_read(fx->mem.b.gate, fx->gw, gate_taps[t]) * gate_gains[t];
                    if (t & 1) gr += s; else gl += s;
                }
    #endif
                fx->gw = (fx->gw + 1u) % FX_GATE_CAP;
                /* Gentle low-pass (about 9 kHz at the half rate) on the return. */
                fx->gate_lp[0] += (gl - fx->gate_lp[0]) * 0.7f;
                fx->gate_lp[1] += (gr - fx->gate_lp[1]) * 0.7f;
                gate_peak = dsp_max(gate_peak, dsp_abs(gl) + dsp_abs(gr) + dsp_abs(x));
            }
            left[n] += fx->gate_lp[0] * 0.5f;
            right[n] += fx->gate_lp[1] * 0.5f;
        }
#endif
    /* Quiet long enough: longer than the longest line, so nothing is still
     * on its way through. */
    const float quiet = 1e-6f;   /* -120 dB */
    fx->delay_quiet = !delay_on || dly_in || delay_peak >= quiet ? 0 : (uint16_t)dsp_min(fx->delay_quiet + frames, 60000.0f);
    fx->reverb_quiet = !reverb_on || rev_in || reverb_peak >= quiet ? 0 : (uint16_t)dsp_min(fx->reverb_quiet + frames, 60000.0f);
    fx->gate_quiet = !gate_on || gate_in || gate_peak >= quiet || dsp_abs(fx->gate_lp[0]) + dsp_abs(fx->gate_lp[1]) >= quiet
                         ? 0 : (uint16_t)dsp_min(fx->gate_quiet + frames, 60000.0f);
    if (fx->delay_quiet > 3u * FX_DELAY_CAP + 512u) {
        fx->delay_sleep = 1;
        fx->delay_quiet = 0;
        memset(fx->mem.b.delay, 0, sizeof fx->mem.b.delay);
        fx->lp[0] = fx->lp[1] = fx->acc[0] = fx->acc[1] = fx->dly_hp[0] = 0;
    }
    if (fx->reverb_quiet > FX_REVERB_CAP + 2u * FX_DIFF_CAP + 512u) {
        fx->reverb_sleep = 1;
        fx->reverb_quiet = 0;
        memset(fx->mem.b.lines, 0, sizeof fx->mem.b.lines);
        memset(fx->mem.b.diff, 0, sizeof fx->mem.b.diff);
        for (int i = 0; i < 4; ++i) fx->damp[i] = 0;
        fx->rev_hp[0] = 0;
        fx->rev_acc = 0;
        fx->rev_out[0][0] = fx->rev_out[0][1] = fx->rev_out[1][0] = fx->rev_out[1][1] = 0;
    }
    if (fx->gate_quiet > 2u * FX_GATE_CAP + 1024u) {
        fx->gate_sleep = 1;
        fx->gate_quiet = 0;
        memset(fx->mem.b.gate, 0, sizeof fx->mem.b.gate);
        memset(fx->gate_diff, 0, sizeof fx->gate_diff);
        fx->gate_hp[0] = fx->gate_acc = fx->gate_lp[0] = fx->gate_lp[1] = 0;
        fx->gate_ef = fx->gate_es = fx->gate_gain = 0;
        fx->gate_hold = 0;
        for (int i = 0; i < 4; ++i) fx->gate_damp[i] = 0;
    }
}

EL_DSP_CODE int fx_distortion_process(el_fx_t *fx, float *send, int frames)
{
    if (!fx->v[FX_DIST_SEND_BEAT] && !fx->v[FX_DIST_SEND_RHYTHM] && !fx->v[FX_DIST_SEND_STRINGS] && !fx->v[FX_DIST_SEND_BASS])
        return 0;
    /* Wet only (the dry signal stays in the mix): parallel distortion. With
     * the type OFF nothing comes back: the distortion's own bypass would
     * return the send itself, a few samples late, and comb the mix. Switching
     * OFF fades the last type out in about 10 ms. */
    int mode = fx->v[FX_DIST_MODE];
    if (mode) fx->dist_last = (uint8_t)mode;
    if (!mode && fx->dist_fade <= 0.0f) return 0;
    dist_process(&fx->dist, send, fx->dist_r, frames, mode ? mode : (fx->dist_last ? fx->dist_last : 1), fx->v[FX_DIST_DRIVE],
                 fx->v[FX_DIST_TONE], 64, 127, 0);
    return 1;
}

EL_DSP_CODE void fx_distortion_mix(el_fx_t *fx, const float *wet, float *left, float *right, int frames)
{
    float target = fx->v[FX_DIST_MODE] ? 1.0f : 0.0f, step = 1.0f / (0.01f * EL_SAMPLE_RATE);
    for (int i = 0; i < frames; ++i) {
        fx->dist_fade = dsp_clamp(fx->dist_fade + (target > fx->dist_fade ? step : -step), 0.0f, 1.0f);
        left[i] += wet[i] * fx->dist_fade;
        right[i] += wet[i] * fx->dist_fade;
    }
}

EL_DSP_CODE void fx_distortion(el_fx_t *fx, float *send, float *left, float *right, int frames)
{
    if (fx_distortion_process(fx, send, frames)) fx_distortion_mix(fx, send, left, right, frames);
}
