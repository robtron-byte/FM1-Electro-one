/* SPDX-License-Identifier: GPL-3.0-only */
/* JX-8P solo für das M-VAVE FM-1 (ADR 0046, 0044): der Baustein bausteine/jx8p (engine/jx8p.c von FM1-Electro, aus
 * fm1-synths, dort aus der JX-8P-iOS-App des Nutzers) auf dem gemeinsamen Gerüst bausteine/varianten/gemeinsam.
 * Löst die fm1-synths-Fassung (FM-1_97) ab. Kern 0 rechnet die Stimmen, Kern 1 Delay und Hall.
 *
 * Werte im Speicher = Rohwerte des Kerns (JX_*, 0..jx8p_param_max), ein Parameter des Gerüsts je Parameter des Kerns.
 * Sounds: 74 = die eigenen des Nutzers (ADR 0051, Gruppe EIGENE, vorn) und die Werks-Sounds, wie im Baustein.
 * Delay- und Hall-Anteil gehören zum Sound (JX_DELAY_SEND, JX_REVERB_SEND, wie in FM1-Electro und fm1-synths). */
#define APP_NAME "JX-8P"
#define GM_TITLE "JX-8P"
#define GM_ACCENT RGB(214, 70, 40)              /* das Orange-Rot der JX-8P-Beschriftung */
#define GM_PARTS 1u
#define GM_NAMEMAX 10u
#define GM_SOUND_VER 1u                         /* Format der eigenen Sounds: 68 Rohwerte JX_* */
#define GM_JR_MAGIC 0x5453584Au                 /* "JXST": Journal dieser Firmware */
#include "gm_types.h"
/* engine/el_config.h deckt sich hier mit shared/syn_config.h (über syn_fx.h schon geladen: EL_DSP_CODE, el_clampi);
 * die App braucht aus jx8p.h nur die Namen (JX_*, Typen), der Kern selbst wird für sich mit el_config.h übersetzt. */
#define EL_CONFIG_H
#include "jx8p.h"
#include "jx8p/jx8p_bs.h"
#include "jx_sounds.h"

#ifndef JX_VOICES
#define JX_VOICES JX8P_BS_VOICES                /* Stimmen (tools/build.py setzt JX8P_BS_VOICES nach der Messung) */
#endif
#define GM_NP JX_COUNT

/* (die Anzeigen laufen vor platform.c: eigene kleine Helfer statt str_cpy / fmt_int) */
static void t_put(char *b, const char *s) { if (!s) s = "?"; while ((*b++ = *s++) != 0) {} }
#define VN(P) static void t_##P(uint8_t v, char *b) { t_put(b, jx8p_param_value_name(P, v)); }
VN(JX_DCO1_RANGE) VN(JX_DCO1_WAVE) VN(JX_DCO1_TUNE) VN(JX_DCO2_RANGE) VN(JX_DCO2_WAVE) VN(JX_DCO2_XMOD)
VN(JX_DCO2_TUNE) VN(JX_DCO_ENV_MODE) VN(JX_MIX_ENV_MODE) VN(JX_VCF_ENV_MODE) VN(JX_CHORUS) VN(JX_LFO_WAVE)
VN(JX_VCA_ENV_MODE) VN(JX_CHORUS_MODE) VN(JX_KEY_MODE) VN(JX_PORTA) VN(JX_KEY_SHIFT) VN(JX_LFO_SHAPE) VN(JX_LFO_SYNC)
VN(JX_LFO_DIV) VN(JX_LFO_KEYSYNC) VN(JX_ENV_RETRIG) VN(JX_ENV_SNAP)
static void t_dyna(uint8_t v, char *b) { static const char *const n[4] = {"OFF", "1", "2", "3"}; t_put(b, n[v & 3u]); }
static void t_chord(uint8_t v, char *b) { t_put(b, mks_chord_names[v < MKS_CHORDS ? v : 0]); }

#define T(P) t_##P
static const gm_param_t gm_params[GM_NP] = {
    [JX_DCO1_RANGE] = {"1 RANGE", 3, C_SRC, T(JX_DCO1_RANGE)}, [JX_DCO1_WAVE] = {"1 WAVE", 3, C_SRC, T(JX_DCO1_WAVE)},
    [JX_DCO1_TUNE] = {"1 TUNE", 24, C_SRC, T(JX_DCO1_TUNE)}, [JX_DCO1_LFO] = {"1 LFO", 127, C_MOD, 0},
    [JX_DCO1_ENV] = {"1 ENV", 127, C_MOD, 0},
    [JX_DCO2_RANGE] = {"2 RANGE", 3, C_SRC, T(JX_DCO2_RANGE)}, [JX_DCO2_WAVE] = {"2 WAVE", 3, C_SRC, T(JX_DCO2_WAVE)},
    [JX_DCO2_XMOD] = {"X-MOD", 3, C_SRC, T(JX_DCO2_XMOD)}, [JX_DCO2_TUNE] = {"2 TUNE", 24, C_SRC, T(JX_DCO2_TUNE)},
    [JX_DCO2_FINE] = {"2 FINE", 127, C_SRC, 0}, [JX_DCO2_LFO] = {"2 LFO", 127, C_MOD, 0},
    [JX_DCO2_ENV] = {"2 ENV", 127, C_MOD, 0},
    [JX_DCO_DYNA] = {"DCO DYN", 3, C_MOD, t_dyna}, [JX_DCO_ENV_MODE] = {"DCO EMD", 3, C_MOD, T(JX_DCO_ENV_MODE)},
    [JX_MIX_DCO1] = {"DCO-1", 127, C_SRC, 0}, [JX_MIX_DCO2] = {"DCO-2", 127, C_SRC, 0},
    [JX_MIX_ENV] = {"MIX ENV", 127, C_MOD, 0}, [JX_MIX_DYNA] = {"MIX DYN", 3, C_MOD, t_dyna},
    [JX_MIX_ENV_MODE] = {"MIX EMD", 3, C_MOD, T(JX_MIX_ENV_MODE)},
    [JX_HPF] = {"HPF", 3, C_FLT, 0}, [JX_CUTOFF] = {"CUTOFF", 127, C_FLT, 0}, [JX_RESO] = {"RESO", 127, C_FLT, 0},
    [JX_VCF_LFO] = {"VCF LFO", 127, C_MOD, 0}, [JX_VCF_ENV] = {"VCF ENV", 127, C_FLT, 0},
    [JX_VCF_KEY] = {"KEY FLW", 127, C_FLT, 0}, [JX_VCF_DYNA] = {"VCF DYN", 3, C_FLT, t_dyna},
    [JX_VCF_ENV_MODE] = {"VCF EMD", 3, C_FLT, T(JX_VCF_ENV_MODE)},
    [JX_VCA_LEVEL] = {"LEVEL", 127, C_AMP, 0}, [JX_VCA_DYNA] = {"VCA DYN", 3, C_AMP, t_dyna},
    [JX_CHORUS] = {"CHORUS", 2, C_FX, T(JX_CHORUS)}, [JX_LFO_WAVE] = {"WAVE", 2, C_MOD, T(JX_LFO_WAVE)},
    [JX_LFO_DELAY] = {"DELAY", 127, C_MOD, 0}, [JX_LFO_RATE] = {"RATE", 127, C_MOD, 0},
    [JX_E1_ATTACK] = {"1 ATT", 127, C_AMP, 0}, [JX_E1_DECAY] = {"1 DEC", 127, C_AMP, 0},
    [JX_E1_SUSTAIN] = {"1 SUS", 127, C_AMP, 0}, [JX_E1_RELEASE] = {"1 REL", 127, C_AMP, 0},
    [JX_E1_KEY] = {"1 KEY", 3, C_AMP, t_dyna},
    [JX_E2_ATTACK] = {"2 ATT", 127, C_AMP, 0}, [JX_E2_DECAY] = {"2 DEC", 127, C_AMP, 0},
    [JX_E2_SUSTAIN] = {"2 SUS", 127, C_AMP, 0}, [JX_E2_RELEASE] = {"2 REL", 127, C_AMP, 0},
    [JX_E2_KEY] = {"2 KEY", 3, C_AMP, t_dyna},
    [JX_VCA_ENV_MODE] = {"VCA MD", 1, C_AMP, T(JX_VCA_ENV_MODE)},
    [JX_BENDER] = {"BEND", 12, C_MOD, 0}, [JX_BEND_LFO] = {"BND LFO", 127, C_MOD, 0},
    [JX_AFTER_VIB] = {"AT VIB", 127, C_MOD, 0}, [JX_AFTER_BRIL] = {"AT BRIL", 127, C_MOD, 0},
    [JX_AFTER_VOL] = {"AT VOL", 127, C_MOD, 0},
    [JX_CHORUS_RATE] = {"CHO RT", 127, C_FX, 0}, [JX_CHORUS_MODE] = {"CHO MD", 2, C_FX, T(JX_CHORUS_MODE)},
    [JX_KEY_MODE] = {"MODE", 3, C_AMP, T(JX_KEY_MODE)}, [JX_PORTA_TIME] = {"GLIDE", 127, C_AMP, 0},
    [JX_PORTA] = {"PORTA", 2, C_AMP, T(JX_PORTA)}, [JX_MOD_SENS] = {"MOD SNS", 127, C_MOD, 0},
    [JX_VOLUME] = {"VOLUME", 127, C_AMP, 0}, [JX_KEY_SHIFT] = {"SHIFT", 24, C_SRC, T(JX_KEY_SHIFT)},
    [JX_DRIFT] = {"DRIFT", 127, C_SRC, 0}, [JX_UNISON_DETUNE] = {"UNI DET", 127, C_SRC, 0},
    [JX_LFO_SHAPE] = {"SHAPE", 7, C_MOD, T(JX_LFO_SHAPE)}, [JX_LFO_SYNC] = {"SYNC", 1, C_MOD, T(JX_LFO_SYNC)},
    [JX_LFO_DIV] = {"NOTE", 13, C_MOD, T(JX_LFO_DIV)}, [JX_LFO_KEYSYNC] = {"KEYSYNC", 1, C_MOD, T(JX_LFO_KEYSYNC)},
    [JX_ENV_RETRIG] = {"RETRIG", 1, C_AMP, T(JX_ENV_RETRIG)}, [JX_ENV_SNAP] = {"SNAP", 1, C_AMP, T(JX_ENV_SNAP)},
    [JX_DELAY_SEND] = {"DELAY", 127, C_FX, 0}, [JX_REVERB_SEND] = {"REVERB", 127, C_FX, 0},
    [JX_CHORD] = {"CHORD", MKS_CHORDS - 1, C_AMP, t_chord},
};
/* INIT: PG8X INIT (INIT RHY der Werks-Sounds) */
static const uint8_t gm_init_v[GM_NP] = {
    3, 3, 12, 0, 0, 2, 3, 0, 12, 64, 0, 0, 0, 3, 127, 83, 26, 0, 3, 3, 79, 70, 0, 66, 0, 0, 3, 112, 2, 1, 2, 0, 1, 21,
    52, 96, 24, 0, 27, 60, 82, 21, 0, 1, 2, 94, 0, 0, 0, 60, 0, 0, 30, 0, 64, 100, 12, 8, 40, 0, 0, 8, 0, 0, 0, 0, 0, 0};

/* ---- Sounds: die Liste des Bausteins (eigene zuerst), Gruppen aus jx_sounds.h */
#define GM_NCAT JX_NCAT
#define gm_cat_name jx_cat_names
typedef struct { const char *name; uint8_t cat; const uint8_t *v; } gm_sound_t;
#define GM_NFAC JX_NSOUND
static gm_sound_t gm_factory[GM_NFAC];

/* ---- Seiten: KNOB 1–4 = vier Felder (fm1-synths ADR 0008, Aufteilung wie fm1-synths jx8p/app/jx_ui.c) */
enum { ST_ENV1 = ST_USER, ST_ENV2 };
static const page_t gm_pages[] = {
    {G_HOME, ST_HOME, "PLAY", {PRM("CUTOFF", JX_CUTOFF), PRM("RESO", JX_RESO), PRM("ENV", JX_VCF_ENV), PRM("RELEASE", JX_E2_RELEASE)}},
    {G_HOME, ST_NAME, "PERFORM", {PRM("CHORUS", JX_CHORUS), PRM("DELAY", JX_DELAY_SEND), PRM("REVERB", JX_REVERB_SEND), PRM("VOLUME", JX_VOLUME)}},
    {G_EDIT, ST_NAME, "DCO-1", {PRM("RANGE", JX_DCO1_RANGE), PRM("WAVE", JX_DCO1_WAVE), PRM("TUNE", JX_DCO1_TUNE), PRM("LFO", JX_DCO1_LFO)}},
    {G_EDIT, ST_NAME, "DCO-2", {PRM("RANGE", JX_DCO2_RANGE), PRM("WAVE", JX_DCO2_WAVE), PRM("X-MOD", JX_DCO2_XMOD), PRM("TUNE", JX_DCO2_TUNE)}},
    {G_EDIT, ST_NAME, "DCO-2 FINE", {PRM("FINE", JX_DCO2_FINE), PRM("LFO", JX_DCO2_LFO), PRM("DRIFT", JX_DRIFT), NO}},
    {G_EDIT, ST_NAME, "DCO ENV", {PRM("DCO-1", JX_DCO1_ENV), PRM("DCO-2", JX_DCO2_ENV), PRM("ENV MD", JX_DCO_ENV_MODE), PRM("DYNA", JX_DCO_DYNA)}},
    {G_EDIT, ST_NAME, "MIXER", {PRM("DCO-1", JX_MIX_DCO1), PRM("DCO-2", JX_MIX_DCO2), PRM("ENV", JX_MIX_ENV), PRM("ENV MD", JX_MIX_ENV_MODE)}},
    {G_EDIT, ST_NAME, "MIXER DYN", {PRM("DYNA", JX_MIX_DYNA), NO, NO, NO}},
    {G_EDIT, ST_NAME, "VCF", {PRM("CUTOFF", JX_CUTOFF), PRM("RESO", JX_RESO), PRM("ENV", JX_VCF_ENV), PRM("HPF", JX_HPF)}},
    {G_EDIT, ST_NAME, "VCF MOD", {PRM("LFO", JX_VCF_LFO), PRM("KEY", JX_VCF_KEY), PRM("ENV MD", JX_VCF_ENV_MODE), PRM("DYNA", JX_VCF_DYNA)}},
    {G_EDIT, ST_NAME, "VCA", {PRM("LEVEL", JX_VCA_LEVEL), PRM("MODE", JX_VCA_ENV_MODE), PRM("DYNA", JX_VCA_DYNA), PRM("VOLUME", JX_VOLUME)}},
    {G_ENV, ST_ENV1, "ENV-1", {PRM("ATTACK", JX_E1_ATTACK), PRM("DECAY", JX_E1_DECAY), PRM("SUSTAIN", JX_E1_SUSTAIN), PRM("RELEASE", JX_E1_RELEASE)}},
    {G_ENV, ST_ENV2, "ENV-2", {PRM("ATTACK", JX_E2_ATTACK), PRM("DECAY", JX_E2_DECAY), PRM("SUSTAIN", JX_E2_SUSTAIN), PRM("RELEASE", JX_E2_RELEASE)}},
    {G_ENV, ST_NAME, "ENV KEY", {PRM("1 KEY", JX_E1_KEY), PRM("2 KEY", JX_E2_KEY), PRM("SNAP", JX_ENV_SNAP), PRM("RETRIG", JX_ENV_RETRIG)}},
    {G_LFO, ST_NAME, "LFO", {PRM("RATE", JX_LFO_RATE), PRM("DELAY", JX_LFO_DELAY), PRM("WAVE", JX_LFO_WAVE), PRM("SYNC", JX_LFO_SYNC)}},
    {G_LFO, ST_NAME, "LFO 2", {PRM("SHAPE", JX_LFO_SHAPE), PRM("NOTE", JX_LFO_DIV), PRM("KEYSYNC", JX_LFO_KEYSYNC), NO}},
    {G_LFO, ST_NAME, "VOICE", {PRM("MODE", JX_KEY_MODE), PRM("PORTA", JX_PORTA), PRM("GLIDE", JX_PORTA_TIME), PRM("SHIFT", JX_KEY_SHIFT)}},
    {G_LFO, ST_NAME, "VOICE 2", {PRM("UNI DET", JX_UNISON_DETUNE), PRM("CHORD", JX_CHORD), PRM("DRIFT", JX_DRIFT), NO}},
    {G_FX, ST_NAME, "CHORUS", {PRM("CHORUS", JX_CHORUS), PRM("MODE", JX_CHORUS_MODE), PRM("RATE", JX_CHORUS_RATE), NO}},
    {G_FX, ST_NAME, "SENDS", {PRM("DELAY", JX_DELAY_SEND), PRM("REVERB", JX_REVERB_SEND), FXK("DLY LVL", FX_DLY_LEVEL), FXK("REV LVL", FX_REV_LEVEL)}},
    {G_FX, ST_NAME, "DELAY", {FXK("TIME", FX_DLY_TIME), FXK("FEEDBACK", FX_DLY_FB), FXK("TONE", FX_DLY_TONE), FXK("PING", FX_DLY_PING)}},
    {G_FX, ST_NAME, "DELAY 2", {FXK("SYNC", FX_DLY_SYNC), FXK("NOTE", FX_DLY_DIV), FXK("MOD", FX_DLY_MOD), NO}},
    {G_FX, ST_NAME, "REVERB", {FXK("SIZE", FX_REV_SIZE), FXK("DECAY", FX_REV_DECAY), FXK("DAMP", FX_REV_DAMP), FXK("TYPE", FX_REV_TYPE)}},
    {G_ARP, ST_ARP, "ARP", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCT", ARP_OCT)}},
    {G_ARP, ST_ARP, "ARP 2", {ARK("GATE", ARP_GATE), ARK("HOLD", ARP_HOLD), NO, NO}},
    {G_SEQ, ST_SEQ, "SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
    {G_GLO, ST_TEMPO, "TEMPO", {{"TEMPO", K_TEMPO, 0}, {"CLOCK", K_CLK, 0}, {"SEND", K_CLK, 1}, {"MIDI CH", K_CHAN, 0}}},
    {G_GLO, ST_NAME, "PATCH", {PRM("BEND", JX_BENDER), PRM("BND LFO", JX_BEND_LFO), PRM("MOD SNS", JX_MOD_SENS), NO}},
    {G_GLO, ST_SYS, "SYSTEM", {{"LEVEL", K_LEVEL, 0}, NO, NO, NO}},
    {G_SEL, ST_BROWSE, "SOUNDS", {NO, NO, NO, NO}},
};
static const layer_t gm_layers[NGROUP] = {
    [G_EDIT] = {"DCO + MIXER", {PRM("X-MOD", JX_DCO2_XMOD), PRM("2 TUNE", JX_DCO2_TUNE), PRM("MIX 1", JX_MIX_DCO1), PRM("MIX 2", JX_MIX_DCO2)}},
    [G_ENV] = {"ENV-1 / ENV-2", {PRM("1 ATT", JX_E1_ATTACK), PRM("1 DEC", JX_E1_DECAY), PRM("2 ATT", JX_E2_ATTACK), PRM("2 REL", JX_E2_RELEASE)}},
    [G_LFO] = {"LFO", {PRM("RATE", JX_LFO_RATE), PRM("DELAY", JX_LFO_DELAY), PRM("PITCH", JX_DCO1_LFO), PRM("FILTER", JX_VCF_LFO)}},
    [G_FX] = {"EFFECTS", {PRM("DELAY", JX_DELAY_SEND), PRM("REVERB", JX_REVERB_SEND), FXK("TIME", FX_DLY_TIME), PRM("CHORUS", JX_CHORUS)}},
    [G_ARP] = {"ARP", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCT", ARP_OCT)}},
    [G_GLO] = {"VOICE", {PRM("MODE", JX_KEY_MODE), PRM("GLIDE", JX_PORTA_TIME), PRM("SHIFT", JX_KEY_SHIFT), PRM("VOLUME", JX_VOLUME)}},
    [G_SEQ] = {"SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
};

/* ---- der Klang: Baustein jx8p (engine/jx8p.c), Zustand im Pool */
#define JX_STATE_MAX 16384u
static uint8_t eng[JX_STATE_MAX] __attribute__((section(".pool"), aligned(16)));
static uint8_t jx_ok;
static void kern_boot(void)
{
    for (unsigned s = 0; s < GM_NFAC; s++) {
        gm_factory[s].name = jx8p_bs_preset_name(s);
        gm_factory[s].cat = jx_sound_cat[s];
        gm_factory[s].v = jx8p_bs_preset_values(s);
    }
    jx_ok = jx8p_bs_state_size() <= JX_STATE_MAX && jx8p_bs_preset_count() == GM_NFAC;
    if (jx_ok) jx8p_bs_init(eng, 44100.0);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_set(unsigned p, unsigned i, uint8_t v)
{
    (void)p;
    if (jx_ok && i < GM_NP) jx8p_bs_set_raw(eng, i, v);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_load(unsigned p, const uint8_t *v)
{
    (void)p;
    if (jx_ok) jx8p_bs_load_values(eng, v);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_on(unsigned p, int note, int vel, int legato)
{
    (void)p;
    if (jx_ok) jx8p_bs_note_on(eng, note, vel, legato);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_off(unsigned p, int note) { (void)p; if (jx_ok) jx8p_bs_note_off(eng, note); }
static __attribute__((section(".dsp_text"), noinline)) void kern_all_off(unsigned p) { (void)p; if (jx_ok) jx8p_bs_all_off(eng); }
/* Bender und Modulationsrad: der Kern hat keine Eingänge dafür (JX_BENDER usw. stehen nur im Sound) */
static __attribute__((section(".dsp_text"), noinline)) void kern_bend(unsigned p, float b) { (void)p; (void)b; }
static __attribute__((section(".dsp_text"), noinline)) void kern_mod(unsigned p, float m) { (void)p; (void)m; }
static void kern_render(unsigned p, float *l, float *r, unsigned n);   /* nach gm_app.c: braucht fx_in */
static int kern_voices(unsigned p) { (void)p; return jx_ok ? jx8p_bs_voices(eng) : 0; }
static int kern_stage(unsigned stage, unsigned p);

#include "gm_app.c"

/* Die Stimmen addieren in l/r; Delay- und Hall-Anteil des Sounds gehen direkt in die Eingänge der Effekte
 * (fx_in, vom Gerüst vor den Parts geleert), mit dem Pegel des Parts wie der trockene Klang. */
static float jx_dly[GM_BLOCK], jx_rev[GM_BLOCK];
static __attribute__((section(".dsp_text"), noinline)) void kern_render(unsigned p, float *l, float *r, unsigned n)
{
    if (!jx_ok) return;
    float lv = part_level[p];
    for (unsigned i = 0; i < n; i++) jx_dly[i] = jx_rev[i] = 0;
    jx8p_bs_set_bpm(eng, (double)bpm_now);
    jx8p_bs_render_sends(eng, l, r, jx_dly, jx_rev, (int)n);
    for (unsigned i = 0; i < n; i++) { fx_in[0][i] += jx_dly[i] * lv; fx_in[1][i] += jx_rev[i] * lv; }
}

/* ---- Bilder: die Hüllkurven (Anstieg, Abfall auf das Halten, Halten, Ausklingen) */
static int kern_stage(unsigned stage, unsigned p)
{
    if (stage != ST_ENV1 && stage != ST_ENV2) return 0;
    const uint8_t *v = up[p].v;
    unsigned b = stage == ST_ENV1 ? JX_E1_ATTACK : JX_E2_ATTACK;
    int32_t xa = 10 + v[b] * 50 / 127, xd = xa + 6 + v[b + 1] * 60 / 127, xs = xd + 40,
            xr = xs + 6 + v[b + 3] * 60 / 127, yb = 140, yt = 44, ys = yb - (yb - yt) * v[b + 2] / 127;
    LN(10, yb, xa, yt, T_SEL); LN(xa, yt, xd, ys, T_SEL); LN(xd, ys, xs, ys, T_SEL); LN(xs, ys, xr, yb, T_SEL);
    R(10, yb + 1, 220, 1, T_LINE);
    if (stage == ST_ENV2) Q(4, 30, 2, v[JX_VCA_ENV_MODE] ? "VCA: ENV-2" : "VCA: GATE", T_DIM);
    draw_sound_line(30);
    return 1;
}
