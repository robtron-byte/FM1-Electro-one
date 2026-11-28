/* SPDX-License-Identifier: GPL-3.0-only */
/* SH-101 solo für das M-VAVE FM-1 (ADR 0042, 0044): der Baustein bausteine/sh101 (MonoBass, eine Stimme in voller
 * Gerätequalität: 4× überabgetastete Ladder, 8× Clipper) auf dem gemeinsamen Gerüst bausteine/varianten/gemeinsam.
 * Kern 0 rechnet die Stimme, Kern 1 Delay und Hall. Sequencer und Arpeggiator kommen aus dem Gerüst (wie beim SH-101:
 * Step-Aufnahme mit REC, Transponieren mit den Tasten); der Arp/Seq des Bausteins bleibt aus (SH101_WITH_ARPSEQ=0).
 *
 * Werte im Speicher je Parameter 0..max (uint8_t). Regler wie die Schieber des SH-101: 0..200, angezeigt 0.0..10.0. */
#define APP_NAME "SH-101"
#define GM_TITLE "SH-101"
#define GM_ACCENT RGB(255, 60, 40)              /* das Rot des SH-101 */
#define GM_BAND_H 12u                           /* (kleinere Zeichenfläche: Platz für die Distortion-Tabellen) */
#define GM_PARTS 1u
#define GM_NAMEMAX 10u
#define GM_SOUND_VER 2u                         /* Format der eigenen Sounds (2: Distortion-Send); älter = weniger Parameter, ergänzt aus INIT */
#define GM_JR_MAGIC 0x54534853u                 /* "SHST": Journal dieser Firmware */
#include "gm_types.h"
#include "sh101/sh101.h"

/* ---- Parameter: Name, Höchstwert, Farbklasse, Anzeige; dazu der Parameter des Bausteins */
enum {
    P_RANGE, P_MOD, P_PW, P_PWM_SRC, P_PULSE, P_SAW, P_SUB, P_NOISE, P_SUB_MODE, P_CUTOFF, P_RESO, P_ENV, P_VCF_LFO,
    P_KYBD, P_VCA, P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE, P_TRIGGER, P_LFO_RATE, P_LFO_WAVE, P_GLIDE, P_GLIDE_MODE,
    P_TRANSPOSE, P_FINE, P_VOLUME, P_DRIVE, P_12DB, P_ENV_FULL, P_RES_COMP, P_WHITE, P_NOISE_FLOOR, P_LFO_SYNC,
    P_LFO_INV, P_LFO_TRIG, P_MOD_SNAP, P_LIN_GLIDE, P_DECLICK, P_VEL_VCA, P_VEL_VCF, P_BEND_VCO, P_BEND_VCF, P_MOD_WHEEL,
    P_FM_AMT, P_FM_SRC,
    P_DIST_SEND, P_DIST_MODE, P_DIST_DRIVE, P_DIST_TONE, P_DIST_LEVEL,   /* Distortion-Send (0.1.2, nicht im Baustein) */
    GM_NP
};
#define C200 200u
/* (die Anzeigen laufen vor platform.c: eigene kleine Helfer statt str_cpy / fmt_int) */
static void t_put(char *b, const char *s) { while ((*b++ = *s++) != 0) {} }
static void t_num(char *b, int v)
{
    char t[8];
    int n = 0;
    if (v < 0) { *b++ = '-'; v = -v; }
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 7);
    while (n) *b++ = t[--n];
    *b = 0;
}
static void t_slider(uint8_t v, char *b) { t_num(b, v / 20); while (*b) b++; *b++ = '.'; *b++ = (char)('0' + v % 20u / 2u); *b = 0; }
static void t_onoff(uint8_t v, char *b) { t_put(b, v ? "ON" : "OFF"); }
static void t_range(uint8_t v, char *b) { static const char *const n[4] = {"16'", "8'", "4'", "2'"}; t_put(b, n[v & 3u]); }
static void t_pwm(uint8_t v, char *b) { static const char *const n[3] = {"ENV", "MAN", "LFO"}; t_put(b, n[v % 3u]); }
static void t_sub(uint8_t v, char *b) { static const char *const n[3] = {"2OCT25", "2OCT", "1OCT"}; t_put(b, n[v % 3u]); }
static void t_vca(uint8_t v, char *b) { t_put(b, v ? "ENV" : "GATE"); }
static void t_trig(uint8_t v, char *b) { static const char *const n[3] = {"LFO", "GATE", "G+TRG"}; t_put(b, n[v % 3u]); }
static void t_wave(uint8_t v, char *b) { static const char *const n[4] = {"TRI", "SQR", "RAND", "NOISE"}; t_put(b, n[v & 3u]); }
static void t_gmode(uint8_t v, char *b) { static const char *const n[3] = {"AUTO", "OFF", "ON"}; t_put(b, n[v % 3u]); }
static void t_trans(uint8_t v, char *b) { static const char *const n[3] = {"-1", "0", "+1"}; t_put(b, n[v % 3u]); }
static void t_fine(uint8_t v, char *b) { int c = ((int)v - 100) / 2; if (c > 0) *b++ = '+'; t_num(b, c); }
static void t_fmsrc(uint8_t v, char *b) { static const char *const n[5] = {"OFF", "PULSE", "SAW", "SUB", "NOISE"}; t_put(b, n[v % 5u]); }
static void t_12(uint8_t v, char *b) { t_put(b, v ? "12DB" : "24DB"); }
static void t_int(uint8_t v, char *b) { t_num(b, v); }
static void t_dmode(uint8_t v, char *b) { static const char *const n[4] = {"TUBE", "OD", "DIST", "RAT"}; t_put(b, n[v & 3u]); }

static const gm_param_t gm_params[GM_NP] = {
    [P_RANGE] = {"RANGE", 3, C_SRC, t_range}, [P_MOD] = {"MOD", C200, C_MOD, t_slider},
    [P_PW] = {"PW", C200, C_SRC, t_slider}, [P_PWM_SRC] = {"PWM SRC", 2, C_SRC, t_pwm},
    [P_PULSE] = {"PULSE", C200, C_SRC, t_slider}, [P_SAW] = {"SAW", C200, C_SRC, t_slider},
    [P_SUB] = {"SUB", C200, C_SRC, t_slider}, [P_NOISE] = {"NOISE", C200, C_SRC, t_slider},
    [P_SUB_MODE] = {"SUB OCT", 2, C_SRC, t_sub}, [P_CUTOFF] = {"CUTOFF", C200, C_FLT, t_slider},
    [P_RESO] = {"RESO", C200, C_FLT, t_slider}, [P_ENV] = {"ENV", C200, C_FLT, t_slider},
    [P_VCF_LFO] = {"VCF MOD", C200, C_MOD, t_slider}, [P_KYBD] = {"KYBD", C200, C_FLT, t_slider},
    [P_VCA] = {"VCA", 1, C_AMP, t_vca}, [P_ATTACK] = {"ATTACK", C200, C_AMP, t_slider},
    [P_DECAY] = {"DECAY", C200, C_AMP, t_slider}, [P_SUSTAIN] = {"SUSTAIN", C200, C_AMP, t_slider},
    [P_RELEASE] = {"RELEASE", C200, C_AMP, t_slider}, [P_TRIGGER] = {"TRIGGER", 2, C_AMP, t_trig},
    [P_LFO_RATE] = {"RATE", C200, C_MOD, t_slider}, [P_LFO_WAVE] = {"WAVE", 3, C_MOD, t_wave},
    [P_GLIDE] = {"GLIDE", C200, C_AMP, t_slider}, [P_GLIDE_MODE] = {"GLIDE", 2, C_AMP, t_gmode},
    [P_TRANSPOSE] = {"TRANSP", 2, C_AMP, t_trans}, [P_FINE] = {"FINE", C200, C_SRC, t_fine},
    [P_VOLUME] = {"VOLUME", C200, C_AMP, t_slider}, [P_DRIVE] = {"DRIVE", C200, C_FX, t_slider},
    [P_12DB] = {"SLOPE", 1, C_FLT, t_12}, [P_ENV_FULL] = {"ENV FULL", 1, C_FLT, t_onoff},
    [P_RES_COMP] = {"RES COMP", 1, C_FLT, t_onoff}, [P_WHITE] = {"WHITE", 1, C_SRC, t_onoff},
    [P_NOISE_FLOOR] = {"FLOOR", 1, C_SRC, t_onoff}, [P_LFO_SYNC] = {"SYNC", 1, C_MOD, t_onoff},
    [P_LFO_INV] = {"INVERT", 1, C_MOD, t_onoff}, [P_LFO_TRIG] = {"KEY TRIG", 1, C_MOD, t_onoff},
    [P_MOD_SNAP] = {"SNAP", 1, C_MOD, t_onoff}, [P_LIN_GLIDE] = {"LINEAR", 1, C_AMP, t_onoff},
    [P_DECLICK] = {"DECLICK", 1, C_AMP, t_onoff}, [P_VEL_VCA] = {"VEL VCA", C200, C_AMP, t_slider},
    [P_VEL_VCF] = {"VEL VCF", C200, C_FLT, t_slider}, [P_BEND_VCO] = {"BEND VCO", C200, C_MOD, t_slider},
    [P_BEND_VCF] = {"BEND VCF", C200, C_MOD, t_slider}, [P_MOD_WHEEL] = {"MOD WHL", C200, C_MOD, t_slider},
    [P_FM_AMT] = {"FM AMT", C200, C_MOD, t_slider}, [P_FM_SRC] = {"FM SRC", 4, C_MOD, t_fmsrc},
    [P_DIST_SEND] = {"DST SEND", 127, C_FX, t_int}, [P_DIST_MODE] = {"MODE", 3, C_FX, t_dmode},
    [P_DIST_DRIVE] = {"DRIVE", 127, C_FX, t_int}, [P_DIST_TONE] = {"TONE", 127, C_FX, t_int},
    [P_DIST_LEVEL] = {"LEVEL", 127, C_FX, t_int},
};
/* der Parameter des Bausteins zu jedem Wert (P_FM_SRC setzt vier Schalter) */
static const uint8_t sh_id[GM_NP] = {
    [P_RANGE] = SH101_DCO_RANGE, [P_MOD] = SH101_DCO_LFO_VALUE, [P_PW] = SH101_DCO_PWM_VALUE,
    [P_PWM_SRC] = SH101_DCO_PWM_MODE, [P_PULSE] = SH101_PULSE_VOLUME, [P_SAW] = SH101_SAW_VOLUME,
    [P_SUB] = SH101_SUB_OSC_VOLUME, [P_NOISE] = SH101_NOISE_VOLUME, [P_SUB_MODE] = SH101_SUB_OSC_MODE,
    [P_CUTOFF] = SH101_FILTER_CUTOFF, [P_RESO] = SH101_FILTER_RESONANCE, [P_ENV] = SH101_FILTER_ENVELOPE_VALUE,
    [P_VCF_LFO] = SH101_FILTER_MODULATION_VALUE, [P_KYBD] = SH101_FILTER_KEYBOARD_VALUE, [P_VCA] = SH101_VCA_MODE,
    [P_ATTACK] = SH101_ADSR_ATTACK, [P_DECAY] = SH101_ADSR_DECAY, [P_SUSTAIN] = SH101_ADSR_SUSTAIN,
    [P_RELEASE] = SH101_ADSR_RELEASE, [P_TRIGGER] = SH101_ADSR_MODE, [P_LFO_RATE] = SH101_LFO_RATE,
    [P_LFO_WAVE] = SH101_LFO_WAVEFORM, [P_GLIDE] = SH101_PORTAMENTO_INTENSITY, [P_GLIDE_MODE] = SH101_PORTAMENTO_MODE,
    [P_TRANSPOSE] = SH101_OCTAVE_TRANSPOSE, [P_FINE] = SH101_MASTER_FINE_TUNE, [P_VOLUME] = SH101_VOLUME,
    [P_DRIVE] = SH101_DISTORTION_AMOUNT, [P_12DB] = SH101_FILTER_12DB, [P_ENV_FULL] = SH101_FILTER_ENVELOPE_VALUE_FULL_RANGE,
    [P_RES_COMP] = SH101_FILTER_VOLUME_CORRECTION, [P_WHITE] = SH101_WHITE_NOISE_ENABLED, [P_NOISE_FLOOR] = SH101_NOISE_FLOOR,
    [P_LFO_SYNC] = SH101_LFO_SYNC, [P_LFO_INV] = SH101_LFO_INVERTED, [P_LFO_TRIG] = SH101_LFO_TRIGGER,
    [P_MOD_SNAP] = SH101_DCO_LFO_VALUE_SNAP, [P_LIN_GLIDE] = SH101_PORTAMENTO_LINEAR, [P_DECLICK] = SH101_ADSR_DECLICK,
    [P_VEL_VCA] = SH101_CONTROL_VELOCITY_VOLUME, [P_VEL_VCF] = SH101_CONTROL_VELOCITY_ENVELOPE,
    [P_BEND_VCO] = SH101_CONTROL_BENDER_DCO, [P_BEND_VCF] = SH101_CONTROL_BENDER_FILTER,
    [P_MOD_WHEEL] = SH101_CONTROL_BENDER_MODULATION, [P_FM_AMT] = SH101_FM_INTENSITY, [P_FM_SRC] = SH101_FM_PULSE,
};
/* INIT: die Vorgaben des Bausteins (Säge und Sub voll, Filter offen, Gate+Trig) */
static const uint8_t gm_init_v[GM_NP] = {
    [P_RANGE] = 1, [P_SAW] = 200, [P_SUB] = 200, [P_CUTOFF] = 200, [P_KYBD] = 200, [P_DECAY] = 100, [P_TRIGGER] = 2,
    [P_LFO_RATE] = 100, [P_GLIDE_MODE] = 1, [P_TRANSPOSE] = 1, [P_FINE] = 100, [P_VOLUME] = 100,
    [P_DIST_MODE] = 0, [P_DIST_DRIVE] = 60, [P_DIST_TONE] = 70, [P_DIST_LEVEL] = 64,   /* TUBE (Wunsch 28.11.) */
};

/* ---- Werkssounds: Abweichungen vom INIT (eigene Einstellungen, keine Daten eines Originals; nach Gehör anzupassen) */
enum { CAT_BASS, CAT_LEAD, CAT_SEQ, CAT_FX, GM_NCAT };
static const char *const gm_cat_name[GM_NCAT] = {"BASS", "LEAD", "SEQUENCE", "FX"};
typedef struct { const char *name; uint8_t cat; uint8_t v[GM_NP]; } gm_sound_t;
typedef struct { const char *name; uint8_t cat; uint8_t n; uint8_t kv[24][2]; } sh_recipe_t;
static const sh_recipe_t recipes[] = {
    {"SAW BASS", CAT_BASS, 9, {{P_SUB, 140}, {P_SUB_MODE, 1}, {P_CUTOFF, 90}, {P_RESO, 40}, {P_ENV, 120}, {P_DECAY, 70}, {P_SUSTAIN, 40}, {P_RELEASE, 20}, {P_VCA, 1}}},
    {"SUB BASS", CAT_BASS, 9, {{P_SAW, 0}, {P_SUB_MODE, 2}, {P_CUTOFF, 70}, {P_ENV, 40}, {P_DECAY, 60}, {P_SUSTAIN, 120}, {P_RELEASE, 30}, {P_VCA, 1}, {P_KYBD, 120}}},
    {"ACID", CAT_BASS, 9, {{P_SUB, 0}, {P_CUTOFF, 40}, {P_RESO, 175}, {P_ENV, 150}, {P_DECAY, 50}, {P_GLIDE, 60}, {P_GLIDE_MODE, 0}, {P_DRIVE, 40}, {P_VCA, 0}}},
    {"PULSE BASS", CAT_BASS, 10, {{P_PULSE, 200}, {P_SAW, 0}, {P_SUB, 120}, {P_PW, 60}, {P_PWM_SRC, 1}, {P_CUTOFF, 80}, {P_RESO, 60}, {P_ENV, 100}, {P_DECAY, 50}, {P_VCA, 1}}},
    {"SQUELCH", CAT_BASS, 7, {{P_SUB, 60}, {P_CUTOFF, 30}, {P_RESO, 190}, {P_ENV, 170}, {P_DECAY, 40}, {P_DRIVE, 80}, {P_VCA, 0}}},
    {"LEAD", CAT_LEAD, 11, {{P_SUB, 0}, {P_CUTOFF, 130}, {P_RESO, 60}, {P_ENV, 60}, {P_ATTACK, 10}, {P_SUSTAIN, 150}, {P_RELEASE, 60}, {P_VCA, 1}, {P_GLIDE, 40}, {P_GLIDE_MODE, 0}, {P_MOD_WHEEL, 80}}},
    {"PWM LEAD", CAT_LEAD, 10, {{P_PULSE, 200}, {P_SAW, 0}, {P_SUB, 0}, {P_PW, 120}, {P_PWM_SRC, 2}, {P_LFO_RATE, 90}, {P_CUTOFF, 140}, {P_RESO, 40}, {P_SUSTAIN, 170}, {P_VCA, 1}}},
    {"WHISTLE", CAT_LEAD, 8, {{P_SAW, 20}, {P_SUB, 0}, {P_RESO, 200}, {P_CUTOFF, 100}, {P_KYBD, 200}, {P_SUSTAIN, 200}, {P_VCA, 1}, {P_RELEASE, 40}}},
    {"PLUCK", CAT_SEQ, 8, {{P_SUB, 100}, {P_CUTOFF, 60}, {P_RESO, 120}, {P_ENV, 130}, {P_DECAY, 35}, {P_SUSTAIN, 0}, {P_RELEASE, 20}, {P_VCA, 1}}},
    {"BLIP", CAT_SEQ, 8, {{P_PULSE, 200}, {P_SAW, 0}, {P_SUB, 0}, {P_RANGE, 2}, {P_CUTOFF, 100}, {P_RESO, 80}, {P_ENV, 160}, {P_DECAY, 20}}},
    {"AUTO TRIG", CAT_SEQ, 8, {{P_TRIGGER, 0}, {P_LFO_RATE, 130}, {P_CUTOFF, 60}, {P_RESO, 120}, {P_ENV, 130}, {P_DECAY, 40}, {P_SUSTAIN, 0}, {P_VCA, 1}}},
    {"NOISE HAT", CAT_FX, 10, {{P_NOISE, 200}, {P_SAW, 0}, {P_SUB, 0}, {P_CUTOFF, 170}, {P_RESO, 30}, {P_KYBD, 0}, {P_VCA, 1}, {P_DECAY, 15}, {P_SUSTAIN, 0}, {P_WHITE, 1}}},
    {"SIREN", CAT_FX, 5, {{P_LFO_RATE, 60}, {P_MOD, 150}, {P_SUB, 0}, {P_SUSTAIN, 200}, {P_VCA, 1}}},
    {"RANDOM", CAT_FX, 8, {{P_LFO_WAVE, 2}, {P_LFO_RATE, 140}, {P_VCF_LFO, 160}, {P_CUTOFF, 80}, {P_RESO, 150}, {P_SUB, 0}, {P_SUSTAIN, 200}, {P_VCA, 1}}},
};
#define GM_NFAC (sizeof recipes / sizeof recipes[0])
static gm_sound_t gm_factory[GM_NFAC];

/* ---- Seiten: KNOB 1–4 = vier Felder (fm1-synths ADR 0008) */
enum { ST_ENV_PIC = ST_USER };
static const page_t gm_pages[] = {
    {G_HOME, ST_HOME, "PLAY", {PRM("CUTOFF", P_CUTOFF), PRM("RESO", P_RESO), PRM("ENV", P_ENV), PRM("DECAY", P_DECAY)}},
    {G_HOME, ST_NAME, "PERFORM", {PRM("GLIDE", P_GLIDE), PRM("DRIVE", P_DRIVE), PRM("VOLUME", P_VOLUME), PRM("TRANSP", P_TRANSPOSE)}},
    {G_EDIT, ST_NAME, "VCO", {PRM("RANGE", P_RANGE), PRM("MOD", P_MOD), PRM("PW", P_PW), PRM("PWM SRC", P_PWM_SRC)}},
    {G_EDIT, ST_NAME, "SOURCE", {PRM("PULSE", P_PULSE), PRM("SAW", P_SAW), PRM("SUB", P_SUB), PRM("NOISE", P_NOISE)}},
    {G_EDIT, ST_NAME, "SOURCE 2", {PRM("SUB OCT", P_SUB_MODE), PRM("WHITE", P_WHITE), PRM("FINE", P_FINE), PRM("FLOOR", P_NOISE_FLOOR)}},
    {G_EDIT, ST_NAME, "VCF", {PRM("CUTOFF", P_CUTOFF), PRM("RESO", P_RESO), PRM("ENV", P_ENV), PRM("KYBD", P_KYBD)}},
    {G_EDIT, ST_NAME, "VCF 2", {PRM("VCF MOD", P_VCF_LFO), PRM("SLOPE", P_12DB), PRM("ENV FULL", P_ENV_FULL), PRM("RES COMP", P_RES_COMP)}},
    {G_EDIT, ST_NAME, "FM + DRIVE", {PRM("FM SRC", P_FM_SRC), PRM("FM AMT", P_FM_AMT), PRM("DRIVE", P_DRIVE), PRM("VOLUME", P_VOLUME)}},
    {G_ENV, ST_ENV_PIC, "ENV", {PRM("ATTACK", P_ATTACK), PRM("DECAY", P_DECAY), PRM("SUSTAIN", P_SUSTAIN), PRM("RELEASE", P_RELEASE)}},
    {G_ENV, ST_NAME, "VCA", {PRM("VCA", P_VCA), PRM("TRIGGER", P_TRIGGER), PRM("DECLICK", P_DECLICK), NO}},
    {G_ENV, ST_NAME, "VELOCITY", {PRM("VEL VCA", P_VEL_VCA), PRM("VEL VCF", P_VEL_VCF), NO, NO}},
    {G_LFO, ST_NAME, "LFO", {PRM("RATE", P_LFO_RATE), PRM("WAVE", P_LFO_WAVE), PRM("MOD", P_MOD), PRM("VCF MOD", P_VCF_LFO)}},
    {G_LFO, ST_NAME, "LFO 2", {PRM("SYNC", P_LFO_SYNC), PRM("INVERT", P_LFO_INV), PRM("KEY TRIG", P_LFO_TRIG), PRM("SNAP", P_MOD_SNAP)}},
    {G_LFO, ST_NAME, "CONTROL", {PRM("BEND VCO", P_BEND_VCO), PRM("BEND VCF", P_BEND_VCF), PRM("MOD WHL", P_MOD_WHEEL), NO}},
    {G_LFO, ST_NAME, "GLIDE", {PRM("GLIDE", P_GLIDE), PRM("MODE", P_GLIDE_MODE), PRM("LINEAR", P_LIN_GLIDE), PRM("TRANSP", P_TRANSPOSE)}},
    {G_FX, ST_NAME, "EFFECTS", {{"DLY SEND", K_SEND, 0}, {"REV SEND", K_SEND, 1}, FXK("DELAY", FX_DLY_LEVEL), FXK("REVERB", FX_REV_LEVEL)}},
    {G_FX, ST_NAME, "DIST", {PRM("DST SEND", P_DIST_SEND), PRM("MODE", P_DIST_MODE), PRM("DRIVE", P_DIST_DRIVE), PRM("TONE", P_DIST_TONE)}},
    {G_FX, ST_NAME, "DIST 2", {PRM("LEVEL", P_DIST_LEVEL), NO, NO, NO}},
    {G_FX, ST_NAME, "DELAY", {FXK("TIME", FX_DLY_TIME), FXK("FEEDBACK", FX_DLY_FB), FXK("TONE", FX_DLY_TONE), FXK("PING", FX_DLY_PING)}},
    {G_FX, ST_NAME, "DELAY 2", {FXK("SYNC", FX_DLY_SYNC), FXK("NOTE", FX_DLY_DIV), FXK("MOD", FX_DLY_MOD), NO}},
    {G_FX, ST_NAME, "REVERB", {FXK("SIZE", FX_REV_SIZE), FXK("DECAY", FX_REV_DECAY), FXK("DAMP", FX_REV_DAMP), FXK("TYPE", FX_REV_TYPE)}},
    {G_ARP, ST_ARP, "ARP", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCT", ARP_OCT)}},
    {G_ARP, ST_ARP, "ARP 2", {ARK("GATE", ARP_GATE), ARK("HOLD", ARP_HOLD), NO, NO}},
    {G_SEQ, ST_SEQ, "SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
    {G_SEQ, ST_LIVE, "LOOP", {LVK("LOOP", LV_SET_ON), LVK("BARS", LV_SET_BARS), LVK("LONGER", LV_SET_GROW), LVK("SHORTER", LV_SET_CUT)}},
    {G_GLO, ST_TEMPO, "TEMPO", {{"TEMPO", K_TEMPO, 0}, {"CLOCK", K_CLK, 0}, {"SEND", K_CLK, 1}, {"MIDI CH", K_CHAN, 0}}},
    {G_GLO, ST_SYS, "SYSTEM", {{"LEVEL", K_LEVEL, 0}, NO, NO, NO}},
    {G_SEL, ST_BROWSE, "SOUNDS", {NO, NO, NO, NO}},
};
static const layer_t gm_layers[NGROUP] = {
    [G_EDIT] = {"SOURCE", {PRM("PULSE", P_PULSE), PRM("SAW", P_SAW), PRM("SUB", P_SUB), PRM("PW", P_PW)}},
    [G_ENV] = {"ENV", {PRM("ATTACK", P_ATTACK), PRM("DECAY", P_DECAY), PRM("SUSTAIN", P_SUSTAIN), PRM("RELEASE", P_RELEASE)}},
    [G_LFO] = {"LFO", {PRM("RATE", P_LFO_RATE), PRM("MOD", P_MOD), PRM("VCF MOD", P_VCF_LFO), PRM("WAVE", P_LFO_WAVE)}},
    [G_FX] = {"EFFECTS", {{"DLY SEND", K_SEND, 0}, {"REV SEND", K_SEND, 1}, PRM("DST SEND", P_DIST_SEND), FXK("TIME", FX_DLY_TIME)}},
    [G_ARP] = {"ARP", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCT", ARP_OCT)}},
    [G_GLO] = {"PERFORM", {PRM("GLIDE", P_GLIDE), PRM("DRIVE", P_DRIVE), PRM("TRANSP", P_TRANSPOSE), PRM("VOLUME", P_VOLUME)}},
    [G_SEQ] = {"SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
};

/* ---- der Klang: Baustein sh101 (Gerätefassung, eine Stimme) */
static sh101_t eng __attribute__((section(".pool"), aligned(16)));
/* Distortion-Send (wie FM1-Electros DIST-Send, gemeinsam/fx/gm_dist.c): kern_render schreibt den Send eines Blocks,
 * Kern 1 rechnet ihn im nächsten Block neben Delay und Hall (GM_FX_EXTRA) und addiert das nasse Signal. */
#include "gm_dist.h"
static volatile uint8_t dist_v[5];              /* Send, Modus, Drive, Tone, Level */
static float dist_buf[2][GM_BLOCK] __attribute__((section(".pool")));
static volatile uint32_t dist_wr;               /* der Puffer, den kern_render gerade füllt */
static float dist_tmp[GM_BLOCK] __attribute__((section(".pool")));
static __attribute__((section(".dsp_text"), noinline)) void sh_dist_fx(float *wl, float *wr)
{
    const float *in = dist_buf[dist_wr ^ 1u];
    if (!dist_v[0]) return;
    for (unsigned i = 0; i < GM_BLOCK; i++) dist_tmp[i] = in[i];
    gm_dist_process(dist_tmp, (int)GM_BLOCK, dist_v[1], dist_v[2], dist_v[3], dist_v[4]);
    for (unsigned i = 0; i < GM_BLOCK; i++) { wl[i] += dist_tmp[i]; wr[i] += dist_tmp[i]; }
}
#define GM_FX_EXTRA(wl, wr) sh_dist_fx(wl, wr)
#define GM_FX_EXTRA_ON() (dist_v[0] != 0)
static uint8_t eng_v[GM_NP];                    /* was der Klang gerade hat (Audioseite) */
#define SH_GAIN 4.0f                            /* der Baustein liefert um 0,1 RMS (offene Säge): so etwa -14 dBFS */
static void kern_set(unsigned p, unsigned i, uint8_t v);
static void kern_boot(void)
{
    for (unsigned s = 0; s < GM_NFAC; s++) {
        gm_factory[s].name = recipes[s].name; gm_factory[s].cat = recipes[s].cat;
        for (unsigned i = 0; i < GM_NP; i++) gm_factory[s].v[i] = gm_init_v[i];
        for (unsigned k = 0; k < recipes[s].n; k++) gm_factory[s].v[recipes[s].kv[k][0]] = recipes[s].kv[k][1];
    }
    gm_dist_init();
    sh101_init(&eng, 44100.0);
    for (unsigned i = 0; i < GM_NP; i++) kern_set(0, i, gm_init_v[i]);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_set(unsigned p, unsigned i, uint8_t v)
{
    (void)p;
    if (i >= GM_NP) return;
    if (v > gm_params[i].max) v = gm_params[i].max;
    eng_v[i] = v;
    if (i >= P_DIST_SEND) {                     /* Distortion-Send: Werte für sh_dist_fx */
        dist_v[i - P_DIST_SEND] = v;
        return;
    }
    if (i == P_FM_SRC) {
        for (unsigned k = 0; k < 4u; k++) sh101_set(&eng, SH101_FM_PULSE + k, v == k + 1u ? 1.0f : 0.0f);
        return;
    }
    sh101_set(&eng, sh_id[i], (float)v / (float)gm_params[i].max);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_load(unsigned p, const uint8_t *v)
{
    for (unsigned i = 0; i < GM_NP; i++) if (v[i] != eng_v[i]) kern_set(p, i, v[i]);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_on(unsigned p, int note, int vel, int legato)
{
    (void)p; (void)legato;                      /* (gebunden spielt der Baustein selbst: Tasten-Priorität wie das SH-101) */
    sh101_note_on(&eng, note, (float)vel * (1.0f / 127.0f));
}
static __attribute__((section(".dsp_text"), noinline)) void kern_off(unsigned p, int note) { (void)p; sh101_note_off(&eng, note); }
static __attribute__((section(".dsp_text"), noinline)) void kern_all_off(unsigned p) { (void)p; sh101_all_notes_off(&eng); }
static __attribute__((section(".dsp_text"), noinline)) void kern_bend(unsigned p, float b) { (void)p; sh101_pitch_bend(&eng, b); }
static __attribute__((section(".dsp_text"), noinline)) void kern_mod(unsigned p, float m) { (void)p; sh101_mod_wheel(&eng, m); }
static volatile float bpm_now;
static void kern_render(unsigned p, float *l, float *r, unsigned n);
static int kern_voices(unsigned p)
{
    (void)p;
    return eng.voices[0].active || eng.voices[0].vca_env.state != 4;
}
static int kern_stage(unsigned stage, unsigned p);

#include "gm_app.c"

static __attribute__((section(".dsp_text"), noinline)) void kern_render(unsigned p, float *l, float *r, unsigned n)
{
    (void)p;
    eng.tempo = (double)bpm_now;
    sh101_prepare(&eng);
    {
        uint32_t w = dist_wr;
        float sg = (float)dist_v[0] * (1.0f / 127.0f);
        for (unsigned i = 0; i < n; i++) {
            float y = sh101_sample(&eng) * SH_GAIN;
            l[i] += y; r[i] += y;
            dist_buf[w][i] = y * sg;
        }
        FM1_C1_SYNC();
        dist_wr = w ^ 1u;                       /* (Kern 1 liest im nächsten Block diesen Puffer) */
    }
}

/* ---- Bilder der Variante */
static int kern_stage(unsigned stage, unsigned p)
{
    if (stage != ST_ENV_PIC) return 0;
    {   /* die Hüllkurve: Anstieg, Abfall auf das Halten, Halten, Ausklingen (Längen grob nach den Reglern) */
        const uint8_t *v = up[p].v;
        int32_t xa = 10 + v[P_ATTACK] * 50 / 200, xd = xa + 6 + v[P_DECAY] * 60 / 200, xs = xd + 40,
                xr = xs + 6 + v[P_RELEASE] * 60 / 200, yb = 140, yt = 44, ys = yb - (yb - yt) * v[P_SUSTAIN] / 200;
        LN(10, yb, xa, yt, T_SEL); LN(xa, yt, xd, ys, T_SEL); LN(xd, ys, xs, ys, T_SEL); LN(xs, ys, xr, yb, T_SEL);
        R(10, yb + 1, 220, 1, T_LINE);
        Q(4, 30, 2, v[P_VCA] ? "VCA: ENV" : "VCA: GATE", T_DIM);
        draw_sound_line(30);
    }
    return 1;
}
