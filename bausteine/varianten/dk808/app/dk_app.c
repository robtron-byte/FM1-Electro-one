/* SPDX-License-Identifier: GPL-3.0-only */
/* 808 solo (Messfassung, ADR 0050): der Baustein bausteine/dk808 (DrumKern, float, DK808_FLOAT=1) über den
 * Gerüst-Adapter dk808_bs auf dem gemeinsamen Gerüst bausteine/varianten/gemeinsam. Ein Drum-Part; sein Sound ist ein
 * Kit (dk808_bs.h: Pegel, Regler, Choke, DIST SEND) und die Distortion (Modus, Drive, Tone, Level). Kern 0 rechnet die
 * Instrumente, Kern 1 Delay, Hall und die Distortion als Send (gemeinsam/fx/gm_dist.c, TUBE zuerst).
 *
 * Noch keine Firmware des Nutzers: Bedienung und Sounds folgen mit 808 solo. DK_LAST=1 (tools/build.py, Umgebung
 * DK_LAST=1) spielt ein eingebautes Messmuster und hält die Last je Instrument in dk_last_peak[] fest (README). */
#define APP_NAME "808"
#define GM_TITLE "TR-808"
#define GM_ACCENT RGB(255, 120, 0)              /* das Orange der 808 */
#define GM_PARTS 1u
#define GM_NAMEMAX 10u
#define GM_SOUND_VER 1u                         /* Kit: DK_BS_NP Rohwerte + 4 Distortion */
#define GM_JR_MAGIC 0x54534B44u                 /* "DKST": Journal dieser Firmware */
#include "gm_types.h"
#include "dk808_bs.h"

enum { P_DIST_MODE = DK_BS_NP, P_DIST_DRIVE, P_DIST_TONE, P_DIST_LEVEL, DK_NP };
#define GM_NP DK_NP

static void t_put(char *b, const char *s) { while ((*b++ = *s++) != 0) {} }
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
 * LFO: Hi-Hats und Becken, ARP: Claves, Maracas, Cowbell) */
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
    {G_FX, ST_NAME, "REVERB", {FXK("SIZE", FX_REV_SIZE), FXK("DECAY", FX_REV_DECAY), FXK("DAMP", FX_REV_DAMP), FXK("REV LVL", FX_REV_LEVEL)}},
    {G_FX, ST_NAME, "CHOKE", {PRM("HAT", DK_BS_CHOKE), PRM("CONGA", DK_BS_CHOKE + 1), PRM("TOM", DK_BS_CHOKE + 2), NO}},
    {G_SEQ, ST_LIVE, "LOOP", {LVK("LOOP", LV_SET_ON), LVK("BARS", LV_SET_BARS), LVK("LONGER", LV_SET_GROW), LVK("SHORTER", LV_SET_CUT)}},
    {G_SEQ, ST_SEQ, "SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
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
    [G_SEQ] = {"SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
};

/* ---- der Klang: dk808 über dk808_bs, Zustand im Pool (SH-101 zeigt: RAM ist knapp) */
static dk808_bs_t eng __attribute__((section(".pool"), aligned(16)));
#include "gm_dist.h"
static volatile uint8_t dist_v[4];              /* Modus, Drive, Tone, Level */
static volatile uint8_t dist_on;
static float dist_buf[2][GM_BLOCK] __attribute__((section(".pool")));
static volatile uint32_t dist_wr;
static float dist_tmp[GM_BLOCK] __attribute__((section(".pool")));
static __attribute__((section(".dsp_text"), noinline)) void dk_dist_fx(float *wl, float *wr)
{
    const float *in = dist_buf[dist_wr ^ 1u];
    if (!dist_on) return;
    for (unsigned i = 0; i < GM_BLOCK; i++) dist_tmp[i] = in[i];
    gm_dist_process(dist_tmp, (int)GM_BLOCK, dist_v[0], dist_v[1], dist_v[2], dist_v[3]);
    for (unsigned i = 0; i < GM_BLOCK; i++) { wl[i] += dist_tmp[i]; wr[i] += dist_tmp[i]; }
}
#define GM_FX_EXTRA(wl, wr) dk_dist_fx(wl, wr)
#define GM_FX_EXTRA_ON() (dist_on != 0)
#ifndef DK_GAIN
#define DK_GAIN 0.7f                            /* der DrumKern spitzt bis etwa 1,4 (BD v127) */
#endif

static void kern_set(unsigned p, unsigned i, uint8_t v);
static void kern_boot(void)
{
    for (unsigned i = 0; i < DK_BS_NP; i++) gm_init_v[i] = dk808_bs_init_kit[i];
    gm_init_v[P_DIST_MODE] = 0; gm_init_v[P_DIST_DRIVE] = 60; gm_init_v[P_DIST_TONE] = 70; gm_init_v[P_DIST_LEVEL] = 64;
#if defined(DK_LAST) && DK_LAST
    gm_init_v[DK_BS_DIST] = 40;                 /* (Messung: Distortion-Send an, Kern 1 rechnet TUBE) */
#endif
    for (unsigned s = 0; s < GM_NFAC; s++) {
        gm_factory[s].cat = CAT_KIT;
        for (unsigned i = 0; i < GM_NP; i++) gm_factory[s].v[i] = gm_init_v[i];
    }
    gm_factory[0].name = "808 KIT";
    gm_factory[1].name = "808 TUBE";             /* derselbe Kit, ein Teil durch die Distortion (TUBE) */
    gm_factory[1].v[DK_BS_DIST] = 40;
    gm_dist_init();
    dk808_bs_init(&eng, 44100.0);
    for (unsigned i = 0; i < GM_NP; i++) kern_set(0, i, gm_init_v[i]);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_set(unsigned p, unsigned i, uint8_t v)
{
    (void)p;
    if (i >= GM_NP) return;
    if (v > gm_params[i].max) v = gm_params[i].max;
    if (i >= P_DIST_MODE) { dist_v[i - P_DIST_MODE] = v; return; }
    if (i == DK_BS_DIST) dist_on = v != 0;
    dk808_bs_set_raw(&eng, i, v);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_load(unsigned p, const uint8_t *v)
{
    for (unsigned i = 0; i < GM_NP; i++) kern_set(p, i, v[i]);
}
/* Tasten: die Drum-Map des DrumKern (MIDI 35..75); Noten ohne Instrument nach dem Ton in der Oktave */
static const uint8_t key_inst[12] = {DK_BD, DK_RS, DK_SD, DK_CP, DK_LT, DK_MT, DK_CH, DK_HT, DK_OH, DK_CY, DK_CB, DK_CL};
static __attribute__((section(".dsp_text"), noinline)) void kern_on(unsigned p, int note, int vel, int legato)
{
    (void)p; (void)legato;
    if (dk808_bs_note_on(&eng, note, vel) < 0 && note >= 0 && vel > 0)
        dk808_trigger(&eng.k, key_inst[note % 12], vel > 127 ? 127 : vel);
}
static __attribute__((section(".dsp_text"), noinline)) void kern_off(unsigned p, int note) { (void)p; (void)note; }
static __attribute__((section(".dsp_text"), noinline)) void kern_all_off(unsigned p) { (void)p; dk808_bs_all_off(&eng); }
static __attribute__((section(".dsp_text"), noinline)) void kern_bend(unsigned p, float b) { (void)p; (void)b; }
static __attribute__((section(".dsp_text"), noinline)) void kern_mod(unsigned p, float m) { (void)p; (void)m; }
static void kern_render(unsigned p, float *l, float *r, unsigned n);
static int kern_voices(unsigned p) { (void)p; return dk808_bs_voices(&eng); }
static int kern_stage(unsigned stage, unsigned p) { (void)stage; (void)p; return 0; }

#include "gm_app.c"

#ifndef DK_LAST
#define DK_LAST 0
#endif
#if DK_LAST
/* Messmuster: Phase 0 nichts (Rauschen und Metallbank laufen immer); dann je 1 s (688 Blöcke) ein Instrument, alle 1/16 bei 120 BPM (86 Blöcke) mit Velocity 127 neu
 * angeschlagen (Phasen 1..17); Phase 18 alle 17 zugleich; Phase 19 ein volles Pattern (BD, SD, CP, CH/OH, CY, CB,
 * Toms, Congas, RS über 16 Schritte). Zu Beginn jeder Phase alles still. Takte je Block (Spitze, Summe, Anzahl) je Phase in dk_last_*. */
enum { DK_PH = 688u, DK_STEP = 86u, DK_NPH = 20u };
volatile uint32_t dk_last_peak[DK_NPH], dk_last_sum[DK_NPH], dk_last_n[DK_NPH];
static uint32_t dk_blk;
static const uint32_t dk_pat[16] = {   /* Bit = Instrument */
    1u << DK_BD | 1u << DK_CH | 1u << DK_CY, 1u << DK_CH, 1u << DK_OH | 1u << DK_LC, 1u << DK_CH | 1u << DK_RS,
    1u << DK_SD | 1u << DK_CP | 1u << DK_CH, 1u << DK_CH | 1u << DK_CB, 1u << DK_OH | 1u << DK_MC, 1u << DK_CH | 1u << DK_BD,
    1u << DK_BD | 1u << DK_CH, 1u << DK_CH | 1u << DK_HC, 1u << DK_OH | 1u << DK_LT, 1u << DK_CH | 1u << DK_MA,
    1u << DK_SD | 1u << DK_CP | 1u << DK_CH, 1u << DK_CH | 1u << DK_MT, 1u << DK_OH | 1u << DK_HT | 1u << DK_CL, 1u << DK_CH | 1u << DK_SD | 1u << DK_LG};
static __attribute__((section(".dsp_text"), noinline)) void dk_last_pattern(void)
{
    uint32_t ph = dk_blk / DK_PH, k = dk_blk % DK_PH;
    if (k == 0) dk808_bs_all_off(&eng);
    if (ph && ph < DK_NPH && k % DK_STEP == 0) {
        uint32_t m = ph <= DK_COUNT ? 1u << (ph - 1u) : ph == DK_COUNT + 1u ? (1u << DK_COUNT) - 1u : dk_pat[(k / DK_STEP + ph * 4u) & 15u];
        for (unsigned i = 0; i < DK_COUNT; i++) if (m >> i & 1u) dk808_trigger(&eng.k, i, 127);
    }
}
#endif

static __attribute__((section(".dsp_text"), noinline)) void kern_render(unsigned p, float *l, float *r, unsigned n)
{
    (void)p;
#if DK_LAST
    uint32_t t0 = fm1_ticks();
    dk_last_pattern();
#endif
    uint32_t w = dist_wr;
    for (unsigned i = 0; i < n; i++) dist_buf[w][i] = 0;
    for (unsigned i = 0; i < n; i++) l[i] = r[i] = 0;   /* (das Gerüst nullt, hier zur Sicherheit für DK_GAIN) */
    dk808_bs_render(&eng, l, r, dist_buf[w], (int)n);
    for (unsigned i = 0; i < n; i++) { l[i] *= DK_GAIN; r[i] *= DK_GAIN; dist_buf[w][i] *= DK_GAIN; }
    FM1_C1_SYNC();
    dist_wr = w ^ 1u;
#if DK_LAST
    {
        uint32_t t = fm1_ticks() - t0, ph = dk_blk / DK_PH;
        if (ph < DK_NPH) {
            if (t > dk_last_peak[ph]) dk_last_peak[ph] = t;
            dk_last_sum[ph] += t; dk_last_n[ph]++;
        }
        dk_blk++;
    }
#endif
}
