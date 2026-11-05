/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "el_fm1.h"
#include "el_ui.h"
#include "el_store.h"
#include "fm1_device.h"

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
static uint32_t block_ticks, load_at, drops_seen;

/* CPU test (docs/adr/0019): how long one operation takes on this FM-1. From
 * Flash (SRAM is short); the loops are a few words and run from the cache
 * after the first round. Each loop runs N times with its operation
 * chained (each step needs the one before), timed by the audio time base;
 * the shortest of five runs counts (an interrupt may land in one). */
enum { BENCH_N = 2000 };
#define KEEP(x) __asm__ volatile("" : "+r"(x))
static uint32_t ticks_since(uint32_t start)
{
    uint32_t end = EL_AUDIO_TIMER;
    return end >= start ? end - start : end + EL_AUDIO_TIMER_PERIOD + 1u - start;
}
static uint32_t bench_one(unsigned kind)
{
    volatile float seed_f = 1.0000001f, small = 1e-39f, two = 2.0f;
    volatile uint32_t seed_i = 12345u;
    float a = seed_f, b = seed_f, c = seed_f, d = seed_f, k = seed_f, z = small;
    uint32_t x = seed_i;
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
    default: break;
    }
    uint32_t used = ticks_since(start);
    volatile float sink = a + b + c + d + z;
    volatile uint32_t sink_i = x;
    (void)sink; (void)sink_i;
    return used;
}
static void bench(uint16_t out[8])
{
    uint32_t clock = fm1_device_clock();
    if (!clock) return;
    for (unsigned kind = EL_BENCH_LOOP; kind <= EL_BENCH_DENORM; ++kind) {
        uint32_t best = 0xffffffffu;
        for (int r = 0; r < 5; ++r) {
            uint32_t t = bench_one(kind);
            if (t < best) best = t;
        }
        /* tenths of a ns per iteration; the loop itself is not subtracted */
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
        el_init(&engine);
        /* The last grooves come back from Flash; only on the very first
         * start (empty Flash) the generated factory set plays. */
        el_store_load(&engine);
        el_ui_init(&ui, &engine);
        bench(ui.bench);
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
