/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Shared send effects: a tempo-synced ping-pong delay and an FDN reverb,
 * after the iOS apps' StereoDelay / Reverb, shrunk for the FM-1's RAM:
 * the delay runs at a third of the rate in 16 bit (up to 0.6 s, darker
 * repeats like a BBD/tape echo), the reverb uses 4 instead of 8 lines in 16
 * bit. About 70 KB. */
#ifndef EL_FX_H
#define EL_FX_H
#include "el_config.h"
#include "dist.h"

enum {
    FX_DELAY_CAP = 8832,   /* third-rate frames: 0.6 s */
    FX_REVERB_CAP = 2048,  /* per line */
    FX_DIFF_CAP = 512,
    FX_GATE_CAP = 8192     /* half-rate frames: 0.37 s non-linear (gated) reverb */
};
enum fx_param { FX_DELAY_DIV, FX_DELAY_FB, FX_DELAY_TONE, FX_DELAY_RETURN,
                FX_REVERB_SIZE, FX_REVERB_DECAY, FX_REVERB_DAMP, FX_REVERB_RETURN,
                /* Shared distortion send: one instance of the apps' distortion
                 * (mono), fed by a send per area. */
                FX_DIST_MODE, FX_DIST_DRIVE, FX_DIST_TONE,
                FX_DIST_SEND_BEAT, FX_DIST_SEND_RHYTHM, FX_DIST_SEND_STRINGS, FX_DIST_SEND_BASS, FX_PARAMS };

/* Gated reverb (A/B switch, docs/adr/0013): 0 = 15 taps about 15 ms apart
 * behind two short diffusers (audible as a flutter on claps); 1 = dense:
 * 24 taps at irregular distances behind four longer diffusers; 2 = a real
 * reverb (four-line feedback network) cut by a gate, as the 80s did it. */
#ifndef FX_GATE_DENSE
#define FX_GATE_DENSE 0
#endif
#define FX_GATE_DIFFS (FX_GATE_DENSE == 1 ? 4 : 2)
#ifndef FX_GATE_K
#define FX_GATE_K 0.7f    /* dense diffusers' coefficient */
#endif

/* The buffers of delay, hall and gated reverb. During a reverb throw
 * (the punch-in) the whole area is the big hall's memory: all three are
 * silent then, and it is cleared before and after. */
typedef struct {
    int16_t delay[2][FX_DELAY_CAP];
    int16_t lines[4][FX_REVERB_CAP];   /* at FX_LINE_SCALE: headroom for the FDN's peaks */
    int16_t diff[2][FX_DIFF_CAP];
    int16_t gate[FX_GATE_CAP];
} fx_buffers_t;
#define FX_LINE_SCALE 8191.0f          /* +-4 full scale in the reverb lines */
enum { FX_HOLE_CAP = sizeof(fx_buffers_t) / sizeof(int16_t) };
/* While a punch-in effect sounds, the buffers are split so all of them can
 * run together: the big hall, the delay (mono, half rate: 0.37 s) and the
 * tape stop (mono, half rate, the rest: 0.23 s behind). */
enum { FX_HALL_REGION = 21760, FX_DELAY_REGION = 8192,
       FX_TAPE_REGION = FX_HOLE_CAP - FX_HALL_REGION - FX_DELAY_REGION };
enum { FX_USE_HALL = 1, FX_USE_TAPE = 2, FX_USE_DELAY = 8 };
typedef union { fx_buffers_t b; int16_t hole[FX_HOLE_CAP]; } fx_memory_t;

/* The big hall of the reverb throw: Dattorro's figure-eight tank, made
 * 1.3 times larger, at half rate in 16 bit, slowly modulated. */
typedef struct { uint32_t off, len, pos; } fx_line_t;
typedef struct {
    fx_line_t in[4], ap1[2], dl1[2], ap2[2], dl2[2];
    float lp[2], feedback[2], phase, acc, prev[2], out[2];
    uint8_t half;
} fx_hole_t;

typedef struct {
    fx_memory_t mem;
    int16_t gate_diff[FX_GATE_DIFFS][256];   /* at FX_LINE_SCALE */
    uint8_t v[FX_PARAMS];
    unsigned dw, rw, fw;
    uint8_t half;          /* frame within the delay's third-rate step (0..2) */
    float acc[2], lp[2], last[2], time, damp[4], lfo[4];
    float rev_hp[2], dly_hp[2], gate_hp[2], gate_acc, gate_lp[2];
    float gate_ef, gate_es, gate_gain, gate_damp[4];   /* FX_GATE_DENSE 2: onset, gate, loop damping */
    uint16_t gate_hold;
    unsigned gw, gfw;
    uint8_t gate_half;
    /* Delay, reverb, gated reverb asleep: input silent and output below
     * -120 dB for longer than their longest line, buffers cleared; they
     * wake with the next input. */
    uint8_t delay_sleep, reverb_sleep, gate_sleep;
    uint16_t delay_quiet, reverb_quiet, gate_quiet;   /* frames silent in and out */
    /* Reverb throw: a big, long hall while held; on release its return
     * fades out quickly and the lines are cleared. */
    uint8_t throw_on;
    uint8_t users;         /* FX_USE_*: punch-ins holding the buffers; delay, hall, gate rest */
    float throw_mix;
    fx_hole_t hole;
    el_dist_t dist;
    uint8_t dist_last;     /* type that sounded last, kept while fading out */
    float dist_fade;       /* 0 .. 1: the send's return; 0 with the type OFF */
    float dist_r[EL_BLOCK];
} el_fx_t;

void fx_init(el_fx_t *fx);
int fx_param_max(unsigned param);
const char *fx_param_name(unsigned param);
const char *fx_delay_division_name(int value);
/* Add the wet signal of the sends to left/right. gate is the send to the
 * non-linear reverb (claps and snares, after TC's NonLin2 that Rother uses). */
const char *fx_dist_mode_name(int value);
/* The distortion send: send (mono, frames <= EL_BLOCK) is distorted in
 * place and added to left/right. Skipped while all sends are at 0. */
EL_DSP_CODE void fx_distortion(el_fx_t *fx, float *send, float *left, float *right, int frames);
/* Punch-ins borrowing the buffers (FX_USE_*): borrow clears the user's
 * region (all of it for the first user), release gives them back. */
void fx_borrow(el_fx_t *fx, unsigned use);
void fx_release(el_fx_t *fx, unsigned use);
int16_t *fx_region(el_fx_t *fx, unsigned use);
/* The big hall of the reverb throw (fx_render calls it while it sounds). */
void hole_init(fx_hole_t *h);
EL_DSP_CODE void hole_render(el_fx_t *fx, const float *in, float *left, float *right, int frames);
EL_DSP_CODE void fx_render(el_fx_t *fx, const float *dly, const float *rev, const float *gate,
                           float *left, float *right, int frames, float bpm);
#endif
