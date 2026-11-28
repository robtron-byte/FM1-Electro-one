/* SPDX-License-Identifier: GPL-3.0-only */
/* MKS-50 solo für das M-VAVE FM-1 (ADR 0047, 0044): der Baustein bausteine/mks50 (engine/mks.c von FM1-Electro, Port
 * der MKS-50-iOS-App des Nutzers) auf dem gemeinsamen Gerüst bausteine/varianten/gemeinsam. Löst die fm1-synths-
 * Fassung (FM-1_95) ab. Kern 0 rechnet die Stimmen, Kern 1 Delay und Hall.
 *
 * Werte im Speicher = Rohwerte des Kerns (MP_*, 0..mks_param_max), ein Parameter des Gerüsts je Parameter des Kerns.
 * Sounds: die eigenen des Nutzers (ADR 0051, Gruppe EIGENE, vorn) und die Werks-Sounds, wie im Baustein.
 * Delay- und Hall-Anteil gehören zum Sound (MP_DELAY_SEND, MP_REVERB_SEND, wie in FM1-Electro und fm1-synths). */
#define APP_NAME "MKS-50"
#define GM_TITLE "MKS-50"
#define GM_ACCENT RGB(60, 150, 230)             /* das Blau der Alpha-Juno-/MKS-50-Beschriftung */
#define GM_PARTS 1u
#define GM_NAMEMAX 10u
#define GM_SOUND_VER 1u                         /* Format der eigenen Sounds: 54 Rohwerte MP_* */
#define GM_JR_MAGIC 0x54534B4Du                 /* "MKST": Journal dieser Firmware */
#define GM_SEND_FIRST 0                         /* Delay-/Hall-Anteil steht im Sound */
#define GM_FX_FIRST_ON 1                        /* Delay und Hall gleich an, wie in fm1-synths */
#include "gm_types.h"
/* engine/el_config.h deckt sich hier mit shared/syn_config.h (über syn_fx.h schon geladen); die App braucht aus mks.h
 * nur die Namen, der Kern selbst wird für sich mit el_config.h übersetzt. */
#define EL_CONFIG_H
#include "mks.h"
#include "mks50/mks50_bs.h"
#include "mk_sounds.h"

#define GM_NP MP_COUNT

static void t_put(char *b, const char *s) { if (!s) s = "?"; while ((*b++ = *s++) != 0) {} }
#define VN(P) static void t_##P(uint8_t v, char *b) { t_put(b, mks_param_value_name(P, v)); }
VN(MP_DCO_ENV_MODE) VN(MP_VCF_ENV_MODE) VN(MP_VCA_ENV_MODE) VN(MP_RANGE) VN(MP_CHORUS) VN(MP_PULSE_SYNC)
VN(MP_LFO_SYNC) VN(MP_LFO_KEYSYNC) VN(MP_KEY_MODE) VN(MP_PORTA) VN(MP_LFO_WAVE) VN(MP_LFO_DIV) VN(MP_ENV_RETRIG)
VN(MP_CHORD)
static void t_shift(uint8_t v, char *b)
{
    int s = (int)v - 12;
    if (s > 0) *b++ = '+';
    if (s < 0) { *b++ = '-'; s = -s; }
    if (s >= 10) *b++ = (char)('0' + s / 10);
    *b++ = (char)('0' + s % 10);
    *b = 0;
}

#define T(P) t_##P
static const gm_param_t gm_params[GM_NP] = {
    [MP_DCO_ENV_MODE] = {"DCO EMD", 3, C_MOD, T(MP_DCO_ENV_MODE)}, [MP_VCF_ENV_MODE] = {"VCF EMD", 3, C_FLT, T(MP_VCF_ENV_MODE)},
    [MP_VCA_ENV_MODE] = {"VCA MD", 3, C_AMP, T(MP_VCA_ENV_MODE)}, [MP_PULSE] = {"PULSE", 3, C_SRC, 0},
    [MP_SAW] = {"SAW", 5, C_SRC, 0}, [MP_SUB] = {"SUB", 5, C_SRC, 0}, [MP_RANGE] = {"RANGE", 3, C_SRC, T(MP_RANGE)},
    [MP_SUB_LEVEL] = {"SUB LVL", 3, C_SRC, 0}, [MP_NOISE] = {"NOISE", 3, C_SRC, 0}, [MP_HPF] = {"HPF", 3, C_FLT, 0},
    [MP_CHORUS] = {"CHORUS", 4, C_FX, T(MP_CHORUS)}, [MP_DCO_LFO] = {"VIBRATO", 127, C_MOD, 0},
    [MP_DCO_ENV] = {"PIT ENV", 127, C_MOD, 0}, [MP_DCO_AFTER] = {"DCO AFT", 127, C_MOD, 0},
    [MP_PW] = {"PW", 127, C_SRC, 0}, [MP_PWM_RATE] = {"PWM RT", 127, C_MOD, 0},
    [MP_CUTOFF] = {"CUTOFF", 127, C_FLT, 0}, [MP_RESO] = {"RESO", 127, C_FLT, 0},
    [MP_VCF_LFO] = {"VCF LFO", 127, C_MOD, 0}, [MP_VCF_ENV] = {"VCF ENV", 127, C_FLT, 0},
    [MP_VCF_KEY] = {"KEY FLW", 127, C_FLT, 0}, [MP_VCF_AFTER] = {"VCF AFT", 127, C_FLT, 0},
    [MP_VCA_LEVEL] = {"LEVEL", 127, C_AMP, 0}, [MP_VCA_AFTER] = {"VCA AFT", 127, C_AMP, 0},
    [MP_LFO_RATE] = {"RATE", 127, C_MOD, 0}, [MP_LFO_DELAY] = {"DELAY", 127, C_MOD, 0},
    [MP_T1] = {"T1", 127, C_AMP, 0}, [MP_L1] = {"L1", 127, C_AMP, 0}, [MP_T2] = {"T2", 127, C_AMP, 0},
    [MP_L2] = {"L2", 127, C_AMP, 0}, [MP_T3] = {"T3", 127, C_AMP, 0}, [MP_L3] = {"L3", 127, C_AMP, 0},
    [MP_T4] = {"T4", 127, C_AMP, 0}, [MP_ENV_KEY] = {"ENV KEY", 127, C_AMP, 0},
    [MP_CHORUS_RATE] = {"CHO RT", 127, C_FX, 0}, [MP_BENDER] = {"BEND", 12, C_MOD, 0},
    [MP_KEY_MODE] = {"MODE", 3, C_AMP, T(MP_KEY_MODE)}, [MP_PORTA_TIME] = {"GLIDE", 127, C_AMP, 0},
    [MP_PORTA] = {"PORTA", 2, C_AMP, T(MP_PORTA)}, [MP_VOLUME] = {"VOLUME", 127, C_AMP, 0},
    [MP_KEY_SHIFT] = {"SHIFT", 24, C_SRC, t_shift}, [MP_PULSE_DETUNE] = {"DETUNE", 127, C_SRC, 0},
    [MP_PULSE_SYNC] = {"SYNC", 1, C_SRC, T(MP_PULSE_SYNC)}, [MP_PULSE_ENV] = {"SYN ENV", 127, C_MOD, 0},
    [MP_DRIFT] = {"DRIFT", 127, C_SRC, 0}, [MP_UNISON_DETUNE] = {"UNI DET", 127, C_SRC, 0},
    [MP_LFO_WAVE] = {"WAVE", 6, C_MOD, T(MP_LFO_WAVE)}, [MP_LFO_SYNC] = {"SYNC", 1, C_MOD, T(MP_LFO_SYNC)},
    [MP_LFO_DIV] = {"NOTE", 13, C_MOD, T(MP_LFO_DIV)}, [MP_LFO_KEYSYNC] = {"KEYSYNC", 1, C_MOD, T(MP_LFO_KEYSYNC)},
    [MP_ENV_RETRIG] = {"RETRIG", 1, C_AMP, T(MP_ENV_RETRIG)},
    [MP_DELAY_SEND] = {"DELAY", 127, C_FX, 0}, [MP_REVERB_SEND] = {"REVERB", 127, C_FX, 0},
    [MP_CHORD] = {"CHORD", MKS_CHORDS - 1, C_AMP, T(MP_CHORD)},
};
/* INIT: INIT TONE der MKS-50-App (Param.defaultValue, wie bausteine/mks50/tools/eigene2c.py) */
static const uint8_t gm_init_v[GM_NP] = {
    0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 127, 0, 0, 0, 0, 0, 110, 0, 64, 0, 0, 127, 0, 127, 0, 127, 20, 0,
    60, 2, 0, 30, 0, 100, 12, 64, 0, 0, 8, 40, 0, 0, 8, 0, 0, 0, 0, 0};

/* ---- Sounds: die Liste des Bausteins (eigene zuerst), Gruppen aus mk_sounds.h */
#define GM_NCAT MK_NCAT
#define gm_cat_name mk_cat_names
typedef struct { const char *name; uint8_t cat; const uint8_t *v; } gm_sound_t;
#define GM_NFAC MK_NSOUND
static gm_sound_t gm_factory[GM_NFAC];

/* ---- Seiten (fm1-synths ADR 0008, Aufteilung wie fm1-synths mks50/app/mk_ui.c) */
enum { ST_ENV_PIC = ST_USER };
static const page_t gm_pages[] = {
    {G_HOME, ST_HOME, "PLAY", {PRM("CUTOFF", MP_CUTOFF), PRM("RESO", MP_RESO), PRM("ENV", MP_VCF_ENV), PRM("RELEASE", MP_T4)}},
    {G_HOME, ST_NAME, "PERFORM", {PRM("CHORUS", MP_CHORUS), PRM("DELAY", MP_DELAY_SEND), PRM("REVERB", MP_REVERB_SEND), PRM("VOLUME", MP_VOLUME)}},
    {G_EDIT, ST_NAME, "OSC", {PRM("PULSE", MP_PULSE), PRM("SAW", MP_SAW), PRM("SUB", MP_SUB), PRM("RANGE", MP_RANGE)}},
    {G_EDIT, ST_NAME, "OSC 2", {PRM("SUB LVL", MP_SUB_LEVEL), PRM("NOISE", MP_NOISE), PRM("PW", MP_PW), PRM("PWM RT", MP_PWM_RATE)}},
    {G_EDIT, ST_NAME, "OSC MOD", {PRM("LFO", MP_DCO_LFO), PRM("ENV", MP_DCO_ENV), PRM("ENV MD", MP_DCO_ENV_MODE), PRM("AFTER", MP_DCO_AFTER)}},
    {G_EDIT, ST_NAME, "SYNC", {PRM("DETUNE", MP_PULSE_DETUNE), PRM("SYNC", MP_PULSE_SYNC), PRM("SYN ENV", MP_PULSE_ENV), PRM("DRIFT", MP_DRIFT)}},
    {G_EDIT, ST_NAME, "FILTER", {PRM("CUTOFF", MP_CUTOFF), PRM("RESO", MP_RESO), PRM("ENV", MP_VCF_ENV), PRM("KEY", MP_VCF_KEY)}},
    {G_EDIT, ST_NAME, "FILTER 2", {PRM("LFO", MP_VCF_LFO), PRM("ENV MD", MP_VCF_ENV_MODE), PRM("HPF", MP_HPF), PRM("AFTER", MP_VCF_AFTER)}},
    {G_EDIT, ST_NAME, "AMP", {PRM("LEVEL", MP_VCA_LEVEL), PRM("MODE", MP_VCA_ENV_MODE), PRM("VOLUME", MP_VOLUME), PRM("AFTER", MP_VCA_AFTER)}},
    {G_ENV, ST_ENV_PIC, "ENV", {PRM("T1", MP_T1), PRM("L1", MP_L1), PRM("T2", MP_T2), PRM("L2", MP_L2)}},
    {G_ENV, ST_ENV_PIC, "ENV 2", {PRM("T3", MP_T3), PRM("L3", MP_L3), PRM("T4", MP_T4), PRM("KEY", MP_ENV_KEY)}},
    {G_LFO, ST_NAME, "LFO", {PRM("RATE", MP_LFO_RATE), PRM("DELAY", MP_LFO_DELAY), PRM("WAVE", MP_LFO_WAVE), PRM("SYNC", MP_LFO_SYNC)}},
    {G_LFO, ST_NAME, "LFO 2", {PRM("NOTE", MP_LFO_DIV), PRM("KEYSYNC", MP_LFO_KEYSYNC), NO, NO}},
    {G_LFO, ST_NAME, "VOICE", {PRM("MODE", MP_KEY_MODE), PRM("PORTA", MP_PORTA), PRM("GLIDE", MP_PORTA_TIME), PRM("SHIFT", MP_KEY_SHIFT)}},
    {G_LFO, ST_NAME, "VOICE 2", {PRM("RETRIG", MP_ENV_RETRIG), PRM("UNI DET", MP_UNISON_DETUNE), PRM("CHORD", MP_CHORD), NO}},
    {G_FX, ST_NAME, "CHORUS", {PRM("CHORUS", MP_CHORUS), PRM("RATE", MP_CHORUS_RATE), NO, NO}},
    {G_FX, ST_NAME, "SENDS", {PRM("DELAY", MP_DELAY_SEND), PRM("REVERB", MP_REVERB_SEND), FXK("DLY LVL", FX_DLY_LEVEL), FXK("REV LVL", FX_REV_LEVEL)}},
    {G_FX, ST_NAME, "DELAY", {FXK("TIME", FX_DLY_TIME), FXK("FEEDBACK", FX_DLY_FB), FXK("TONE", FX_DLY_TONE), FXK("PING", FX_DLY_PING)}},
    {G_FX, ST_NAME, "DELAY 2", {FXK("SYNC", FX_DLY_SYNC), FXK("NOTE", FX_DLY_DIV), FXK("MOD", FX_DLY_MOD), NO}},
    {G_FX, ST_NAME, "REVERB", {FXK("SIZE", FX_REV_SIZE), FXK("DECAY", FX_REV_DECAY), FXK("DAMP", FX_REV_DAMP), FXK("TYPE", FX_REV_TYPE)}},
    {G_ARP, ST_ARP, "ARP", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCT", ARP_OCT)}},
    {G_ARP, ST_ARP, "ARP 2", {ARK("GATE", ARP_GATE), ARK("HOLD", ARP_HOLD), NO, NO}},
    {G_SEQ, ST_SEQ, "SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
    {G_GLO, ST_TEMPO, "TEMPO", {{"TEMPO", K_TEMPO, 0}, {"CLOCK", K_CLK, 0}, {"SEND", K_CLK, 1}, {"MIDI CH", K_CHAN, 0}}},
    {G_GLO, ST_SYS, "SYSTEM", {{"LEVEL", K_LEVEL, 0}, NO, NO, NO}},
    {G_SEL, ST_BROWSE, "SOUNDS", {NO, NO, NO, NO}},
};
static const layer_t gm_layers[NGROUP] = {
    [G_EDIT] = {"OSC", {PRM("PULSE", MP_PULSE), PRM("SAW", MP_SAW), PRM("SUB", MP_SUB), PRM("PW", MP_PW)}},
    [G_ENV] = {"ENVELOPE", {PRM("ATTACK", MP_T1), PRM("DECAY", MP_T2), PRM("SUSTAIN", MP_L3), PRM("RELEASE", MP_T4)}},
    [G_LFO] = {"LFO", {PRM("RATE", MP_LFO_RATE), PRM("DELAY", MP_LFO_DELAY), PRM("PITCH", MP_DCO_LFO), PRM("FILTER", MP_VCF_LFO)}},
    [G_FX] = {"EFFECTS", {PRM("DELAY", MP_DELAY_SEND), PRM("REVERB", MP_REVERB_SEND), FXK("TIME", FX_DLY_TIME), PRM("CHORUS", MP_CHORUS)}},
    [G_ARP] = {"ARP", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCT", ARP_OCT)}},
    [G_GLO] = {"VOICE", {PRM("MODE", MP_KEY_MODE), PRM("GLIDE", MP_PORTA_TIME), PRM("SHIFT", MP_KEY_SHIFT), PRM("VOLUME", MP_VOLUME)}},
    [G_SEQ] = {"SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
};

/* ---- der Klang: Baustein mks50 (engine/mks.c), Zustand im Pool */
#define MK_STATE_MAX 8192u                      /* Zustand des Bausteins (PC: 6,9 KB; kern_boot prüft) */
static uint8_t eng[MK_STATE_MAX] __attribute__((section(".pool"), aligned(16)));
static uint8_t mk_ok;
static void kern_boot(void)
{
    for (unsigned s = 0; s < GM_NFAC; s++) {
        gm_factory[s].name = mks50_bs_preset_name(s);
        gm_factory[s].cat = mk_sound_cat[s];
        gm_factory[s].v = mks50_bs_preset_values(s);
    }
    mk_ok = mks50_bs_state_size() <= MK_STATE_MAX && mks50_bs_preset_count() == GM_NFAC;
    if (mk_ok) mks50_bs_init(eng, 44100.0);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_set(unsigned p, unsigned i, uint8_t v)
{
    (void)p;
    if (mk_ok && i < GM_NP) mks50_bs_set_raw(eng, i, v);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_load(unsigned p, const uint8_t *v) { (void)p; if (mk_ok) mks50_bs_load_values(eng, v); }
static __attribute__((section(".dsp_text"), noinline)) void kern_on(unsigned p, int note, int vel, int legato)
{
    (void)p;
    if (mk_ok) mks50_bs_note_on(eng, note, vel, legato);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_off(unsigned p, int note) { (void)p; if (mk_ok) mks50_bs_note_off(eng, note); }
static __attribute__((section(".dsp_text"), noinline)) void kern_all_off(unsigned p) { (void)p; if (mk_ok) mks50_bs_all_off(eng); }
/* Bender und Modulationsrad: der Kern hat keine Eingänge dafür (BEND und die AFTER-Tiefen stehen nur im Sound) */
static __attribute__((section(".dsp_text"), noinline)) void kern_bend(unsigned p, float b) { (void)p; (void)b; }
static __attribute__((section(".dsp_text"), noinline)) void kern_mod(unsigned p, float m) { (void)p; (void)m; }
static void kern_render(unsigned p, float *l, float *r, unsigned n);   /* nach gm_app.c: braucht fx_in */
static int kern_voices(unsigned p) { (void)p; return mk_ok ? mks50_bs_voices(eng) : 0; }
static int kern_stage(unsigned stage, unsigned p);

#include "gm_app.c"

/* Die Stimmen addieren in l/r; Delay- und Hall-Anteil des Sounds gehen direkt in die Eingänge der Effekte. */
static float mk_dly[GM_BLOCK], mk_rev[GM_BLOCK];
static __attribute__((section(".dsp_text"), noinline)) void kern_render(unsigned p, float *l, float *r, unsigned n)
{
    if (!mk_ok) return;
    float lv = part_level[p];
    for (unsigned i = 0; i < n; i++) mk_dly[i] = mk_rev[i] = 0;
    mks50_bs_set_bpm(eng, (double)bpm_now);
    mks50_bs_render_sends(eng, l, r, mk_dly, mk_rev, (int)n);
    for (unsigned i = 0; i < n; i++) { fx_in[0][i] += mk_dly[i] * lv; fx_in[1][i] += mk_rev[i] * lv; }
}

/* ---- Bild: die Hüllkurve des MKS-50 (T1 auf L1, T2 auf L2, T3 auf L3 = Halten, T4 auf null) */
static int kern_stage(unsigned stage, unsigned p)
{
    if (stage != ST_ENV_PIC) return 0;
    const uint8_t *v = up[p].v;
    int32_t yb = 140, yt = 44, h = yb - yt;
    int32_t x1 = 10 + v[MP_T1] * 40 / 127, x2 = x1 + 4 + v[MP_T2] * 40 / 127, x3 = x2 + 4 + v[MP_T3] * 40 / 127,
            xs = x3 + 30, x4 = xs + 4 + v[MP_T4] * 40 / 127;
    int32_t y1 = yb - h * v[MP_L1] / 127, y2 = yb - h * v[MP_L2] / 127, y3 = yb - h * v[MP_L3] / 127;
    LN(10, yb, x1, y1, T_SEL); LN(x1, y1, x2, y2, T_SEL); LN(x2, y2, x3, y3, T_SEL); LN(x3, y3, xs, y3, T_SEL);
    LN(xs, y3, x4, yb, T_SEL);
    R(10, yb + 1, 220, 1, T_LINE);
    draw_sound_line(30);
    return 1;
}
