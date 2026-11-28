/* SPDX-License-Identifier: GPL-3.0-only */
/* JP-6 solo für das M-VAVE FM-1 (ADR 0048, 0044): der Baustein bausteine/jp6 (engine/jp6.c mit der Distortion
 * engine/dist.c von FM1-Electro, Port der JP-6-iOS-App des Nutzers) auf dem gemeinsamen Gerüst
 * bausteine/varianten/gemeinsam. Kern 0 rechnet die Stimmen, Kern 1 Delay und Hall.
 *
 * Werte im Speicher = Rohwerte des Kerns (JP_*, 0..jp6_param_max), ein Parameter des Gerüsts je Parameter des Kerns.
 * Sounds: die eigenen des Nutzers (ADR 0051, Gruppe EIGENE, vorn) und die Werks-Sounds, wie im Baustein.
 * Delay- und Hall-Anteil gehören zum Sound (JP_DELAY_SEND, JP_REVERB_SEND, wie in FM1-Electro). */
#define APP_NAME "JP-6"
#define GM_TITLE "JP-6"
#define GM_ACCENT RGB(230, 170, 40)             /* das Bernstein der Jupiter-Beschriftung */
#define GM_PARTS 1u
#define GM_NAMEMAX 10u
#define GM_SOUND_VER 1u                         /* Format der eigenen Sounds: 69 Rohwerte JP_* */
#define GM_JR_MAGIC 0x5453504Au                 /* "JPST": Journal dieser Firmware */
#define GM_SEND_FIRST 0                         /* Delay-/Hall-Anteil steht im Sound */
#define GM_FX_FIRST_ON 1
#define GM_OUT_GAIN (32767.0f * 0.6f * 1.4f)    /* FM1-Electro spielt den JP-6 mit Pegel 1,4 (engine/synth.c) */
#include "gm_types.h"
/* engine/el_config.h deckt sich hier mit shared/syn_config.h (über syn_fx.h schon geladen); die App braucht aus jp6.h
 * nur die Namen, der Kern selbst wird für sich mit el_config.h übersetzt. */
#define EL_CONFIG_H
#include "jp6.h"
#include "jp6/jp6_bs.h"
#include "jp_sounds.h"

#define GM_NP JP_COUNT

static void t_put(char *b, const char *s) { while ((*b++ = *s++) != 0) {} }
static void t_num(char *b, unsigned v)
{
    char t[4];
    int n = 0;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 3);
    while (n) *b++ = t[--n];
    *b = 0;
}
/* Name des Werts aus dem Kern, sonst die Zahl (HPF: 0 = OFF, sonst die Zahl) */
#define VN(P) static void t_##P(uint8_t v, char *b) { const char *s = jp6_param_value_name(P, v); if (s) t_put(b, s); else t_num(b, v); }
VN(JP_VCO1_TRI) VN(JP_VCO1_SAW) VN(JP_VCO1_PULSE) VN(JP_VCO1_SQUARE) VN(JP_VCO1_RANGE) VN(JP_SYNC) VN(JP_VCO2_TRI)
VN(JP_VCO2_SAW) VN(JP_VCO2_PULSE) VN(JP_VCO2_NOISE) VN(JP_VCO2_RANGE) VN(JP_VCO2_TUNE) VN(JP_PWM_SOURCE)
VN(JP_VCO_MOD_DEST) VN(JP_HPF) VN(JP_VCF_MODE) VN(JP_VCF_ENV_SRC) VN(JP_ENV1_POL) VN(JP_LFO_WAVE) VN(JP_KEY_MODE)
VN(JP_PORTA) VN(JP_KEY_SHIFT) VN(JP_LFO_SYNC) VN(JP_LFO_DIV) VN(JP_LFO_KEYSYNC) VN(JP_ENV_RETRIG) VN(JP_CHORUS)
VN(JP_CHORUS_MODE) VN(JP_DIST_MODE) VN(JP_CHORD)

#define T(P) t_##P
static const gm_param_t gm_params[GM_NP] = {
    [JP_VCO1_TRI] = {"1 TRI", 1, C_SRC, T(JP_VCO1_TRI)}, [JP_VCO1_SAW] = {"1 SAW", 1, C_SRC, T(JP_VCO1_SAW)},
    [JP_VCO1_PULSE] = {"1 PULSE", 1, C_SRC, T(JP_VCO1_PULSE)}, [JP_VCO1_SQUARE] = {"1 SQR", 1, C_SRC, T(JP_VCO1_SQUARE)},
    [JP_VCO1_RANGE] = {"1 RANGE", 4, C_SRC, T(JP_VCO1_RANGE)}, [JP_XMOD] = {"X-MOD", 127, C_MOD, 0},
    [JP_SYNC] = {"SYNC", 2, C_SRC, T(JP_SYNC)},
    [JP_VCO2_TRI] = {"2 TRI", 1, C_SRC, T(JP_VCO2_TRI)}, [JP_VCO2_SAW] = {"2 SAW", 1, C_SRC, T(JP_VCO2_SAW)},
    [JP_VCO2_PULSE] = {"2 PULSE", 1, C_SRC, T(JP_VCO2_PULSE)}, [JP_VCO2_NOISE] = {"NOISE", 1, C_SRC, T(JP_VCO2_NOISE)},
    [JP_VCO2_RANGE] = {"2 RANGE", 5, C_SRC, T(JP_VCO2_RANGE)}, [JP_VCO2_TUNE] = {"2 TUNE", 24, C_SRC, T(JP_VCO2_TUNE)},
    [JP_VCO2_FINE] = {"2 FINE", 127, C_SRC, 0},
    [JP_PWM_DEPTH] = {"PW/PWM", 127, C_SRC, 0}, [JP_PWM_SOURCE] = {"PWM SRC", 2, C_MOD, T(JP_PWM_SOURCE)},
    [JP_VCO_LFO] = {"VCO LFO", 127, C_MOD, 0}, [JP_VCO_ENV] = {"PIT ENV", 127, C_MOD, 0},
    [JP_VCO_MOD_DEST] = {"MOD DST", 2, C_MOD, T(JP_VCO_MOD_DEST)}, [JP_MIX] = {"MIX", 127, C_SRC, 0},
    [JP_HPF] = {"HPF", 127, C_FLT, T(JP_HPF)}, [JP_VCF_MODE] = {"VCF MD", 2, C_FLT, T(JP_VCF_MODE)},
    [JP_CUTOFF] = {"CUTOFF", 127, C_FLT, 0}, [JP_RESO] = {"RESO", 127, C_FLT, 0},
    [JP_VCF_ENV] = {"VCF ENV", 127, C_FLT, 0}, [JP_VCF_ENV_SRC] = {"ENV SRC", 1, C_FLT, T(JP_VCF_ENV_SRC)},
    [JP_VCF_LFO] = {"VCF LFO", 127, C_MOD, 0}, [JP_VCF_KEY] = {"KEY FLW", 127, C_FLT, 0},
    [JP_VCF_VELO] = {"VCF VEL", 127, C_FLT, 0},
    [JP_VCA_LEVEL] = {"LEVEL", 127, C_AMP, 0}, [JP_VCA_LFO] = {"VCA LFO", 127, C_MOD, 0},
    [JP_VCA_VELO] = {"VCA VEL", 127, C_AMP, 0},
    [JP_LFO_RATE] = {"RATE", 127, C_MOD, 0}, [JP_LFO_DELAY] = {"DELAY", 127, C_MOD, 0},
    [JP_LFO_WAVE] = {"WAVE", 6, C_MOD, T(JP_LFO_WAVE)},
    [JP_ENV1_A] = {"1 ATT", 127, C_AMP, 0}, [JP_ENV1_D] = {"1 DEC", 127, C_AMP, 0},
    [JP_ENV1_S] = {"1 SUS", 127, C_AMP, 0}, [JP_ENV1_R] = {"1 REL", 127, C_AMP, 0},
    [JP_ENV1_KEY] = {"1 KEY", 127, C_AMP, 0}, [JP_ENV1_POL] = {"1 POL", 1, C_AMP, T(JP_ENV1_POL)},
    [JP_ENV2_A] = {"2 ATT", 127, C_AMP, 0}, [JP_ENV2_D] = {"2 DEC", 127, C_AMP, 0},
    [JP_ENV2_S] = {"2 SUS", 127, C_AMP, 0}, [JP_ENV2_R] = {"2 REL", 127, C_AMP, 0},
    [JP_ENV2_KEY] = {"2 KEY", 127, C_AMP, 0}, [JP_XMOD_ENV] = {"XMD ENV", 127, C_MOD, 0},
    [JP_KEY_MODE] = {"MODE", 3, C_AMP, T(JP_KEY_MODE)}, [JP_PORTA_TIME] = {"GLIDE", 127, C_AMP, 0},
    [JP_PORTA] = {"PORTA", 2, C_AMP, T(JP_PORTA)}, [JP_VOLUME] = {"VOLUME", 127, C_AMP, 0},
    [JP_KEY_SHIFT] = {"SHIFT", 24, C_SRC, T(JP_KEY_SHIFT)}, [JP_DRIFT] = {"DRIFT", 127, C_SRC, 0},
    [JP_UNISON_DETUNE] = {"UNI DET", 127, C_SRC, 0},
    [JP_LFO_SYNC] = {"SYNC", 1, C_MOD, T(JP_LFO_SYNC)}, [JP_LFO_DIV] = {"NOTE", 13, C_MOD, T(JP_LFO_DIV)},
    [JP_LFO_KEYSYNC] = {"KEYSYNC", 1, C_MOD, T(JP_LFO_KEYSYNC)}, [JP_ENV_RETRIG] = {"RETRIG", 1, C_AMP, T(JP_ENV_RETRIG)},
    [JP_CHORUS] = {"CHORUS", 1, C_FX, T(JP_CHORUS)}, [JP_CHORUS_RATE] = {"CHO RT", 127, C_FX, 0},
    [JP_CHORUS_MODE] = {"CHO MD", 3, C_FX, T(JP_CHORUS_MODE)},
    [JP_DIST_MODE] = {"DIST", 4, C_FX, T(JP_DIST_MODE)}, [JP_DIST_DRIVE] = {"DRIVE", 127, C_FX, 0},
    [JP_DIST_TONE] = {"TONE", 127, C_FX, 0}, [JP_DIST_LEVEL] = {"D LEVEL", 127, C_FX, 0},
    [JP_DIST_MIX] = {"D MIX", 127, C_FX, 0},
    [JP_DELAY_SEND] = {"DELAY", 127, C_FX, 0}, [JP_REVERB_SEND] = {"REVERB", 127, C_FX, 0},
    [JP_CHORD] = {"CHORD", JP6_CHORDS - 1, C_AMP, T(JP_CHORD)},
};
/* INIT: INIT TONE der JP-6-App (Param.defaultValue, wie bausteine/jp6/tools/eigene2c.py) */
static const uint8_t gm_init_v[GM_NP] = {
    0, 1, 0, 0, 2, 0, 0, 0, 1, 0, 0, 3, 12, 64, 0, 1, 0, 0, 2, 64, 0, 0, 127, 0, 0, 0, 0, 0, 0, 110, 0, 0, 64, 0, 0,
    0, 64, 64, 40, 0, 0, 0, 64, 127, 30, 0, 0, 0, 30, 0, 100, 12, 16, 40, 0, 8, 0, 0, 0, 60, 0, 0, 64, 80, 64, 127,
    0, 0, 0};

/* ---- Sounds: die Liste des Bausteins (eigene zuerst), Gruppen aus jp_sounds.h */
#define GM_NCAT JP_NCAT
#define gm_cat_name jp_cat_names
typedef struct { const char *name; uint8_t cat; const uint8_t *v; } gm_sound_t;
#define GM_NFAC JP_NSOUND
static gm_sound_t gm_factory[GM_NFAC];

/* ---- Seiten (fm1-synths ADR 0008: KNOB 1–4 = vier Felder; Aufteilung nach dem Panel des Jupiter-6) */
enum { ST_ENV1 = ST_USER, ST_ENV2 };
static const page_t gm_pages[] = {
    {G_HOME, ST_HOME, "PLAY", {PRM("CUTOFF", JP_CUTOFF), PRM("RESO", JP_RESO), PRM("ENV", JP_VCF_ENV), PRM("RELEASE", JP_ENV2_R)}},
    {G_HOME, ST_NAME, "PERFORM", {PRM("CHORUS", JP_CHORUS), PRM("DELAY", JP_DELAY_SEND), PRM("REVERB", JP_REVERB_SEND), PRM("VOLUME", JP_VOLUME)}},
    {G_EDIT, ST_NAME, "VCO-1", {PRM("TRI", JP_VCO1_TRI), PRM("SAW", JP_VCO1_SAW), PRM("PULSE", JP_VCO1_PULSE), PRM("SQUARE", JP_VCO1_SQUARE)}},
    {G_EDIT, ST_NAME, "VCO-1 MOD", {PRM("RANGE", JP_VCO1_RANGE), PRM("X-MOD", JP_XMOD), PRM("XMD ENV", JP_XMOD_ENV), PRM("SYNC", JP_SYNC)}},
    {G_EDIT, ST_NAME, "VCO-2", {PRM("TRI", JP_VCO2_TRI), PRM("SAW", JP_VCO2_SAW), PRM("PULSE", JP_VCO2_PULSE), PRM("NOISE", JP_VCO2_NOISE)}},
    {G_EDIT, ST_NAME, "VCO-2 TUNE", {PRM("RANGE", JP_VCO2_RANGE), PRM("TUNE", JP_VCO2_TUNE), PRM("FINE", JP_VCO2_FINE), PRM("MIX", JP_MIX)}},
    {G_EDIT, ST_NAME, "VCO MOD", {PRM("LFO", JP_VCO_LFO), PRM("ENV", JP_VCO_ENV), PRM("DEST", JP_VCO_MOD_DEST), PRM("DRIFT", JP_DRIFT)}},
    {G_EDIT, ST_NAME, "PWM", {PRM("PW/PWM", JP_PWM_DEPTH), PRM("SOURCE", JP_PWM_SOURCE), NO, NO}},
    {G_EDIT, ST_NAME, "VCF", {PRM("CUTOFF", JP_CUTOFF), PRM("RESO", JP_RESO), PRM("ENV", JP_VCF_ENV), PRM("MODE", JP_VCF_MODE)}},
    {G_EDIT, ST_NAME, "VCF MOD", {PRM("HPF", JP_HPF), PRM("ENV SRC", JP_VCF_ENV_SRC), PRM("LFO", JP_VCF_LFO), PRM("KEY", JP_VCF_KEY)}},
    {G_EDIT, ST_NAME, "VCA", {PRM("LEVEL", JP_VCA_LEVEL), PRM("LFO", JP_VCA_LFO), PRM("VCF VEL", JP_VCF_VELO), PRM("VCA VEL", JP_VCA_VELO)}},
    {G_ENV, ST_ENV1, "ENV-1", {PRM("ATTACK", JP_ENV1_A), PRM("DECAY", JP_ENV1_D), PRM("SUSTAIN", JP_ENV1_S), PRM("RELEASE", JP_ENV1_R)}},
    {G_ENV, ST_ENV2, "ENV-2", {PRM("ATTACK", JP_ENV2_A), PRM("DECAY", JP_ENV2_D), PRM("SUSTAIN", JP_ENV2_S), PRM("RELEASE", JP_ENV2_R)}},
    {G_ENV, ST_NAME, "ENV KEY", {PRM("1 KEY", JP_ENV1_KEY), PRM("1 POL", JP_ENV1_POL), PRM("2 KEY", JP_ENV2_KEY), PRM("RETRIG", JP_ENV_RETRIG)}},
    {G_LFO, ST_NAME, "LFO", {PRM("RATE", JP_LFO_RATE), PRM("DELAY", JP_LFO_DELAY), PRM("WAVE", JP_LFO_WAVE), PRM("SYNC", JP_LFO_SYNC)}},
    {G_LFO, ST_NAME, "LFO 2", {PRM("NOTE", JP_LFO_DIV), PRM("KEYSYNC", JP_LFO_KEYSYNC), NO, NO}},
    {G_LFO, ST_NAME, "VOICE", {PRM("MODE", JP_KEY_MODE), PRM("PORTA", JP_PORTA), PRM("GLIDE", JP_PORTA_TIME), PRM("SHIFT", JP_KEY_SHIFT)}},
    {G_LFO, ST_NAME, "VOICE 2", {PRM("UNI DET", JP_UNISON_DETUNE), PRM("CHORD", JP_CHORD), NO, NO}},
    {G_FX, ST_NAME, "DIST", {PRM("DIST", JP_DIST_MODE), PRM("DRIVE", JP_DIST_DRIVE), PRM("TONE", JP_DIST_TONE), PRM("LEVEL", JP_DIST_LEVEL)}},
    {G_FX, ST_NAME, "CHORUS", {PRM("CHORUS", JP_CHORUS), PRM("MODE", JP_CHORUS_MODE), PRM("RATE", JP_CHORUS_RATE), PRM("D MIX", JP_DIST_MIX)}},
    {G_FX, ST_NAME, "SENDS", {PRM("DELAY", JP_DELAY_SEND), PRM("REVERB", JP_REVERB_SEND), FXK("DLY LVL", FX_DLY_LEVEL), FXK("REV LVL", FX_REV_LEVEL)}},
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
    [G_EDIT] = {"VCO", {PRM("X-MOD", JP_XMOD), PRM("2 TUNE", JP_VCO2_TUNE), PRM("MIX", JP_MIX), PRM("PW/PWM", JP_PWM_DEPTH)}},
    [G_ENV] = {"ENV-1 / ENV-2", {PRM("1 ATT", JP_ENV1_A), PRM("1 DEC", JP_ENV1_D), PRM("2 ATT", JP_ENV2_A), PRM("2 REL", JP_ENV2_R)}},
    [G_LFO] = {"LFO", {PRM("RATE", JP_LFO_RATE), PRM("DELAY", JP_LFO_DELAY), PRM("PITCH", JP_VCO_LFO), PRM("FILTER", JP_VCF_LFO)}},
    [G_FX] = {"EFFECTS", {PRM("DELAY", JP_DELAY_SEND), PRM("REVERB", JP_REVERB_SEND), PRM("DRIVE", JP_DIST_DRIVE), PRM("CHORUS", JP_CHORUS)}},
    [G_ARP] = {"ARP", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCT", ARP_OCT)}},
    [G_GLO] = {"VOICE", {PRM("MODE", JP_KEY_MODE), PRM("GLIDE", JP_PORTA_TIME), PRM("SHIFT", JP_KEY_SHIFT), PRM("VOLUME", JP_VOLUME)}},
    [G_SEQ] = {"SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
};

/* ---- der Klang: Baustein jp6 (engine/jp6.c, engine/dist.c), Zustand im Pool */
#define JP_STATE_MAX 8192u                      /* Zustand des Bausteins (PC: 6,3 KB; kern_boot prüft) */
static uint8_t eng[JP_STATE_MAX] __attribute__((section(".pool"), aligned(16)));
static uint8_t jp_ok;
static void kern_boot(void)
{
    for (unsigned s = 0; s < GM_NFAC; s++) {
        gm_factory[s].name = jp6_bs_preset_name(s);
        gm_factory[s].cat = jp_sound_cat[s];
        gm_factory[s].v = jp6_bs_preset_values(s);
    }
    jp_ok = jp6_bs_state_size() <= JP_STATE_MAX && jp6_bs_preset_count() == GM_NFAC;
    if (jp_ok) jp6_bs_init(eng, 44100.0);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_set(unsigned p, unsigned i, uint8_t v)
{
    (void)p;
    if (jp_ok && i < GM_NP) jp6_bs_set_raw(eng, i, v);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_load(unsigned p, const uint8_t *v) { (void)p; if (jp_ok) jp6_bs_load_values(eng, v); }
static __attribute__((section(".dsp_text"), noinline)) void kern_on(unsigned p, int note, int vel, int legato)
{
    (void)p;
    if (jp_ok) jp6_bs_note_on(eng, note, vel, legato);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_off(unsigned p, int note) { (void)p; if (jp_ok) jp6_bs_note_off(eng, note); }
static __attribute__((section(".dsp_text"), noinline)) void kern_all_off(unsigned p) { (void)p; if (jp_ok) jp6_bs_all_off(eng); }
/* Bender und Modulationsrad: der Kern hat keine Eingänge dafür */
static __attribute__((section(".dsp_text"), noinline)) void kern_bend(unsigned p, float b) { (void)p; (void)b; }
static __attribute__((section(".dsp_text"), noinline)) void kern_mod(unsigned p, float m) { (void)p; (void)m; }
static void kern_render(unsigned p, float *l, float *r, unsigned n);   /* nach gm_app.c: braucht fx_in */
static int kern_voices(unsigned p) { (void)p; return jp_ok ? jp6_bs_voices(eng) : 0; }
static int kern_stage(unsigned stage, unsigned p);

#include "gm_app.c"

/* Die Stimmen addieren in l/r; Delay- und Hall-Anteil des Sounds gehen direkt in die Eingänge der Effekte. */
static float jp_dly[GM_BLOCK], jp_rev[GM_BLOCK];
static __attribute__((section(".dsp_text"), noinline)) void kern_render(unsigned p, float *l, float *r, unsigned n)
{
    if (!jp_ok) return;
    float lv = part_level[p];
    for (unsigned i = 0; i < n; i++) jp_dly[i] = jp_rev[i] = 0;
    jp6_bs_set_bpm(eng, (double)bpm_now);
    jp6_bs_render_sends(eng, l, r, jp_dly, jp_rev, (int)n);
    for (unsigned i = 0; i < n; i++) { fx_in[0][i] += jp_dly[i] * lv; fx_in[1][i] += jp_rev[i] * lv; }
}

/* ---- Bilder: die Hüllkurven (Anstieg, Abfall auf das Halten, Halten, Ausklingen) */
static int kern_stage(unsigned stage, unsigned p)
{
    if (stage != ST_ENV1 && stage != ST_ENV2) return 0;
    const uint8_t *v = up[p].v;
    unsigned b = stage == ST_ENV1 ? JP_ENV1_A : JP_ENV2_A;
    int32_t xa = 10 + v[b] * 50 / 127, xd = xa + 6 + v[b + 1] * 60 / 127, xs = xd + 40,
            xr = xs + 6 + v[b + 3] * 60 / 127, yb = 140, yt = 44, ys = yb - (yb - yt) * v[b + 2] / 127;
    LN(10, yb, xa, yt, T_SEL); LN(xa, yt, xd, ys, T_SEL); LN(xd, ys, xs, ys, T_SEL); LN(xs, ys, xr, yb, T_SEL);
    R(10, yb + 1, 220, 1, T_LINE);
    if (stage == ST_ENV1 && v[JP_ENV1_POL]) Q(4, 30, 2, "ENV-1: INV", T_DIM);
    draw_sound_line(30);
    return 1;
}
