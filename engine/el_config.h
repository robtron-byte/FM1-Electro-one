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
#ifdef FM1_HOST_TEST
#define EL_DSP_CODE
#define EL_DSP_DATA
#define EL_DSP_HELPER
#else
#define EL_DSP_CODE __attribute__((section(".volatile_ram_code"), noinline, used))
#define EL_DSP_DATA __attribute__((section(".volatile_ram.fm1"), aligned(4)))
/* Small helpers on the audio path: in SRAM when not inlined. */
#define EL_DSP_HELPER __attribute__((section(".volatile_ram_code")))
#endif

#define EL_FENCE() __sync_synchronize()

static inline int el_clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float el_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
#endif
