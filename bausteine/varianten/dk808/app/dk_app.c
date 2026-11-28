/* SPDX-License-Identifier: GPL-3.0-only */
/* 808 solo für das M-VAVE FM-1 (ADR 0050, Feature B20): eine Drum-Machine wie der Bereich BEAT von FM1-Electro.
 * Klang: der Baustein dk808 (DrumKern, alle 17 Instrumente, float) auf zwei Zuständen (Kern 0: alles außer DK_K1_MASK,
 * Kern 1: CH, OH, CY neben Delay, Hall und Distortion). Sequencer, Generator und Kits: app/dk_beat.c (aus FM1-Electro
 * engine/drums.c, drum_factory.c). Bedienung: app/dk_ui.c (aus FM1-Electro ui/el_ui.c, Bereich BEAT), statt gm_ui.c
 * (GM_UI_FILE). Vom Gerüst bleiben Audio, Takt (intern, MIDI-Clock), MIDI, Kern 1, Effekte und die Plattform.
 *
 * Ablauf je Block: GM_PRE_C1 (Kern 0, bevor Kern 1 startet) wendet die Befehle der Bedienung an, rechnet Steps, Rolls
 * und Chokes und schlägt beide Zustände an; dann rechnet Kern 0 Zustand A (kern_render), Kern 1 Zustand B, Delay,
 * Hall und die Distortion (GM_FX_EXTRA). DK_LAST=1: eingebautes Messmuster (README). */
#define GM_TITLE "TR-808"
#define GM_ACCENT RGB(255, 120, 0)
#define GM_PARTS 1u
#define GM_NAMEMAX 10u
#define GM_SOUND_VER 1u
#define GM_JR_MAGIC 0x54534B44u                 /* "DKST" (das Journal des Gerüsts bleibt hier ungenutzt) */
#define GM_SEND_FIRST 0
#define GM_UI_FILE "dk_ui.c"
#include "gm_types.h"
#include "dk_beat.h"

/* Das Gerüst verlangt Tabellen für Sounds und Seiten; 808 solo nutzt sie nicht (eigene Bedienung, Kits in dk_beat.c) */
enum { GM_NP = 1 };
static const gm_param_t gm_params[GM_NP] = {{"-", 1, C_AMP, 0}};
static const uint8_t gm_init_v[GM_NP] = {0};
enum { GM_NCAT = 1 };
static const char *const gm_cat_name[GM_NCAT] = {"KITS"};
typedef struct { const char *name; uint8_t cat; uint8_t v[GM_NP]; } gm_sound_t;
#define GM_NFAC 1u
static gm_sound_t gm_factory[GM_NFAC] = {{"808", 0, {0}}};
static const page_t gm_pages[] __attribute__((unused)) = {{G_HOME, ST_HOME, "PLAY", {{"", K_NONE, 0}, {"", K_NONE, 0}, {"", K_NONE, 0}, {"", K_NONE, 0}}}};
static const layer_t gm_layers[NGROUP] __attribute__((unused)) = {{0}};

#define DSPF __attribute__((section(".dsp_text"), noinline))
#include "gm_dist.h"
#ifndef DK_GAIN
#define DK_GAIN 1.6f                            /* Velocity 100: Spitzen 0,2 (CB) bis 0,6 (HC); BD so etwa -13 dBFS */
#endif
/* Distortion (FM1-Electro: HOME + EDIT): Modus, Drive, Tone, Level; der Send der 808 steht in db_engA/B.dist */
static volatile uint8_t dist_v[4] = {0, 60, 70, 64};
static float dist_buf[2][GM_BLOCK] __attribute__((section(".pool")));   /* Send von Kern 0: Block n schreibt [n & 1] */
static float dist_k1[GM_BLOCK] __attribute__((section(".pool")));       /* Send von Kern 1 (Block davor) */
static float k1_l[GM_BLOCK], k1_r[GM_BLOCK];
static float k1_snd[2][GM_BLOCK] __attribute__((section(".pool")));     /* Delay/Hall-Send von Kern 1 (Block davor) */
static void dk_fx_pre(float *sl, float *sr);
static void dk_k1(float *wl, float *wr);
static void dk_pre_c1(void);
#define GM_FX_PRE(sl, sr) dk_fx_pre(sl, sr)
#define GM_FX_EXTRA(wl, wr) dk_k1(wl, wr)
#define GM_FX_EXTRA_ON() 1
#define GM_PRE_C1() dk_pre_c1()
static void dk_post_mix(float *ml, float *mr, float *wl, float *wr);
#define GM_POST_MIX(ml, mr, wl, wr) dk_post_mix(ml, mr, wl, wr)

static void kern_boot(void) { gm_dist_init(); }   /* (der Klang: db_init/db_restore in app_main, vor dem Audio) */
static DSPF void kern_set(unsigned p, unsigned i, uint8_t v) { (void)p; (void)i; (void)v; }
static DSPF void kern_load(unsigned p, const uint8_t *v) { (void)p; (void)v; }
/* MIDI-Noten (und die Live-Schleife des Gerüsts): das Teil nach der Drum-Map, aufgenommen, wenn REC an ist */
static DSPF void kern_on(unsigned p, int note, int vel, int legato) { (void)p; (void)legato; db_note(note, vel); }
static DSPF void kern_off(unsigned p, int note) { (void)p; (void)note; }
static DSPF void kern_all_off(unsigned p) { (void)p; db_post(DB_PANIC, 0, 0, 0, 0); }
static DSPF void kern_bend(unsigned p, float b) { (void)p; (void)b; }
static DSPF void kern_mod(unsigned p, float m) { (void)p; (void)m; }
static void kern_render(unsigned p, float *l, float *r, unsigned n);
static int kern_voices(unsigned p) { (void)p; return db_voices(); }
static int kern_stage(unsigned stage, unsigned p) { (void)stage; (void)p; return 0; }

#include "gm_app.c"

#if defined(DK_LAST) && DK_LAST
/* Messmuster: Phase 0 nichts, dann je 1 s (688 Blöcke) ein Instrument alle 1/16 (Velocity 127), Phase 18 alle 17
 * zugleich, Phase 19 ein volles Pattern; Delay, Hall und Distortion-Send an. Takte je Block je Phase in dk_last_*. */
enum { DK_PH = 688u, DK_STEP = 86u, DK_NPH = 20u };
volatile uint32_t dk_last_peak[DK_NPH], dk_last_sum[DK_NPH], dk_last_n[DK_NPH];
volatile uint32_t dk_last_peak1[DK_NPH], dk_last_sum1[DK_NPH];
static volatile uint32_t dk_blk, dk_t1, dk_k0;   /* dk_k0: Takte auf Kern 0 in diesem Block (Sequencer, A, Punch-ins) */
static DSPF void dk_last_pattern(void)
{
    static const uint32_t pat[16] = {
        1u << P_BD | 1u << P_CH | 1u << P_CY, 1u << P_CH, 1u << P_OH | 1u << P_LC, 1u << P_CH | 1u << P_RS,
        1u << P_SD | 1u << P_CP | 1u << P_CH, 1u << P_CH | 1u << P_CB, 1u << P_OH | 1u << P_HC, 1u << P_CH | 1u << P_BD,
        1u << P_BD | 1u << P_CH, 1u << P_CH | 1u << P_HC, 1u << P_OH | 1u << P_LT, 1u << P_CH | 1u << P_MA,
        1u << P_SD | 1u << P_CP | 1u << P_CH, 1u << P_CH | 1u << P_LT, 1u << P_OH | 1u << P_HT | 1u << P_CL, 1u << P_CH | 1u << P_SD | 1u << P_LG};
    uint32_t ph = dk_blk / DK_PH, k = dk_blk % DK_PH, hit = 0;
    if (dk_blk == 100u) {
        syn_fx_set(FX_DLY_LEVEL, 100); syn_fx_set(FX_REV_LEVEL, 100); fx_on = 1;
        part_send[0][0] = part_send[0][1] = 0.5f;
        db_engA.dist = db_engB.dist = 0.3f;
        pn.punch = 0x7Fu & ~(8u | 2u);          /* alle Punch-ins an; nicht REPEAT (spielt das Muster um) und THROW (blendet aus: Delay/Hall ruhen) */
    }
    if (k == 0) db_post(DB_PANIC, 0, 0, 0, 0);
    if (ph && ph < DK_NPH && k % DK_STEP == 0)
        hit = ph <= DB_PARTS ? 1u << (ph - 1u) : ph == DB_PARTS + 1u ? (1u << DB_PARTS) - 1u : pat[(k / DK_STEP + ph * 4u) & 15u];
    for (unsigned i = 0; i < DB_PARTS; i++) if (hit >> i & 1u) db_post(DB_HIT, (uint8_t)i, 127, 0, 0);
}
#endif

/* Kern 0, bevor Kern 1 startet: der Sequencer (Befehle, Steps mit Swing, Rolls) für beide Zustände */
static DSPF void dk_pre_c1(void)
{
#if defined(DK_LAST) && DK_LAST
    uint32_t t0 = fm1_ticks();
#endif
    float tick_len = EL_SR * 60.0f / ((tempo_set < 40.0f ? 40.0f : tempo_set) * 24.0f);
    float ticks = (float)clk_ticks + (ext_follow ? 0.0f : int_acc / tick_len);
#if defined(DK_LAST) && DK_LAST
    dk_last_pattern();
#endif
    db_block(xport == 1, ticks, bpm_now, GM_BLOCK);
#if defined(DK_LAST) && DK_LAST
    dk_k0 = fm1_ticks() - t0;
#endif
}
/* Kern 0: Zustand A; sein Distortion-Send für Kern 1 im nächsten Block */
static DSPF void kern_render(unsigned p, float *l, float *r, unsigned n)
{
    uint32_t q = block_no & 1u;
    (void)p;
#if defined(DK_LAST) && DK_LAST
    uint32_t t0 = fm1_ticks();
#endif
    for (unsigned i = 0; i < n; i++) dist_buf[q][i] = 0;
    db_render_k0(l, r, dist_buf[q], (int)n);
    for (unsigned i = 0; i < n; i++) { l[i] *= DK_GAIN; r[i] *= DK_GAIN; dist_buf[q][i] *= DK_GAIN * part_level[0]; }
    FM1_C1_SYNC();
#if defined(DK_LAST) && DK_LAST
    dk_k0 += fm1_ticks() - t0;
#endif
}
/* Kern 1, vor Delay und Hall: die Sends von Zustand B aus dem Block davor */
static DSPF void dk_fx_pre(float *sl, float *sr)
{
#if defined(DK_LAST) && DK_LAST
    dk_t1 = FM1_C1_T4;
#endif
    for (unsigned i = 0; i < GM_BLOCK; i++) { sl[i] += k1_snd[0][i]; sr[i] += k1_snd[1][i]; }
}
/* Kern 1, nach Delay und Hall: die Distortion des Blocks davor (Sends beider Zustände), dann Zustand B */
static DSPF void dk_k1(float *wl, float *wr)
{
    uint32_t q = (block_no - 1u) & 1u;
    if (db_engA.dist > 0.0f || db_engB.dist > 0.0f) {
        for (unsigned i = 0; i < GM_BLOCK; i++) dist_k1[i] += dist_buf[q][i];
        gm_dist_process(dist_k1, (int)GM_BLOCK, dist_v[0], dist_v[1], dist_v[2], dist_v[3]);
        for (unsigned i = 0; i < GM_BLOCK; i++) { wl[i] += dist_k1[i]; wr[i] += dist_k1[i]; }
    }
    for (unsigned i = 0; i < GM_BLOCK; i++) k1_l[i] = k1_r[i] = dist_k1[i] = 0;
    db_render_k1(k1_l, k1_r, dist_k1, (int)GM_BLOCK);
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

#if defined(DK_LAST) && DK_LAST
/* Messung: nach den Punch-ins (GM_POST_MIX) steht die Arbeit von Kern 0 dieses Blocks fest */
static DSPF void dk_last_k0(uint32_t t)
{
    uint32_t ph = dk_blk / DK_PH;
    t += dk_k0;
    if (ph < DK_NPH) { if (t > dk_last_peak[ph]) dk_last_peak[ph] = t; dk_last_sum[ph] += t; dk_last_n[ph]++; }
    dk_blk++;
}
#endif
