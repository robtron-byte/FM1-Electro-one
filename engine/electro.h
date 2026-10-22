/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * FM1-Electro engine: transport plus the instruments that follow it.
 * Today: the 808 drum machine. Next: MKS-50 / JP-6 voice engines with
 * arpeggiator, step sequencer, note looper and shared delay/reverb. */
#ifndef EL_ELECTRO_H
#define EL_ELECTRO_H
#include "el_config.h"
#include "el_queue.h"
#include "transport.h"
#include "drums.h"

enum el_command {
    EL_CMD_START,        /* c = 1 when it came from MIDI (follow its clock) */
    EL_CMD_STOP,
    EL_CMD_CONTINUE,
    EL_CMD_BPM,          /* value = BPM x 10 */
    EL_CMD_CLOCK_SOURCE, /* a = EL_CLOCK_INTERNAL / EL_CLOCK_MIDI */
    EL_CMD_MIDI_TICK,    /* one F8 */
    EL_CMD_MASTER,       /* value = 0..127 */
    EL_CMD_DRUM_HIT      /* a = part, value = velocity 1..127; records when armed */
};

typedef struct {
    el_transport_t transport;
    dr_machine_t drums;
    el_queue_t queue;       /* from the control task */
    el_queue_t midi_queue;  /* from the MIDI routing task */
    float master;
    volatile uint32_t blocks;
} el_engine_t;

/* Control task, before audio starts. */
void el_init(el_engine_t *e);
/* Control task. Returns 0, or -1 when the queue is full. */
int el_post(el_engine_t *e, uint8_t type, uint8_t a, int16_t value);
/* MIDI routing task: its own queue, so the two tasks never share a producer
 * side. Handles clock, Start/Continue/Stop and drum notes on channel 10. */
int el_midi_input(el_engine_t *e, const uint8_t *message, size_t bytes);
/* Audio interrupt: fill frames of interleaved stereo signed Q23. */
EL_DSP_CODE void el_render(el_engine_t *e, int32_t *output, unsigned frames);
/* Same, as float -1..1 interleaved (host tools). */
EL_DSP_CODE void el_render_float(el_engine_t *e, float *output, unsigned frames);
#endif
