/* SPDX-License-Identifier: GPL-3.0-only */
/* 808 solo: die Punch-in-Effekte von FM1-Electro (engine/electro.c, dort GPL-3.0-only WITH GPL-3.0-linking-exception;
 * Wunsch des Nutzers 28.11.2026, ADR 0050), Teil der Einheit dk_app.c. Gleiche Effekte, gleiche Werte, gleiche Abläufe:
 * STUTTER, REVERB THROW, TAPE STOP, REPEAT (im Sequencer, dk_beat.c), DELAY, BITCRUSH, PHASER, je gehalten an.
 * Eingebunden in dk_ui.c (nach den Einstellungen us), damit Bedienung und Klang dieselben Werte sehen.
 * Anders als dort: sie rechnen auf der fertigen Summe des Gerüsts (Haken GM_POST_MIX, Kern 0); TAPE STOP und DELAY haben
 * eigene Puffer im RAM statt den Speicher des Halls zu leihen; REVERB THROW speist den Hall des Gerüsts (syn_fx) und
 * öffnet ihn, solange er klingt. Das Raster: ein Step kam in diesem Block (db.fired), am Anfang des Blocks. */
/* Mathe-Hilfen aus shared/third_party/fm1_open/dsp_math.h (MIT, m-vave/fm1_open über FM1-Electro; shared/LICENSES.md),
 * hier abgeschrieben: die Kopfdatei holt <string.h>, das in dieser Einheit mit der libc der Plattform kollidiert */
#ifndef DSP_PI
#define DSP_PI 3.14159265358979323846f
#endif
#ifndef DSP_MATH_FN
#if defined(EL_PLATFORM)
#define DSP_MATH_FN static __attribute__((section(".dsp_text"), noinline, unused))
#else
#define DSP_MATH_FN static inline
#endif
#endif
/* FM1-Electro: the one-line helpers always inline (the device build's LTO
 * inline threshold is 5: they were called per sample, ~3 % of the audio
 * time for dsp_clamp alone). */
#define DSP_TINY static inline __attribute__((always_inline))
DSP_TINY float dsp_abs(float x) { return x < 0 ? -x : x; }
DSP_TINY float dsp_clamp(float x,float a,float b) { return x<a?a:x>b?b:x; }
DSP_TINY float dsp_min(float a,float b) { return a<b?a:b; }
DSP_TINY float dsp_max(float a,float b) { return a>b?a:b; }
/* Taylor expansion after reduction to a fractional exponent in [-0.5,0.5]. */
DSP_MATH_FN float dsp_exp2(float x) {
    x=dsp_clamp(x,-120.0f,120.0f);
    int n=(int)(x+(x<0?-0.5f:0.5f));
    float a=(x-(float)n)*0.6931471805599453f;
    float p=1.0f+a*(1.0f+a*(0.5f+a*(1.0f/6.0f+a*(1.0f/24.0f+a*(1.0f/120.0f+a/720.0f)))));
    uint32_t bits=(uint32_t)(n+127)<<23;float scale;__builtin_memcpy(&scale,&bits,sizeof scale);
    return p*scale;
}
static inline float dsp_exp(float x) { return dsp_exp2(x*1.4426950408889634f); }
static inline float dsp_decay(float seconds) { return dsp_exp(-1.0f/(44100.0f*dsp_max(seconds,0.00001f))); }
DSP_MATH_FN float dsp_sin(float x) {
    x*=1.0f/(2.0f*DSP_PI); x-=(float)(int)x;
    if(x>.5f)x-=1; if(x<-.5f)x+=1;
    float a=x*(2.0f*DSP_PI);
    if(a>DSP_PI*.5f)a=DSP_PI-a; if(a<-DSP_PI*.5f)a=-DSP_PI-a;
    float q=a*a;
    return a*(1.0f+q*(-1.0f/6.0f+q*(1.0f/120.0f+q*(-1.0f/5040.0f+q*(1.0f/362880.0f-q/39916800.0f)))));
}
static inline float dsp_cos(float x) {return dsp_sin(x+DSP_PI*.5f);}
static inline float dsp_tan(float x) {return dsp_sin(x)/dsp_cos(x);}
DSP_MATH_FN float dsp_sqrt(float x) {
    if(x<=0)return 0;
    uint32_t bits;__builtin_memcpy(&bits,&x,4);bits=(bits>>1)+0x1fc00000u;
    float y;__builtin_memcpy(&y,&bits,4);
    for(unsigned i=0;i<4;i++)y=.5f*(y+x/y);
    return y;
}
DSP_MATH_FN float dsp_log2(float x) {
    x=dsp_max(x,1e-30f);uint32_t bits;__builtin_memcpy(&bits,&x,4);
    int e=(int)(bits>>23)-127;bits=(bits&0x7fffffu)|0x3f800000u;
    float m;__builtin_memcpy(&m,&bits,4);
    if(m>1.41421356f){m*=.5f;e++;}
    float z=(m-1)/(m+1),q=z*z;
    return (float)e+2.8853900817779268f*z*(1+q*(1.0f/3+q*(1.0f/5+q*(1.0f/7+q/9))));
}
static inline float dsp_tanh(float x) {
    if(x>=8)return 1;if(x<=-8)return -1;
    float p=dsp_exp2(2.8853900817779268f*x);return (p-1)/(p+1);
}

enum { PN_PUNCHES = 7, PN_DIVS = 6 };
enum { PN_STUTTER = 1, PN_THROW = 2, PN_TAPE = 4, PN_REPEAT = 8, PN_DELAY = 16, PN_CRUSH = 32, PN_PHASER = 64 };
enum { PN_TAPE_REGION = 6144, PN_DELAY_REGION = 8192 };   /* (wie FM1-Electro: DELAY 8192, TAPE eine Viertelsekunde Verzug) */
static const char *const pn_names[PN_PUNCHES] = {"STUTTER", "REVERB THROW", "TAPE STOP", "REPEAT", "DELAY", "BITCRUSH", "PHASER"};
static const char *const pn_div_names[PN_DIVS] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const float pn_div_16ths[PN_DIVS] = {4.0f, 2.0f, 4.0f / 3.0f, 1.0f, 2.0f / 3.0f, 0.5f};
static const uint8_t pn_div_reset[PN_DIVS] = {4, 2, 4, 1, 2, 1};
static const char *const pn_crush_names[PN_DIVS] = {"12 BIT", "10 BIT", "8 BIT", "6 BIT", "5 BIT", "4 BIT"};
static const char *const pn_phaser_names[PN_DIVS] = {"1/4", "1/2", "1 BAR", "2 BARS", "4 BARS", "8 BARS"};
static const char *const pn_tape_names[PN_DIVS] = {"1/16", "1/8", "3/16", "1/4", "3/8", "1/2"};
static const float pn_tape_16ths[PN_DIVS] = {1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
static const char *const pn_step_names[16] = {"STEP 1", "STEP 2", "STEP 3", "STEP 4", "STEP 5", "STEP 6", "STEP 7", "STEP 8",
                                               "STEP 9", "STEP 10", "STEP 11", "STEP 12", "STEP 13", "STEP 14", "STEP 15", "STEP 16"};
/* was weiße Taste v (0..5, REPEAT 0..15) für Punch-in n einstellt, oder 0 (REVERB THROW: nichts) */
static const char *pn_value_name(unsigned n, unsigned v)
{
    if (n >= PN_PUNCHES || (1u << n) == PN_THROW) return 0;
    if ((1u << n) == PN_REPEAT) return v < 16u ? pn_step_names[v] : 0;
    if (v >= PN_DIVS) return 0;
    if ((1u << n) == PN_CRUSH) return pn_crush_names[v];
    if ((1u << n) == PN_PHASER) return pn_phaser_names[v];
    if ((1u << n) == PN_TAPE) return pn_tape_names[v];
    return pn_div_names[v];
}

static struct {
    volatile uint8_t punch;                     /* gehaltene Punch-ins (PN_*), von der Bedienung gesetzt */
    volatile uint8_t div[PN_PUNCHES];           /* Wert je Punch-in (weiße Taste) */
    int stutter_sync;
    uint32_t sync_step;
    float stutter_phase, stutter_gain, dry_gain, throw_lp;
    uint8_t tape_half, dl_half, crush_n, tape_on, dl_on;
    float tape_mix, tape_rp, tape_rp2, tape_xf, tape_rate, tape_t, tape_acc;
    uint32_t tape_wp, dl_wp;
    float dl_in, dl_time, dl_acc, dl_lp, dl_hp, dl_hpx, dl_env;
    float crush_mix, crush_hold[2];
    float ph_mix, ph_phase, ph_a[2], ph_z[2][6], ph_fb[2];
    uint32_t rev_hold;                          /* REVERB THROW: Blöcke, die der Hall noch offen bleibt */
    uint8_t rev_opened;
} pn = {.div = {3, 0, 3, 0, 1, 2, 2}, .stutter_gain = 1.0f, .dry_gain = 1.0f};
static int16_t pn_tape_mem[PN_TAPE_REGION], pn_dl_mem[PN_DELAY_REGION];

static DSPF int pn_on_grid(unsigned i, unsigned div) { return (int)i == pn.stutter_sync && pn.sync_step % pn_div_reset[div] == 0; }
static DSPF void pn_put(int16_t *m, uint32_t i, float x) { m[i] = (int16_t)(dsp_clamp(x, -1.0f, 1.0f) * 32767.0f); }

/* REVERB THROW (gehalten): die trockene Summe blendet über zwei Schläge aus und speist dabei den Hall (dunkler, um
 * 2,5 kHz); dann klingt nur der Hall. Losgelassen: die Summe ist in 20 ms zurück, der Hall klingt aus. */
static DSPF void pn_throw(float *ml, float *mr, unsigned frames)
{
    int on = (pn.punch & PN_THROW) != 0;
    if (on && !pn.rev_opened) {                 /* (der Hall des Gerüsts muss offen sein, auch wenn er ab Werk zu ist) */
        syn_fx_set(FX_REV_LEVEL, 100); fx_on = 1; pn.rev_opened = 1;
    }
    if (on) pn.rev_hold = (uint32_t)(4.0f * EL_SR / GM_BLOCK);
    else if (pn.rev_opened && pn.rev_hold && --pn.rev_hold == 0) {   /* 4 s nach dem Loslassen: der Hall wie eingestellt */
        syn_fx_set(FX_REV_LEVEL, us.fx[FX_REV_LEVEL]); fx_on = us.fx[FX_DLY_LEVEL] || us.fx[FX_REV_LEVEL]; pn.rev_opened = 0;
    }
    if (!on && pn.dry_gain >= 1.0f) return;
    float beat = 60.0f / dsp_max(bpm_now, 30.0f) * EL_SR;
    float down = 1.0f / (2.0f * beat), up = 1.0f / (0.02f * EL_SR);
    for (unsigned i = 0; i < frames; ++i) {
        pn.dry_gain = dsp_clamp(pn.dry_gain + (on ? -down : up), 0.0f, 1.0f);
        float g = pn.dry_gain * pn.dry_gain;
        float mix = 0.5f * (ml[i] + mr[i]);
        pn.throw_lp += (mix - pn.throw_lp) * 0.3f;
        ml[i] *= g; mr[i] *= g;
        fx_in[0][i] *= g;                       /* (das Delay des Blocks, der jetzt hineingeht) */
        fx_in[1][i] = fx_in[1][i] * g + (on ? pn.throw_lp * pn.dry_gain * 1.8f : 0.0f);
    }
}
/* STUTTER (gehalten): die Summe im gewählten Notenwert gegated (1/16), offen die erste Hälfte, 1,5 ms Flanken, im
 * Raster der Steps */
static DSPF void pn_stutter(float *left, float *right, unsigned frames)
{
    unsigned which = pn.punch & PN_STUTTER, div = pn.div[0];
    if (!which && pn.stutter_gain >= 1.0f) return;
    float sixteenth = 15.0f / dsp_max(bpm_now, 30.0f) * EL_SR;
    float rate = 1.0f / (sixteenth * pn_div_16ths[div]), edge = 1.0f / (0.0015f * EL_SR);
    for (unsigned i = 0; i < frames; ++i) {
        if (pn_on_grid(i, div)) pn.stutter_phase = 0.0f;
        pn.stutter_phase += rate;
        if (pn.stutter_phase >= 1.0f) pn.stutter_phase -= 1.0f;
        float target = !which || pn.stutter_phase < 0.5f ? 1.0f : 0.0f;
        pn.stutter_gain += dsp_clamp(target - pn.stutter_gain, -edge, edge);
        left[i] *= pn.stutter_gain;
        right[i] *= pn.stutter_gain;
    }
}
/* TAPE STOP (gehalten): die Summe (mono, halbe Rate im Puffer) mit von 1 auf 0 fallender Geschwindigkeit über die
 * gewählte Länge (1/8); längere Stops rutschen mit 25 ms Überblendung nach. Losgelassen: in 10 ms wieder live. */
static DSPF float pn_tape_read(const int16_t *m, float rp, uint32_t cap)
{
    uint32_t k = (uint32_t)rp, k1 = k + 1u < cap ? k + 1u : 0u;
    return (m[k] + (m[k1] - m[k]) * (rp - (float)k)) * (1.0f / 32767.0f);
}
static DSPF void pn_tape(float *left, float *right, unsigned frames)
{
    int held = (pn.punch & PN_TAPE) != 0;
    const uint32_t cap = PN_TAPE_REGION;
    int16_t *m = pn_tape_mem;
    if (!held && !pn.tape_on) return;
    if (held && !pn.tape_on) {
        for (uint32_t i = 0; i < cap; i++) m[i] = 0;
        pn.tape_on = 1; pn.tape_mix = 0; pn.tape_wp = 0; pn.tape_half = 0;
        pn.tape_rp = (float)cap - 1.5f; pn.tape_xf = -1.0f; pn.tape_t = 0; pn.tape_rate = 1;
    }
    float sixteenth = 15.0f / dsp_max(bpm_now, 30.0f) * EL_SR, xf = 1.0f / (0.01f * EL_SR);
    float stop_len = dsp_clamp(sixteenth * pn_tape_16ths[pn.div[2]], 0.04f * EL_SR, 20.0f * EL_SR);
    const float slip_at = (float)cap - 800.0f, slip_xf = 1.0f / (0.025f * EL_SR);
    for (unsigned i = 0; i < frames; ++i) {
        float l = left[i], r = right[i];
        pn.tape_acc += 0.5f * (l + r);
        pn.tape_half ^= 1u;
        if (!pn.tape_half) {
            pn_put(m, pn.tape_wp, 0.5f * pn.tape_acc);
            pn.tape_acc = 0.0f;
            pn.tape_wp = pn.tape_wp + 1u < cap ? pn.tape_wp + 1u : 0u;
        }
        if (held) {
            pn.tape_t += 1.0f;
            pn.tape_rate = dsp_max(0.0f, 1.0f - pn.tape_t / stop_len);
            pn.tape_mix = dsp_min(1.0f, pn.tape_mix + xf);
        } else pn.tape_mix = dsp_max(0.0f, pn.tape_mix - xf);
        float lag = (float)pn.tape_wp + 0.5f * pn.tape_half - pn.tape_rp;
        if (lag < 0.0f) lag += (float)cap;
        if (pn.tape_xf < 0.0f && lag > slip_at) {
            pn.tape_rp2 = pn.tape_rp + 0.5f * (float)cap;
            if (pn.tape_rp2 >= (float)cap) pn.tape_rp2 -= (float)cap;
            pn.tape_xf = 0.0f;
        }
        float y = pn_tape_read(m, pn.tape_rp, cap), st = 0.5f * pn.tape_rate;
        pn.tape_rp += st;
        if (pn.tape_rp >= (float)cap) pn.tape_rp -= (float)cap;
        if (pn.tape_xf >= 0.0f) {
            float g = pn.tape_xf;
            y = y * dsp_sqrt(1.0f - g) + pn_tape_read(m, pn.tape_rp2, cap) * dsp_sqrt(g);
            pn.tape_rp2 += st;
            if (pn.tape_rp2 >= (float)cap) pn.tape_rp2 -= (float)cap;
            pn.tape_xf += slip_xf;
            if (pn.tape_xf >= 1.0f) { pn.tape_rp = pn.tape_rp2; pn.tape_xf = -1.0f; }
        }
        y *= dsp_min(1.0f, pn.tape_rate * 6.0f);
        left[i] = l + (y - l) * pn.tape_mix;
        right[i] = r + (y - r) * pn.tape_mix;
    }
    if (!held && pn.tape_mix <= 0.0f) pn.tape_on = 0;
}
/* DELAY (gehalten): Dub-Echo (mono, halbe Rate) im gewählten Notenwert (1/8, höchstens eine halbe Sekunde), Rückkopplung
 * 72 %, jede Runde dunkler und dünner; ein neuer Wert gleitet hin. Losgelassen: die Echos klingen aus. */
static DSPF void pn_delay(float *left, float *right, unsigned frames)
{
    int held = (pn.punch & PN_DELAY) != 0;
    const uint32_t cap = PN_DELAY_REGION;
    int16_t *m = pn_dl_mem;
    float sixteenth = 15.0f / dsp_max(bpm_now, 30.0f) * EL_SR;
    float target = dsp_clamp(0.5f * sixteenth * pn_div_16ths[pn.div[4]], 64.0f, (float)(cap - 4u));
    if (!held && !pn.dl_on) return;
    if (held && !pn.dl_on) {
        for (uint32_t i = 0; i < cap; i++) m[i] = 0;
        pn.dl_on = 1; pn.dl_wp = 0; pn.dl_half = 0;
        pn.dl_in = pn.dl_acc = pn.dl_lp = pn.dl_hp = pn.dl_hpx = pn.dl_env = 0.0f;
        pn.dl_time = target;
    }
    const float ramp = 1.0f / (0.005f * EL_SR), lp = 0.45f, hp = 0.979f;
    for (unsigned i = 0; i < frames; ++i) {
        pn.dl_time += (target - pn.dl_time) * 0.0004f;
        float pos = (float)pn.dl_wp + (pn.dl_half ? 0.5f : 0.0f) - pn.dl_time;
        if (pos < 0.0f) pos += (float)cap;
        uint32_t k = (uint32_t)pos, k1 = k + 1u < cap ? k + 1u : 0u;
        float f = pos - (float)k, y = (m[k] + (m[k1] - m[k]) * f) * (1.0f / 32767.0f);
        pn.dl_in = held ? dsp_min(1.0f, pn.dl_in + ramp) : dsp_max(0.0f, pn.dl_in - ramp);
        float x = pn.dl_in * 0.5f * (left[i] + right[i]) + 0.72f * y;
        pn.dl_lp += (x - pn.dl_lp) * lp;
        float h = hp * (pn.dl_hp + pn.dl_lp - pn.dl_hpx);
        pn.dl_hpx = pn.dl_lp;
        pn.dl_hp = h;
        pn.dl_acc += h;
        pn.dl_half ^= 1u;
        if (!pn.dl_half) {
            pn_put(m, pn.dl_wp, dsp_tanh(0.5f * pn.dl_acc));
            pn.dl_acc = 0.0f;
            pn.dl_wp = pn.dl_wp + 1u < cap ? pn.dl_wp + 1u : 0u;
        }
        left[i] += 0.8f * y;
        right[i] += 0.8f * y;
        pn.dl_env = dsp_max(dsp_abs(y), pn.dl_env * 0.9999f);
    }
    if (!held && pn.dl_in <= 0.0f && pn.dl_env < 0.0002f) pn.dl_on = 0;
}
/* BITCRUSH (gehalten): weniger Bits und weniger Rate (12 Bit jedes Sample .. 4 Bit jedes 8.), 5 ms hinein und hinaus */
static DSPF void pn_crush(float *left, float *right, unsigned frames)
{
    static const float levels[PN_DIVS] = {2048.0f, 512.0f, 128.0f, 32.0f, 16.0f, 8.0f};
    static const uint8_t hold[PN_DIVS] = {1, 2, 3, 4, 6, 8};
    int held = (pn.punch & PN_CRUSH) != 0;
    if (!held && pn.crush_mix <= 0.0f) return;
    unsigned v = pn.div[5];
    float q = levels[v], ramp = 1.0f / (0.005f * EL_SR);
    for (unsigned i = 0; i < frames; ++i) {
        if (++pn.crush_n >= hold[v]) {
            pn.crush_n = 0;
            for (unsigned c = 0; c < 2; ++c) {
                float x = dsp_clamp(c ? right[i] : left[i], -1.0f, 1.0f) * q;
                pn.crush_hold[c] = (float)(int32_t)(x + (x >= 0.0f ? 0.5f : -0.5f)) / q;
            }
        }
        pn.crush_mix = held ? dsp_min(1.0f, pn.crush_mix + ramp) : dsp_max(0.0f, pn.crush_mix - ramp);
        left[i] += (pn.crush_hold[0] - left[i]) * pn.crush_mix;
        right[i] += (pn.crush_hold[1] - right[i]) * pn.crush_mix;
    }
}
/* PHASER (gehalten): sechs Allpässe je Seite, 200 Hz bis 3 kHz einmal je gewählter Länge (1 Takt), die Seiten eine
 * Vierteldrehung versetzt, etwas Rückkopplung; 10 ms hinein und hinaus */
static DSPF void pn_phaser(float *left, float *right, unsigned frames)
{
    static const float lengths[PN_DIVS] = {4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f};
    int held = (pn.punch & PN_PHASER) != 0;
    if (!held && pn.ph_mix <= 0.0f) return;
    if (held && pn.ph_mix <= 0.0f) {
        pn.ph_phase = 0.0f;
        for (unsigned c = 0; c < 2; ++c) { for (unsigned s = 0; s < 6; ++s) pn.ph_z[c][s] = 0.0f; pn.ph_fb[c] = 0.0f; }
    }
    float sixteenth = 15.0f / dsp_max(bpm_now, 30.0f) * EL_SR;
    float rate = 1.0f / (sixteenth * lengths[pn.div[6]]), ramp = 1.0f / (0.01f * EL_SR);
    for (unsigned i = 0; i < frames; ++i) {
        if ((i & 15u) == 0) {
            for (unsigned c = 0; c < 2; ++c) {
                float ph = pn.ph_phase + (c ? 0.25f : 0.0f);
                float tri = 0.5f - 0.5f * dsp_cos(2.0f * DSP_PI * ph);
                float fc = 200.0f * dsp_exp2(tri * 3.9f);
                float t = dsp_tan(DSP_PI * fc / EL_SR);
                pn.ph_a[c] = (t - 1.0f) / (t + 1.0f);
            }
        }
        pn.ph_phase += rate;
        if (pn.ph_phase >= 1.0f) pn.ph_phase -= 1.0f;
        pn.ph_mix = held ? dsp_min(1.0f, pn.ph_mix + ramp) : dsp_max(0.0f, pn.ph_mix - ramp);
        for (unsigned c = 0; c < 2; ++c) {
            float x = c ? right[i] : left[i], a = pn.ph_a[c], y = x + 0.5f * pn.ph_fb[c];
            for (unsigned s = 0; s < 6; ++s) {
                float o = a * y + pn.ph_z[c][s];
                pn.ph_z[c][s] = y - a * o;
                y = o;
            }
            pn.ph_fb[c] = y;
            float out = 0.5f * (x + y);
            if (c) right[i] = x + (out - x) * pn.ph_mix;
            else left[i] = x + (out - x) * pn.ph_mix;
        }
    }
}
/* GM_POST_MIX: THROW auf der trockenen Summe, dann die Effekte dazu, dann die übrigen wie in FM1-Electro */
#if defined(DK_LAST) && DK_LAST
static void dk_last_k0(uint32_t t);
static DSPF void dk_post_mix2(float *ml, float *mr, float *wl, float *wr);
static DSPF void dk_post_mix(float *ml, float *mr, float *wl, float *wr)
{
    uint32_t t0 = fm1_ticks();
    dk_post_mix2(ml, mr, wl, wr);
    dk_last_k0(fm1_ticks() - t0);
}
#define dk_post_mix dk_post_mix2
#endif
static DSPF void dk_post_mix(float *ml, float *mr, float *wl, float *wr)
{
    pn.stutter_sync = db.fired ? 0 : -1;
    pn.sync_step = db.fired_step;
    pn_throw(ml, mr, GM_BLOCK);
    if (!(pn.punch & 0x7Fu) && pn.stutter_gain >= 1.0f && !pn.tape_on && !pn.dl_on && pn.crush_mix <= 0.0f && pn.ph_mix <= 0.0f) return;
    for (unsigned i = 0; i < GM_BLOCK; i++) { ml[i] += wl[i]; mr[i] += wr[i]; wl[i] = wr[i] = 0.0f; }
    pn_delay(ml, mr, GM_BLOCK);
    pn_crush(ml, mr, GM_BLOCK);
    pn_phaser(ml, mr, GM_BLOCK);
    pn_stutter(ml, mr, GM_BLOCK);
    pn_tape(ml, mr, GM_BLOCK);
}
