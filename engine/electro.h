/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * FM1-Electro engine: transport plus the instruments that follow it.
 * The 808 drum machine, three synth areas (MKS-50 / JP-6) with step
 * sequencer and arpeggiator, shared effects and the mix bus. */
#ifndef EL_ELECTRO_H
#define EL_ELECTRO_H
#include "el_config.h"
#include "el_queue.h"
#include "transport.h"
#include "drums.h"
#include "synth_track.h"
#include "fx.h"
#include "mixbus.h"

enum el_command {
    EL_CMD_START,        /* c = 1 when it came from MIDI (follow its clock) */
    EL_CMD_STOP,
    EL_CMD_CONTINUE,
    EL_CMD_BPM,          /* value = BPM x 10 */
    EL_CMD_CLOCK_SOURCE, /* a = EL_CLOCK_INTERNAL / EL_CLOCK_MIDI */
    EL_CMD_MIDI_TICK,    /* one F8 */
    EL_CMD_MASTER,       /* value = 0..127 */
    EL_CMD_DRUM_HIT,     /* a = part, value = velocity 1..127; records when armed */
    /* Synth areas: a = track 0 rhythm, 1 strings, 2 bass (EL_TRACKS). */
    EL_CMD_NOTE,         /* b = note, value = velocity (0 = off); records when armed, c = 1: never */
    EL_CMD_STEP_TOGGLE,  /* b = step: on/off with the last played note */
    EL_CMD_STEP_FLAG,    /* b = step, c = ST_ACCENT / ST_TIE: toggle (switches the step on) */
    EL_CMD_STEP_NOTE,    /* b = step, value = semitones to add (switches the step on) */
    EL_CMD_STEP_CHORD,   /* b = step, value = chord 0..8 */
    EL_CMD_STEP_SET,     /* b = step, c = note, value = flags | chord << 8 */
    EL_CMD_STEP_WRITE,   /* step record: as STEP_SET, and the pattern ends after b */
    EL_CMD_STEP_TIE,     /* b = step, c = 1: tie it into the next step, 0: no tie */
    EL_CMD_TRACK_TRANSPOSE, /* value = semitones added to every step on playback */
    EL_CMD_TRACK_LENGTH, /* b = 1..32 */
    EL_CMD_TRACK_GATE,   /* b = 10..100 */
    EL_CMD_TRACK_PARAM,  /* b = mks parameter, value */
    EL_CMD_TRACK_PRESET, /* b = sound index, c = engine: loads the staged patch */
    EL_CMD_TRACK_PATTERN,/* b = pattern slot: copies the staged pattern */
    EL_CMD_TRACK_MUTE,   /* b = on */
    EL_CMD_TRACK_LEVEL,  /* value = 0..127 */
    EL_CMD_TRACK_CHORD,  /* b = live chord 0..8 */
    EL_CMD_TRACK_RATE,   /* b = step length ST_RATE_* */
    EL_CMD_STEP_INV,     /* b = step, value = inversion 0..2 */
    EL_CMD_TRACK_CLEAR,  /* clear the current pattern */
    EL_CMD_TRACK_ARP,    /* b = ARP_* setting, value */
    EL_CMD_RECORD_AREA,  /* a = area 0 beat .. 3 bass, b = on; other areas disarm */
    EL_CMD_FX,           /* a = fx parameter, value */
    EL_CMD_MIX,          /* a = 1: mix and master processing on, 0: off (A/B) */
    EL_CMD_TRACK_SELECT, /* a = track, b = slot A-D; c = 1 switches now, else at the next bar */
    EL_CMD_MIDI_CHANNEL, /* a = area, value = channel 0..15 or EL_MIDI_OFF */
    EL_CMD_OPTION,       /* a = EL_OPT_*, value */
    EL_CMD_PUNCH,        /* a = EL_PUNCH_* bit, b = 1 held / 0 let go */
    EL_CMD_PUNCH_RATE,   /* a = EL_PUNCH_* bits, b = white key 0..5 (repeat 0..15; el_punch_value_name) */
    EL_CMD_STEP_COND,    /* b = step, value = EL_COND_* (docs/adr/0025) */
    EL_CMD_STEP_PROB     /* b = step, value = 0..EL_PROBS-1, 0 = always (docs/adr/0025) */
};
/* Punch-in effects, active while held (HOME, black keys 5-11, in this
 * order). The bit of effect n is 1 << n. */
enum { EL_PUNCH_STUTTER = 1, EL_PUNCH_THROW = 2, EL_PUNCH_TAPE = 4, EL_PUNCH_REPEAT = 8,
       EL_PUNCH_DELAY = 16, EL_PUNCH_CRUSH = 32, EL_PUNCH_PHASER = 64 };
enum { EL_PUNCHES = 7, EL_PUNCH_DIVS = 6 };
extern const char *const el_punch_div_names[EL_PUNCH_DIVS];
/* What white key v (0..EL_PUNCH_DIVS-1) sets for punch-in n, or NULL when
 * it has nothing to set (the reverb throw). */
const char *el_punch_value_name(unsigned n, unsigned v);
/* Repeat takes a step of the bar: white keys 1-16. */
enum { EL_REPEAT_STEPS = 16 };

enum { EL_TRACKS = 3, EL_AREAS = 4 };
/* MIDI channel per area (0-based); EL_MIDI_OFF ignores the area's notes.
 * Two areas on one channel both play. */
enum { EL_MIDI_OFF = 16 };
/* Settings made once in the OPTIONS menu (HOME + SAVE), kept in Flash. */
enum {
    EL_OPT_MIDI_CLOCK,  /* 1: follow MIDI clock and Start/Stop, 0: ignore them */
    EL_OPT_MASTER,      /* 1: mix bus and master processing on, 0: off */
    EL_OPT_OUT_LEVEL,   /* 0..127 output level (127 = full) */
    EL_OPT_START,       /* EL_START_*: slots at power-on (where they were, or A) */
    EL_OPT_HINTS,       /* 1: help line at the bottom of the screen */
    /* Master compressor (in the MASTER FX chain, before saturation and limiter). */
    EL_OPT_COMP_THRESHOLD,  /* 0..30: threshold -0 .. -30 dB */
    EL_OPT_COMP_RATIO,      /* index into el_comp_ratios */
    EL_OPT_COMP_ATTACK,     /* index into el_comp_attacks (ms) */
    EL_OPT_COMP_RELEASE,    /* index into el_comp_releases (ms) */
    EL_OPT_COMP_MAKEUP,     /* 0..24: makeup 0 .. +12 dB in half dB */
    EL_OPT_REC_STEP,        /* bit per synth track: REC records step by step (SH-101), else live */
    EL_OPT_DIST_KICK_FULL,  /* 1: the bass drum keeps its lows into the distortion; 0 (default): cut like the rest */
    /* The master compressor's parallel share (docs/adr/0031): 0..20 steps of 5 % of the
     * uncompressed signal; 0 = all compressed (the menu shows MIX 100 %). Stored apart (version 25). */
    EL_OPT_COMP_DRY,
    EL_OPTIONS = 16,        /* in memory */
    EL_OPTIONS_STORED = 12  /* the first ones: where records of versions up to 24 end */
};
/* The last ratio is CLUB (EL_COMP_CLUB, docs/adr/0029): 4:1 with its own
 * attack, program release and a parallel share; attack and release set
 * in OPTIONS do not apply then. */
enum { EL_COMP_RATIOS = 9, EL_COMP_CLUB = 8, EL_COMP_ATTACKS = 8, EL_COMP_RELEASES = 8 };
extern const float el_comp_ratios[EL_COMP_RATIOS];
extern const uint16_t el_comp_attacks[EL_COMP_ATTACKS], el_comp_releases[EL_COMP_RELEASES];
/* Largest value of an option. */
int el_option_max(unsigned option);
enum { EL_START_LAST, EL_START_SLOT_A };
enum { EL_AREA_BEAT, EL_AREA_RHYTHM, EL_AREA_STRINGS, EL_AREA_BASS };

/* Render stages timed on the device: drums, RHYTHM, STRINGS, BASS, effects
 * (distortion, reverb, delay, gated reverb, throw), master bus, punch-ins. */
enum { EL_PROF_DRUMS, EL_PROF_RHYTHM, EL_PROF_STRINGS, EL_PROF_BASS, EL_PROF_FX, EL_PROF_MASTER, EL_PROF_PUNCH, EL_PROF_STAGES };

typedef struct {
    el_transport_t transport;
    dr_machine_t drums;
    st_track_t track[EL_TRACKS];
    el_fx_t fx;
    el_mixbus_t mix;
    uint8_t staged_patch[SYN_VALUES];      /* control writes, a command copies */
    st_pattern_t staged_pattern;
    volatile uint32_t staged_posted, staged_taken;
    el_queue_t queue;       /* from the control task */
    el_queue_t midi_queue;  /* from the MIDI routing task */
    float master;
    volatile uint8_t midi_channel[EL_AREAS];   /* read by the MIDI routing task */
    volatile uint8_t option[EL_OPTIONS];
    /* Punch-in: held effects, stutter phase (16ths, reset on every step),
     * dry level and how long the reverb throw has been fed. */
    volatile uint8_t punch;
    float stutter_phase, stutter_gain, dry_gain, throw_fed, throw_lp, kick_low;
    /* The dry buses' last DIST_LATENCY samples (kick, drums L R, bass, synths L R): delayed
     * by as much as the distortion send's wet signal (docs/adr/0030). */
    float dry_tail[6][DIST_LATENCY];
    /* White key value per punch-in (el_punch_value_name). */
    uint8_t punch_div[EL_PUNCHES];
    uint32_t sync_step;
    /* Tape stop: the finished mix through its buffer region in the FX
     * memory (borrowed while it sounds). */
    uint8_t tape_half;
    float tape_mix, tape_rp, tape_rp2, tape_xf, tape_rate, tape_t, tape_acc;
    uint32_t tape_wp;
    /* Delay (its own region, half rate, mono), bitcrusher, phaser. */
    uint8_t dl_half, crush_n;
    float dl_in, dl_time, dl_acc, dl_lp, dl_hp, dl_hpx, dl_env;
    uint32_t dl_wp;
    float crush_mix, crush_hold[2];
    float ph_mix, ph_phase, ph_a[2], ph_z[2][6], ph_fb[2];
    int stutter_sync;
    volatile uint32_t blocks;
    /* Time per render stage (device only, docs/adr/0019): ticks of the SDK's
     * audio time base summed since the control task last took them. */
    volatile uint32_t prof[EL_PROF_STAGES];
    volatile uint32_t synth_peak;   /* the slowest synth stage (one call, ticks) since it was read */
    /* Two cores: the ticks of this block and of the block before of the
     * synths (second core), drums (first core) and the bus parts (sends and
     * reverb throw, distortion, the rest); they decide where the effects and
     * master of the block before run (electro.c bus_place). */
    volatile uint32_t syn_ticks, drum_ticks, prev_syn_ticks, prev_drum_ticks;
    volatile uint32_t pre_ticks, dist_ticks, post_ticks, prev_pre_ticks, prev_dist_ticks, prev_post_ticks;
    /* Second core (docs/adr/0021): the platform's hand-over of a job and the
     * wait for it (0 = done; else run it here). NULL: one core. */
    int (*par_run)(void (*job)(uint32_t), uint32_t arg);
    int (*par_wait)(void);
} el_engine_t;

/* Control task, before audio starts. */
void el_init(el_engine_t *e);
/* Audio side or before audio starts: the master compressor from the options. */
void el_apply_comp(el_engine_t *e);
/* Control task, before audio starts: generate every style slot (drums and
 * the three synth parts, in the style's harmony) and load the default
 * sounds. User slots are left alone. */
void el_generate_all(el_engine_t *e, uint32_t seed);
/* Control task, before audio starts, when nothing is stored: every slot
 * empty (drums and the three synth areas), a random kit and a random
 * factory sound per synth area (seed: anything that differs between
 * starts). */
void el_blank_start(el_engine_t *e, uint32_t seed);
/* Control task: the three synth parts (rhythm, strings, bass) that go with
 * a drum groove, from its style. */
void el_generate_parts(const dr_pattern_t *d, uint32_t seed, st_pattern_t out[EL_TRACKS]);
/* Control task. Returns 0, or -1 when the queue is full. */
int el_post(el_engine_t *e, uint8_t type, uint8_t a, int16_t value);
int el_post3(el_engine_t *e, uint8_t type, uint8_t a, uint8_t b, uint8_t c, int16_t value);
/* Control task: load a factory sound / a whole pattern into a synth track.
 * Returns 0, -1 queue full, -2 while the previous one is still pending. */
int el_load_preset(el_engine_t *e, unsigned track, unsigned preset);
/* Same with any values (a user sound): id is what the track shows as its
 * sound (factory index, or EL_USER_SOUND + slot). */
int el_load_sound(el_engine_t *e, unsigned track, unsigned id, unsigned engine, const uint8_t *values);
enum { EL_USER_SOUND = 200, EL_USER_SOUNDS = 32, EL_SOUND_NAME = 10 };
/* A user sound slot (stored by fm1/el_store.c). */
typedef struct {
    char name[EL_SOUND_NAME + 1];
    uint8_t engine, used;
    uint8_t v[SYN_VALUES];
} el_sound_t;
int el_load_pattern(el_engine_t *e, unsigned track, unsigned slot, const st_pattern_t *p);
/* MIDI routing task: its own queue, so the two tasks never share a producer
 * side. Handles clock, Start/Continue/Stop and the notes of each area on
 * its channel (midi_channel; default drums 10, RHYTHM 1, STRINGS 2, BASS 3). */
int el_midi_input(el_engine_t *e, const uint8_t *message, size_t bytes);
/* Audio interrupt: fill frames of interleaved stereo signed Q23. */
EL_DSP_CODE void el_render(el_engine_t *e, int32_t *output, unsigned frames);
/* Same, as float -1..1 interleaved (host tools). */
EL_DSP_CODE void el_render_float(el_engine_t *e, float *output, unsigned frames);
#endif
