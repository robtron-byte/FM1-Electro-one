/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Tempo-synced arpeggiator of one synth area, after the iOS apps'
 * Arpeggiator: collects the held keys (expanded by the live chord) and plays
 * one note per grid step, UP / DOWN / UP-DOWN / RANDOM / ORDER over 1-4
 * octaves. Follows the 16th clock while the transport runs and keeps going
 * on its own at the set tempo while it is stopped. Audio-owned. */
#ifndef EL_ARP_H
#define EL_ARP_H
#include "el_config.h"

enum { ARP_ON, ARP_MODE, ARP_RATE, ARP_OCTAVES, ARP_GATE, ARP_HOLD, ARP_PARAMS };
enum { ARP_MODES = 5, ARP_RATES = 11, ARP_KEYS = 16, ARP_POOL = 80, ARP_TICKS_PER_16TH = 6 };

/* Note events of one call, off before on (-1: none). */
typedef struct { int16_t off, on; uint8_t velocity; } arp_events_t;

typedef struct {
    uint8_t v[ARP_PARAMS];        /* settings (stored with the area) */
    uint8_t keys[ARP_KEYS], velocities[ARP_KEYS], count;
    uint8_t held[16];             /* keys physically down (bit per note) */
    uint8_t chord;                /* live chord of the area, ST chord byte */
    uint8_t sub, grid_valid;
    int16_t sounding;
    uint32_t tick, last_grid, position, random;
    float countdown, tick_frames, gate_left;
} arp_t;

extern const char *const arp_mode_names[ARP_MODES];
extern const char *const arp_rate_names[ARP_RATES];
int arp_param_max(unsigned param);

void arp_init(arp_t *a);
/* Key from the panel or MIDI (velocity 0 = release). May start the first
 * note right away. */
EL_DSP_CODE arp_events_t arp_key(arp_t *a, int note, int velocity);
/* Settings; switching off or leaving HOLD can end the sounding note. */
EL_DSP_CODE arp_events_t arp_set(arp_t *a, unsigned param, int value);
/* Transport 16th fired (running): realign to the clock. */
EL_DSP_CODE arp_events_t arp_sync(arp_t *a, uint32_t step, float sixteenth_frames);
/* Frames until arp_advance has something to do; limit when nothing. */
EL_DSP_CODE int arp_frames_until(const arp_t *a, int limit);
/* After frames were rendered. running: transport runs (sub-ticks wait for
 * arp_sync after the 16th); stopped: free-running at bpm. */
EL_DSP_CODE arp_events_t arp_advance(arp_t *a, int frames, int running, float bpm);
/* Forget the keys and return the note to release (-1: none). */
EL_DSP_CODE int arp_clear(arp_t *a);
static inline int arp_active(const arp_t *a) { return a->v[ARP_ON] != 0; }
#endif
