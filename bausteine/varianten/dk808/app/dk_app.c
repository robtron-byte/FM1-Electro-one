/* SPDX-License-Identifier: GPL-3.0-only */
/* 808 solo für das M-VAVE FM-1 (ADR 0050, Feature B20): der Baustein bausteine/dk808 (DrumKern, alle 17 Instrumente,
 * float, DK808_FLOAT=1) über den Gerüst-Adapter dk808_bs auf dem gemeinsamen Gerüst bausteine/varianten/gemeinsam.
 *
 * Zwei Kerne: zwei Zustände des Kerns mit getrennten Masken (dk808_set_run). Kern 0 rechnet alle Instrumente außer
 * denen in DK_K1_MASK (Standard: CH, OH, CY mit der Metallbank), Kern 1 diese neben Delay, Hall und Distortion
 * (GM_FX_PRE/GM_FX_EXTRA). Zusammen klingen sie wie ein Zustand mit allen (Test bausteine/dk808 test/bs.c). Mit
 * DK_K1_MASK=DK808_MASK_ALL läuft die ganze 808 auf Kern 1 (vorbereitet für die Kombis 808 + 101 und 808 + 101 + JX).
 * Anschläge gelten in beiden Zuständen im selben Block: Tasten und MIDI kommen vor dem Start von Kern 1, die Steps
 * des Sequencers einen Block später über pend[] (beide Kerne lesen denselben Eintrag).
 *
 * Sound = Kit (dk808_bs.h: Pegel, drei Regler, Choke, DIST SEND) + Distortion (Modus, Drive, Tone, Level).
 * Sequencer wie BEAT in FM1-Electro: 16 Steps je Instrument und eine Akzent-Zeile, gespeichert im Set des Gerüsts
 * (GM_SET_EXTRA, Journal). Seite STEPS (Taste SEQ): weiße Tasten = Steps, schwarze = Instrument. Live-Aufnahme LOOP
 * vom Gerüst (ADR 0053). DK_LAST=1: eingebautes Messmuster (README). */
#define APP_NAME "808"
#define GM_TITLE "TR-808"
#define GM_ACCENT RGB(255, 120, 0)              /* das Orange der 808 */
#define GM_PARTS 1u
#define GM_NAMEMAX 10u
#define GM_SOUND_VER 1u                         /* Kit: DK_BS_NP Rohwerte + 4 Distortion */
#define GM_JR_MAGIC 0x54534B44u                 /* "DKST": Journal dieser Firmware */
#define LV_EV_MAX 512                           /* Live-Aufnahme: Drums sind dicht (Gerüst: 192) */
#define GM_SEND_FIRST 0                         /* Delay/Hall zuerst zu (wie der DrumKern), Distortion im Kit */
#include "gm_types.h"
#include "dk808_bs.h"

#ifndef DK_K1_MASK
#define DK_K1_MASK DK808_METAL_USERS            /* Instrumente auf Kern 1 */
#endif
#ifndef DK_GAIN
#define DK_GAIN 1.6f                            /* Velocity 100: Spitzen 0,2 (CB) bis 0,6 (HC); BD so etwa -13 dBFS */
#endif
enum { P_DIST_MODE = DK_BS_NP, P_DIST_DRIVE, P_DIST_TONE, P_DIST_LEVEL, DK_NP };
#define GM_NP DK_NP

/* ---- Sequencer im Set: gewähltes Instrument, Länge, 18 Zeilen (17 Instrumente, Akzent) × 2 Bytes = 16 Steps */
enum { DK_ACC = DK_COUNT, DK_ROWS, X_INST = 0, X_LEN, X_PAT, DK_NX = X_PAT + 2 * DK_ROWS };
#define GM_SET_EXTRA 38u                        /* = DK_NX (Präprozessor braucht die Zahl) */
_Static_assert(DK_NX == 38, "GM_SET_EXTRA = DK_NX");
static const uint8_t dk_note[DK_COUNT] = {36, 58, 38, 37, 39, 64, 63, 62, 41, 47, 43, 42, 46, 49, 75, 70, 56};
static const char *dk_abbr(unsigned i) { return i < DK_COUNT ? dk808_instrument_abbr(i) : "ACC"; }
static int gm_extra_max(unsigned i) { return i == X_INST ? DK_ROWS - 1 : i == X_LEN ? 16 : 255; }
static int gm_extra_default(unsigned i)
{
    /* Grundmuster beim ersten Start: BD 1 9 11, SD 5 13, CH auf den Achteln */
    static const uint16_t def[DK_ROWS] = {[DK_BD] = 0x0501, [DK_SD] = 0x1010, [DK_CH] = 0x5555};
    if (i == X_INST) return 0;
    if (i == X_LEN) return 16;
    if (i >= X_PAT && i < DK_NX) { unsigned r = (i - X_PAT) / 2u; return (def[r] >> ((i - X_PAT) & 1u) * 8u) & 0xFF; }
    return 0;
}
static void t_put(char *b, const char *s) { while ((*b++ = *s++) != 0) {} }
static void gm_extra_text(unsigned i, int v, char *b)
{
    if (i == X_INST) { t_put(b, dk_abbr((unsigned)v)); return; }
    int n = 0; char t[4];
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 3);
    while (n) *b++ = t[--n];
    *b = 0;
}
static void kern_extra(unsigned i, unsigned v);

static void t_dmode(uint8_t v, char *b) { static const char *const n[4] = {"TUBE", "OD", "DIST", "RAT"}; t_put(b, n[v & 3u]); }
static void t_onoff(uint8_t v, char *b) { t_put(b, v ? "ON" : "OFF"); }

static const gm_param_t gm_params[GM_NP] = {
    [DK_BS_LEVEL + DK_BD] = {"BD LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_LG] = {"LG LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_SD] = {"SD LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_RS] = {"RS LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_CP] = {"CP LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_LC] = {"LC LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_MC] = {"MC LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_HC] = {"HC LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_LT] = {"LT LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_MT] = {"MT LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_HT] = {"HT LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_CH] = {"CH LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_OH] = {"OH LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_CY] = {"CY LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_CL] = {"CL LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_MA] = {"MA LVL", 100, C_AMP, 0},
    [DK_BS_LEVEL + DK_CB] = {"CB LVL", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_BD * 3 + 0] = {"TONE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_BD * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_BD * 3 + 2] = {"SWEEP", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_LG * 3 + 0] = {"DEPTH", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_LG * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_LG * 3 + 2] = {"SWEEP", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_SD * 3 + 0] = {"TONE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_SD * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_SD * 3 + 2] = {"SNAPPY", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_RS * 3 + 0] = {"TUNE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_RS * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_RS * 3 + 2] = {"-", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_CP * 3 + 0] = {"TONE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_CP * 3 + 1] = {"REVERB", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_CP * 3 + 2] = {"-", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_LC * 3 + 0] = {"TUNE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_LC * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_LC * 3 + 2] = {"-", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_MC * 3 + 0] = {"TUNE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_MC * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_MC * 3 + 2] = {"-", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_HC * 3 + 0] = {"TUNE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_HC * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_HC * 3 + 2] = {"-", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_LT * 3 + 0] = {"TUNE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_LT * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_LT * 3 + 2] = {"SNAPPY", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_MT * 3 + 0] = {"TUNE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_MT * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_MT * 3 + 2] = {"SNAPPY", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_HT * 3 + 0] = {"TUNE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_HT * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_HT * 3 + 2] = {"SNAPPY", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_CH * 3 + 0] = {"TONE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_CH * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_CH * 3 + 2] = {"BP FREQ", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_OH * 3 + 0] = {"TONE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_OH * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_OH * 3 + 2] = {"BP FREQ", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_CY * 3 + 0] = {"TONE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_CY * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_CY * 3 + 2] = {"BP FREQ", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_CL * 3 + 0] = {"TUNE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_CL * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_CL * 3 + 2] = {"-", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_MA * 3 + 0] = {"TONE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_MA * 3 + 1] = {"-", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_MA * 3 + 2] = {"-", 100, C_MOD, 0},
    [DK_BS_KNOB + DK_CB * 3 + 0] = {"TUNE", 100, C_SRC, 0},
    [DK_BS_KNOB + DK_CB * 3 + 1] = {"DECAY", 100, C_AMP, 0},
    [DK_BS_KNOB + DK_CB * 3 + 2] = {"-", 100, C_MOD, 0},
    [DK_BS_CHOKE] = {"HAT CHK", 1, C_AMP, t_onoff}, [DK_BS_CHOKE + 1] = {"CNG CHK", 1, C_AMP, t_onoff},
    [DK_BS_CHOKE + 2] = {"TOM CHK", 1, C_AMP, t_onoff}, [DK_BS_DIST] = {"DST SEND", 100, C_FX, 0},
    [P_DIST_MODE] = {"MODE", 3, C_FX, t_dmode}, [P_DIST_DRIVE] = {"DRIVE", 127, C_FX, 0},
    [P_DIST_TONE] = {"TONE", 127, C_FX, 0}, [P_DIST_LEVEL] = {"LEVEL", 127, C_FX, 0},
};

static uint8_t gm_init_v[GM_NP];                /* INIT: dk808_bs_init_kit, Distortion TUBE (kern_boot) */

enum { CAT_KIT, GM_NCAT };
static const char *const gm_cat_name[GM_NCAT] = {"KITS"};
typedef struct { const char *name; uint8_t cat; uint8_t v[GM_NP]; } gm_sound_t;
#define GM_NFAC 2u
static gm_sound_t gm_factory[GM_NFAC];

/* ---- Seiten: KNOB 1–4 = Pegel und die drei Regler eines Instruments (EDIT: BD..CP, ENV: Congas und Toms,
 * LFO: Hi-Hats und Becken, ARP: Claves, Maracas, Cowbell); SEQ: STEPS und LOOP */
enum { ST_STEPS = ST_USER };
static const page_t gm_pages[] = {
    {G_HOME, ST_HOME, "PLAY", {PRM("BD", DK_BS_LEVEL + DK_BD), PRM("SD", DK_BS_LEVEL + DK_SD), PRM("CH", DK_BS_LEVEL + DK_CH), PRM("OH", DK_BS_LEVEL + DK_OH)}},
    {G_HOME, ST_NAME, "MIX", {PRM("CP", DK_BS_LEVEL + DK_CP), PRM("RS", DK_BS_LEVEL + DK_RS), PRM("CY", DK_BS_LEVEL + DK_CY), PRM("CB", DK_BS_LEVEL + DK_CB)}},
    {G_EDIT, ST_NAME, "BASS DRUM", {PRM("LEVEL", DK_BS_LEVEL + DK_BD), PRM("TONE", DK_BS_KNOB + DK_BD * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_BD * 3 + 1), PRM("SWEEP", DK_BS_KNOB + DK_BD * 3 + 2)}},
    {G_EDIT, ST_NAME, "LASER GUN", {PRM("LEVEL", DK_BS_LEVEL + DK_LG), PRM("DEPTH", DK_BS_KNOB + DK_LG * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_LG * 3 + 1), PRM("SWEEP", DK_BS_KNOB + DK_LG * 3 + 2)}},
    {G_EDIT, ST_NAME, "SNARE DRUM", {PRM("LEVEL", DK_BS_LEVEL + DK_SD), PRM("TONE", DK_BS_KNOB + DK_SD * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_SD * 3 + 1), PRM("SNAPPY", DK_BS_KNOB + DK_SD * 3 + 2)}},
    {G_EDIT, ST_NAME, "RIM SHOT", {PRM("LEVEL", DK_BS_LEVEL + DK_RS), PRM("TUNE", DK_BS_KNOB + DK_RS * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_RS * 3 + 1), NO}},
    {G_EDIT, ST_NAME, "HAND CLAP", {PRM("LEVEL", DK_BS_LEVEL + DK_CP), PRM("TONE", DK_BS_KNOB + DK_CP * 3 + 0), PRM("REVERB", DK_BS_KNOB + DK_CP * 3 + 1), NO}},
    {G_ENV, ST_NAME, "LOW CONGA", {PRM("LEVEL", DK_BS_LEVEL + DK_LC), PRM("TUNE", DK_BS_KNOB + DK_LC * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_LC * 3 + 1), NO}},
    {G_ENV, ST_NAME, "MID CONGA", {PRM("LEVEL", DK_BS_LEVEL + DK_MC), PRM("TUNE", DK_BS_KNOB + DK_MC * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_MC * 3 + 1), NO}},
    {G_ENV, ST_NAME, "HI CONGA", {PRM("LEVEL", DK_BS_LEVEL + DK_HC), PRM("TUNE", DK_BS_KNOB + DK_HC * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_HC * 3 + 1), NO}},
    {G_ENV, ST_NAME, "LOW TOM", {PRM("LEVEL", DK_BS_LEVEL + DK_LT), PRM("TUNE", DK_BS_KNOB + DK_LT * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_LT * 3 + 1), PRM("SNAPPY", DK_BS_KNOB + DK_LT * 3 + 2)}},
    {G_ENV, ST_NAME, "MID TOM", {PRM("LEVEL", DK_BS_LEVEL + DK_MT), PRM("TUNE", DK_BS_KNOB + DK_MT * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_MT * 3 + 1), PRM("SNAPPY", DK_BS_KNOB + DK_MT * 3 + 2)}},
    {G_ENV, ST_NAME, "HI TOM", {PRM("LEVEL", DK_BS_LEVEL + DK_HT), PRM("TUNE", DK_BS_KNOB + DK_HT * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_HT * 3 + 1), PRM("SNAPPY", DK_BS_KNOB + DK_HT * 3 + 2)}},
    {G_LFO, ST_NAME, "CLOSED HAT", {PRM("LEVEL", DK_BS_LEVEL + DK_CH), PRM("TONE", DK_BS_KNOB + DK_CH * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_CH * 3 + 1), PRM("BP FREQ", DK_BS_KNOB + DK_CH * 3 + 2)}},
    {G_LFO, ST_NAME, "OPEN HAT", {PRM("LEVEL", DK_BS_LEVEL + DK_OH), PRM("TONE", DK_BS_KNOB + DK_OH * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_OH * 3 + 1), PRM("BP FREQ", DK_BS_KNOB + DK_OH * 3 + 2)}},
    {G_LFO, ST_NAME, "CYMBAL", {PRM("LEVEL", DK_BS_LEVEL + DK_CY), PRM("TONE", DK_BS_KNOB + DK_CY * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_CY * 3 + 1), PRM("BP FREQ", DK_BS_KNOB + DK_CY * 3 + 2)}},
    {G_ARP, ST_NAME, "CLAVES", {PRM("LEVEL", DK_BS_LEVEL + DK_CL), PRM("TUNE", DK_BS_KNOB + DK_CL * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_CL * 3 + 1), NO}},
    {G_ARP, ST_NAME, "MARACAS", {PRM("LEVEL", DK_BS_LEVEL + DK_MA), PRM("TONE", DK_BS_KNOB + DK_MA * 3 + 0), NO, NO}},
    {G_ARP, ST_NAME, "COW BELL", {PRM("LEVEL", DK_BS_LEVEL + DK_CB), PRM("TUNE", DK_BS_KNOB + DK_CB * 3 + 0), PRM("DECAY", DK_BS_KNOB + DK_CB * 3 + 1), NO}},
    {G_FX, ST_NAME, "SENDS", {{"DLY SEND", K_SEND, 0}, {"REV SEND", K_SEND, 1}, PRM("DST SEND", DK_BS_DIST), FXK("DLY LVL", FX_DLY_LEVEL)}},
    {G_FX, ST_NAME, "DIST", {PRM("MODE", P_DIST_MODE), PRM("DRIVE", P_DIST_DRIVE), PRM("TONE", P_DIST_TONE), PRM("LEVEL", P_DIST_LEVEL)}},
    {G_FX, ST_NAME, "DELAY", {FXK("TIME", FX_DLY_TIME), FXK("FEEDBACK", FX_DLY_FB), FXK("TONE", FX_DLY_TONE), FXK("PING", FX_DLY_PING)}},
    {G_FX, ST_NAME, "DELAY 2", {FXK("SYNC", FX_DLY_SYNC), FXK("NOTE", FX_DLY_DIV), FXK("MOD", FX_DLY_MOD), NO}},
    {G_FX, ST_NAME, "REVERB", {FXK("SIZE", FX_REV_SIZE), FXK("DECAY", FX_REV_DECAY), FXK("DAMP", FX_REV_DAMP), FXK("REV LVL", FX_REV_LEVEL)}},
    {G_FX, ST_NAME, "CHOKE", {PRM("HAT", DK_BS_CHOKE), PRM("CONGA", DK_BS_CHOKE + 1), PRM("TOM", DK_BS_CHOKE + 2), NO}},
    {G_SEQ, ST_STEPS, "STEPS", {{"INSTR", K_EXTRA, X_INST}, {"LENGTH", K_EXTRA, X_LEN}, NO, NO}},
    {G_SEQ, ST_LIVE, "LOOP", {LVK("LOOP", LV_SET_ON), LVK("BARS", LV_SET_BARS), LVK("LONGER", LV_SET_GROW), LVK("SHORTER", LV_SET_CUT)}},
    {G_GLO, ST_TEMPO, "TEMPO", {{"TEMPO", K_TEMPO, 0}, {"CLOCK", K_CLK, 0}, {"SEND", K_CLK, 1}, {"MIDI CH", K_CHAN, 0}}},
    {G_GLO, ST_SYS, "SYSTEM", {{"LEVEL", K_LEVEL, 0}, NO, NO, NO}},
    {G_SEL, ST_BROWSE, "SOUNDS", {NO, NO, NO, NO}},
};
static const layer_t gm_layers[NGROUP] = {
    [G_EDIT] = {"LEVELS", {PRM("BD", DK_BS_LEVEL + DK_BD), PRM("SD", DK_BS_LEVEL + DK_SD), PRM("CH", DK_BS_LEVEL + DK_CH), PRM("OH", DK_BS_LEVEL + DK_OH)}},
    [G_ENV] = {"TOMS", {PRM("LT", DK_BS_LEVEL + DK_LT), PRM("MT", DK_BS_LEVEL + DK_MT), PRM("HT", DK_BS_LEVEL + DK_HT), PRM("CONGAS", DK_BS_LEVEL + DK_MC)}},
    [G_LFO] = {"DECAY", {PRM("BD", DK_BS_KNOB + DK_BD * 3 + 1), PRM("SD", DK_BS_KNOB + DK_SD * 3 + 1), PRM("OH", DK_BS_KNOB + DK_OH * 3 + 1), PRM("CY", DK_BS_KNOB + DK_CY * 3 + 1)}},
    [G_FX] = {"EFFECTS", {{"DLY SEND", K_SEND, 0}, {"REV SEND", K_SEND, 1}, PRM("DST SEND", DK_BS_DIST), PRM("DRIVE", P_DIST_DRIVE)}},
    [G_ARP] = {"PERC", {PRM("CL", DK_BS_LEVEL + DK_CL), PRM("MA", DK_BS_LEVEL + DK_MA), PRM("CB", DK_BS_LEVEL + DK_CB), PRM("CP", DK_BS_LEVEL + DK_CP)}},
    [G_GLO] = {"MIX", {PRM("CP", DK_BS_LEVEL + DK_CP), PRM("RS", DK_BS_LEVEL + DK_RS), PRM("CY", DK_BS_LEVEL + DK_CY), PRM("CB", DK_BS_LEVEL + DK_CB)}},
    [G_SEQ] = {"STEPS", {{"INSTR", K_EXTRA, X_INST}, {"LENGTH", K_EXTRA, X_LEN}, NO, NO}},
};

/* ---- der Klang: zwei Zustände (Kern 0: engA, Kern 1: engB), im Pool (RAM ist knapp) */
static dk808_bs_t engA __attribute__((section(".pool"), aligned(16)));
static dk808_bs_t engB __attribute__((section(".pool"), aligned(16)));
#define DSPF __attribute__((section(".dsp_text"), noinline))
#include "gm_dist.h"
static volatile uint8_t dist_v[4];              /* Modus, Drive, Tone, Level */
static float dist_buf[2][GM_BLOCK] __attribute__((section(".pool")));   /* Send von Kern 0: Block n schreibt [n & 1] */
static float dist_k1[GM_BLOCK] __attribute__((section(".pool")));       /* Send von Kern 1 (Block davor) */
static float k1_l[GM_BLOCK], k1_r[GM_BLOCK];
static float k1_snd[2][GM_BLOCK] __attribute__((section(".pool")));     /* Delay/Hall-Send von Kern 1 (Block davor) */
/* Steps für beide Kerne: Kern 0 schreibt im Block n pend[n & 1], beide lesen im Block n + 1 pend[n & 1] */
static volatile uint32_t pend_hit[2], pend_acc[2], pend_off[2];
static uint8_t xv[DK_NX];                       /* Sequencer auf der Audioseite (kern_extra) */
static volatile uint8_t dk_step = 0xFF;         /* laufender Step (Anzeige), 0xFF = steht */
static void dk_fire(dk808_bs_t *e, uint32_t hit, uint32_t acc)
{
    for (unsigned i = 0; i < DK_COUNT; i++)
        if (hit >> i & 1u) dk808_trigger(&e->k, i, acc ? 127 : 100);
}
#if defined(DK_LAST) && DK_LAST
enum { DK_PH = 688u, DK_STEP = 86u, DK_NPH = 20u };
volatile uint32_t dk_last_peak[DK_NPH], dk_last_sum[DK_NPH], dk_last_n[DK_NPH];
volatile uint32_t dk_last_peak1[DK_NPH], dk_last_sum1[DK_NPH];
static volatile uint32_t dk_blk, dk_t1;
#endif
static void dk_fx_pre(float *sl, float *sr);
static void dk_k1(float *wl, float *wr);
#define GM_FX_PRE(sl, sr) dk_fx_pre(sl, sr)
#define GM_FX_EXTRA(wl, wr) dk_k1(wl, wr)
#define GM_FX_EXTRA_ON() 1
static int dk_key(uint32_t k, int down, uint32_t keys);
static void dk_lights(void);
#define GM_KEY(k, down, keys) dk_key(k, down, keys)
#define GM_LIGHTS() dk_lights()

static void kern_set(unsigned p, unsigned i, uint8_t v);
static void kern_boot(void)
{
    for (unsigned i = 0; i < DK_BS_NP; i++) gm_init_v[i] = dk808_bs_init_kit[i];
    gm_init_v[P_DIST_MODE] = 0; gm_init_v[P_DIST_DRIVE] = 60; gm_init_v[P_DIST_TONE] = 70; gm_init_v[P_DIST_LEVEL] = 64;
#if defined(DK_LAST) && DK_LAST
    gm_init_v[DK_BS_DIST] = 40;                 /* (Messung: Distortion-Send an) */
#endif
    for (unsigned s = 0; s < GM_NFAC; s++) {
        gm_factory[s].cat = CAT_KIT;
        for (unsigned i = 0; i < GM_NP; i++) gm_factory[s].v[i] = gm_init_v[i];
    }
    gm_factory[0].name = "808 KIT";
    gm_factory[1].name = "808 TUBE";            /* derselbe Kit, ein Teil durch die Distortion (TUBE) */
    gm_factory[1].v[DK_BS_DIST] = 40;
    gm_dist_init();
    dk808_bs_init(&engA, 44100.0);
    dk808_bs_init(&engB, 44100.0);
    dk808_set_run(&engA.k, DK808_MASK_ALL & ~(uint32_t)(DK_K1_MASK));
    dk808_set_run(&engB.k, (uint32_t)(DK_K1_MASK));
    for (unsigned i = 0; i < GM_NP; i++) kern_set(0, i, gm_init_v[i]);
    for (unsigned i = 0; i < DK_NX; i++) xv[i] = (uint8_t)gm_extra_default(i);
}
static DSPF void kern_extra(unsigned i, unsigned v) { if (i < DK_NX) xv[i] = (uint8_t)v; }
static DSPF void kern_set(unsigned p, unsigned i, uint8_t v)
{
    (void)p;
    if (i >= GM_NP) return;
    if (v > gm_params[i].max) v = gm_params[i].max;
    if (i >= P_DIST_MODE) { dist_v[i - P_DIST_MODE] = v; return; }
    dk808_bs_set_raw(&engA, i, v);
    dk808_bs_set_raw(&engB, i, v);
}
static DSPF void kern_load(unsigned p, const uint8_t *v)
{
    for (unsigned i = 0; i < GM_NP; i++) kern_set(p, i, v[i]);
}
/* Noten: die Drum-Map des DrumKern (MIDI 35..75; die Tasten senden diese Noten, dk_key) */
static DSPF void kern_on(unsigned p, int note, int vel, int legato)
{
    (void)p; (void)legato;
    dk808_bs_note_on(&engA, note, vel);
    dk808_bs_note_on(&engB, note, vel);
}
static DSPF void kern_off(unsigned p, int note) { (void)p; (void)note; }
static DSPF void kern_all_off(unsigned p) { (void)p; dk808_bs_all_off(&engA); dk808_bs_all_off(&engB); }
static DSPF void kern_bend(unsigned p, float b) { (void)p; (void)b; }
static DSPF void kern_mod(unsigned p, float m) { (void)p; (void)m; }
static void kern_render(unsigned p, float *l, float *r, unsigned n);
static int kern_voices(unsigned p) { (void)p; return dk808_bs_voices(&engA) + dk808_bs_voices(&engB); }
static int kern_stage(unsigned stage, unsigned p);

#include "gm_app.c"

static DSPF void dk_fx_pre(float *sl, float *sr)
{
#if defined(DK_LAST) && DK_LAST
    dk_t1 = FM1_C1_T4;
#endif
    for (unsigned i = 0; i < GM_BLOCK; i++) { sl[i] += k1_snd[0][i]; sr[i] += k1_snd[1][i]; }
}
/* Kern 1: Distortion des Blocks davor (Sends beider Kerne), dann die Instrumente von engB für diesen Block */
static DSPF void dk_k1(float *wl, float *wr)
{
    uint32_t q = (block_no - 1u) & 1u;
    const float *in = dist_buf[q];
    if (engA.dist > 0 || engB.dist > 0) {
        for (unsigned i = 0; i < GM_BLOCK; i++) dist_k1[i] += in[i];
        gm_dist_process(dist_k1, (int)GM_BLOCK, dist_v[0], dist_v[1], dist_v[2], dist_v[3]);
        for (unsigned i = 0; i < GM_BLOCK; i++) { wl[i] += dist_k1[i]; wr[i] += dist_k1[i]; }
    }
    if (pend_off[q]) dk808_bs_all_off(&engB);
    dk_fire(&engB, pend_hit[q], pend_acc[q]);
    for (unsigned i = 0; i < GM_BLOCK; i++) k1_l[i] = k1_r[i] = dist_k1[i] = 0;
    dk808_bs_render(&engB, k1_l, k1_r, dist_k1, (int)GM_BLOCK);
    {
        float lv = part_level[0] * DK_GAIN, s0 = part_send[0][0] * lv, s1 = part_send[0][1] * lv;
        for (unsigned i = 0; i < GM_BLOCK; i++) {
            float y = k1_l[i];
            wl[i] += y * lv; wr[i] += y * lv;
            k1_snd[0][i] = y * s0; k1_snd[1][i] = y * s1;
            dist_k1[i] *= lv;
        }
    }
#if defined(DK_LAST) && DK_LAST
    {
        uint32_t t = FM1_C1_T4 - dk_t1, ph = dk_blk / DK_PH;
        if (ph < DK_NPH) { if (t > dk_last_peak1[ph]) dk_last_peak1[ph] = t; dk_last_sum1[ph] += t; }
    }
#endif
}

/* Kern 0: zuerst die Steps des Blocks davor (Kern 1 schlägt sie im selben Block an), dann der nächste Step */
static uint32_t seq_last = 0xFFFFFFFFu;
static uint8_t xport_was;
static DSPF void dk_seq(uint32_t q)
{
    uint32_t hit = 0, acc = 0, off = 0;
    if (xport == 1) {
        uint32_t abs = clk_ticks / 6u, len = xv[X_LEN] ? xv[X_LEN] : 16u;
        if (xport_was != 1) seq_last = 0xFFFFFFFFu;
        if (abs != seq_last) {
            unsigned st = (unsigned)(abs % len), byte = st >> 3, bit = st & 7u;
            seq_last = abs;
            for (unsigned i = 0; i < DK_COUNT; i++) if (xv[X_PAT + 2u * i + byte] >> bit & 1u) hit |= 1u << i;
            if (xv[X_PAT + 2u * DK_ACC + byte] >> bit & 1u) acc = 1;
            dk_step = (uint8_t)st;
        }
    } else dk_step = 0xFF;
    xport_was = xport;
#if defined(DK_LAST) && DK_LAST
    {   /* Messmuster: Phase 0 nichts, 1..17 je ein Instrument alle 1/16, 18 alle zugleich, 19 volles Pattern */
        static const uint32_t pat[16] = {
            1u << DK_BD | 1u << DK_CH | 1u << DK_CY, 1u << DK_CH, 1u << DK_OH | 1u << DK_LC, 1u << DK_CH | 1u << DK_RS,
            1u << DK_SD | 1u << DK_CP | 1u << DK_CH, 1u << DK_CH | 1u << DK_CB, 1u << DK_OH | 1u << DK_MC, 1u << DK_CH | 1u << DK_BD,
            1u << DK_BD | 1u << DK_CH, 1u << DK_CH | 1u << DK_HC, 1u << DK_OH | 1u << DK_LT, 1u << DK_CH | 1u << DK_MA,
            1u << DK_SD | 1u << DK_CP | 1u << DK_CH, 1u << DK_CH | 1u << DK_MT, 1u << DK_OH | 1u << DK_HT | 1u << DK_CL, 1u << DK_CH | 1u << DK_SD | 1u << DK_LG};
        uint32_t ph = dk_blk / DK_PH, k = dk_blk % DK_PH;
        if (k == 0) { dk808_bs_all_off(&engA); off = 1; }
        if (ph && ph < DK_NPH && k % DK_STEP == 0) {
            hit = ph <= DK_COUNT ? 1u << (ph - 1u) : ph == DK_COUNT + 1u ? (1u << DK_COUNT) - 1u : pat[(k / DK_STEP + ph * 4u) & 15u];
            acc = 1;
        }
    }
#endif
    pend_hit[q] = hit; pend_acc[q] = acc; pend_off[q] = off;
}
static DSPF void kern_render(unsigned p, float *l, float *r, unsigned n)
{
    (void)p;
#if defined(DK_LAST) && DK_LAST
    uint32_t t0 = fm1_ticks();
#endif
    uint32_t q = block_no & 1u;
    dk_fire(&engA, pend_hit[q ^ 1u], pend_acc[q ^ 1u]);
    dk_seq(q);
    for (unsigned i = 0; i < n; i++) dist_buf[q][i] = 0;
    dk808_bs_render(&engA, l, r, dist_buf[q], (int)n);
    for (unsigned i = 0; i < n; i++) { l[i] *= DK_GAIN; r[i] *= DK_GAIN; dist_buf[q][i] *= DK_GAIN * part_level[0]; }
    FM1_C1_SYNC();
#if defined(DK_LAST) && DK_LAST
    {
        uint32_t t = fm1_ticks() - t0, ph = dk_blk / DK_PH;
        if (ph < DK_NPH) { if (t > dk_last_peak[ph]) dk_last_peak[ph] = t; dk_last_sum[ph] += t; dk_last_n[ph]++; }
        dk_blk++;
    }
#endif
}

/* ---- Tasten: auf der Seite STEPS weiße = Steps 1–16 (EDIT + weiß: Akzent), schwarze = Instrument wählen und hören
 * (EDIT + schwarz: seine Zeile löschen). Sonst spielen die weißen Tasten die Instrumente 1–16 der Reihe nach
 * (BD LG SD RS CP LC MC HC LT MT HT CH OH CY CL MA), die schwarzen die elf wichtigsten (wie auf STEPS). */
static const uint8_t dk_black[11] = {DK_BD, DK_SD, DK_RS, DK_CP, DK_CB, DK_CY, DK_OH, DK_CH, DK_LT, DK_MT, DK_HT};
static int key_black(uint32_t k) { uint32_t n = (53u + k) % 12u; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }
static unsigned key_index(uint32_t k) { unsigned n = 0; for (uint32_t j = 0; j < k; j++) n += key_black(j) == key_black(k); return n; }
static int on_steps(void) { return pages[page].stage == ST_STEPS && !layer_shown(); }
static void x_put(unsigned i, unsigned v) { extrav[i] = (uint8_t)v; ev_put(EV_EXTRA, 0, i, v); }
static int dk_key(uint32_t k, int down, uint32_t keys)
{
    (void)keys;
    unsigned idx = key_index(k), edit = (fm1_in.buttons >> BTN_EDIT) & 1u;
    if (!on_steps() || (layer && !edit)) {  /* spielen */
        int inst = key_black(k) ? (idx < 11u ? dk_black[idx] : -1) : (idx < 16u ? (int)idx : -1);
        if (inst < 0) return 1;
        if (down) ev_put(EV_ON, 0, dk_note[inst], 100); else ev_put(EV_OFF, 0, dk_note[inst], 0);
        return 1;
    }
    if (!down) return 1;
    if (edit) layer_used = 1;
    if (key_black(k)) {
        if (idx >= 11u) return 1;
        unsigned inst = dk_black[idx];
        if (edit) {
            x_put(X_PAT + 2u * inst, 0); x_put(X_PAT + 2u * inst + 1u, 0);
            char b[24]; char *q = put(b, dk_abbr(inst)); put(q, ": CLEARED")[0] = 0; msg_set(b, 0);
        } else {
            x_put(X_INST, inst);
            ev_put(EV_ON, 0, dk_note[inst], 100);
        }
        return 1;
    }
    if (idx >= 16u) return 1;
    {
        unsigned row = edit ? DK_ACC : extrav[X_INST], i = X_PAT + 2u * row + (idx >> 3);
        x_put(i, extrav[i] ^ (1u << (idx & 7u)));
    }
    return 1;
}
static int step_on(unsigned row, unsigned s) { return extrav[X_PAT + 2u * row + (s >> 3)] >> (s & 7u) & 1u; }
static void dk_lights(void)
{
    if (!on_steps()) return;
    unsigned row = (fm1_in.buttons >> BTN_EDIT) & 1u ? DK_ACC : extrav[X_INST], len = extrav[X_LEN] ? extrav[X_LEN] : 16u;
    uint8_t cur = dk_step;
    for (uint32_t k = 0; k < 27u; k++) {
        unsigned idx = key_index(k);
        int on = 0;
        if (key_black(k)) on = idx < 11u && dk_black[idx] == extrav[X_INST];
        else if (idx < 16u) on = idx < len && (step_on(row, idx) ^ (cur == idx && fm1_ms / 60u % 2u));
        fm1_led_key(KEY_LED(k), on);
    }
}

/* ---- Bild der Seite STEPS: 17 Instrumente und Akzent × 16 Steps, gewählte Zeile hell, laufender Step markiert */
static int kern_stage(unsigned stage, unsigned p)
{
    (void)p;
    if (stage != ST_STEPS) return 0;
    unsigned cur = extrav[X_INST], len = extrav[X_LEN] ? extrav[X_LEN] : 16u;
    uint8_t ps = dk_step;
    char b[24], *q = put(b, dk_abbr(cur));
    q = put(q, "  "); fmt_int(q, (int32_t)len); q += str_len(q); put(q, " STEPS")[0] = 0;
    Q(4, 30, 2, b, T_DIM);
    for (unsigned r = 0; r < DK_ROWS; r++)
        for (unsigned s = 0; s < 16u; s++) {
            int32_t x = 4 + (int32_t)s * 14, y = 42 + (int32_t)r * 6;
            uint16_t c = s >= len ? T_BG : step_on(r, s) ? (r == cur ? T_SEL : r == DK_ACC ? T_ACC : T_VIOLET)
                                                     : (r == cur || s % 4u == 0) ? T_LINE : T_BG;
            if (s == ps && s < len) c = step_on(r, s) ? T_ACC : T_DIM;
            R(x, y, 12, 5, c);
        }
    return 1;
}
