/* SPDX-License-Identifier: GPL-3.0-only */
/* JX-8P + SH-101 für das M-VAVE FM-1 (ADR 0052): vier Parts, jeder spielt den JX-8P (Pool mit 4 Stimmen, der Pool-Kern
 * jx.c der JX-8P-Firmware, bitgleich mit dem Baustein jx8p) oder den SH-101 (Baustein sh101, mono, höchstens ein Part).
 * Bedienung wie die Kombi MKS-50 + JX-8P aus fm1-synths: Seite PARTS wählt den Klangerzeuger je Part, MIX die Pegel,
 * MIDI die Kanäle; ALGORITHM wählt den Part. Gerüst bausteine/varianten/gemeinsam mit GM_ENGINES = 2.
 *
 * Kerne: Kern 0 rechnet den SH-101 und das Gerüst, Kern 1 den JX-8P-Pool, Delay, Hall und den Distortion-Send (Haken
 * GM_FX_EXTRA). Die Sends der JX-Parts (aus ihren Sounds) und der Distortion-Send kommen einen Block (1,45 ms) später
 * zu den Effekten. */
#define APP_NAME "JX+SH"
#define GM_TITLE "JX+SH"
#define GM_PARTS 4u
#define GM_NAMEMAX 10u
#define GM_SOUND_VER 1u
#define GM_JR_MAGIC 0x48535854u                 /* "TXSH": Journal dieser Firmware */
#define GM_FAC_FUNCS 1
#define GM_SET_EXTRA 8u
#define GM_SEQ_IOS 1                            /* Sequencer der iOS-Apps: Akzent, Tie, G.VAR, NEW/VARY, Bank (ADR 0052) */
#include "gm_types.h"
#include "sh101/sh101.h"
#include "jx.h"
#include "gm_dist.h"

/* ---- zwei Klangerzeuger: E_JX, E_SH; welcher, steht im letzten Wert des Sounds */
enum { E_JX, E_SH };
enum { ENG = JP_COUNT, GM_NP = JP_COUNT + 1 };
#define GM_ENGINES 2u
#define GM_ENG_OF(v) ((unsigned)((v)[ENG] ? E_SH : E_JX))
#define GM_PARAMS_E(e) ((e) == E_SH ? prm_sh : prm_jx)
#define GM_INIT_E(e) ((e) == E_SH ? init_sh : init_jx)
#define GM_PAGES_E(e) ((e) == E_SH ? pages_sh : pages_jx)
#define GM_NPAGE_E(e) ((e) == E_SH ? sizeof pages_sh / sizeof pages_sh[0] : sizeof pages_jx / sizeof pages_jx[0])
#define GM_LAYERS_E(e) ((e) == E_SH ? layers_sh : layers_jx)
#define GM_CAT_ENG(c) ((unsigned)((c) >= CAT_SH0 ? E_SH : E_JX))
#define GM_ENG_OK(p, e) jxsh_eng_ok(p, e)
#define GM_ENG_NAME(e) ((e) == E_SH ? "SH-101" : "JX-8P")
#define GM_FIRST_IDX(p) jxsh_first_idx(p)
#define GM_FIRST_ENG(p) ((p) == 3u ? E_SH : E_JX)
enum { ST_PARTS_PG = ST_PARTS, ST_ENV_PIC = ST_NAME };

/* ---- weitere Einstellungen (set.extra): Distortion für alle Parts, Send je Part */
enum { X_MODE, X_DRIVE, X_TONE, X_LEVEL, X_SEND, X_SEND_SEL = 0x40 };   /* X_SEND + Part; X_SEND_SEL: Send des gewählten Parts */
#define GM_EXTRA_IDX(i) ((i) == X_SEND_SEL ? X_SEND + sel : (i))
static int gm_extra_max(unsigned i) { return i == X_MODE ? 3 : 127; }
static int gm_extra_default(unsigned i) { static const uint8_t d[8] = {0, 60, 70, 64, 0, 0, 0, 0}; return d[i & 7u]; }
static void gm_extra_text(unsigned i, int v, char *b)
{
    static const char *const n[4] = {"TUBE", "OD", "DIST", "RAT"};
    const char *s = i == X_MODE ? n[v & 3] : 0;
    if (s) { while ((*b++ = *s++) != 0) {} return; }
    { char t[4]; int k = 0; do { t[k++] = (char)('0' + v % 10); v /= 10; } while (v && k < 3); while (k) *b++ = t[--k]; *b = 0; }
}

/* ---- Parameter: Name, Höchstwert, Farbklasse, Anzeige; dazu der Parameter des Bausteins */
enum {
    P_RANGE, P_MOD, P_PW, P_PWM_SRC, P_PULSE, P_SAW, P_SUB, P_NOISE, P_SUB_MODE, P_CUTOFF, P_RESO, P_ENV, P_VCF_LFO,
    P_KYBD, P_VCA, P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE, P_TRIGGER, P_LFO_RATE, P_LFO_WAVE, P_GLIDE, P_GLIDE_MODE,
    P_TRANSPOSE, P_FINE, P_VOLUME, P_DRIVE, P_12DB, P_ENV_FULL, P_RES_COMP, P_WHITE, P_NOISE_FLOOR, P_LFO_SYNC,
    P_LFO_INV, P_LFO_TRIG, P_MOD_SNAP, P_LIN_GLIDE, P_DECLICK, P_VEL_VCA, P_VEL_VCF, P_BEND_VCO, P_BEND_VCF, P_MOD_WHEEL,
    P_FM_AMT, P_FM_SRC,
    P_DIST_SEND, P_DIST_MODE, P_DIST_DRIVE, P_DIST_TONE, P_DIST_LEVEL,   /* Distortion-Send (0.1.2, nicht im Baustein) */
    SH_NP
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

static const gm_param_t prm_sh[GM_NP] = {
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
    [GM_NP - 1] = {"ENGINE", 1, C_AMP, t_onoff},
};
/* der Parameter des Bausteins zu jedem Wert (P_FM_SRC setzt vier Schalter) */
static const uint8_t sh_id[SH_NP] = {
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
static const uint8_t init_sh[GM_NP] = {
    [P_RANGE] = 1, [P_SAW] = 200, [P_SUB] = 200, [P_CUTOFF] = 200, [P_KYBD] = 200, [P_DECAY] = 100, [P_TRIGGER] = 2,
    [P_LFO_RATE] = 100, [P_GLIDE_MODE] = 1, [P_TRANSPOSE] = 1, [P_FINE] = 100, [P_VOLUME] = 100,
    [P_DIST_MODE] = 0, [P_DIST_DRIVE] = 60, [P_DIST_TONE] = 70, [P_DIST_LEVEL] = 64,   /* TUBE (Wunsch 28.11.) */
    [GM_NP - 1] = 1,
};


/* JX: Tabelle beim Start aus jx_param_name / jx_param_max (Rohwerte wie im Kern) */
static gm_param_t prm_jx[GM_NP];
static const uint8_t init_jx[GM_NP] = {   /* INIT TONE der JX-8P-Firmware (jx_app.c init_v), ENGINE 0 */
    [JP_DCO1_RANGE] = 1, [JP_DCO1_WAVE] = 3, [JP_DCO1_TUNE] = 12, [JP_DCO2_RANGE] = 1, [JP_DCO2_WAVE] = 3,
    [JP_DCO2_TUNE] = 12, [JP_DCO2_FINE] = 64, [JP_DCO_ENV_MODE] = 3, [JP_MIX_ENV_MODE] = 3, [JP_VCF_ENV_MODE] = 3,
    [JP_MIX_DCO1] = 127, [JP_CUTOFF] = 127, [JP_VCA_LEVEL] = 100, [JP_VCA_ENV_MODE] = 1, [JP_LFO_WAVE] = 2,
    [JP_LFO_RATE] = 64, [JP_E1_DECAY] = 40, [JP_E1_SUSTAIN] = 127, [JP_E1_RELEASE] = 20, [JP_E2_DECAY] = 40,
    [JP_E2_SUSTAIN] = 127, [JP_E2_RELEASE] = 20, [JP_CHORUS_RATE] = 60, [JP_BENDER] = 2, [JP_BEND_LFO] = 64,
    [JP_PORTA_TIME] = 30, [JP_MOD_SENS] = 64, [JP_VOLUME] = 100, [JP_KEY_SHIFT] = 12, [JP_DRIFT] = 8,
    [JP_UNISON_DETUNE] = 40, [JP_LFO_DIV] = 8};
extern const jx_preset_t jx_eigene[];
extern const unsigned jx_eigene_count;
static void t_none(uint8_t v, char *b) { t_num(b, v); }
static uint8_t jx_cls(unsigned p)
{
    if (p <= JP_DCO2_ENV || p == JP_DCO_DYNA || p == JP_DCO_ENV_MODE) return C_SRC;
    if (p >= JP_MIX_DCO1 && p <= JP_MIX_ENV_MODE) return C_SRC;
    if (p >= JP_HPF && p <= JP_VCF_DYNA) return C_FLT;
    if (p == JP_LFO_WAVE || p == JP_LFO_DELAY || p == JP_LFO_RATE || p == JP_LFO_SHAPE || p == JP_LFO_SYNC || p == JP_LFO_KEYSYNC) return C_MOD;
    if (p == JP_CHORUS || p == JP_CHORUS_RATE || p == JP_CHORUS_MODE || p == JP_DELAY_SEND || p == JP_REVERB_SEND) return C_FX;
    return C_AMP;
}

/* ---- Werkssounds: zuerst die eigenen Presets des Nutzers (JX-8P-iOS-App), dann die JX-Werks-Töne nach Gruppen, dann
 * die SH-101-Sounds */
enum { SC_BASS, SC_LEAD, SC_SEQ, SC_FX };
enum { CAT_EIG = 0, CAT_JXINIT = 12, CAT_SH0 = 13, GM_NCAT = CAT_SH0 + 4 };
static const char *gm_cat_name[GM_NCAT] = {"EIGENE"};
typedef struct { const char *name; uint8_t cat; uint8_t n; uint8_t kv[24][2]; } sh_recipe_t;
static const sh_recipe_t recipes[] = {
    {"SAW BASS", SC_BASS, 9, {{P_SUB, 140}, {P_SUB_MODE, 1}, {P_CUTOFF, 90}, {P_RESO, 40}, {P_ENV, 120}, {P_DECAY, 70}, {P_SUSTAIN, 40}, {P_RELEASE, 20}, {P_VCA, 1}}},
    {"SUB BASS", SC_BASS, 9, {{P_SAW, 0}, {P_SUB_MODE, 2}, {P_CUTOFF, 70}, {P_ENV, 40}, {P_DECAY, 60}, {P_SUSTAIN, 120}, {P_RELEASE, 30}, {P_VCA, 1}, {P_KYBD, 120}}},
    {"ACID", SC_BASS, 9, {{P_SUB, 0}, {P_CUTOFF, 40}, {P_RESO, 175}, {P_ENV, 150}, {P_DECAY, 50}, {P_GLIDE, 60}, {P_GLIDE_MODE, 0}, {P_DRIVE, 40}, {P_VCA, 0}}},
    {"PULSE BASS", SC_BASS, 10, {{P_PULSE, 200}, {P_SAW, 0}, {P_SUB, 120}, {P_PW, 60}, {P_PWM_SRC, 1}, {P_CUTOFF, 80}, {P_RESO, 60}, {P_ENV, 100}, {P_DECAY, 50}, {P_VCA, 1}}},
    {"SQUELCH", SC_BASS, 7, {{P_SUB, 60}, {P_CUTOFF, 30}, {P_RESO, 190}, {P_ENV, 170}, {P_DECAY, 40}, {P_DRIVE, 80}, {P_VCA, 0}}},
    {"LEAD", SC_LEAD, 11, {{P_SUB, 0}, {P_CUTOFF, 130}, {P_RESO, 60}, {P_ENV, 60}, {P_ATTACK, 10}, {P_SUSTAIN, 150}, {P_RELEASE, 60}, {P_VCA, 1}, {P_GLIDE, 40}, {P_GLIDE_MODE, 0}, {P_MOD_WHEEL, 80}}},
    {"PWM LEAD", SC_LEAD, 10, {{P_PULSE, 200}, {P_SAW, 0}, {P_SUB, 0}, {P_PW, 120}, {P_PWM_SRC, 2}, {P_LFO_RATE, 90}, {P_CUTOFF, 140}, {P_RESO, 40}, {P_SUSTAIN, 170}, {P_VCA, 1}}},
    {"WHISTLE", SC_LEAD, 8, {{P_SAW, 20}, {P_SUB, 0}, {P_RESO, 200}, {P_CUTOFF, 100}, {P_KYBD, 200}, {P_SUSTAIN, 200}, {P_VCA, 1}, {P_RELEASE, 40}}},
    {"PLUCK", SC_SEQ, 8, {{P_SUB, 100}, {P_CUTOFF, 60}, {P_RESO, 120}, {P_ENV, 130}, {P_DECAY, 35}, {P_SUSTAIN, 0}, {P_RELEASE, 20}, {P_VCA, 1}}},
    {"BLIP", SC_SEQ, 8, {{P_PULSE, 200}, {P_SAW, 0}, {P_SUB, 0}, {P_RANGE, 2}, {P_CUTOFF, 100}, {P_RESO, 80}, {P_ENV, 160}, {P_DECAY, 20}}},
    {"AUTO TRIG", SC_SEQ, 8, {{P_TRIGGER, 0}, {P_LFO_RATE, 130}, {P_CUTOFF, 60}, {P_RESO, 120}, {P_ENV, 130}, {P_DECAY, 40}, {P_SUSTAIN, 0}, {P_VCA, 1}}},
    {"NOISE HAT", SC_FX, 10, {{P_NOISE, 200}, {P_SAW, 0}, {P_SUB, 0}, {P_CUTOFF, 170}, {P_RESO, 30}, {P_KYBD, 0}, {P_VCA, 1}, {P_DECAY, 15}, {P_SUSTAIN, 0}, {P_WHITE, 1}}},
    {"SIREN", SC_FX, 5, {{P_LFO_RATE, 60}, {P_MOD, 150}, {P_SUB, 0}, {P_SUSTAIN, 200}, {P_VCA, 1}}},
    {"RANDOM", SC_FX, 8, {{P_LFO_WAVE, 2}, {P_LFO_RATE, 140}, {P_VCF_LFO, 160}, {P_CUTOFF, 80}, {P_RESO, 150}, {P_SUB, 0}, {P_SUSTAIN, 200}, {P_VCA, 1}}},
};

#define NREC (sizeof recipes / sizeof recipes[0])
static uint16_t fac_src[200];                   /* < 1000: jx_eigene, < 2000: jx_presets, sonst SH-Rezept */
static uint8_t fac_c[200];
static unsigned fac_n;
#define GM_NFAC fac_n
static uint8_t fac_buf[GM_NP];
static const char *gm_fac_name(unsigned i)
{
    unsigned s = fac_src[i];
    return s < 1000u ? jx_eigene[s].name : s < 2000u ? jx_presets[s - 1000u].name : recipes[s - 2000u].name;
}
static unsigned gm_fac_cat(unsigned i) { return fac_c[i]; }
static const uint8_t *gm_fac_v(unsigned i)
{
    unsigned s = fac_src[i];
    if (s < 2000u) {
        const uint8_t *v = s < 1000u ? jx_eigene[s].v : jx_presets[s - 1000u].v;
        for (unsigned k = 0; k < JP_COUNT; k++) fac_buf[k] = v[k];
        fac_buf[ENG] = 0;
    } else {
        const sh_recipe_t *r = &recipes[s - 2000u];
        for (unsigned k = 0; k < GM_NP; k++) fac_buf[k] = init_sh[k];
        for (unsigned k = 0; k < r->n; k++) fac_buf[r->kv[k][0]] = r->kv[k][1];
    }
    return fac_buf;
}
static unsigned jxsh_first_idx(unsigned p)
{
    for (unsigned i = 0; i < fac_n; i++) if ((p == 3u) == (fac_src[i] >= 2000u)) { if (p == 3u || p == 0u) return i; if (i + p < fac_n && fac_src[i + p] < 2000u) return i + p; return i; }
    return 0;
}

/* ---- Seiten je Erzeuger */
static const page_t pages_sh[] = {
    {G_HOME, ST_HOME, "PLAY", {PRM("CUTOFF", P_CUTOFF), PRM("RESO", P_RESO), PRM("ENV", P_ENV), PRM("DECAY", P_DECAY)}},
    {G_HOME, ST_PARTS_PG, "PARTS", {{"A", K_ENGINE, 0}, {"B", K_ENGINE, 1}, {"C", K_ENGINE, 2}, {"D", K_ENGINE, 3}}},
    {G_HOME, ST_PARTS_PG, "MIX", {{"A", K_LEVEL, 0}, {"B", K_LEVEL, 1}, {"C", K_LEVEL, 2}, {"D", K_LEVEL, 3}}},
    {G_EDIT, ST_NAME, "VCO", {PRM("RANGE", P_RANGE), PRM("MOD", P_MOD), PRM("PW", P_PW), PRM("PWM SRC", P_PWM_SRC)}},
    {G_EDIT, ST_NAME, "SOURCE", {PRM("PULSE", P_PULSE), PRM("SAW", P_SAW), PRM("SUB", P_SUB), PRM("NOISE", P_NOISE)}},
    {G_EDIT, ST_NAME, "SOURCE 2", {PRM("SUB OCT", P_SUB_MODE), PRM("WHITE", P_WHITE), PRM("FINE", P_FINE), PRM("FLOOR", P_NOISE_FLOOR)}},
    {G_EDIT, ST_VVCF, "VCF", {PRM("CUTOFF", P_CUTOFF), PRM("RESO", P_RESO), PRM("ENV", P_ENV), PRM("KYBD", P_KYBD)}},
    {G_EDIT, ST_VVCF, "VCF 2", {PRM("VCF MOD", P_VCF_LFO), PRM("SLOPE", P_12DB), PRM("ENV FULL", P_ENV_FULL), PRM("RES COMP", P_RES_COMP)}},
    {G_EDIT, ST_NAME, "FM + DRIVE", {PRM("FM SRC", P_FM_SRC), PRM("FM AMT", P_FM_AMT), PRM("DRIVE", P_DRIVE), PRM("VOLUME", P_VOLUME)}},
    {G_ENV, ST_VENV, "ENV", {PRM("ATTACK", P_ATTACK), PRM("DECAY", P_DECAY), PRM("SUSTAIN", P_SUSTAIN), PRM("RELEASE", P_RELEASE)}},
    {G_ENV, ST_NAME, "VCA", {PRM("VCA", P_VCA), PRM("TRIGGER", P_TRIGGER), PRM("DECLICK", P_DECLICK), NO}},
    {G_ENV, ST_NAME, "VELOCITY", {PRM("VEL VCA", P_VEL_VCA), PRM("VEL VCF", P_VEL_VCF), NO, NO}},
    {G_LFO, ST_VLFO, "LFO", {PRM("RATE", P_LFO_RATE), PRM("WAVE", P_LFO_WAVE), PRM("MOD", P_MOD), PRM("VCF MOD", P_VCF_LFO)}},
    {G_LFO, ST_VLFO, "LFO 2", {PRM("SYNC", P_LFO_SYNC), PRM("INVERT", P_LFO_INV), PRM("KEY TRIG", P_LFO_TRIG), PRM("SNAP", P_MOD_SNAP)}},
    {G_LFO, ST_NAME, "CONTROL", {PRM("BEND VCO", P_BEND_VCO), PRM("BEND VCF", P_BEND_VCF), PRM("MOD WHL", P_MOD_WHEEL), NO}},
    {G_LFO, ST_NAME, "GLIDE", {PRM("GLIDE", P_GLIDE), PRM("MODE", P_GLIDE_MODE), PRM("LINEAR", P_LIN_GLIDE), PRM("TRANSP", P_TRANSPOSE)}},
    {G_ARP, ST_ARP, "ARP", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCT", ARP_OCT)}},
    {G_ARP, ST_ARP, "ARP 2", {ARK("GATE", ARP_GATE), ARK("HOLD", ARP_HOLD), NO, NO}},
    {G_SEQ, ST_SEQ, "SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("G.VAR", SQ_GVAR)}},
    {G_SEQ, ST_SEQ, "PATTERN", {SQK("STYLE", SQ_STYLE), SXK("NEW", SX_NEW), SXK("VARY", SX_VARY), SXK("LENGTH", SX_LEN)}},
    {G_SEQ, ST_SEQ, "STEP", {SXK("STEP", SX_STEP), SXK("NOTE", SX_NOTE), SXK("ACCENT", SX_ACC), SXK("TIE", SX_TIE)}},
    {G_SEQ, ST_SEQ, "BANK", {SXK("SLOT", SX_SLOT), SXK("STORE", SX_STORE), SXK("CLEAR", SX_CLEAR), SQK("SHIFT", SQ_SHIFT)}},
    {G_SEQ, ST_LIVE, "LOOP", {LVK("LOOP", LV_SET_ON), LVK("BARS", LV_SET_BARS), LVK("LONGER", LV_SET_GROW), LVK("SHORTER", LV_SET_CUT)}},
    {G_FX, ST_VDIST, "DIST", {{"MODE", K_EXTRA, X_MODE}, {"DRIVE", K_EXTRA, X_DRIVE}, {"TONE", K_EXTRA, X_TONE}, {"LEVEL", K_EXTRA, X_LEVEL}}},
    {G_FX, ST_VDIST, "DIST SEND", {{"A", K_EXTRA, X_SEND + 0}, {"B", K_EXTRA, X_SEND + 1}, {"C", K_EXTRA, X_SEND + 2}, {"D", K_EXTRA, X_SEND + 3}}},
    {G_FX, ST_VDLY, "DELAY", {FXK("TIME", FX_DLY_TIME), FXK("FEEDBK", FX_DLY_FB), FXK("TONE", FX_DLY_TONE), FXK("LEVEL", FX_DLY_LEVEL)}},
    {G_FX, ST_VDLY, "DELAY 2", {FXK("SYNC", FX_DLY_SYNC), FXK("NOTE", FX_DLY_DIV), FXK("PING", FX_DLY_PING), FXK("MOD", FX_DLY_MOD)}},
    {G_FX, ST_VREV, "REVERB", {FXK("SIZE", FX_REV_SIZE), FXK("DECAY", FX_REV_DECAY), FXK("DAMP", FX_REV_DAMP), FXK("LEVEL", FX_REV_LEVEL)}},
    {G_FX, ST_VREV, "REVERB 2", {FXK("TYPE", FX_REV_TYPE), FXK("MOD", FX_REV_MOD), NO, NO}},
    {G_GLO, ST_TEMPO, "TEMPO", {{"TEMPO", K_TEMPO, 0}, {"CLOCK", K_CLK, 0}, {"SEND", K_CLK, 1}, NO}},
    {G_GLO, ST_PARTS_PG, "MIDI", {{"A", K_CHANP, 0}, {"B", K_CHANP, 1}, {"C", K_CHANP, 2}, {"D", K_CHANP, 3}}},
    {G_GLO, ST_SYS, "SYSTEM", {{"LEVEL", K_LEVEL, 4}, NO, NO, NO}},
    {G_SEL, ST_BROWSE, "SOUNDS", {NO, NO, NO, NO}},
    {G_FX, ST_NAME, "SENDS", {{"DLY SEND", K_SEND, 0}, {"REV SEND", K_SEND, 1}, NO, NO}},
};

static const layer_t layers_sh[NGROUP] = {
    [G_EDIT] = {"SOURCE", {PRM("PULSE", P_PULSE), PRM("SAW", P_SAW), PRM("SUB", P_SUB), PRM("PW", P_PW)}},
    [G_ENV] = {"ENV", {PRM("ATTACK", P_ATTACK), PRM("DECAY", P_DECAY), PRM("SUSTAIN", P_SUSTAIN), PRM("RELEASE", P_RELEASE)}},
    [G_LFO] = {"LFO", {PRM("RATE", P_LFO_RATE), PRM("MOD", P_MOD), PRM("VCF MOD", P_VCF_LFO), PRM("WAVE", P_LFO_WAVE)}},
    [G_ARP] = {"ARP", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCT", ARP_OCT)}},
    [G_FX] = {"EFFECTS", {{"DLY SEND", K_SEND, 0}, {"REV SEND", K_SEND, 1}, FXK("TIME", FX_DLY_TIME), {"DIST", K_EXTRA, X_SEND_SEL}}},
    [G_SEQ] = {"SEQUENCER", {SQK("SEQ", SQ_ON), SXK("NEW", SX_NEW), SXK("SLOT", SX_SLOT), SQK("G.VAR", SQ_GVAR)}},
    [G_GLO] = {"LEVELS", {{"A", K_LEVEL, 0}, {"B", K_LEVEL, 1}, {"C", K_LEVEL, 2}, {"D", K_LEVEL, 3}}},   /* wie beim JX-Part */
};


static const page_t pages_jx[] = {
    {G_HOME, ST_HOME, "PLAY", {PRM("CUTOFF", JP_CUTOFF), PRM("RESO", JP_RESO), PRM("ENV", JP_VCF_ENV), PRM("RELEASE", JP_E2_RELEASE)}},
    {G_HOME, ST_PARTS_PG, "PARTS", {{"A", K_ENGINE, 0}, {"B", K_ENGINE, 1}, {"C", K_ENGINE, 2}, {"D", K_ENGINE, 3}}},
    {G_HOME, ST_PARTS_PG, "MIX", {{"A", K_LEVEL, 0}, {"B", K_LEVEL, 1}, {"C", K_LEVEL, 2}, {"D", K_LEVEL, 3}}},
    {G_EDIT, ST_NAME, "DCO-1", {PRM("RANGE", JP_DCO1_RANGE), PRM("WAVE", JP_DCO1_WAVE), PRM("TUNE", JP_DCO1_TUNE), PRM("LFO", JP_DCO1_LFO)}},
    {G_EDIT, ST_NAME, "DCO-2", {PRM("RANGE", JP_DCO2_RANGE), PRM("WAVE", JP_DCO2_WAVE), PRM("X-MOD", JP_DCO2_XMOD), PRM("TUNE", JP_DCO2_TUNE)}},
    {G_EDIT, ST_NAME, "DCO-2", {PRM("FINE", JP_DCO2_FINE), PRM("LFO", JP_DCO2_LFO), NO, NO}},
    {G_EDIT, ST_NAME, "DCO ENV", {PRM("DCO-1", JP_DCO1_ENV), PRM("DCO-2", JP_DCO2_ENV), PRM("ENV", JP_DCO_ENV_MODE), PRM("DYNA", JP_DCO_DYNA)}},
    {G_EDIT, ST_NAME, "MIXER", {PRM("DCO-1", JP_MIX_DCO1), PRM("DCO-2", JP_MIX_DCO2), PRM("ENV", JP_MIX_ENV), PRM("ENV MD", JP_MIX_ENV_MODE)}},
    {G_EDIT, ST_VVCF, "VCF", {PRM("CUTOFF", JP_CUTOFF), PRM("RESO", JP_RESO), PRM("ENV", JP_VCF_ENV), PRM("HPF", JP_HPF)}},
    {G_EDIT, ST_VVCF, "VCF", {PRM("LFO", JP_VCF_LFO), PRM("KEY", JP_VCF_KEY), PRM("ENV MD", JP_VCF_ENV_MODE), PRM("DYNA", JP_VCF_DYNA)}},
    {G_EDIT, ST_NAME, "VCA", {PRM("LEVEL", JP_VCA_LEVEL), PRM("MODE", JP_VCA_ENV_MODE), PRM("DYNA", JP_VCA_DYNA), PRM("VOLUME", JP_VOLUME)}},
    {G_ENV, ST_VENV, "ENV-1", {PRM("ATTACK", JP_E1_ATTACK), PRM("DECAY", JP_E1_DECAY), PRM("SUSTAIN", JP_E1_SUSTAIN), PRM("RELEASE", JP_E1_RELEASE)}},
    {G_ENV, ST_VENV, "ENV-2", {PRM("ATTACK", JP_E2_ATTACK), PRM("DECAY", JP_E2_DECAY), PRM("SUSTAIN", JP_E2_SUSTAIN), PRM("RELEASE", JP_E2_RELEASE)}},
    {G_ENV, ST_NAME, "ENV KEY", {PRM("1 KEY", JP_E1_KEY), PRM("2 KEY", JP_E2_KEY), PRM("SNAP", JP_ENV_SNAP), PRM("RETRIG", JP_ENV_RETRIG)}},
    {G_LFO, ST_VLFO, "LFO", {PRM("RATE", JP_LFO_RATE), PRM("DELAY", JP_LFO_DELAY), PRM("WAVE", JP_LFO_WAVE), PRM("SYNC", JP_LFO_SYNC)}},
    {G_LFO, ST_VLFO, "LFO", {PRM("SHAPE", JP_LFO_SHAPE), PRM("KEYSYNC", JP_LFO_KEYSYNC), NO, NO}},
    {G_FX, ST_NAME, "CHORUS", {PRM("CHORUS", JP_CHORUS), PRM("MODE", JP_CHORUS_MODE), PRM("DELAY", JP_DELAY_SEND), PRM("REVERB", JP_REVERB_SEND)}},
    {G_FX, ST_NAME, "CHORUS", {PRM("RATE", JP_CHORUS_RATE), NO, NO, NO}},
    {G_ARP, ST_ARP, "ARP", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCTAVES", ARP_OCT)}},
    {G_ARP, ST_ARP, "ARP", {ARK("GATE", ARP_GATE), ARK("HOLD", ARP_HOLD), {"TEMPO", K_TEMPO, 0}, NO}},
    {G_GLO, ST_NAME, "VOICE", {PRM("MODE", JP_KEY_MODE), PRM("GLIDE", JP_PORTA), PRM("TIME", JP_PORTA_TIME), PRM("SHIFT", JP_KEY_SHIFT)}},
    {G_GLO, ST_NAME, "VOICE", {PRM("UNI DET", JP_UNISON_DETUNE), PRM("DRIFT", JP_DRIFT), NO, NO}},
    {G_SEQ, ST_SEQ, "SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("G.VAR", SQ_GVAR)}},
    {G_SEQ, ST_SEQ, "PATTERN", {SQK("STYLE", SQ_STYLE), SXK("NEW", SX_NEW), SXK("VARY", SX_VARY), SXK("LENGTH", SX_LEN)}},
    {G_SEQ, ST_SEQ, "STEP", {SXK("STEP", SX_STEP), SXK("NOTE", SX_NOTE), SXK("ACCENT", SX_ACC), SXK("TIE", SX_TIE)}},
    {G_SEQ, ST_SEQ, "BANK", {SXK("SLOT", SX_SLOT), SXK("STORE", SX_STORE), SXK("CLEAR", SX_CLEAR), SQK("SHIFT", SQ_SHIFT)}},
    {G_SEQ, ST_LIVE, "LOOP", {LVK("LOOP", LV_SET_ON), LVK("BARS", LV_SET_BARS), LVK("LONGER", LV_SET_GROW), LVK("SHORTER", LV_SET_CUT)}},
    {G_FX, ST_VDIST, "DIST", {{"MODE", K_EXTRA, X_MODE}, {"DRIVE", K_EXTRA, X_DRIVE}, {"TONE", K_EXTRA, X_TONE}, {"LEVEL", K_EXTRA, X_LEVEL}}},
    {G_FX, ST_VDIST, "DIST SEND", {{"A", K_EXTRA, X_SEND + 0}, {"B", K_EXTRA, X_SEND + 1}, {"C", K_EXTRA, X_SEND + 2}, {"D", K_EXTRA, X_SEND + 3}}},
    {G_FX, ST_VDLY, "DELAY", {FXK("TIME", FX_DLY_TIME), FXK("FEEDBK", FX_DLY_FB), FXK("TONE", FX_DLY_TONE), FXK("LEVEL", FX_DLY_LEVEL)}},
    {G_FX, ST_VDLY, "DELAY 2", {FXK("SYNC", FX_DLY_SYNC), FXK("NOTE", FX_DLY_DIV), FXK("PING", FX_DLY_PING), FXK("MOD", FX_DLY_MOD)}},
    {G_FX, ST_VREV, "REVERB", {FXK("SIZE", FX_REV_SIZE), FXK("DECAY", FX_REV_DECAY), FXK("DAMP", FX_REV_DAMP), FXK("LEVEL", FX_REV_LEVEL)}},
    {G_FX, ST_VREV, "REVERB 2", {FXK("TYPE", FX_REV_TYPE), FXK("MOD", FX_REV_MOD), NO, NO}},
    {G_GLO, ST_TEMPO, "TEMPO", {{"TEMPO", K_TEMPO, 0}, {"CLOCK", K_CLK, 0}, {"SEND", K_CLK, 1}, NO}},
    {G_GLO, ST_PARTS_PG, "MIDI", {{"A", K_CHANP, 0}, {"B", K_CHANP, 1}, {"C", K_CHANP, 2}, {"D", K_CHANP, 3}}},
    {G_GLO, ST_SYS, "SYSTEM", {{"LEVEL", K_LEVEL, 4}, NO, NO, NO}},
    {G_SEL, ST_BROWSE, "SOUNDS", {NO, NO, NO, NO}},
};
static const layer_t layers_jx[NGROUP] = {
    [G_EDIT] = {"DCO + MIXER", {PRM("X-MOD", JP_DCO2_XMOD), PRM("2 TUNE", JP_DCO2_TUNE), PRM("MIX 1", JP_MIX_DCO1), PRM("MIX 2", JP_MIX_DCO2)}},
    [G_ENV] = {"ENV-1 / ENV-2", {PRM("1 ATTACK", JP_E1_ATTACK), PRM("1 DECAY", JP_E1_DECAY), PRM("2 ATTACK", JP_E2_ATTACK), PRM("2 RELEASE", JP_E2_RELEASE)}},
    [G_LFO] = {"LFO", {PRM("RATE", JP_LFO_RATE), PRM("DELAY", JP_LFO_DELAY), PRM("PITCH", JP_DCO1_LFO), PRM("FILTER", JP_VCF_LFO)}},
    [G_FX] = {"EFFECTS", {PRM("DELAY", JP_DELAY_SEND), PRM("REVERB", JP_REVERB_SEND), FXK("TIME", FX_DLY_TIME), PRM("CHORUS", JP_CHORUS)}},
    [G_ARP] = {"ARP + CHORD", {ARK("ARP", ARP_ON), ARK("MODE", ARP_MODE), ARK("RATE", ARP_DIV), ARK("OCT", ARP_OCT)}},
    [G_GLO] = {"LEVELS", {{"A", K_LEVEL, 0}, {"B", K_LEVEL, 1}, {"C", K_LEVEL, 2}, {"D", K_LEVEL, 3}}},
    [G_SEQ] = {"SEQUENCER", {SQK("SEQ", SQ_ON), SXK("NEW", SX_NEW), SXK("SLOT", SX_SLOT), SQK("G.VAR", SQ_GVAR)}},
};

/* ---- der Klang */
static sh101_t eng __attribute__((section(".pool"), aligned(16)));
static jx_t synth;
static uint8_t peng[GM_PARTS];                  /* Erzeuger je Part (Audioseite) */
static int sh_part = -1;                        /* der Part, der den SH-101 spielt (-1: keiner) */
static uint8_t eng_v[SH_NP];
static volatile uint8_t xv[GM_SET_EXTRA];       /* Distortion: Modus, Drive, Tone, Level, Sends A–D */
/* Gain-Staging der Kombi (ADR 0052): gleiche Lautheit bei gleichem MIX. Gemessen im Emulator, je eine Note C, Median der
 * klingenden Werkssounds: JX 44,9 dB, SH 63,4 dB (Abstand 18,5 dB). Aufgeteilt: SH −6 dB, JX +12,5 dB (Sends mit). */
#define JXSH_TRIM_SH 0.5f
#define JXSH_TRIM_JX 4.2f
#define SH_GAIN (4.0f * JXSH_TRIM_SH)           /* SH-101 solo: 4,0 */
#define DSPF __attribute__((section(".dsp_text"), noinline))
static int up_eng_of(unsigned p);
static int jxsh_eng_ok(unsigned p, unsigned e)
{
    if (e != E_SH) return 1;
    for (unsigned q = 0; q < GM_PARTS; q++) if (q != p && up_eng_of(q) == E_SH) return 0;   /* nur ein SH-101 (mono) */
    return 1;
}
static void kern_set(unsigned p, unsigned i, uint8_t v);
static void kern_boot(void)
{
    const unsigned ne = jx_eigene_count;
    for (unsigned i = 1; i < 12u; i++) gm_cat_name[i] = jx_cat_names[i];
    gm_cat_name[CAT_JXINIT] = "JX INIT";
    gm_cat_name[CAT_SH0 + SC_BASS] = "SH BASS"; gm_cat_name[CAT_SH0 + SC_LEAD] = "SH LEAD";
    gm_cat_name[CAT_SH0 + SC_SEQ] = "SH SEQ"; gm_cat_name[CAT_SH0 + SC_FX] = "SH FX";
    for (unsigned i = 0; i < ne && fac_n < 200u; i++) { fac_src[fac_n] = (uint16_t)i; fac_c[fac_n++] = CAT_EIG; }
    for (unsigned i = 0; i < jx_preset_count && fac_n < 200u; i++) {
        unsigned c = jx_presets[i].cat;
        fac_src[fac_n] = (uint16_t)(1000u + i); fac_c[fac_n++] = (uint8_t)(c && c < 12u ? c : CAT_JXINIT);
    }
    for (unsigned i = 0; i < NREC && fac_n < 200u; i++) { fac_src[fac_n] = (uint16_t)(2000u + i); fac_c[fac_n++] = (uint8_t)(CAT_SH0 + recipes[i].cat); }
    for (unsigned k = 0; k < JP_COUNT; k++) {
        prm_jx[k].label = jx_param_name(k); prm_jx[k].max = (uint8_t)jx_param_max(k); prm_jx[k].cls = jx_cls(k); prm_jx[k].text = t_none;
    }
    prm_jx[ENG].label = "ENGINE"; prm_jx[ENG].max = 1; prm_jx[ENG].cls = C_AMP; prm_jx[ENG].text = t_onoff;
    jx_init(&synth, 4, 20261128u);
    synth.cores = 1;                            /* (der ganze Pool auf Kern 1) */
    gm_dist_init();
    sh101_init(&eng, 44100.0);
    for (unsigned i = 0; i < SH_NP; i++) eng_v[i] = 0xFFu;
}
static DSPF void sh_set(unsigned i, uint8_t v)
{
    if (i >= SH_NP) return;
    if (v > prm_sh[i].max) v = prm_sh[i].max;
    eng_v[i] = v;
    if (i >= P_DIST_SEND) return;               /* (die Distortion ist hier global, set.extra) */
    if (i == P_FM_SRC) { for (unsigned k = 0; k < 4u; k++) sh101_set(&eng, SH101_FM_PULSE + k, v == k + 1u ? 1.0f : 0.0f); return; }
    sh101_set(&eng, sh_id[i], (float)v / (float)prm_sh[i].max);
}
static DSPF void kern_load(unsigned p, const uint8_t *v)
{
    unsigned e = v[ENG] ? E_SH : E_JX;
    if (e != peng[p]) {                         /* der Part spielt jetzt den anderen Erzeuger */
        if (peng[p] == E_SH) { sh101_all_notes_off(&eng); if (sh_part == (int)p) sh_part = -1; }
        else jx_all_off(&synth, p);
        peng[p] = (uint8_t)e;
    }
    if (e == E_SH) {
        sh_part = (int)p;
        for (unsigned i = 0; i < SH_NP; i++) if (v[i] != eng_v[i]) sh_set(i, v[i]);
    } else jx_part_load(&synth, p, v);
}
static DSPF void kern_set(unsigned p, unsigned i, uint8_t v)
{
    if (peng[p] == E_SH) sh_set(i, v);
    else if (i < JP_COUNT) jx_part_set(&synth, p, i, v);
}
static DSPF void kern_on(unsigned p, int note, int vel, int legato)
{
    if (peng[p] == E_SH) { if (sh_part == (int)p) sh101_note_on(&eng, note, (float)vel * (1.0f / 127.0f)); }
    else jx_note_on(&synth, p, note, (float)vel * (1.0f / 127.0f), legato);
}
static DSPF void kern_off(unsigned p, int note) { if (peng[p] == E_SH) { if (sh_part == (int)p) sh101_note_off(&eng, note); } else jx_note_off(&synth, p, note); }
static DSPF void kern_all_off(unsigned p) { if (peng[p] == E_SH) sh101_all_notes_off(&eng); else jx_all_off(&synth, p); }
static DSPF void kern_bend(unsigned p, float b) { if (peng[p] == E_SH) sh101_pitch_bend(&eng, b); }
static DSPF void kern_mod(unsigned p, float m) { if (peng[p] == E_SH) sh101_mod_wheel(&eng, m); }
static DSPF void kern_extra(unsigned i, unsigned v) { if (i < GM_SET_EXTRA) xv[i] = (uint8_t)v; }
static int kern_voices(unsigned p)
{
    if (peng[p] == E_SH) return sh_part == (int)p && (eng.voices[0].active || eng.voices[0].vca_env.state != 4);
    return (int)jx_part_voices(&synth, p);
}
static int kern_stage(unsigned stage, unsigned p) { (void)stage; (void)p; return 0; }
static int jxsh_viz(unsigned stage, unsigned p, float *v);
#define GM_VIZ(stage, p, v) jxsh_viz(stage, p, v)   /* Grafik-Kopf (ADR 0055) */
static void kern_render(unsigned p, float *l, float *r, unsigned n);
static void jxsh_c1(float *wl, float *wr);
static int jxsh_c1_on(void);
#define GM_FX_EXTRA(wl, wr) jxsh_c1(wl, wr)
#define GM_FX_EXTRA_ON() jxsh_c1_on()

#include "gm_app.c"

static int up_eng_of(unsigned p) { return (int)GM_ENG_OF(up[p].v); }

/* Grafik-Kopf (ADR 0055): Werte des Sounds und der Zustand der Klangerzeuger (LFO-Phase, Hüllkurve der jüngsten Stimme) */
static float viz_env_stage(unsigned st) { return st == SYN_ENV_IDLE ? -1.0f : st == SYN_ENV_T1 ? 0.0f : st == SYN_ENV_RELEASE ? 3.0f : 1.0f; }
static int jxsh_viz(unsigned stage, unsigned p, float *v)
{
    const uint8_t *u = up[p].v;
    if (stage == ST_VDIST) { v[0] = xv[X_MODE]; v[1] = xv[X_DRIVE] / 127.0f; v[2] = xv[X_TONE] / 127.0f; v[3] = xv[X_LEVEL] / 127.0f; return 1; }
    if (GM_ENG_OF(u) == E_SH) {
        static const int8_t wave[4] = {0, 1, 4, 5};
        switch (stage) {
        case ST_VLFO:
            v[0] = u[P_LFO_RATE] / 200.0f; v[1] = (u[P_MOD] > u[P_VCF_LFO] ? u[P_MOD] : u[P_VCF_LFO]) / 200.0f;
            v[2] = wave[u[P_LFO_WAVE] & 3u]; v[3] = sh_part == (int)p ? (float)eng.lfo_phase * (1.0f / 256.0f) : -1.0f;
            return 1;
        case ST_VENV: {
            const sh101_env_t *e = &eng.voices[0].vca_env;
            v[0] = u[P_ATTACK] / 200.0f; v[1] = u[P_DECAY] / 200.0f; v[2] = u[P_SUSTAIN] / 200.0f; v[3] = u[P_RELEASE] / 200.0f;
            v[4] = sh_part != (int)p || e->state == 4 ? -1.0f : e->state == 3 ? 3.0f : e->state == 1 || e->state == 2 ? 1.0f : 0.0f;
            v[5] = (float)e->value;
            return 1;
        }
        case ST_VVCF: v[0] = u[P_CUTOFF] / 200.0f; v[1] = u[P_RESO] / 200.0f; v[2] = u[P_12DB] ? 1.0f : 0.0f; v[3] = u[P_ENV] / 200.0f; return 1;
        default: return 0;
        }
    }
    switch (stage) {
    case ST_VLFO: {
        static const int8_t wave[7] = {0, 6, 2, 3, 1, 4, 5};   /* jx.c: TRI SINE SAW-UP SAW-DN SQUARE S&H RANDOM */
        int shape = u[JP_LFO_SHAPE] > 0 ? u[JP_LFO_SHAPE] - 1 : u[JP_LFO_WAVE] == 0 ? 5 : u[JP_LFO_WAVE] == 1 ? 4 : 1;
        uint8_t d = u[JP_DCO1_LFO] > u[JP_DCO2_LFO] ? u[JP_DCO1_LFO] : u[JP_DCO2_LFO];
        v[0] = u[JP_LFO_RATE] / 127.0f; v[1] = (d > u[JP_VCF_LFO] ? d : u[JP_VCF_LFO]) / 127.0f; v[2] = wave[shape % 7];
        v[3] = synth.part[p].lfo_phase;
        return 1;
    }
    case ST_VENV: {
        int two = pages[page].k[0].param == JP_E2_ATTACK;
        const jx_voice_t *best = 0;
        v[0] = u[two ? JP_E2_ATTACK : JP_E1_ATTACK] / 127.0f; v[1] = u[two ? JP_E2_DECAY : JP_E1_DECAY] / 127.0f;
        v[2] = u[two ? JP_E2_SUSTAIN : JP_E1_SUSTAIN] / 127.0f; v[3] = u[two ? JP_E2_RELEASE : JP_E1_RELEASE] / 127.0f;
        for (unsigned i = 0; i < synth.pool; i++) {   /* die jüngste klingende Stimme des Parts */
            const jx_voice_t *vo = &synth.voice[i];
            if (vo->part == p && vo->active && (!best || vo->age > best->age)) best = vo;
        }
        if (best) { const syn_env_t *e = two ? &best->env2 : &best->env1; v[4] = viz_env_stage(e->stage); v[5] = e->value; }
        return 1;
    }
    case ST_VVCF: v[0] = u[JP_CUTOFF] / 127.0f; v[1] = u[JP_RESO] / 127.0f; v[2] = 0; v[3] = u[JP_VCF_ENV] / 127.0f; return 1;
    default: return 0;
    }
}

/* ---- Puffer zwischen den Kernen, nach Blocknummer abwechselnd (Kern 1 schreibt Block N, Kern 0 liest N−1) */
static float jx_snd[2][2][GM_BLOCK] __attribute__((section(".pool")));   /* JX-Sends Delay, Hall */
static float dist_sh[2][GM_BLOCK] __attribute__((section(".pool")));
static float dist_jx[2][GM_BLOCK] __attribute__((section(".pool")));
static float c1_l[GM_BLOCK], c1_r[GM_BLOCK], c1_d[GM_BLOCK];
static int jxsh_c1_on(void)
{
    for (unsigned p = 0; p < GM_PARTS; p++) if (peng[p] == E_JX || xv[X_SEND + p]) return 1;
    return 0;
}
static DSPF void jxsh_c1(float *wl, float *wr)
{
    uint32_t w = block_no & 1u, rd = w ^ 1u;
    unsigned jxsend = 0, any_jx = 0;
    for (unsigned p = 0; p < GM_PARTS; p++) if (peng[p] == E_JX) { any_jx = 1; if (xv[X_SEND + p] > jxsend) jxsend = xv[X_SEND + p]; }
    /* Distortion: Sends des Blocks davor */
    if (xv[X_SEND] | xv[X_SEND + 1] | xv[X_SEND + 2] | xv[X_SEND + 3]) {
        for (unsigned i = 0; i < GM_BLOCK; i++) c1_d[i] = dist_sh[rd][i] + dist_jx[rd][i];
        gm_dist_process(c1_d, (int)GM_BLOCK, xv[X_MODE], xv[X_DRIVE], xv[X_TONE], xv[X_LEVEL]);
        for (unsigned i = 0; i < GM_BLOCK; i++) { wl[i] += c1_d[i]; wr[i] += c1_d[i]; }
    }
    /* der JX-8P-Pool dieses Blocks */
    for (unsigned i = 0; i < GM_BLOCK; i++) c1_l[i] = c1_r[i] = 0;
    if (any_jx) {
        for (unsigned p = 0; p < GM_PARTS; p++) if (peng[p] == E_JX) synth.part[p].level = part_level[p];
        jx_render(&synth, c1_l, c1_r, bpm_now);
    }
    {
        float g = (float)jxsend * (1.0f / 127.0f);
        for (unsigned i = 0; i < GM_BLOCK; i++) {
            c1_l[i] *= JXSH_TRIM_JX; c1_r[i] *= JXSH_TRIM_JX;
            wl[i] += c1_l[i]; wr[i] += c1_r[i];
            jx_snd[w][0][i] = any_jx ? synth.send_sum[0][i] * JXSH_TRIM_JX : 0.0f;
            jx_snd[w][1][i] = any_jx ? synth.send_sum[1][i] * JXSH_TRIM_JX : 0.0f;
            dist_jx[w][i] = (c1_l[i] + c1_r[i]) * 0.5f * g;
        }
    }
}
static DSPF void kern_render(unsigned p, float *l, float *r, unsigned n)
{
    uint32_t w = block_no & 1u, rd = w ^ 1u;
#ifdef JXSH_LAST
    if (p == 0u && block_no == 300u) {          /* nur Messung im Emulator (-DJXSH_LAST): schwerster Fall */
        fx_on = 1; xv[X_MODE] = 3;                 /* RAT */
        for (unsigned k = 0; k < GM_PARTS; k++) { xv[X_SEND + k] = JXSH_LAST >= 4 ? 0 : 64;   /* 4: ohne Distortion (SH + JX) */ part_send[k][0] = part_send[k][1] = 0.5f; }
        for (unsigned i = 0; i < fac_n; i++)
            if (fac_src[i] >= 2000u && recipes[fac_src[i] - 2000u].name[1] == 'Q') kern_load(3, gm_fac_v(i));   /* SQUELCH */
        jx_part_set(&synth, 0, JP_CHORUS, (uint8_t)jx_param_max(JP_CHORUS));
        if (JXSH_LAST != 2) kern_on(3, 36, 100, 0);                                  /* 2: nur JX */
        if (JXSH_LAST != 3) for (int k = 0; k < 4; k++) kern_on(0, 60 + 4 * k, 100, 0);   /* 3: nur SH */
    }
#endif
    if (p == 0u) {                              /* die JX-Sends des Blocks davor zu Delay und Hall */
        for (unsigned i = 0; i < GM_BLOCK; i++) { fx_in[0][i] += jx_snd[rd][0][i]; fx_in[1][i] += jx_snd[rd][1][i]; }
        for (unsigned i = 0; i < GM_BLOCK; i++) dist_sh[w][i] = 0;
    }
    if (peng[p] != E_SH || sh_part != (int)p) return;
    eng.tempo = (double)bpm_now;
    sh101_prepare(&eng);
    {
        float g = (float)xv[X_SEND + p] * (1.0f / 127.0f);
        for (unsigned i = 0; i < n; i++) {
            float y = sh101_sample(&eng) * SH_GAIN;
            l[i] += y; r[i] += y;
            dist_sh[w][i] = y * g * part_level[p];
        }
    }
}
