/* SPDX-License-Identifier: GPL-3.0-only */
/* JP-6 solo for the M-VAVE FM-1 (FM1-Electro-one, bausteine/varianten/jp6, ADR 0048): the JX-8P firmware's app of
 * fm1-synths (github.com/robtron-byte/fm1-synths, commit 2a7b4fa, jx8p/app/jx_app.c) with the JP-6 pool engine
 * engine/jpp.c (FM1-Electro's engine/jp6.c in four parts, a shared pool, both cores). What follows is the JX-8P app's
 * description, true for this one with JP-6 for JX-8P.
 *
 * JX-8P for the M-VAVE FM-1 (docs/adr/0003). One unit with the platform (platform/firmware/src/platform.c:
 * Dinghy / Felucca by Leo Kuroshita, Hügelton Instruments, with SLOOP's and FM1-Electro's changes,
 * GPL-3.0-only; platform/ORIGIN.md).
 *
 * The MKS-50 firmware's app (../../mks50/app/mk_app.c, same repository and licence) with the JX-8P engine:
 * a pool of six voices plays four parts, each with its sound, chorus and MIDI channel, on both cores; MIDI
 * is read in the audio interrupt, the panel's notes come through a queue, so nothing on the screen waits
 * in front of a note (../../mks50/docs/adr/0004). Arp, chord, MIDI sync, delay and reverb as there.
 *
 * The panel and the screen: app/jp_ui.c. */
#define APP_NAME "JP-6"
#include "syn_config.h"
#define HALF_FRAMES SYN_BLOCK
static void app_idle(void);
#define LCD_IDLE app_idle
static void jp_boot(void);
#define APP_BOOT jp_boot
#define CV_MAX (240u * 30u)                     /* the screen is drawn in bands of 30 lines (app/jp_ui.c): the pool keeps the rest for the effects */
#include "platform.c"
/* engine/el_config.h (through jp6.h) and shared/syn_config.h define the same names: syn_config.h is the one here */
#define EL_CONFIG_H
#include "jpp.h"
#include "syn_fx.h"
#include "jp_theme.h"

#define DSP __attribute__((section(".dsp_text"), noinline))

/* ---- the second core. hal/fm1_cpu1.h: X0X's (Charles Vestal), hardened in
 * SLOOP 2.8.3; the start as SLOOP's src/cpu1.c cpu1_boot (Andre Rezemini) and
 * FM1-Electro's fm1/el_app.c: started at power-on before the audio, proved
 * with a self-test job; any fault later and the platform holds it: one core
 * until power-off */
static uint8_t cpu1_ok;
static volatile uint32_t cpu1_test_out;
static uint32_t cpu1_lcg(uint32_t x)
{
    for (uint32_t i = 0; i < 64u; i++) x = x * 1664525u + 1013904223u;
    return x;
}
static void cpu1_selftest(uint32_t arg) { cpu1_test_out = cpu1_lcg(arg); }

/* ---- the synth and what the main loop asks of it (applied in the audio
 * interrupt, at the start of a block: one writer, one reader) */
static jpp_t synth;
static uint8_t ready;
enum { EV_ON, EV_OFF, EV_ALL_OFF, EV_SET, EV_LEVEL, EV_FX, EV_ARP, EV_TEMPO, EV_PANIC, EV_CLK, EV_TEST, EV_SEQ, EV_SEQ_LEN };
typedef struct { uint8_t type, part, a, b; } jp_ev_t;
#define EVQ 128u
static jp_ev_t evq[EVQ];
static volatile uint32_t ev_w, ev_r;
static void ev_put(uint32_t type, uint32_t part, uint32_t a, uint32_t b)
{
    uint32_t w = ev_w, n;
    for (n = 0; w - ev_r >= EVQ && n < 200000u; n++) {}   /* full: the audio interrupt empties it every 1.5 ms */
    if (w - ev_r >= EVQ) return;
    evq[w % EVQ] = (jp_ev_t){(uint8_t)type, (uint8_t)part, (uint8_t)a, (uint8_t)b};
    FM1_C1_SYNC();
    ev_w = w + 1u;
}
/* a whole sound for a part: written, then flagged; the audio interrupt takes it and clears the flag */
static uint8_t pend_v[JPP_PARTS][JP_COUNT];
static volatile uint8_t pend_flag[JPP_PARTS];
static volatile uint8_t part_ch[JPP_PARTS] = {1, 2, 3, 4};   /* MIDI channel of each part: 0 = all (OMNI), 1..16 */
static uint8_t want_cores = 2;

static void jp_boot(void)
{
    jpp_init(&synth, JPP_POOL, 20261114u);
    syn_fx_init();
    fm1_guard_pc_open();                        /* it boots through the chip's ROM */
    int started = fm1_cpu1_start() == 0;
    fm1_guard_enable(FM1_GUARD_PC);
    if (started) {
        cpu1_test_out = 0;
        if (!fm1_cpu1_run(cpu1_selftest, 20261110u) || fm1_cpu1_wait() != 0 || cpu1_test_out != cpu1_lcg(20261110u))
            fm1_cpu1_hold();
        else
            cpu1_ok = 1;
    }
    ready = 1;
}

/* ---- what a block costs, in timer ticks (24 a microsecond; both cores read
 * the same timer). The audio interrupt adds up, the main loop takes the sums
 * every half second. */
#define BLOCK_TICKS ((uint32_t)(24000000ull * JPP_BLOCK / 44100u))
enum { M_ALL, M_K0, M_K1, M_V0, M_V1, M_C0, M_C1, M_EV, M_BEGIN, M_FX, M_N };
static struct {
    uint32_t sum[M_N], peak[M_N], blocks, late;
} meas;
static volatile uint32_t c1_voices_t, c1_finish_t, c1_control_t, c1_fx_t;
static volatile uint32_t ph_voices[2];          /* the block whose voices this core has finished */
static uint32_t block_no;

/* ---- tempo (../../mks50/docs/adr/0010): the TEMPO setting, or the MIDI clock while one comes */
static volatile float bpm_now = 120.0f;
static float tempo_set = 120.0f;
static uint32_t clk_t_last, clk_t24, clk_n;
static volatile uint8_t clk_running;           /* a clock came in the last 250 ms */
static volatile uint32_t clk_ticks;             /* the song position in ticks (24 a quarter note) */
/* MIDI sync (../../mks50/docs/adr/0012) */
enum { SRC_AUTO, SRC_INT, SRC_MIDI };
static uint8_t clk_src, clk_send;               /* where the clock comes from; send it over USB (0 off, 1 on) */
static volatile uint8_t xport;                  /* transport: 0 free (no START seen), 1 running, 2 stopped */
static volatile uint8_t xport_req;              /* the panel: 1 start, 2 stop (PLAY held, internal clock) */
static volatile uint8_t ext_follow;             /* this block: the ticks come from MIDI */
static volatile uint16_t tempo_taken;           /* the clock's last tempo in 0.1 BPM, kept as TEMPO when the internal clock takes over (for the panel; 0 none) */
static float int_acc;                           /* the internal clock: samples into the current tick */
static DSP void xport_start(void);
static DSP void xport_stop(void);
#define BPM bpm_now

/* ---- delay and reverb (../../mks50/docs/adr/0009): fed by the sends of the block before, run on core 1 after its parts
 * (or on core 0 when core 1 has nothing): the wet signal comes 1.45 ms after the dry one */
static float fx_in[2][JPP_BLOCK], fx_wet[2][JPP_BLOCK];
static uint8_t fx_on;                           /* delay or reverb level above 0 */
static DSP void fx_run(void)
{
    for (uint32_t i = 0; i < JPP_BLOCK; i++) fx_wet[0][i] = fx_wet[1][i] = 0;
    if (fx_on) syn_fx_process(fx_in[0], fx_in[1], fx_wet[0], fx_wet[1], JPP_BLOCK, bpm_now);
}
static volatile uint32_t ph_control[2];
/* wait until core `who` has reached block blk in ph[]: 0 = yes, -1 = not within 4 ms */
static DSP int wait_phase(volatile uint32_t *ph, uint32_t blk)
{
    uint32_t t = FM1_C1_T4;
    while (*ph != blk) {
        FM1_C1_SYNC();
        if (FM1_C1_T4 - t > 24000u * 4u) return -1;
    }
    return 0;
}

static DSP void c1_job(uint32_t blk)
{
    uint32_t t0, t1;
    t0 = FM1_C1_T4;
    jpp_control(&synth, 1, BPM);
    c1_control_t = FM1_C1_T4 - t0;
    FM1_C1_SYNC();
    ph_control[1] = blk;
    FM1_C1_SYNC();
    t0 = FM1_C1_T4;
    fx_run();                                   /* the block before's sends: while core 0 renders its voices */
    c1_fx_t = FM1_C1_T4 - t0;
    if (wait_phase(&ph_control[0], blk)) return;   /* (core 0 gives it up first, at 4 ms) */
    t0 = FM1_C1_T4;
    jpp_voices(&synth, 1);
    t1 = FM1_C1_T4;
    c1_voices_t = t1 - t0;
    FM1_C1_SYNC();
    ph_voices[1] = blk;
    FM1_C1_SYNC();
    if (wait_phase(&ph_voices[0], blk)) return;    /* a part's sum wants both cores' voices */
    t1 = FM1_C1_T4;
    jpp_finish(&synth, 1);
    c1_finish_t = FM1_C1_T4 - t1;
    FM1_C1_SYNC();
}

static DSP int give_up(void)                    /* core 1 did not answer: held, one core from now on (the caller does its work) */
{
    fm1_cpu1_hold();
    fm1_cpu1_bar();
    cpu1_ok = 0;
    return 0;
}

static DSP void m_add(uint32_t i, uint32_t t)
{
    meas.sum[i] += t;
    if (t > meas.peak[i]) meas.peak[i] = t;
}

/* ======================================================================================================
 * Playing (../../mks50/docs/adr/0010): every note, from the keys or MIDI, goes through the part's arpeggiator (when on)
 * and its chord (key mode CHORD), then to the engine. Audio interrupt only.
 * ====================================================================================================== */
/* the chord types of the iOS app (Shared/Model/ChordMemory.swift, as FM1-Electro's): offsets, -1 ends */
static const int8_t chords[8][5] = {{0, 3, 7, 10, -1}, {0, 2, 7, -1}, {0, 3, 7, -1}, {0, 7, 12, -1},
                                    {0, 5, 7, -1}, {0, 4, 7, 10, -1}, {0, 4, 7, -1}, {0, 3, 6, 10, -1}};
static uint8_t chord_used[JPP_PARTS][128];      /* the chord a held note started (type + 1), for its note-off */
static DSP int chord_of(unsigned p) { return synth.part[p].v[JP_KEY_MODE] == 1 ? synth.part[p].v[JP_CHORD] & 7 : -1; }
static DSP void notes_on(unsigned p, int note, float vel)
{
    int c = chord_of(p);
    if (note < 0 || note > 127) return;
    chord_used[p][note] = (uint8_t)(c + 1);
    if (c < 0) { jpp_note_on(&synth, p, note, vel, 0); return; }
    for (int i = 0; i < 5 && chords[c][i] >= 0; i++) if (note + chords[c][i] < 128) jpp_note_on(&synth, p, note + chords[c][i], vel, 0);
}
static DSP void notes_off(unsigned p, int note)
{
    int c;
    if (note < 0 || note > 127) return;
    c = (int)chord_used[p][note] - 1;
    chord_used[p][note] = 0;
    if (c < 0) { jpp_note_off(&synth, p, note); return; }
    for (int i = 0; i < 5 && chords[c][i] >= 0; i++) if (note + chords[c][i] < 128) jpp_note_off(&synth, p, note + chords[c][i]);
}

/* the arpeggiator, one per part: UP DOWN UP/DN RANDOM ORDER, 1-4 octaves, note values as the delay's,
 * gate 5..100 % (the iOS app's Arpeggiator.swift), HOLD latches what was played last */
enum { ARP_ON, ARP_MODE, ARP_DIV, ARP_OCT, ARP_GATE, ARP_HOLD, ARP_N };
static const float arp_q[FX_DIVS] = {0.125f, 1.0f / 6, 0.25f, 0.375f, 1.0f / 3, 0.5f, 0.75f, 2.0f / 3, 1, 1.5f, 2, 4};
static const uint8_t arp_ticks[FX_DIVS] = {3, 4, 6, 9, 8, 12, 18, 16, 24, 36, 48, 96};
typedef struct {
    uint8_t cfg[ARP_N];
    uint8_t keys[16], nkeys, phys;              /* the keys of the pattern (order played), how many are still held */
    uint8_t vel;
    int16_t sounding;                           /* the note playing (-1 none) */
    uint32_t pos, step_len, since, gate_len;    /* samples */
    uint32_t rng;
    uint8_t fresh;                              /* a new pattern: its first step now */
} arp_t;
static arp_t arp[JPP_PARTS];
static volatile uint8_t arp_view[JPP_PARTS][24], arp_view_n[JPP_PARTS], arp_view_at[JPP_PARTS];   /* for the screen */

static DSP void arp_stop_note(arp_t *a, unsigned p)
{
    if (a->sounding >= 0) { jpp_note_off(&synth, p, a->sounding); a->sounding = -1; }
}
static DSP int arp_pool(unsigned p, uint8_t *out)     /* the pattern's notes: keys x chord tones x octaves */
{
    arp_t *a = &arp[p];
    uint8_t base[48];
    int n = 0, c = chord_of(p), mode = a->cfg[ARP_MODE];
    for (int k = 0; k < a->nkeys; k++) {
        if (c < 0) { if (n < 48) base[n++] = a->keys[k]; continue; }
        for (int i = 0; i < 5 && chords[c][i] >= 0 && n < 48; i++) base[n++] = (uint8_t)(a->keys[k] + chords[c][i]);
    }
    if (mode != 4)                              /* ORDER keeps the order played; the others sort */
        for (int i = 1; i < n; i++) for (int j = i; j > 0 && base[j] < base[j - 1]; j--) { uint8_t t = base[j]; base[j] = base[j - 1]; base[j - 1] = t; }
    {
        int o = a->cfg[ARP_OCT] + 1, m = 0;
        for (int oo = 0; oo < o; oo++) for (int i = 0; i < n && m < 96; i++) { int x = base[i] + 12 * oo; if (x < 128) out[m++] = (uint8_t)x; }
        return m;
    }
}
static DSP void arp_step(unsigned p)
{
    arp_t *a = &arp[p];
    uint8_t pool[96];
    int n = arp_pool(p, pool), i, mode = a->cfg[ARP_MODE];
    arp_stop_note(a, p);
    if (!n) return;
    if (a->fresh) a->pos = 0;
    switch (mode) {
    case 1: i = n - 1 - (int)(a->pos % (uint32_t)n); break;
    case 2: { int per = n > 1 ? 2 * n - 2 : 1, q = (int)(a->pos % (uint32_t)per); i = q < n ? q : per - q; break; }
    case 3:   /* (xorshift: from 0 it stays 0, so it starts from the clock the first time) */
        if (!a->rng) a->rng = ((fm1_ticks() * 2654435761u) ^ (0x9E3779B9u + p)) | 1u;
        a->rng ^= a->rng << 13; a->rng ^= a->rng >> 17; a->rng ^= a->rng << 5; i = (int)(a->rng % (uint32_t)n); break;
    default: i = (int)(a->pos % (uint32_t)n); break;
    }
    a->pos++;
    a->fresh = 0;
    a->sounding = pool[i];
    jpp_note_on(&synth, p, a->sounding, a->vel * (1.0f / 127.0f), 0);
    a->since = 0;
    {   /* the screen's copy */
        int m = n < 24 ? n : 24;
        for (int k = 0; k < m; k++) arp_view[p][k] = pool[k];
        arp_view_n[p] = (uint8_t)m; arp_view_at[p] = (uint8_t)(i < 24 ? i : 23);
    }
}
static DSP void arp_key(unsigned p, int note, int vel)  /* vel 0: let go */
{
    arp_t *a = &arp[p];
    if (vel) {
        if (a->cfg[ARP_HOLD] && a->phys == 0) a->nkeys = 0;   /* latched: a new chord replaces the old one */
        for (int k = 0; k < a->nkeys; k++) if (a->keys[k] == note) { for (int j = k + 1; j < a->nkeys; j++) a->keys[j - 1] = a->keys[j]; a->nkeys--; break; }
        if (a->nkeys == 16) { for (int j = 1; j < 16; j++) a->keys[j - 1] = a->keys[j]; a->nkeys--; }
        if (a->nkeys == 0) a->fresh = 1;
        a->keys[a->nkeys++] = (uint8_t)note;
        a->vel = (uint8_t)vel;
        if (a->phys < 255) a->phys++;
    } else {
        if (a->phys) a->phys--;
        if (!a->cfg[ARP_HOLD])
            for (int k = 0; k < a->nkeys; k++) if (a->keys[k] == note) { for (int j = k + 1; j < a->nkeys; j++) a->keys[j - 1] = a->keys[j]; a->nkeys--; break; }
        if (!a->nkeys) { arp_stop_note(a, p); arp_view_n[p] = 0; }
    }
}
#define SQ_PARTS JPP_PARTS
#define SQ_BLOCK JPP_BLOCK
#define SQ_NOTE_ON(p, n, v, l) jpp_note_on(&synth, p, n, v, l)
#define SQ_NOTE_OFF(p, n) jpp_note_off(&synth, p, n)
/* ======================================================================================================
 * The step sequencer (docs/adr/0015), one per part, as the SH-101's: up to 64 steps, each a note, a rest or a
 * note tied to the next (legato); recorded step by step on the panel (REC), played on the clock's grid like the
 * arp (RATE, GATE), transposed by the keys while it plays (C4 = 0). Its notes, steps and settings are `sq`,
 * saved in blob 3 in a pause (the panel); the audio side owns the settings (EV_SEQ), the panel the steps.
 * ====================================================================================================== */
enum { SEQ_STEPS = 64, SQ_REST = 1, SQ_TIE = 2 };
enum { SQ_ON, SQ_DIV, SQ_GATE, SQ_SHIFT, SQ_N };   /* SHIFT: 0..48, 24 = none */
enum { SQ_VER = 1 };
static struct {
    uint8_t ver, len[SQ_PARTS];
    uint8_t cfg[SQ_PARTS][SQ_N];
    uint8_t note[SQ_PARTS][SEQ_STEPS], flag[SQ_PARTS][SEQ_STEPS];
} sq;
typedef struct {
    uint8_t pos, tied, fresh;
    int16_t sounding;                           /* the note playing (-1 none) */
    uint32_t since, step_len, gate_len;         /* samples */
} sqrun_t;
static sqrun_t sqr[SQ_PARTS];
/* step recording (docs/adr/0015): the part REC records (+1, 0 none; set by the panel). Its keys sound instead of
 * transposing, and its MIDI notes also go to the panel (a queue: note, velocity, 0 = let go) */
static volatile uint8_t seq_rec_p, rec_mn[32], rec_mv[32], rec_mw, rec_mr;
static volatile uint8_t sq_view_at[SQ_PARTS];   /* the step playing, for the screen */
static DSP void seq_stop_note(unsigned p)
{
    if (sqr[p].sounding >= 0) { SQ_NOTE_OFF(p, sqr[p].sounding); sqr[p].sounding = -1; }
    sqr[p].tied = 0;
}
static DSP void seq_step(unsigned p)
{
    sqrun_t *s = &sqr[p];
    unsigned n = sq.len[p] <= SEQ_STEPS ? sq.len[p] : SEQ_STEPS, i, f;
    int note;
    if (!n) { seq_stop_note(p); return; }
    if (s->fresh) { s->pos = 0; s->fresh = 0; }
    i = s->pos % n;
    s->pos = (uint8_t)((i + 1u) % n);
    f = sq.flag[p][i];
    note = (int)sq.note[p][i] + (int)sq.cfg[p][SQ_SHIFT] - 24;
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    if (f & SQ_REST) seq_stop_note(p);
    else if (s->sounding >= 0 && s->tied) {     /* tied from the step before: legato (the same note goes on) */
        if (note != s->sounding) { SQ_NOTE_ON(p, note, 0.8f, 1); SQ_NOTE_OFF(p, s->sounding); }
        s->sounding = (int16_t)note;
    } else {
        seq_stop_note(p);
        SQ_NOTE_ON(p, note, 0.8f, 0);
        s->sounding = (int16_t)note;
    }
    s->tied = (uint8_t)((f & SQ_TIE) && !(f & SQ_REST));
    s->since = 0;
    sq_view_at[p] = (uint8_t)i;
}
static DSP void seq_set(unsigned p, unsigned prm, int v)
{
    if (prm >= SQ_N) return;
    if (prm == SQ_ON && !v) seq_stop_note(p);
    if (prm == SQ_ON && v && !sq.cfg[p][SQ_ON]) { sqr[p].fresh = 1; sqr[p].since = 0; }
    sq.cfg[p][prm] = (uint8_t)v;
}
/* one block: the steps that fall into it (the clock's ticks while one runs, else by time), as arp_block */
static DSP void seq_block(unsigned ticks)
{
    for (unsigned p = 0; p < SQ_PARTS; p++) {
        sqrun_t *s = &sqr[p];
        uint32_t step;
        unsigned div = sq.cfg[p][SQ_DIV] % FX_DIVS;
        if (!sq.cfg[p][SQ_ON] || !sq.len[p]) continue;
        step = (uint32_t)(arp_q[div] * 60.0f / bpm_now * EL_SAMPLE_RATE);
        if (step < SQ_BLOCK) step = SQ_BLOCK;
        s->step_len = step;
        s->gate_len = (uint32_t)((0.05f + 0.95f * sq.cfg[p][SQ_GATE] / 127.0f) * (float)step);
        if (xport == 2) { s->since += SQ_BLOCK; if (s->sounding >= 0 && s->since >= s->gate_len) seq_stop_note(p); continue; }
        if (s->fresh) { seq_step(p); continue; }
        if (ext_follow || xport == 1) {
            for (unsigned t = 0; t < ticks; t++) if ((clk_ticks - ticks + t + 1u) % arp_ticks[div] == 0) seq_step(p);
        } else if (s->since + SQ_BLOCK >= step) seq_step(p);
        s->since += SQ_BLOCK;
        if (s->sounding >= 0 && !s->tied && s->since >= s->gate_len) { SQ_NOTE_OFF(p, s->sounding); s->sounding = -1; }
    }
}

static DSP void play_on(unsigned p, int note, int vel)
{
    if (sq.cfg[p][SQ_ON] && sq.len[p] && seq_rec_p != p + 1u) {   /* the sequence plays: the key transposes it (C4 = 0, as the SH-101) */
        int t = note - 60 + 24;
        sq.cfg[p][SQ_SHIFT] = (uint8_t)(t < 0 ? 0 : t > 48 ? 48 : t);
        return;
    }
    if (arp[p].cfg[ARP_ON]) arp_key(p, note, vel ? vel : 1);
    else notes_on(p, note, vel * (1.0f / 127.0f));
}
static DSP void play_off(unsigned p, int note)
{
    if (sq.cfg[p][SQ_ON] && sq.len[p] && seq_rec_p != p + 1u) return;
    if (arp[p].cfg[ARP_ON]) arp_key(p, note, 0);
    else notes_off(p, note);
}
static DSP void arp_set(unsigned p, unsigned prm, int v)
{
    arp_t *a = &arp[p];
    if (prm >= ARP_N) return;
    if (prm == ARP_ON && !v && a->cfg[ARP_ON]) { arp_stop_note(a, p); a->nkeys = 0; a->phys = 0; arp_view_n[p] = 0; }
    if (prm == ARP_HOLD && !v && a->cfg[ARP_HOLD] && !a->phys) { arp_stop_note(a, p); a->nkeys = 0; arp_view_n[p] = 0; }
    a->cfg[prm] = (uint8_t)v;
}
/* the transport (../../mks50/docs/adr/0012): START puts the arps and the synced LFOs on beat 1, STOP silences the arps */
static DSP void xport_start(void)
{
    for (unsigned p = 0; p < JPP_PARTS; p++) sqr[p].fresh = 1;
    clk_ticks = 0;
    int_acc = 0;
    xport = 1;
    for (unsigned p = 0; p < JPP_PARTS; p++) { arp[p].pos = 0; if (arp[p].nkeys) arp[p].fresh = 1; synth.part[p].lfo_phase = 0; }
}
static DSP void xport_stop(void)
{
    for (unsigned p = 0; p < JPP_PARTS; p++) seq_stop_note(p);
    xport = 2;
    for (unsigned p = 0; p < JPP_PARTS; p++) arp_stop_note(&arp[p], p);
}
/* the LFO of a sound with LFO SYNC follows the song position (unless KEY SYNC restarts it at each note) */
static const uint16_t lfo_div_ticks[14] = {3, 4, 6, 9, 8, 12, 18, 16, 24, 36, 48, 96, 192, 384};
static DSP void lfo_lock(void)
{
    for (unsigned p = 0; p < JPP_PARTS; p++) {
        const uint8_t *v = synth.part[p].v;
        uint32_t per;
        if (!v[JP_LFO_SYNC] || v[JP_LFO_KEYSYNC]) continue;
        per = lfo_div_ticks[v[JP_LFO_DIV] < 14 ? v[JP_LFO_DIV] : 13];
        synth.part[p].lfo_phase = (float)(clk_ticks % per) / (float)per;
    }
}

/* one block: the steps that fall into it (by the clock's ticks while one runs, else by time) */
static DSP void arp_block(unsigned ticks)
{
    for (unsigned p = 0; p < JPP_PARTS; p++) {
        arp_t *a = &arp[p];
        uint32_t step;
        if (!a->cfg[ARP_ON] || !a->nkeys) continue;
        step = (uint32_t)(arp_q[a->cfg[ARP_DIV] % FX_DIVS] * 60.0f / bpm_now * EL_SAMPLE_RATE);
        if (step < JPP_BLOCK) step = JPP_BLOCK;
        a->step_len = step;
        a->gate_len = (uint32_t)((0.05f + 0.95f * a->cfg[ARP_GATE] / 127.0f) * (float)step);
        if (xport == 2) { a->since += JPP_BLOCK; if (a->sounding >= 0 && a->since >= a->gate_len) arp_stop_note(a, p); continue; }
        if (a->fresh) { arp_step(p); continue; }
        if (ext_follow || xport == 1) {         /* on the clock's grid (MIDI, or the internal clock once started) */
            for (unsigned t = 0; t < ticks; t++) if ((clk_ticks - ticks + t + 1u) % arp_ticks[a->cfg[ARP_DIV] % FX_DIVS] == 0) arp_step(p);
        } else if (a->since + JPP_BLOCK >= step) arp_step(p);
        a->since += JPP_BLOCK;
        if (a->sounding >= 0 && a->since >= a->gate_len) arp_stop_note(a, p);
    }
}

static uint32_t midi_ticks;                      /* clock ticks taken this block */
static volatile uint8_t midi_hit[JPP_PARTS];            /* MIDI for a part (its channel): counted here, shown in the parts' tabs */
static DSP void midi_to_synth(void)
{
    uint8_t st, d1, d2;
    int src;
    while ((src = midi_get(&st, &d1, &d2)) != 0) {
        uint32_t ty = st & 0xF0u, ch = st & 15u;
        if (st >= 0xF0u) {                      /* clock, transport, song position */
            int follow = clk_src != SRC_INT;
            if (clk_send && src == 2) {         /* a clock from the TRS jack goes on to USB (one from USB never back) */
                if (st == 0xF2u) midi_out_event(3u | 0xF2u << 8 | (uint32_t)d1 << 16 | (uint32_t)d2 << 24);
                else if (st >= 0xF8u) midi_out_event(0xFu | (uint32_t)st << 8);
            }
            if (st == 0xF8u) {                  /* clock: 24 a quarter note */
                uint32_t now = fm1_ticks();
                clk_t_last = now; midi_ticks++;
                if (follow && xport != 2) clk_ticks++;
                if (++clk_n >= 24u) {
                    uint32_t dt = now - clk_t24;
                    clk_n = 0; clk_t24 = now;
                    float b = 60.0f * 24000000.0f / (float)(dt | 1u);   /* (never 0: the compiler works a division out before its test) */
                    /* only a real change, rounded to 0.1 BPM (the iOS app's way): a clock that jitters
                     * a little would bend the delay's echoes */
                    float d = b - bpm_now;
                    if (follow && b > 20.0f && b < 400.0f && (d > 0.003f * bpm_now || d < -0.003f * bpm_now))
                        bpm_now = (float)(int)(b * 10.0f + 0.5f) * 0.1f;
                }
            } else if (!follow) {
            } else if (st == 0xFAu) { clk_n = 0; clk_t24 = fm1_ticks(); xport_start(); }   /* START: from the top */
            else if (st == 0xFBu) xport = 1;    /* CONTINUE: from where the position is */
            else if (st == 0xFCu) xport_stop(); /* STOP */
            else if (st == 0xF2u) clk_ticks = ((uint32_t)d1 | (uint32_t)d2 << 7) * 6u;   /* SONG POSITION: in 16ths */
            continue;
        }
        for (uint32_t p = 0; p < JPP_PARTS; p++) {
            uint32_t c = part_ch[p];
            if (c && c - 1u != ch) continue;
            midi_hit[p]++;
            if (seq_rec_p == p + 1u && (ty == 0x90u || ty == 0x80u)) {   /* recording: the note is a step, too */
                uint8_t w = rec_mw;
                rec_mn[w & 31u] = (uint8_t)d1; rec_mv[w & 31u] = (uint8_t)(ty == 0x90u ? d2 : 0u); rec_mw = (uint8_t)(w + 1u);
            }
            if (ty == 0x90u && d2) play_on(p, d1, d2);
            else if (ty == 0x80u || ty == 0x90u) play_off(p, d1);
            else if (ty == 0xB0u && (d1 == 123u || d1 == 120u)) { arp_set(p, ARP_ON, arp[p].cfg[ARP_ON]); jpp_all_off(&synth, p); }
        }
    }
}

/* the load test (docs/adr/0002): n notes held over parts A, B, C (each with its chorus); the pool grows to n
 * when n is more than JPP_POOL, and is JPP_POOL again with the test off */
static const uint8_t test_notes[8] = {48, 55, 60, 64, 67, 72, 76, 79};
static DSP void load_test(unsigned n)
{
    for (unsigned p = 0; p < 3u; p++) jpp_all_off(&synth, p);
    for (unsigned p = 0; p < JPP_PARTS; p++)         /* (every voice free before the pool changes) */
        for (unsigned i = 0; i < JP6_MAX_VOICES; i++) {
            jp6_voice_t *v = &synth.part[p].voice[i];
            v->active = 0; v->gate = 0; v->key = -1;
        }
    synth.pool = (uint8_t)(n > JPP_POOL ? n : JPP_POOL);
    for (unsigned i = 0; i < n && i < 8u; i++) jpp_note_on(&synth, i % 3u, test_notes[i], 0.8f, 0);
}

static DSP void events_to_synth(void)
{
    for (uint32_t p = 0; p < JPP_PARTS; p++)
        if (pend_flag[p]) {
            jpp_part_load(&synth, p, pend_v[p]);
            FM1_C1_SYNC();
            pend_flag[p] = 0;
        }
    while (ev_r != ev_w) {
        jp_ev_t e = evq[ev_r % EVQ];
        ev_r++;
        if (e.part >= JPP_PARTS) continue;
        switch (e.type) {
        case EV_ON: play_on(e.part, e.a, e.b); break;
        case EV_OFF: play_off(e.part, e.a); break;
        case EV_ALL_OFF: jpp_all_off(&synth, e.part); break;
        case EV_PANIC:
            seq_stop_note(e.part);
            arp[e.part].nkeys = 0; arp[e.part].phys = 0; arp_stop_note(&arp[e.part], e.part); arp_view_n[e.part] = 0;
            memset(chord_used[e.part], 0, sizeof chord_used[e.part]);
            jpp_all_off(&synth, e.part);
            break;
        case EV_FX: syn_fx_set(e.a, e.b); break;
        case EV_ARP: arp_set(e.part, e.a, e.b); break;
        case EV_TEMPO: tempo_set = (float)(e.a + 40u); break;
        case EV_CLK: if (e.a == 0) clk_src = e.b; else clk_send = e.b; break;
        case EV_SEQ: seq_set(e.part, e.a, e.b); break;
        case EV_SEQ_LEN:   /* b 1: it grows while it plays (recording), on its grid */
            if (!e.b || !e.a) seq_stop_note(e.part);
            if (!e.b) sqr[e.part].fresh = 1;
            sq.len[e.part] = e.a;
            break;
        case EV_TEST: load_test(e.a); break;
        case EV_SET: jpp_part_set(&synth, e.part, e.a, e.b); break;
        case EV_LEVEL: synth.part[e.part].level = (float)e.a * (1.0f / 127.0f); break;
        default: break;
        }
    }
}

/* ---- the sound: the platform's audio interrupt, JPP_BLOCK frames */
static float mix_l[JPP_BLOCK], mix_r[JPP_BLOCK];
static int8_t scope_buf[512];                   /* the last output samples, for the screen's scope */
static volatile uint32_t scope_w;
#define OUT_GAIN (32767.0f * 0.6f * 1.4f)       /* FM1-Electro-one: the JP-6 at level 1.4, as FM1-Electro plays it (engine/synth.c) */

static volatile uint32_t gaps, last_start;       /* blocks whose interrupt came late (the one before took too long, or the
                                                 * interrupts were off: a flash write): heard as a click */
static volatile uint32_t silent_blocks;          /* blocks in a row whose output stayed under -80 dB (the state journal erases then) */
static DSP void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0, t, tv0, tc0, wait = 0, started = 0, blk, loud = 0;
    if (!ready || n != JPP_BLOCK) return;
    t0 = fm1_ticks();
    if (last_start && t0 - last_start > BLOCK_TICKS + BLOCK_TICKS / 2u) gaps++;   /* (a block's time and a half since the last) */
    last_start = t0;
    midi_ticks = 0;
    midi_to_synth();
    events_to_synth();
    clk_running = (uint8_t)(fm1_ticks() - clk_t_last < 24000u * 250u && clk_t_last);   /* (now, not t0: a tick taken in this block is later than t0) */
    {
        uint8_t was = ext_follow;
        ext_follow = (uint8_t)(clk_src == SRC_MIDI || (clk_src == SRC_AUTO && clk_running));
        if (was && !ext_follow) {               /* back to the internal clock: it goes on at the clock's last tempo */
            float b = bpm_now < 40.0f ? 40.0f : bpm_now > 240.0f ? 240.0f : bpm_now;
            tempo_set = b;
            tempo_taken = (uint16_t)(b * 10.0f + 0.5f);
        }
    }
    {
        unsigned ticks = midi_ticks;
        if (!ext_follow) {                      /* the internal clock: ticks at the TEMPO setting, sent when asked */
            float tick_len = EL_SAMPLE_RATE * 60.0f / (tempo_set * 24.0f);   /* (tempo_set >= 40) */
            bpm_now = tempo_set;
            if (xport_req == 1u) { xport_start(); if (clk_send) midi_out_event(0xFu | 0xFAu << 8); }
            else if (xport_req == 2u) { xport_stop(); if (clk_send) midi_out_event(0xFu | 0xFCu << 8); }
            xport_req = 0;
            ticks = 0;
            int_acc += (float)JPP_BLOCK;
            while (int_acc >= tick_len) {
                int_acc -= tick_len;
                ticks++;
                if (xport != 2) clk_ticks++;
                if (clk_send) midi_out_event(0xFu | 0xF8u << 8);
            }
        } else xport_req = 0;
        if (ext_follow || xport == 1) lfo_lock();
        arp_block(xport == 2 ? 0 : ticks);
        seq_block(xport == 2 ? 0 : ticks);
    }
    {   /* delay + reverb ~23 % of a block, a voice ~8 % (MKS-50 firmware, emulator, 2026-11-12): ~3 voices; kept free on
         * core 1 only while a part sends to them (without a send they sleep: ~1 %, and core 1 takes its share of voices) */
        unsigned sends = 0;
        for (unsigned p = 0; p < JPP_PARTS; p++) sends |= synth.part[p].v[JP_DELAY_SEND] | synth.part[p].v[JP_REVERB_SEND];
        synth.extra1 = (uint8_t)(fx_on && sends ? 28 : 0);
    }
    m_add(M_EV, fm1_ticks() - t0);
    synth.cores = (uint8_t)(want_cores > 1 && cpu1_ok && fm1_c1_on ? 2 : 1);
    blk = ++block_no;
    c1_voices_t = c1_finish_t = c1_control_t = c1_fx_t = 0;   /* (one core: the display shows no old numbers for core 1) */
    t = fm1_ticks();
    jpp_plan(&synth);
    if (synth.cores > 1 && (synth.voices_on[1] || synth.parts_on[1] || synth.ctl_on[1] || fx_on))
        started = (uint32_t)fm1_cpu1_run(c1_job, blk);
    jpp_control(&synth, 0, BPM);
    if (!started) jpp_control(&synth, 1, BPM);    /* (nothing there with one core) */
    FM1_C1_SYNC();
    ph_control[0] = blk;
    FM1_C1_SYNC();
    m_add(M_BEGIN, fm1_ticks() - t);
    if (started) {
        t = fm1_ticks();
        if (wait_phase(&ph_control[1], blk)) {
            started = (uint32_t)give_up();
            jpp_control(&synth, 1, BPM);          /* what it did not finish */
        }
        wait += fm1_ticks() - t;
    }
    t = fm1_ticks();
    jpp_voices(&synth, 0);
    tv0 = fm1_ticks() - t;
    FM1_C1_SYNC();
    ph_voices[0] = blk;
    FM1_C1_SYNC();
    if (started) {
        t = fm1_ticks();
        if (wait_phase(&ph_voices[1], blk)) started = (uint32_t)give_up();
        wait += fm1_ticks() - t;
    }
    if (!started) jpp_voices(&synth, 1);
    t = fm1_ticks();
    jpp_finish(&synth, 0);
    tc0 = fm1_ticks() - t;
    if (started) {
        t = fm1_ticks();
        if (fm1_cpu1_wait() != 0) cpu1_ok = 0;
        wait += fm1_ticks() - t;
    } else {
        jpp_finish(&synth, 1);
        t = fm1_ticks();
        fx_run();
        c1_fx_t = fm1_ticks() - t;
    }
    for (uint32_t i = 0; i < JPP_BLOCK; i++) { mix_l[i] = fx_wet[0][i]; mix_r[i] = fx_wet[1][i]; }
    jpp_mix(&synth, mix_l, mix_r);
    for (uint32_t i = 0; i < JPP_BLOCK; i++) { fx_in[0][i] = synth.send_sum[0][i]; fx_in[1][i] = synth.send_sum[1][i]; }
    for (uint32_t i = 0; i < JPP_BLOCK; i++) {
        float l = mix_l[i] * OUT_GAIN, r = mix_r[i] * OUT_GAIN;
        l = l > 32767.0f ? 32767.0f : l < -32767.0f ? -32767.0f : l;
        r = r > 32767.0f ? 32767.0f : r < -32767.0f ? -32767.0f : r;
        out[2u * i] += (int32_t)l;
        out[2u * i + 1u] += (int32_t)r;
        scope_buf[(scope_w + i) & 511u] = (int8_t)((int32_t)l >> 8);
        if (l > 3.0f || l < -3.0f || r > 3.0f || r < -3.0f) loud = 1;
    }
    silent_blocks = loud ? 0u : silent_blocks + 1u;
    scope_w += JPP_BLOCK;
    t = fm1_ticks() - t0;
    m_add(M_ALL, t);
    m_add(M_K0, t - wait);
    m_add(M_K1, c1_control_t + c1_voices_t + c1_finish_t + (started ? c1_fx_t : 0));
    m_add(M_FX, c1_fx_t);
    m_add(M_V0, tv0);
    m_add(M_V1, c1_voices_t);
    m_add(M_C0, tc0);
    m_add(M_C1, c1_finish_t);
    meas.blocks++;
    if (t > BLOCK_TICKS) meas.late++;
}

static char *put(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
static char *put_n(char *p, uint32_t v, uint32_t width)   /* right-aligned, spaces */
{
    char b[10];
    uint32_t n = 0;
    do { b[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 10u);
    for (uint32_t i = n; i < width; i++) *p++ = ' ';
    while (n) *p++ = b[--n];
    return p;
}
static const char *find_char(const char *s, char c) { for (; *s; s++) if (*s == c) return s; return 0; }
static void keys_poll(void);
static uint8_t idle_keys;
static void app_idle(void) { if (idle_keys) keys_poll(); }   /* while the screen's DMA runs: the panel's keys (MIDI is read in the audio interrupt) */

/* ======================================================================================================
 * Sounds: the factory sounds (engine/jx_presets.c, each with its category of jx_cat_names) and the user's
 * own, in one flash sector (blob 0). A part holds a working copy; `base` is what it was loaded as, so a
 * change shows (the red square) until it is saved. Categories without a sound are skipped.
 * ====================================================================================================== */
#include "jp_cats.h"                          /* NFC, jp_cat_names, jp_preset_cat (tools/cats.py) */
enum { CAT_OWN = NFC, NCAT = NFC + 1, CAT_INIT = 0, NOWN = 40, NAMEMAX = 10 };
static const char *cat_name[NCAT];
/* INIT TONE: the JP-6 iOS app's (JP6-iOS, Shared/Model/ToneData.swift initTone = Param.defaultValue, as
 * bausteine/jp6/tools/eigene2c.py): VCO-1 and VCO-2 saw, filter open */
static const uint8_t init_v[JP_COUNT] = {
    0, 1, 0, 0, 2, 0, 0, 0, 1, 0, 0, 3, 12, 64, 0, 1, 0, 0, 2, 64, 0, 0, 127, 0, 0, 0, 0, 0, 0, 110, 0, 0, 64, 0, 0,
    0, 64, 64, 40, 0, 0, 0, 64, 127, 30, 0, 0, 0, 30, 0, 100, 12, 16, 40, 0, 8, 0, 0, 0, 60, 0, 0, 64, 80, 64, 127,
    0, 0, 0};
/* the factory list: engine/jp6_presets.c (FM1-Electro's), INIT TONE, then the user's own presets from the iOS app
 * (bausteine/jp6/jp6_eigene.c, ADR 0051); they show in EIGENE (first after INIT) and in a group by their area */
#include "jp6_eigene.h"
#define FAC_N (jp6_preset_count + 1u + jp6_eigene_count)
#define FAC_EIGEN (jp6_preset_count + 1u)
static const uint8_t eigen_cat[4] = {6, 6, 7, 1};     /* area -> group: (none) KEYS, RHYTHM KEYS, STRINGS STR+BRASS, BASS */
static const char *fac_name(unsigned i)
{
    return i < jp6_preset_count ? jp6_presets[i].name : i == jp6_preset_count ? "INIT TONE" : jp6_eigene[i - FAC_EIGEN].name;
}
static const uint8_t *fac_v(unsigned i)
{
    return i < jp6_preset_count ? jp6_presets[i].v : i == jp6_preset_count ? init_v : jp6_eigene[i - FAC_EIGEN].v;
}
static unsigned fac_cat(unsigned i)
{
    return i < jp6_preset_count ? jp_preset_cat[i] : i == jp6_preset_count ? CAT_INIT : eigen_cat[jp6_eigene[i - FAC_EIGEN].area & 3u];
}
static int fac_eigen(unsigned i) { return i >= FAC_EIGEN && i < FAC_N; }
/* the j-th in browsing order: INIT TONE first, then engine/jp6_presets.c, then the user's presets */
#define FAC_AT(j) ((j) == 0u ? jp6_preset_count : (j) <= jp6_preset_count ? (j) - 1u : (j))

typedef struct { char name[NAMEMAX + 1]; uint8_t cat, used; uint8_t v[JP_COUNT]; } own_t;
static own_t own[NOWN];
_Static_assert(sizeof(own_t) * NOWN <= 4096u - 64u, "the own sounds fit their sector");

typedef struct {
    uint8_t kind, idx;                          /* 0 factory sound idx, 1 own sound idx */
    uint8_t chan, level, cat_view, cat, dirty;
    char name[NAMEMAX + 1];
    uint8_t v[JP_COUNT], base[JP_COUNT];
} upart_t;
static upart_t up[JPP_PARTS];
static struct { uint8_t ok, kind, idx; char name[NAMEMAX + 1]; uint8_t v[JP_COUNT]; } restore[JPP_PARTS];

static unsigned cat_count(unsigned c)
{
    unsigned n = 0;
    if (c < NFC) for (unsigned i = 0; i < FAC_N; i++) n += fac_cat(i) == c;
    if (c == CAT_OWN) for (unsigned i = FAC_EIGEN; i < FAC_N; i++) n++;
    for (unsigned j = 0; j < NOWN; j++) n += own[j].used && (c == CAT_OWN || own[j].cat == c);
    return n;
}
static int cat_item(unsigned c, unsigned n, uint8_t *kind, uint8_t *idx)    /* the n-th sound of a category */
{
    if (c < NFC) for (unsigned j = 0; j < FAC_N; j++)
        if (fac_cat(FAC_AT(j)) == c && n-- == 0) { *kind = 0; *idx = (uint8_t)FAC_AT(j); return 0; }
    if (c == CAT_OWN) for (unsigned j = FAC_EIGEN; j < FAC_N; j++)
        if (n-- == 0) { *kind = 0; *idx = (uint8_t)j; return 0; }   /* EIGENE: the user's presets first */
    for (unsigned j = 0; j < NOWN; j++)
        if (own[j].used && (c == CAT_OWN || own[j].cat == c) && n-- == 0) { *kind = 1; *idx = (uint8_t)j; return 0; }
    return -1;
}
static int cat_pos(unsigned c, unsigned kind, unsigned idx)
{
    unsigned n = cat_count(c);
    for (unsigned i = 0; i < n; i++) {
        uint8_t k, x;
        if (!cat_item(c, i, &k, &x) && k == kind && x == idx) return (int)i;
    }
    return -1;
}
/* browsing order: INIT, EIGENE (the user's presets and own sounds, ADR 0051: first), then the factory's groups */
static unsigned cat_rank(unsigned c) { return c == CAT_INIT ? 0u : c == CAT_OWN ? 1u : c < NFC ? c + 1u : 0u; }
static unsigned cat_at(unsigned r) { return r == 0u ? CAT_INIT : r == 1u ? CAT_OWN : r - 1u < NFC ? r - 1u : 0u; }
static unsigned next_cat(unsigned c, int dir)
{
    unsigned r = cat_rank(c);
    for (unsigned n = 0; n < NCAT; n++) {
        r = (r + NCAT + (unsigned)dir) % NCAT;
        c = cat_at(r);
        if (cat_count(c)) return c;
    }
    return c;
}

static void push_part(unsigned p)
{
    uint32_t n;
    for (n = 0; pend_flag[p] && n < 200000u; n++) {}
    for (uint32_t i = 0; i < JP_COUNT; i++) pend_v[p][i] = up[p].v[i];
    FM1_C1_SYNC();
    pend_flag[p] = 1;
}
static void mark_dirty(unsigned p) { up[p].dirty = memcmp(up[p].v, up[p].base, JP_COUNT) != 0; }

static void load_sound(unsigned p, unsigned kind, unsigned idx)
{
    upart_t *u = &up[p];
    const uint8_t *src;
    const char *name;
    if (u->dirty) {                             /* the changes stay reachable: HOME held brings them back */
        restore[p].ok = 1; restore[p].kind = u->kind; restore[p].idx = u->idx;
        memcpy(restore[p].v, u->v, JP_COUNT); memcpy(restore[p].name, u->name, sizeof u->name);
    }
    if (kind == 1u && (idx >= NOWN || !own[idx].used)) kind = 0, idx = 0;
    if (kind == 0u && idx >= FAC_N) idx = 0;
    if (kind) { src = own[idx].v; name = own[idx].name; u->cat = own[idx].cat; }
    else { src = fac_v(idx); name = fac_name(idx); u->cat = (uint8_t)fac_cat(idx); }
    u->kind = (uint8_t)kind; u->idx = (uint8_t)idx;
    memcpy(u->base, src, JP_COUNT); memcpy(u->v, src, JP_COUNT);
    str_cpy(u->name, name, sizeof u->name);
    if ((!kind && !fac_eigen(idx)) || u->cat_view != CAT_OWN) u->cat_view = u->cat;   /* (EIGENE stays EIGENE) */
    u->dirty = 0;
    push_part(p);
}

static void step_sound(unsigned p, int32_t e)   /* left: the sound before, right: the next; past an end, the next category */
{
    unsigned c = up[p].cat_view, k = up[p].kind, i = up[p].idx;
    int dir = e > 0 ? 1 : -1;
    for (int32_t n = e < 0 ? -e : e; n > 0; n--) {
        int pos = cat_pos(c, k, i), cnt = (int)cat_count(c);
        uint8_t kk, ii;
        if (pos < 0) pos = dir > 0 ? -1 : cnt;
        pos += dir;
        if (pos >= cnt) { c = next_cat(c, 1); pos = 0; }
        else if (pos < 0) { c = next_cat(c, -1); pos = (int)cat_count(c) - 1; }
        if (cat_item(c, (unsigned)pos, &kk, &ii)) break;
        k = kk; i = ii;
    }
    up[p].cat_view = (uint8_t)c;
    if (k != up[p].kind || i != up[p].idx) load_sound(p, k, i);
}

/* ---- flash: the own sounds (blob 0) and the parts (settings), after Dinghy's: a save erases a sector
 * (~50 ms with the interrupts off), so the parts are saved 2 s after the last change when nothing sounds,
 * the own sounds when the player presses SAVE */
static struct {
    uint8_t ver, sel; int8_t oct; uint8_t tempo;            /* tempo: BPM - 40 */
    struct { uint8_t kind, idx, chan, level; uint8_t arp[ARP_N]; } p[JPP_PARTS];
    uint8_t fx[FX_COUNT];
    uint8_t clk_src, clk_send;              /* MIDI sync (../../mks50/docs/adr/0012) */
} set;
enum { SET_VER = 1 };
static uint8_t set_dirty;
static uint32_t t_set;

static int quiet(void)
{
    for (unsigned p = 0; p < JPP_PARTS; p++) if (jpp_part_voices(&synth, p)) return 0;
    return 1;
}
static uint8_t fxv[FX_COUNT], arpc[JPP_PARTS][ARP_N], tempo = 80;   /* the panel's copies: effects, arps, BPM - 40 */
static uint8_t clkv[2];                         /* the panel's copies: clock source, send */
static void set_pack(void)
{
    set.ver = SET_VER;
    set.tempo = tempo;
    for (unsigned p = 0; p < JPP_PARTS; p++) {
        set.p[p].kind = up[p].kind; set.p[p].idx = up[p].idx; set.p[p].chan = up[p].chan; set.p[p].level = up[p].level;
        memcpy(set.p[p].arp, arpc[p], ARP_N);
    }
    memcpy(set.fx, fxv, FX_COUNT);
    set.clk_src = clkv[0]; set.clk_send = clkv[1];
}

static uint8_t test_v[3] = {0, 0, 1};         /* the SYSTEM page's load test: voices (0 off), sound, cores (0: 1, 1: 2) */
#include "jp_ui.c"
