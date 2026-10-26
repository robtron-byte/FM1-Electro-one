/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "el_editor.h"
#include "el_fmt.h"
#include <string.h>
#include "dsp_math.h"
#include <stdio.h>

#define N ED_NONE
enum { BACK = 0x0000, GRID = 0x2945, TEXT = 0xef5d, DIM = 0x738e, FAINT = 0x4208, PLOT = 0x0861 };

/* MKS-50: the Alpha Juno's panel order. The aftertouch depths are left out
 * (the FM-1 has no aftertouch). */
static const ed_section_t mks_sections[] = {
    {"DCO", "DCO", ED_LIST, 16, {MP_RANGE, MP_SAW, MP_PULSE, MP_SUB, MP_SUB_LEVEL, MP_NOISE, MP_PW, MP_PWM_RATE,
                                 MP_DCO_LFO, MP_DCO_ENV, MP_DCO_ENV_MODE, MP_PULSE_DETUNE, MP_PULSE_SYNC, MP_PULSE_ENV,
                                 MP_DRIFT, MP_BENDER}},
    {"VCF", "VCF", ED_VCF, 7, {MP_CUTOFF, MP_RESO, MP_VCF_ENV, MP_VCF_LFO, MP_HPF, MP_VCF_KEY, MP_VCF_ENV_MODE}},
    {"VCA", "VCA", ED_LIST, 3, {MP_VCA_LEVEL, MP_VCA_ENV_MODE, MP_VOLUME}},
    {"ENVELOPE", "ENV", ED_ENV, 9, {MP_T1, MP_L1, MP_T2, MP_L2, MP_T3, MP_L3, MP_T4, MP_ENV_KEY, MP_ENV_RETRIG}},
    {"LFO", "LFO", ED_LFO, 6, {MP_LFO_RATE, MP_LFO_DELAY, MP_LFO_WAVE, MP_LFO_SYNC, MP_LFO_DIV, MP_LFO_KEYSYNC}},
    {"CHORUS", "CHO", ED_LIST, 2, {MP_CHORUS, MP_CHORUS_RATE}},
    {"KEY / PORTAMENTO", "KEY", ED_LIST, 6, {MP_KEY_MODE, MP_PORTA, MP_PORTA_TIME, MP_KEY_SHIFT, MP_UNISON_DETUNE, MP_CHORD}},
    {"EFFECT SENDS", "FX", ED_LIST, 2, {MP_DELAY_SEND, MP_REVERB_SEND}},
};

/* JP-6: the Jupiter-6 panel order. */
static const ed_section_t jp6_sections[] = {
    {"VCO-1", "VC1", ED_LIST, 8, {JP_VCO1_TRI, JP_VCO1_SAW, JP_VCO1_PULSE, JP_VCO1_SQUARE, JP_VCO1_RANGE, JP_XMOD,
                                  JP_XMOD_ENV, JP_SYNC}},
    {"VCO-2", "VC2", ED_LIST, 7, {JP_VCO2_TRI, JP_VCO2_SAW, JP_VCO2_PULSE, JP_VCO2_NOISE, JP_VCO2_RANGE, JP_VCO2_TUNE,
                                  JP_VCO2_FINE}},
    {"VCO MODULATOR", "MOD", ED_LIST, 6, {JP_PWM_DEPTH, JP_PWM_SOURCE, JP_VCO_LFO, JP_VCO_ENV, JP_VCO_MOD_DEST, JP_DRIFT}},
    {"VCF", "VCF", ED_VCF, 10, {JP_CUTOFF, JP_RESO, JP_VCF_ENV, JP_VCF_LFO, JP_VCF_MODE, JP_HPF, JP_MIX, JP_VCF_KEY,
                                JP_VCF_ENV_SRC, JP_VCF_VELO}},
    {"VCA", "VCA", ED_LIST, 4, {JP_VCA_LEVEL, JP_VCA_LFO, JP_VCA_VELO, JP_VOLUME}},
    {"ENVELOPE 1", "EN1", ED_ENV, 6, {JP_ENV1_A, JP_ENV1_D, JP_ENV1_S, JP_ENV1_R, JP_ENV1_KEY, JP_ENV1_POL}},
    {"ENVELOPE 2", "EN2", ED_ENV, 6, {JP_ENV2_A, JP_ENV2_D, JP_ENV2_S, JP_ENV2_R, JP_ENV2_KEY, JP_ENV_RETRIG}},
    {"LFO", "LFO", ED_LFO, 6, {JP_LFO_RATE, JP_LFO_DELAY, JP_LFO_WAVE, JP_LFO_SYNC, JP_LFO_DIV, JP_LFO_KEYSYNC}},
    {"KEY / PORTAMENTO", "KEY", ED_LIST, 6, {JP_KEY_MODE, JP_PORTA, JP_PORTA_TIME, JP_KEY_SHIFT, JP_UNISON_DETUNE, JP_CHORD}},
    {"CHORUS / DISTORTION", "FX", ED_LIST, 10, {JP_CHORUS, JP_CHORUS_RATE, JP_CHORUS_MODE, JP_DELAY_SEND, JP_DIST_MODE,
                                                JP_DIST_DRIVE, JP_DIST_TONE, JP_DIST_LEVEL, JP_DIST_MIX, JP_REVERB_SEND}},
};
#undef N

unsigned ed_sections(unsigned engine)
{
    return engine == SYN_JP6 ? sizeof jp6_sections / sizeof jp6_sections[0] : sizeof mks_sections / sizeof mks_sections[0];
}

const ed_section_t *ed_section(unsigned engine, unsigned section)
{
    unsigned n = ed_sections(engine);
    if (section >= n) section = n - 1u;
    return engine == SYN_JP6 ? &jp6_sections[section] : &mks_sections[section];
}

unsigned ed_param(unsigned engine, unsigned section, unsigned page, unsigned slot)
{
    const ed_section_t *s = ed_section(engine, section);
    unsigned i = page * 4u + slot;
    return i < s->count ? s->param[i] : ED_NONE;
}

/* ---- drawing ------------------------------------------------------------------ */

static void dot(gfx_band_t *b, int x, int y, uint16_t c) { gfx_fill(b, x, y, 2, 2, c); }

static void line(gfx_band_t *b, int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (int guard = 0; guard < 1000; ++guard) {
        dot(b, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static float val(const syn_part_t *p, unsigned param) { return syn_values(p)[param] / (float)dsp_max(1, syn_param_max(p->engine, param)); }

/* Envelope as stages: x widths follow the times, y the levels. */
static void draw_env(gfx_band_t *b, const syn_part_t *p, const ed_section_t *s, uint16_t c, int x, int y, int w, int h)
{
    float lv[5], tm[5];
    int stages;
    if (p->engine == SYN_JP6) {   /* A D (S) R */
        lv[0] = 1; lv[1] = val(p, s->param[2]); lv[2] = lv[1]; lv[3] = 0;
        tm[0] = val(p, s->param[0]); tm[1] = val(p, s->param[1]); tm[2] = -1; tm[3] = val(p, s->param[3]);
        stages = 4;
    } else {                      /* T1 L1 T2 L2 T3 L3 (sustain) T4 */
        lv[0] = val(p, MP_L1); lv[1] = val(p, MP_L2); lv[2] = val(p, MP_L3); lv[3] = lv[2]; lv[4] = 0;
        tm[0] = val(p, MP_T1); tm[1] = val(p, MP_T2); tm[2] = val(p, MP_T3); tm[3] = -1; tm[4] = val(p, MP_T4);
        stages = 5;
    }
    /* Width proportional to each time: a time of 0 is a straight vertical
     * edge (an attack of 0 jumps to full level). */
    float total = 0;
    for (int i = 0; i < stages; ++i) total += tm[i] < 0 ? 0.6f : tm[i];
    float px = (float)x, py = (float)(y + h - 1);
    for (int i = 0; i < stages; ++i) {
        float seg = (tm[i] < 0 ? 0.6f : tm[i]) / total * (float)w;
        float nx = px + seg, ny = (float)(y + h - 1) - lv[i] * (float)(h - 2);
        line(b, (int)px, (int)py, (int)nx, (int)ny, tm[i] < 0 ? DIM : c);
        px = nx; py = ny;
    }
}

float ed_vcf_lfo_octaves(const syn_part_t *p)
{
    return p->engine == SYN_JP6 ? syn_values(p)[JP_VCF_LFO] / 32.0f : syn_values(p)[MP_VCF_LFO] / 48.0f;
}

float ed_lfo_value(const syn_part_t *p)
{
    return p->engine == SYN_JP6 ? p->u.jp6.lfo_value : p->u.mks.lfo_value;
}

/* Filter response on a 20 Hz .. 20 kHz log axis, -36 .. +18 dB. */
static void draw_vcf(gfx_band_t *b, const syn_part_t *p, uint16_t c, int x, int y, int w, int h)
{
    int jp = p->engine == SYN_JP6;
    float cut = val(p, jp ? JP_CUTOFF : MP_CUTOFF), reso = val(p, jp ? JP_RESO : MP_RESO);
    unsigned mode = jp ? syn_values(p)[JP_VCF_MODE] : 0;
    /* JP-6 HPF 20 Hz .. 4 kHz; MKS-50 HPF 0 bass boost, 1 flat, 2 225 Hz, 3 700 Hz. */
    unsigned mks_hpf = jp ? 1u : syn_values(p)[MP_HPF];
    float hp = jp ? (syn_values(p)[JP_HPF] ? 20.0f * dsp_exp2(val(p, JP_HPF) * 7.6f) : 0.0f)
                  : mks_hpf == 2 ? 225.0f : mks_hpf == 3 ? 700.0f : 0.0f;
    /* VCF LFO: the curve follows the LFO as the engine does (cutoff in
     * octaves + LFO x depth; the picture spans 10 octaves), the band shows
     * how far it sweeps. */
    float depth = ed_vcf_lfo_octaves(p) / 10.0f, base = cut;
    if (depth > 0.0f) cut = dsp_clamp(cut + ed_lfo_value(p) * depth, 0.0f, 1.0f);
    float fc = 20.0f * dsp_exp2(cut * 10.0f), q = 0.6f + reso * reso * 9.0f;
    int zero = y + h / 3;
    if (depth > 0.0f) {
        int lo = x + (int)(dsp_clamp(base - depth, 0.0f, 1.0f) * (float)w), hi = x + (int)(dsp_clamp(base + depth, 0.0f, 1.0f) * (float)w);
        gfx_fill(b, lo, y, hi - lo + 1, h, 0x18c3);
    }
    gfx_fill(b, x, zero, w, 1, FAINT);
    int prev = -1;
    for (int i = 0; i < w; i += 2) {
        float f = 20.0f * dsp_exp2((float)i / (float)w * 10.0f), r = f / fc;
        float re = 1.0f - r * r, im = r / q, mag2 = 1.0f / (re * re + im * im);   /* 2-pole low-pass */
        float g;
        if (mode == 1) g = mag2 * im * im;                                         /* band-pass, 12 dB */
        else if (mode == 2) g = mag2 * mag2 * r * r * r * r * r * r * r * r;      /* high-pass, 24 dB */
        else g = mag2 / (1.0f + r * r * r * r);                                    /* low-pass, 24 dB */
        if (hp > 0) { float s = f / hp; g *= s * s / (1.0f + s * s); }
        if (mks_hpf == 0) { float s = f / 70.0f; g *= 1.0f + 3.0f / (1.0f + s * s); }   /* +6 dB below 70 Hz */
        float db = 10.0f * dsp_log2(dsp_max(g, 1e-6f)) * 0.30103f;
        int py = zero - (int)(db * (float)h / 54.0f);
        if (py < y) py = y;
        if (py > y + h - 2) py = y + h - 2;
        if (prev >= 0) line(b, x + i - 2, prev, x + i, py, c);
        prev = py;
    }
    int cx = x + (int)(base * (float)w);
    gfx_fill(b, cx, y, 1, h, DIM);
}

static void draw_lfo(gfx_band_t *b, const syn_part_t *p, uint16_t c, int x, int y, int w, int h)
{
    int jp = p->engine == SYN_JP6;
    unsigned wave = syn_values(p)[jp ? JP_LFO_WAVE : MP_LFO_WAVE];
    float delay = val(p, jp ? JP_LFO_DELAY : MP_LFO_DELAY);
    static const float steps[8] = {0.4f, -0.7f, 0.9f, 0.1f, -0.3f, 0.75f, -0.9f, 0.2f};
    int mid = y + h / 2, prev = mid, dw = (int)(delay * (float)w * 0.35f);
    gfx_fill(b, x, mid, w, 1, FAINT);
    for (int i = 0; i < w; i += 2) {
        float ph = (float)i / (float)w * 3.0f, f = ph - (float)(int)ph, v;
        switch (wave) {
        case 1: v = dsp_sin(2.0f * DSP_PI * f); break;
        case 2: v = 2.0f * f - 1.0f; break;
        case 3: v = 1.0f - 2.0f * f; break;
        case 4: v = f < 0.5f ? 1.0f : -1.0f; break;
        case 5: v = steps[(int)(ph * 2.0f) & 7]; break;
        case 6: { int k = (int)(ph * 2.0f); float t = ph * 2.0f - (float)k; v = steps[k & 7] + (steps[(k + 1) & 7] - steps[k & 7]) * t; break; }
        default: v = f < 0.25f ? 4.0f * f : f < 0.75f ? 2.0f - 4.0f * f : 4.0f * f - 4.0f; break;
        }
        float fade = i < dw ? (float)i / (float)dw : 1.0f;   /* LFO delay: the depth fades in */
        int py = mid - (int)(v * fade * (float)(h / 2 - 2));
        if (i) line(b, x + i - 2, prev, x + i, py, c);
        prev = py;
    }
}

/* Knob name for the list, at most 9 characters: a first word that only
 * repeats the section ("VCO1 SAW" under VC1, "LFO RATE" under LFO) is
 * left out. */
static void short_name(char out[10], const char *name, const char *tab)
{
    const char *space = strchr(name, ' ');
    if (space && space[1] && name[0] == tab[0] && name[1] == tab[1]) name = space + 1;
    unsigned n = 0;
    while (n < 9 && name[n]) { out[n] = name[n]; ++n; }
    out[n] = 0;
}

/* A round dot 5 pixels across, centred on x, y. */
static void dot5(gfx_band_t *b, int x, int y, uint16_t c)
{
    gfx_fill(b, x - 1, y - 2, 3, 1, c);
    gfx_fill(b, x - 2, y - 1, 5, 3, c);
    gfx_fill(b, x - 1, y + 2, 3, 1, c);
}

/* Every knob of the section as a bar; the shown page bright. */
static void draw_list(gfx_band_t *b, const syn_part_t *p, const ed_section_t *s, unsigned page, uint16_t c, int x, int y, int w, int h)
{
    int rows = (int)(s->count + 1u) / 2, col_w = w / 2, row_h = rows ? h / rows : h;
    if (row_h > 14) row_h = 14;
    for (unsigned i = 0; i < s->count; ++i) {
        int cx = x + (int)(i % 2u) * col_w, cy = y + (int)(i / 2u) * row_h;
        if (!gfx_touches(b, cy, row_h)) continue;
        int shown = i / 4u == page;
        char name[10];
        short_name(name, syn_param_name(p->engine, s->param[i]), s->tab);
        gfx_text(b, cx, cy + 1, name, shown ? TEXT : DIM, 1);
        int bw = col_w - 60, max = syn_param_max(p->engine, s->param[i]);
        if (max >= 1 && max <= 7) {
            /* Few settings (switches, waves, modes): one dot each, the
             * chosen one lit. */
            int value = syn_values(p)[s->param[i]];
            for (int k = 0; k <= max; ++k) dot5(b, cx + 58 + k * 7, cy + 4, k == value ? (shown ? c : DIM) : GRID);
        } else {
            gfx_fill(b, cx + 56, cy + 2, bw, 5, GRID);
            gfx_fill(b, cx + 56, cy + 2, (int)(val(p, s->param[i]) * (float)bw), 5, shown ? c : DIM);
        }
    }
}

void ed_draw(gfx_band_t *b, const syn_part_t *part, unsigned section, unsigned page, uint16_t color, int y, int h)
{
    unsigned engine = part->engine, n = ed_sections(engine);
    const ed_section_t *s = ed_section(engine, section);
    /* Tabs: every section, the current one filled. */
    int tab_w = GFX_W / (int)n;
    if (gfx_touches(b, y, 11))
        for (unsigned i = 0; i < n; ++i) {
            const ed_section_t *t = ed_section(engine, i);
            int tx = (int)i * tab_w;
            if (i == section) gfx_fill(b, tx + 1, y, tab_w - 2, 11, color);
            else gfx_fill(b, tx + 1, y + 9, tab_w - 2, 2, FAINT);
            gfx_text(b, tx + (tab_w - gfx_text_width(t->tab, 1)) / 2, y + 2, t->tab, i == section ? BACK : DIM, 1);
        }
    char title[40];
    unsigned pages = ed_pages(s);
    if (pages > 1) el_snprintf(title, sizeof title, "%s  %u/%u", s->name, page + 1u, pages);
    else el_snprintf(title, sizeof title, "%s", s->name);
    if (gfx_touches(b, y + 15, 9)) gfx_text(b, 4, y + 15, title, color, 1);
    int py = y + 28, ph = h - 28;
    if (!gfx_touches(b, py, ph)) return;
    if (s->kind == ED_LIST) { draw_list(b, part, s, page, color, 4, py, GFX_W - 8, ph); return; }
    gfx_fill(b, 4, py, GFX_W - 8, ph, PLOT);
    if (s->kind == ED_ENV) draw_env(b, part, s, color, 10, py + 4, GFX_W - 20, ph - 8);
    else if (s->kind == ED_VCF) draw_vcf(b, part, color, 6, py + 2, GFX_W - 12, ph - 4);
    else draw_lfo(b, part, color, 8, py + 2, GFX_W - 16, ph - 4);
}
