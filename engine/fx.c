/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
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

static inline float soft(float x)
{
    x = dsp_clamp(x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

/* Linear read from a third-rate 16-bit line, position in third-rate frames. */
static inline float delay_read(const int16_t *line, unsigned write, float back)
{
    float pos = (float)write - back;
    int i = (int)pos;
    if ((float)i > pos) --i;
    float frac = pos - (float)i;
    i %= FX_DELAY_CAP;
    if (i < 0) i += FX_DELAY_CAP;
    float a = line[i], b = line[i + 1 < FX_DELAY_CAP ? i + 1 : 0];
    return (a + (b - a) * frac) * (1.0f / 32767.0f);
}

/* Non-linear reverb taps: 16 echoes from 20 to 330 ms at nearly flat level,
 * then nothing - the classic gated 80s / electro snare and clap. Positions
 * in half-rate frames, alternating sides. */
#if !FX_GATE_DENSE
static const uint16_t gate_taps[16] EL_DSP_DATA = {441, 783, 1102, 1433, 1764, 2117, 2448, 2800,
                                                    3131, 3484, 3815, 4167, 4498, 4851, 5182, 7277};
static const float gate_gains[16] EL_DSP_DATA = {0.62f, 0.58f, 0.6f, 0.55f, 0.57f, 0.53f, 0.55f, 0.5f,
                                                  0.52f, 0.47f, 0.48f, 0.42f, 0.4f, 0.32f, 0.22f, 0.0f};
#else
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
#endif

/* Reverb lines and diffusers in 16 bit (truncated: symmetric, no DC in
 * the loops), with headroom. */
static inline EL_DSP_HELPER int16_t line_store(float x)
{
    return (int16_t)(dsp_clamp(x, -3.9998f, 3.9998f) * FX_LINE_SCALE);
}

static inline EL_DSP_HELPER float gate_read(const int16_t *line, unsigned write, unsigned back)
{
    return line[(write + FX_GATE_CAP - back) % FX_GATE_CAP] * (1.0f / 32767.0f);
}

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
}

/* 16-bit lines at a quarter of full scale: room for the tank's peaks. */
static inline float hole_get(const int16_t *m, const fx_line_t *l, float back)
{
    float p = (float)l->pos - back;
    while (p < 0.0f) p += (float)l->len;
    unsigned i = (unsigned)p;
    float f = p - (float)i, a = m[l->off + i % l->len], b = m[l->off + (i + 1u) % l->len];
    return (a + (b - a) * f) * (4.0f / 32767.0f);
}

static inline void hole_put(int16_t *m, fx_line_t *l, float x)
{
    m[l->off + l->pos] = (int16_t)(dsp_clamp(x * 0.25f, -1.0f, 1.0f) * 32767.0f);
    l->pos = l->pos + 1u < l->len ? l->pos + 1u : 0u;
}

static inline float hole_allpass(int16_t *m, fx_line_t *l, float x, float g, float back)
{
    float d = hole_get(m, l, back), w = x + g * d;
    hole_put(m, l, w);
    return d - g * w;
}

static inline float hole_tap(const int16_t *m, const fx_hole_t *h, int t, int line, int back)
{
    const fx_line_t *l = line == 0 ? &h->dl1[t] : line == 1 ? &h->ap2[t] : &h->dl2[t];
    return hole_get(m, l, (float)back * HOLE_SCALE);
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
                    y += hole_tap(m, h, hole_taps[s][k][0], hole_taps[s][k][1], hole_taps[s][k][2]) * (float)hole_taps[s][k][3];
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
    float damp = 1.0f - dsp_exp(-2.0f * DSP_PI * damp_hz / EL_SAMPLE_RATE);
    float diffusion = 0.55f + 0.17f * size, mod = 1.5f + 8.0f * size;
    unsigned dsize[2] = {(unsigned)(diff_lengths[0] * (0.5f + size)), (unsigned)(diff_lengths[1] * (0.5f + size))};
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
    for (int n = 0; n < frames; ++n) {
        if (delay_on) {
            fx->time += (target - fx->time) * 0.0002f;
            /* Read between the stored frames by the step within the current
             * one, so the echoes are interpolated, not held. */
            float back = fx->time - (float)fx->half * (1.0f / 3.0f);
            float yl = delay_read(fx->mem.b.delay[0], fx->dw, back), yr = delay_read(fx->mem.b.delay[1], fx->dw, back);
            /* Ping-pong: the input enters left, each side feeds the other.
             * The send is high-passed (about 200 Hz) to keep the echoes off
             * the kick and bass. */
            fx->dly_hp[0] += (dly[n] - fx->dly_hp[0]) * 0.0281f;
            float in = dly[n] - fx->dly_hp[0];
            float wl = soft(in + fb * yr), wr = soft(fb * yl);
            fx->lp[0] += (wl - fx->lp[0]) * tone;
            fx->lp[1] += (wr - fx->lp[1]) * tone;
            fx->acc[0] += fx->lp[0];
            fx->acc[1] += fx->lp[1];
            if (++fx->half >= 3u) { /* every third frame: store the averaged three */
                fx->half = 0;
                fx->mem.b.delay[0][fx->dw] = (int16_t)(dsp_clamp(fx->acc[0] * (1.0f / 3.0f), -1, 1) * 32767.0f);
                fx->mem.b.delay[1][fx->dw] = (int16_t)(dsp_clamp(fx->acc[1] * (1.0f / 3.0f), -1, 1) * 32767.0f);
                fx->acc[0] = fx->acc[1] = 0;
                fx->dw = (fx->dw + 1u) % FX_DELAY_CAP;
            }
            left[n] += yl * dret;
            right[n] += yr * dret;
            delay_peak = dsp_max(delay_peak, dsp_max(dsp_abs(yl), dsp_abs(yr)));
        }
        if (reverb_on) {
            /* High-passed send (about 300 Hz): a clean low end under the hall. */
            fx->rev_hp[0] += (rev[n] - fx->rev_hp[0]) * 0.0418f;
            float x = rev[n] - fx->rev_hp[0];
            for (int d = 0; d < 2; ++d) {
                float delayed = fx->mem.b.diff[d][(fx->fw - dsize[d]) & (FX_DIFF_CAP - 1)] * (1.0f / FX_LINE_SCALE);
                float w = x + diffusion * delayed;
                fx->mem.b.diff[d][fx->fw & (FX_DIFF_CAP - 1)] = line_store(w);
                x = delayed - diffusion * w;
            }
            float y[4], sum = 0;
            for (int i = 0; i < 4; ++i) {
                fx->lfo[i] += (0.23f + 0.17f * (float)i) / EL_SAMPLE_RATE;
                if (fx->lfo[i] >= 1.0f) fx->lfo[i] -= 1.0f;
                float p = fx->lfo[i], tri = p < 0.5f ? 4 * p - 1 : 3 - 4 * p;
                float pos = (float)fx->rw - (lengths[i] + mod * tri);
                int k = (int)pos;
                if ((float)k > pos) --k;
                float frac = pos - (float)k;
                float a = fx->mem.b.lines[i][(unsigned)k & (FX_REVERB_CAP - 1)] * (1.0f / FX_LINE_SCALE);
                float b = fx->mem.b.lines[i][(unsigned)(k + 1) & (FX_REVERB_CAP - 1)] * (1.0f / FX_LINE_SCALE);
                fx->damp[i] += (a + (b - a) * frac - fx->damp[i]) * damp;
                y[i] = fx->damp[i];
                sum += y[i] * gains[i];
            }
            sum *= 0.5f; /* Householder: 2 / N */
            for (int i = 0; i < 4; ++i)
                fx->mem.b.lines[i][fx->rw & (FX_REVERB_CAP - 1)] = line_store(y[i] * gains[i] - sum + x * ((i & 1) ? -0.5f : 0.5f));
            fx->rw = (fx->rw + 1u) & (FX_REVERB_CAP - 1);
            fx->fw = (fx->fw + 1u) & (FX_DIFF_CAP - 1);
            left[n] += 0.6f * (y[0] - y[2] + 0.5f * y[1]) * rret;
            right[n] += 0.6f * (y[1] - y[3] + 0.5f * y[0]) * rret;
            reverb_peak = dsp_max(reverb_peak, dsp_abs(y[0]) + dsp_abs(y[1]) + dsp_abs(y[2]) + dsp_abs(y[3]));
        }
        if (!gate_on) continue;
        /* Gated reverb: diffused input at half rate, flat tap cluster. */
        fx->gate_hp[0] += (gate[n] - fx->gate_hp[0]) * 0.0418f;
        fx->gate_acc += gate[n] - fx->gate_hp[0];
        fx->gate_half ^= 1u;
        if (!fx->gate_half) {
            float x = fx->gate_acc * 0.5f;
            fx->gate_acc = 0;
#if FX_GATE_DENSE
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
            fx->mem.b.gate[fx->gw] = (int16_t)(dsp_clamp(x, -1, 1) * 32767.0f);
            float gl = 0, gr = 0;
#if FX_GATE_DENSE
            for (int t = 0; t < GATE_DENSE_TAPS; ++t) {
                float s = gate_read(fx->mem.b.gate, fx->gw, gate_dense_taps[t]) * gate_dense_gains[t];
                if (gate_dense_right[t]) gr += s; else gl += s;
            }
#else
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
    }
    if (fx->gate_quiet > 2u * FX_GATE_CAP + 1024u) {
        fx->gate_sleep = 1;
        fx->gate_quiet = 0;
        memset(fx->mem.b.gate, 0, sizeof fx->mem.b.gate);
        memset(fx->gate_diff, 0, sizeof fx->gate_diff);
        fx->gate_hp[0] = fx->gate_acc = fx->gate_lp[0] = fx->gate_lp[1] = 0;
    }
}

EL_DSP_CODE void fx_distortion(el_fx_t *fx, float *send, float *left, float *right, int frames)
{
    if (!fx->v[FX_DIST_SEND_BEAT] && !fx->v[FX_DIST_SEND_RHYTHM] && !fx->v[FX_DIST_SEND_STRINGS] && !fx->v[FX_DIST_SEND_BASS])
        return;
    /* Wet only (the dry signal stays in the mix): parallel distortion. With
     * the type OFF nothing comes back: the distortion's own bypass would
     * return the send itself, a few samples late, and comb the mix. Switching
     * OFF fades the last type out in about 10 ms. */
    int mode = fx->v[FX_DIST_MODE];
    if (mode) fx->dist_last = (uint8_t)mode;
    if (!mode && fx->dist_fade <= 0.0f) return;
    dist_process(&fx->dist, send, fx->dist_r, frames, mode ? mode : (fx->dist_last ? fx->dist_last : 1), fx->v[FX_DIST_DRIVE],
                 fx->v[FX_DIST_TONE], 64, 127, 0);
    float target = mode ? 1.0f : 0.0f, step = 1.0f / (0.01f * EL_SAMPLE_RATE);
    for (int i = 0; i < frames; ++i) {
        fx->dist_fade = dsp_clamp(fx->dist_fade + (target > fx->dist_fade ? step : -step), 0.0f, 1.0f);
        left[i] += send[i] * fx->dist_fade;
        right[i] += send[i] * fx->dist_fade;
    }
}
