/* SPDX-License-Identifier: GPL-3.0-only
 * fm1-synths: build configuration every synth shares, for device and host builds.
 * From the MKS-50 firmware's engine/mk_config.h (there after FM1-Electro's engine/el_config.h,
 * github.com/robtron-byte/FM1-Electro-one, commit e7b3b14). A synth's own config header
 * (mks50/engine/mk_config.h, ...) includes this one and adds its voice pool and names. */
#ifndef SYN_CONFIG_H
#define SYN_CONFIG_H
#include <stddef.h>
#include <stdint.h>

#define EL_SAMPLE_RATE 44100.0f
/* Frames per audio block (the I2S half buffer on the device): 64 = 1.45 ms
 * (mks50/docs/adr/0004). A multiple of the control rate (16). The build sets it with -DSYN_BLOCK. */
#ifndef SYN_BLOCK
#define SYN_BLOCK 64
#endif
#define EL_BLOCK SYN_BLOCK

/* Everything called from the audio interrupt lives in internal SRAM on the
 * device (.dsp_text / .dsp_rodata, copied at boot: a taken branch costs half
 * there of what it costs from the flash's cache). Host builds ignore it. */
#if defined(EL_PLATFORM)
#ifndef EL_DSP_SECTION
#define EL_DSP_SECTION ".dsp_text"              /* (a build may place an object's audio code elsewhere) */
#endif
#define EL_DSP_CODE __attribute__((section(EL_DSP_SECTION), noinline, used))
#ifndef EL_DSP_DATA_SECTION
#define EL_DSP_DATA_SECTION ".dsp_rodata"       /* (combo: the JX-8P's tables in the RAM code area, ".ram_text.jxd") */
#endif
#define EL_DSP_DATA __attribute__((section(EL_DSP_DATA_SECTION), aligned(4)))
#define EL_DSP_HELPER __attribute__((section(EL_DSP_SECTION)))
#define EL_POOL __attribute__((section(".pool"), aligned(4)))   /* big zeroed buffers: the platform's pool */
#else
#define EL_DSP_CODE
#define EL_DSP_DATA
#define EL_DSP_HELPER
#define EL_POOL
#endif

static inline int el_clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float el_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
#endif
