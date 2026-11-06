/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * FM1-Electro: shared build configuration for device and host builds. */
#ifndef EL_CONFIG_H
#define EL_CONFIG_H
#include <stddef.h>
#include <stdint.h>
#include "fm1_block.h"

#define EL_SAMPLE_RATE ((float)FM1_SAMPLE_RATE)
#define EL_BLOCK FM1_BLOCK_FRAMES

/* Everything called from the audio interrupt must live in internal SRAM on
 * the device (see FM1_AUDIO_CODE in the SDK). Host builds ignore placement. */
/* EL_PLATFORM: the build on our own platform (platform/, docs/adr/0021): the
 * audio code in SRAM too (.dsp_text, copied at boot: a taken branch costs half
 * there of what it costs from the flash's cache, CPU test 2026-11-05). */
#if defined(EL_PLATFORM)
#define EL_DSP_CODE __attribute__((section(".dsp_text"), noinline, used))
#define EL_DSP_DATA __attribute__((section(".dsp_rodata"), aligned(4)))
#define EL_DSP_HELPER __attribute__((section(".dsp_text")))
#define EL_DSP_VOICE __attribute__((section(".dsp_text"), noinline))
#elif defined(FM1_HOST_TEST)
#define EL_DSP_CODE
#define EL_DSP_DATA
#define EL_DSP_HELPER
#define EL_DSP_VOICE
#else
#define EL_DSP_CODE __attribute__((section(".volatile_ram_code"), noinline, used))
#define EL_DSP_DATA __attribute__((section(".volatile_ram.fm1"), aligned(4)))
/* Small helpers on the audio path: in SRAM when not inlined. */
#define EL_DSP_HELPER __attribute__((section(".volatile_ram_code")))
/* Larger per-voice code called from one render loop: kept out of line, so
 * each exists once in SRAM and Flash instead of copied into its caller. */
#define EL_DSP_VOICE __attribute__((section(".volatile_ram_code"), noinline))
#endif

#define EL_FENCE() __sync_synchronize()

/* Step condition (docs/adr/0025): a step plays on pass n of every m of its
 * pattern (passes counted from the pattern's start): 1:1 always, 1:2 the
 * first of every two, 2:2 the second, 1:4 .. 4:4 one of every four. */
enum { EL_COND_ALWAYS, EL_COND_1_2, EL_COND_2_2, EL_COND_1_4, EL_COND_2_4, EL_COND_3_4, EL_COND_4_4, EL_CONDS };
static inline int el_cond_plays(unsigned cond, unsigned pass)
{
    static const uint8_t every[EL_CONDS] = {1, 2, 2, 4, 4, 4, 4}, which[EL_CONDS] = {0, 0, 1, 0, 1, 2, 3};
    return cond >= EL_CONDS || pass % every[cond] == which[cond];
}

static inline int el_clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float el_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
/* The BEAT kit in fixed point (engine/dr_fixed.c, docs/adr/0022): on the own
 * platform; the host and FM1 Open's SDK keep the float 808. */
#ifndef TR808_FIXED
#if defined(EL_PLATFORM)
#define TR808_FIXED 1
#else
#define TR808_FIXED 0
#endif
#endif
#endif
