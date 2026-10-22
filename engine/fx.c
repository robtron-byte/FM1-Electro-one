/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Device build: all code and constants of this file run in internal SRAM,
 * like FM1 Open's own DSP (safe while Flash is written). */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
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
static const char *const names[FX_PARAMS] = {"TIME", "FEEDBACK", "TONE", "DLY RET", "SIZE", "DECAY", "DAMP", "REV RET"};
static const float base_lengths[4] EL_DSP_DATA = {1116, 1277, 1422, 1557};
static const unsigned diff_lengths[2] EL_DSP_DATA = {379, 277};

void fx_init(el_fx_t *fx)
{
    static const uint8_t defaults[FX_PARAMS] = {6, 52, 70, 100, 70, 72, 50, 100};
    memset(fx, 0, sizeof *fx);
    memcpy(fx->v, defaults, sizeof defaults);
    fx->lfo[1] = 0.25f; fx->lfo[2] = 0.5f; fx->lfo[3] = 0.75f;
}

int fx_param_max(unsigned param) { return param == FX_DELAY_DIV ? 11 : param < FX_PARAMS ? 127 : 0; }
const char *fx_param_name(unsigned param) { return param < FX_PARAMS ? names[param] : ""; }
const char *fx_delay_division_name(int value) { return division_names[el_clampi(value, 0, 11)]; }

static inline float soft(float x)
{
    x = dsp_clamp(x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

/* Linear read from a half-rate 16-bit line, position in half-rate frames. */
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
static const uint16_t gate_taps[16] EL_DSP_DATA = {441, 783, 1102, 1433, 1764, 2117, 2448, 2800,
                                                    3131, 3484, 3815, 4167, 4498, 4851, 5182, 7277};
static const float gate_gains[16] EL_DSP_DATA = {0.62f, 0.58f, 0.6f, 0.55f, 0.57f, 0.53f, 0.55f, 0.5f,
                                                  0.52f, 0.47f, 0.48f, 0.42f, 0.4f, 0.32f, 0.22f, 0.0f};

static inline float gate_read(const int16_t *line, unsigned write, unsigned back)
{
    return line[(write + FX_GATE_CAP - back) % FX_GATE_CAP] * (1.0f / 32767.0f);
}

EL_DSP_CODE void fx_render(el_fx_t *fx, const float *dly, const float *rev, const float *gate,
                           float *left, float *right, int frames, float bpm)
{
    const uint8_t *v = fx->v;
    /* Delay: tempo-synced time, glided so tempo changes bend instead of click. */
    float seconds = 60.0f / (bpm < 30 ? 30 : bpm) * quarters[v[FX_DELAY_DIV] < 12 ? v[FX_DELAY_DIV] : 11];
    float target = dsp_clamp(seconds * EL_SAMPLE_RATE * 0.5f, 8.0f, FX_DELAY_CAP - 4.0f);
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

    for (int n = 0; n < frames; ++n) {
        if (delay_on) {
            fx->time += (target - fx->time) * 0.0002f;
            float yl = delay_read(fx->delay[0], fx->dw, fx->time), yr = delay_read(fx->delay[1], fx->dw, fx->time);
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
            fx->half ^= 1u;
            if (!fx->half) { /* every second frame: store the averaged pair */
                fx->delay[0][fx->dw] = (int16_t)(dsp_clamp(fx->acc[0] * 0.5f, -1, 1) * 32767.0f);
                fx->delay[1][fx->dw] = (int16_t)(dsp_clamp(fx->acc[1] * 0.5f, -1, 1) * 32767.0f);
                fx->acc[0] = fx->acc[1] = 0;
                fx->dw = (fx->dw + 1u) % FX_DELAY_CAP;
            }
            left[n] += yl * dret;
            right[n] += yr * dret;
        }
        if (reverb_on) {
            /* High-passed send (about 300 Hz): a clean low end under the hall. */
            fx->rev_hp[0] += (rev[n] - fx->rev_hp[0]) * 0.0418f;
            float x = rev[n] - fx->rev_hp[0];
            for (int d = 0; d < 2; ++d) {
                float delayed = fx->diff[d][(fx->fw - dsize[d]) & (FX_DIFF_CAP - 1)];
                float w = x + diffusion * delayed;
                fx->diff[d][fx->fw & (FX_DIFF_CAP - 1)] = w;
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
                float a = fx->lines[i][(unsigned)k & (FX_REVERB_CAP - 1)];
                float b = fx->lines[i][(unsigned)(k + 1) & (FX_REVERB_CAP - 1)];
                fx->damp[i] += (a + (b - a) * frac - fx->damp[i]) * damp;
                y[i] = fx->damp[i];
                sum += y[i] * gains[i];
            }
            sum *= 0.5f; /* Householder: 2 / N */
            for (int i = 0; i < 4; ++i)
                fx->lines[i][fx->rw & (FX_REVERB_CAP - 1)] = y[i] * gains[i] - sum + x * ((i & 1) ? -0.5f : 0.5f);
            fx->rw = (fx->rw + 1u) & (FX_REVERB_CAP - 1);
            fx->fw = (fx->fw + 1u) & (FX_DIFF_CAP - 1);
            left[n] += 0.6f * (y[0] - y[2] + 0.5f * y[1]) * rret;
            right[n] += 0.6f * (y[1] - y[3] + 0.5f * y[0]) * rret;
        }
        /* Gated reverb: diffused input at half rate, flat tap cluster. */
        fx->gate_hp[0] += (gate[n] - fx->gate_hp[0]) * 0.0418f;
        fx->gate_acc += gate[n] - fx->gate_hp[0];
        fx->gate_half ^= 1u;
        if (!fx->gate_half) {
            float x = fx->gate_acc * 0.5f;
            fx->gate_acc = 0;
            static const unsigned sizes[2] = {142, 107};
            for (int d = 0; d < 2; ++d) {
                float delayed = fx->gate_diff[d][(fx->gfw - sizes[d]) & 255u];
                float w = x + 0.6f * delayed;
                fx->gate_diff[d][fx->gfw & 255u] = w;
                x = delayed - 0.6f * w;
            }
            fx->gfw = (fx->gfw + 1u) & 255u;
            fx->gate[fx->gw] = (int16_t)(dsp_clamp(x, -1, 1) * 32767.0f);
            float gl = 0, gr = 0;
            for (int t = 0; t < 15; ++t) {
                float s = gate_read(fx->gate, fx->gw, gate_taps[t]) * gate_gains[t];
                if (t & 1) gr += s; else gl += s;
            }
            fx->gw = (fx->gw + 1u) % FX_GATE_CAP;
            /* Gentle low-pass (about 9 kHz at the half rate) on the return. */
            fx->gate_lp[0] += (gl - fx->gate_lp[0]) * 0.7f;
            fx->gate_lp[1] += (gr - fx->gate_lp[1]) * 0.7f;
        }
        left[n] += fx->gate_lp[0] * 0.5f;
        right[n] += fx->gate_lp[1] * 0.5f;
    }
}
