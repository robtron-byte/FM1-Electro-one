/* SPDX-License-Identifier: GPL-3.0-only */
/* FM1-Electro on its own platform (docs/adr/0021): engine, store and UI as on
 * FM1 Open's SDK (fm1/el_fm1.c), driven by the platform's main loop and audio
 * interrupt (fm1/el_app.c). */
#include "el_glue.h"
#include "electro.h"
#include "el_ui.h"
#include "el_store.h"

/* The engine (about 100 KB) in the platform's pool: zeroed at boot. */
static el_engine_t engine __attribute__((section(".pool")));
static el_ui_t ui;
static volatile uint8_t ready;

/* Load: ticks of the 24 MHz counter per audio callback (the platform renders
 * 256 frames, two engine blocks) and per engine block; the slowest callback
 * of each half second; the stage times per engine block (ADR 0019). */
#define TICKS_PER_FRAME_Q8 (24000000ull * 256u / 44100u)
static volatile uint32_t load_peak, load_frames;
static uint32_t load_at, prof_blocks;
static volatile uint32_t drops;

static float block[2 * EL_BLOCK];

/* The CPU clock measured (docs/adr/0019): a chain of dependent integer adds
 * (one cycle each: the next needs the last) from SRAM, 64 and 32 a round;
 * the difference against the 24 MHz counter leaves the loop's own cost out. */
#define CLK_ADD(x) do { x = x + 3u; __asm__ volatile("" : "+r"(x)); } while (0)
#define CLK_ADD8(x) CLK_ADD(x); CLK_ADD(x); CLK_ADD(x); CLK_ADD(x); CLK_ADD(x); CLK_ADD(x); CLK_ADD(x); CLK_ADD(x)
static EL_DSP_CODE uint32_t clk_chain(unsigned rounds, int wide)
{
    uint32_t x = 1u, t0 = el_platform_ticks();
    for (unsigned r = 0; r < rounds; ++r) {
        CLK_ADD8(x); CLK_ADD8(x); CLK_ADD8(x); CLK_ADD8(x);
        if (wide) { CLK_ADD8(x); CLK_ADD8(x); CLK_ADD8(x); CLK_ADD8(x); }
    }
    uint32_t t = el_platform_ticks() - t0;
    __asm__ volatile("" : : "r"(x));
    return t;
}
static uint16_t clock_mhz(void)
{
    enum { ROUNDS = 20000 };
    clk_chain(100, 1);                         /* (warm) */
    uint32_t t64 = clk_chain(ROUNDS, 1), t32 = clk_chain(ROUNDS, 0);
    if (t64 <= t32) return 0;
    /* 32 adds x ROUNDS cycles took (t64 - t32) ticks of 24 MHz */
    return (uint16_t)(32ull * ROUNDS * 24u / (t64 - t32));
}

static uint16_t clk_mhz;
void el_glue_start(void)
{
    el_init(&engine);
    /* The grooves come back from flash; only the very first start (nothing
     * stored) plays the generated factory set. */
    /* nothing stored: empty patterns, a random kit and random sounds */
    if (el_store_load(&engine) == 1) el_blank_start(&engine, el_platform_ticks() * 2654435761u + 1u);
    clk_mhz = clock_mhz();
    el_ui_init(&ui, &engine);
    ui.save = el_store_save;
    ui.prepare = el_store_prepare;
    ui.sound_get = el_sound_get;
    ui.sound_put = el_sound_put;
    if (el_platform_cpu1_ok()) {   /* the synth areas on the second core (docs/adr/0021) */
        engine.par_run = el_platform_cpu1_run;
        engine.par_wait = el_platform_cpu1_wait;
    }
    __sync_synchronize();
    ready = 1;
}

void el_glue_render(int32_t *out, unsigned frames)
{
    if (!ready) return;
    uint32_t start = el_platform_ticks();
    for (unsigned pos = 0; pos < frames; pos += EL_BLOCK) {
        unsigned n = frames - pos < EL_BLOCK ? frames - pos : EL_BLOCK;
        el_render_float(&engine, block, n);
        for (unsigned i = 0; i < 2u * n; ++i) {
            float v = block[i] * 32767.0f;
            v = v > 32767.0f ? 32767.0f : v < -32767.0f ? -32767.0f : v;
            out[2u * pos + i] += (int32_t)v;
        }
    }
    uint32_t used = el_platform_ticks() - start;
    if (used > load_peak) load_peak = used;
    if (used > (uint32_t)(TICKS_PER_FRAME_Q8 * frames / 256u)) drops++;   /* later than the other half: a gap */
    load_frames = frames;
}

void el_glue_midi(const uint8_t *message, size_t bytes)
{
    if (ready) el_midi_input(&engine, message, bytes);
}

void el_glue_control(const struct fm1_controls *controls, uint32_t now_ms)
{
    if (!ready) return;
    if (now_ms - load_at >= 500u && load_frames) {
        uint32_t budget = (uint32_t)(TICKS_PER_FRAME_Q8 * load_frames / 256u);
        uint32_t peak = load_peak;
        load_peak = 0;
        load_at = now_ms;
        ui.load = (int16_t)((uint64_t)peak * 100u / budget);
        if (ui.load > (int16_t)ui.load_max) ui.load_max = (uint16_t)ui.load;
        ui.drops = drops;
        ui.cpu1_on = (uint8_t)el_platform_cpu1_ok();
        ui.plat = 1;
        {
            /* The clock as X0X reads it off an FM-1 (as fm1/el_fm1.c): the PLL
             * from the 24 MHz reference, the system clock from source 0x10014
             * (6: 2/3 of the PLL). */
            uint32_t r0 = *(volatile uint32_t *)0x119A0u, r8 = *(volatile uint32_t *)0x119A8u;
            uint32_t rc = *(volatile uint32_t *)0x119ACu, src = *(volatile uint32_t *)0x10014u & 0xFu;
            uint32_t ref = (r0 & 0xC000000u) != 0x8000000u ? 24u : 0u;
            uint32_t pll = ref ? ref * ((rc & 0xFFFu) + 2u) / (((r8 >> 2) & 31u) + 2u) : 0u;
            ui.clk_pll = (uint16_t)pll;
            ui.clk_src = (uint8_t)src;
            ui.clk_sys = (uint16_t)(src == 6u ? pll * 2u / 3u : 0u);
            ui.clk_timer = clk_mhz;
        }
        ui.load_drops = 0;
        /* stage shares: their ticks against the engine blocks rendered meanwhile */
        uint32_t blocks = engine.blocks, n = blocks - prof_blocks;
        uint32_t per_block = (uint32_t)(TICKS_PER_FRAME_Q8 * EL_BLOCK / 256u);
        prof_blocks = blocks;
        for (unsigned i = 0; i < EL_PROF_STAGES; ++i) {
            uint32_t t = engine.prof[i];
            engine.prof[i] = 0;
            uint64_t pct = n ? (uint64_t)t * 100u / ((uint64_t)n * per_block) : 0;
            ui.prof[i] = (uint8_t)(pct > 255u ? 255u : pct);
        }
        ui.prof_valid = n != 0;
        uint32_t sp = engine.synth_peak;
        engine.synth_peak = 0;
        uint64_t c1 = (uint64_t)sp * 100u / per_block;
        ui.c1_peak = (uint8_t)(c1 > 255u ? 255u : c1);
        ui.bench[0] = 1;   /* show the stage line in OPTIONS (the CPU test is the SDK build's) */
    }
    el_ui_control(&ui, controls, now_ms);
}
