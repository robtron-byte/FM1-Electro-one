/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "el_fm1.h"
#include "el_ui.h"
#include "el_store.h"
#include "fm1_device.h"
#include <string.h>

static el_engine_t engine;
static el_ui_t ui;
static volatile uint8_t ready;
static int master_shown = -1;

/* Audio load (docs/adr/0019): the SDK's audio time base (the counter its own
 * audio interrupt times blocks with) around each render; the slowest block of
 * each half second against a block's length. */
#define EL_AUDIO_TIMER (*(volatile uint32_t *)0x1EEF0F0u)
#define EL_AUDIO_TIMER_PERIOD (*(volatile uint32_t *)0x1EEF0F4u)   /* it counts 0..PERIOD */
static volatile uint32_t load_peak;   /* ticks, written by the audio interrupt */
static uint32_t block_ticks, load_at, drops_seen, prof_blocks;

/* CPU test (docs/adr/0019): how long one step takes on this FM-1. The flash
 * loops are a few words and run from the cache after the first round; the
 * R* kinds run the same from SRAM, like the audio code. Each loop runs N
 * steps with its operation chained (each needs the one before), timed by
 * the audio time base; the shortest of five runs counts (an interrupt may
 * land in one). The loads use the engine's memory, which is free before
 * el_init. */
enum { BENCH_N = 2000, BENCH_RND_WORDS = 16384 };
#define KEEP(x) __asm__ volatile("" : "+r"(x))
static uint32_t ticks_since(uint32_t start)
{
    uint32_t end = EL_AUDIO_TIMER;
    return end >= start ? end - start : end + EL_AUDIO_TIMER_PERIOD + 1u - start;
}
static FM1_AUDIO_CODE __attribute__((noinline)) uint32_t bench_callee(uint32_t x) { KEEP(x); return x + 1u; }
static FM1_AUDIO_CODE __attribute__((noinline)) uint32_t bench_ram(unsigned kind)
{
    volatile float seed_f = 1.0000001f;
    float a = seed_f, k = seed_f;
    uint32_t x = 1u, start = EL_AUDIO_TIMER;
    volatile uint32_t seed_i = 12345u;
    uint32_t y = seed_i, z = seed_i + 1u, w = seed_i + 2u, v = seed_i + 3u, m = seed_i | 1u;
    if (kind == EL_BENCH_RLOOP) for (int i = 0; i < BENCH_N; ++i) KEEP(x);
    else if (kind == EL_BENCH_RMUL) for (int i = 0; i < BENCH_N; ++i) { a = a * k; KEEP(a); }
    else if (kind == EL_BENCH_RFADD) for (int i = 0; i < BENCH_N; ++i) { a = a + k; KEEP(a); }
    else if (kind == EL_BENCH_RIADD) for (int i = 0; i < BENCH_N; ++i) { y = y + m; KEEP(y); }
    else if (kind == EL_BENCH_RIMUL) for (int i = 0; i < BENCH_N; ++i) { y = y * m + 1013904223u; KEEP(y); }
    else if (kind == EL_BENCH_RIMUL4)
        for (int i = 0; i < BENCH_N; ++i) { y = y * m + 1u; z = z * m + 3u; w = w * m + 5u; v = v * m + 7u; KEEP(y); KEEP(z); KEEP(w); KEEP(v); }
    else if (kind == EL_BENCH_RMUL64)
        for (int i = 0; i < BENCH_N; ++i) { y = (uint32_t)(((int64_t)(int32_t)y * (int32_t)m) >> 31); KEEP(y); }
    else for (int i = 0; i < BENCH_N; ++i) x = bench_callee(x);
    x += y + z + w + v;
    uint32_t end = EL_AUDIO_TIMER;
    volatile float sink = a; volatile uint32_t sink_i = x; (void)sink; (void)sink_i;
    return end >= start ? end - start : end + EL_AUDIO_TIMER_PERIOD + 1u - start;
}
static uint32_t bench_one(unsigned kind, uint32_t *mem)
{
    volatile float seed_f = 1.0000001f, small = 1e-39f, two = 2.0f;
    volatile uint32_t seed_i = 12345u;
    float a = seed_f, b = seed_f, c = seed_f, d = seed_f, k = seed_f, z = small;
    uint32_t x = seed_i;
    if (kind == EL_BENCH_RLOOP || kind == EL_BENCH_RMUL || kind == EL_BENCH_CALL || kind >= EL_BENCH_RIADD) return bench_ram(kind);
    uint32_t start = EL_AUDIO_TIMER;
    switch (kind) {
    case EL_BENCH_LOOP: for (int i = 0; i < BENCH_N; ++i) KEEP(x); break;
    case EL_BENCH_INT: for (int i = 0; i < BENCH_N; ++i) { x = x * 1664525u + 1013904223u; KEEP(x); } break;
    case EL_BENCH_FADD: for (int i = 0; i < BENCH_N; ++i) { a = a + k; KEEP(a); } break;
    case EL_BENCH_FMUL: for (int i = 0; i < BENCH_N; ++i) { a = a * k; KEEP(a); } break;
    case EL_BENCH_FMUL4:   /* four independent chains: does the FPU overlap them? */
        for (int i = 0; i < BENCH_N; ++i) { a *= k; b *= k; c *= k; d *= k; KEEP(a); KEEP(b); KEEP(c); KEEP(d); }
        break;
    case EL_BENCH_FDIV: { float t = two; for (int i = 0; i < BENCH_N; ++i) { a = t / a; KEEP(a); } } break;
    case EL_BENCH_DENORM: for (int i = 0; i < BENCH_N; ++i) { z = z * k; KEEP(z); } break;
    case EL_BENCH_LDSEQ: { const float *f = (const float *)mem; for (int i = 0; i < BENCH_N; ++i) { a += f[i]; KEEP(a); } } break;
    case EL_BENCH_LDRND: for (int i = 0; i < BENCH_N; ++i) { x = mem[x]; KEEP(x); } break;
    default: break;
    }
    uint32_t used = ticks_since(start);
    volatile float sink = a + b + c + d + z;
    volatile uint32_t sink_i = x;
    (void)sink; (void)sink_i;
    return used;
}
/* The clock as X0X reads it off an FM-1 (fm1-x0x docs/plans/2026-10-04-
 * dual-core-and-clock.md): the PLL from a 24 MHz reference, the system clock
 * from source 0x10014 & 0xF (6: 2/3 of the PLL; others shown as found). */
static void clock_read(el_ui_t *u)
{
    uint32_t r0 = *(volatile uint32_t *)0x119A0u, r8 = *(volatile uint32_t *)0x119A8u;
    uint32_t rc = *(volatile uint32_t *)0x119ACu, src = *(volatile uint32_t *)0x10014u & 0xFu;
    uint32_t ref = (r0 & 0xC000000u) != 0x8000000u ? 24u : 0u;
    uint32_t pll = ref ? ref * ((rc & 0xFFFu) + 2u) / (((r8 >> 2) & 31u) + 2u) : 0u;
    u->clk_pll = (uint16_t)(pll ? pll : 1u);
    u->clk_src = (uint8_t)src;
    u->clk_sys = (uint16_t)(src == 6u ? pll * 2u / 3u : 0u);
    u->clk_timer = (uint16_t)(fm1_device_clock() / 1000000u);
}

static void bench(uint16_t out[16], uint32_t *mem)
{
    uint32_t clock = fm1_device_clock();
    if (!clock) return;
    /* A random cycle through BENCH_RND_WORDS words (each step lands far from
     * the last), and small floats for the sequential loads. */
    uint32_t r = 0x2545f497u;
    for (unsigned i = 0; i < BENCH_RND_WORDS; ++i) mem[i] = i;
    for (unsigned i = BENCH_RND_WORDS - 1u; i > 0; --i) {
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        unsigned j = r % i, t = mem[i]; mem[i] = mem[j]; mem[j] = t;   /* Sattolo: one cycle */
    }
    float *seq = (float *)(mem + BENCH_RND_WORDS);
    for (unsigned i = 0; i < BENCH_N; ++i) seq[i] = 1e-3f;
    for (unsigned kind = EL_BENCH_LOOP; kind < EL_BENCH_KINDS; ++kind) {
        uint32_t best = 0xffffffffu;
        for (int run = 0; run < 5; ++run) {
            uint32_t t = bench_one(kind, kind == EL_BENCH_LDSEQ ? (uint32_t *)seq : mem);
            if (t < best) best = t;
        }
        /* tenths of a ns per step; the loop itself is not subtracted */
        uint64_t tenth_ns = (uint64_t)best * 10000000000ull / clock / BENCH_N;
        out[kind] = (uint16_t)(tenth_ns > 65535u ? 65535u : tenth_ns ? tenth_ns : 1u);
    }
}

FM1_AUDIO_CODE int el_fm1_render(int32_t *output, unsigned frames)
{
    if (!output || frames != FM1_BLOCK_FRAMES) return -1;
    if (!ready) {
        for (unsigned i = 0; i < FM1_BLOCK_WORDS; ++i) output[i] = 0;
        return 0;
    }
    uint32_t start = EL_AUDIO_TIMER;
    el_render(&engine, output, frames);
    uint32_t end = EL_AUDIO_TIMER;
    uint32_t used = end >= start ? end - start : end + EL_AUDIO_TIMER_PERIOD + 1u - start;
    if (used > load_peak) load_peak = used;
    return 0;
}

int el_fm1_control(void)
{
    struct fm1_controls controls;
    if (!ready) {
        /* groove_screen_init() (el_sdk_app.c) has started the panel and LCD. */
        /* The CPU test first: it borrows the engine's memory. */
        _Static_assert(sizeof engine >= (BENCH_RND_WORDS + BENCH_N) * 4u, "CPU test memory");
        bench(ui.bench, (uint32_t *)(void *)&engine);
        el_init(&engine);
        /* The last grooves come back from Flash; only on the very first
         * start (empty Flash) the generated factory set plays. */
        el_store_load(&engine);
        uint16_t measured[16];
        memcpy(measured, ui.bench, sizeof measured);
        el_ui_init(&ui, &engine);
        memcpy(ui.bench, measured, sizeof measured);
        clock_read(&ui);
        ui.save = el_store_save;
        ui.sound_get = el_sound_get;
        ui.sound_put = el_sound_put;
        /* Again before the audio starts: its first silent block would
         * trap (docs/adr/0018). */
        el_fm1_trap_off();
        EL_FENCE();
        ready = 1;
    }
    fm1_controls_read(&controls);
    /* MASTER knob: 0..1023 ADC to 0..127, with a little hysteresis. */
    int master = (int)controls.knob * 127 / 1023;
    if (master_shown < 0 || master > master_shown + 1 || master < master_shown - 1 ||
        (master == 0 && master_shown != 0) || (master == 127 && master_shown != 127)) {
        if (el_post(&engine, EL_CMD_MASTER, 0, (int16_t)master) == 0) master_shown = master;
    }
    uint32_t now = fm1_millis();
    if (!block_ticks) {
        uint32_t clock = fm1_device_clock();
        block_ticks = (uint32_t)((uint64_t)clock * FM1_BLOCK_FRAMES / 44118u);
    }
    if (block_ticks && now - load_at >= 500u) {
        struct fm1_audio_stats s;
        fm1_audio_stats_read(&s);
        uint32_t peak = load_peak;
        load_peak = 0;
        load_at = now;
        ui.load = (int16_t)((uint64_t)peak * 100u / block_ticks);
        ui.load_drops = s.overruns != drops_seen;
        drops_seen = s.overruns;
        /* Stage shares: their ticks against the blocks rendered meanwhile. */
        uint32_t blocks = engine.blocks, n = blocks - prof_blocks;
        prof_blocks = blocks;
        for (unsigned i = 0; i < EL_PROF_STAGES; ++i) {
            uint32_t t = engine.prof[i];
            engine.prof[i] = 0;   /* a block in between is lost from one figure: fine */
            uint64_t pct = n ? (uint64_t)t * 100u / ((uint64_t)n * block_ticks) : 0;
            ui.prof[i] = (uint8_t)(pct > 255u ? 255u : pct);
        }
        ui.prof_valid = n != 0;
    }
    el_ui_control(&ui, &controls, now);
    return 1;
}

void *el_fm1_boot_scratch(size_t bytes)
{
    return !ready && bytes <= sizeof engine ? (void *)&engine : NULL;
}

int el_fm1_midi(unsigned source, const uint8_t *message, size_t bytes)
{
    (void)source;
    if (!ready) return 0;
    return el_midi_input(&engine, message, bytes);
}
