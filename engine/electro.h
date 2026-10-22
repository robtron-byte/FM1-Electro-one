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
    EL_CMD_START,
    EL_CMD_STOP,
    EL_CMD_CONTINUE,
    EL_CMD_BPM,          /* value = BPM x 10 */
    EL_CMD_CLOCK_SOURCE, /* a = EL_CLOCK_INTERNAL / EL_CLOCK_MIDI */
    EL_CMD_MIDI_TICK,    /* one F8 */
    EL_CMD_MASTER        /* value = 0..127 */
};

typedef struct {
    el_transport_t transport;
    dr_machine_t drums;
    el_queue_t queue;
    float master;
    volatile uint32_t blocks;
} el_engine_t;

/* Control task, before audio starts. */
void el_init(el_engine_t *e);
/* Control or MIDI routing task (one producer at a time per queue). */
int el_post(el_engine_t *e, uint8_t type, uint8_t a, int16_t value);
/* Audio interrupt: fill frames of interleaved stereo signed Q23. */
EL_DSP_CODE void el_render(el_engine_t *e, int32_t *output, unsigned frames);
/* Same, as float -1..1 interleaved (host tools). */
EL_DSP_CODE void el_render_float(el_engine_t *e, float *output, unsigned frames);
#endif
