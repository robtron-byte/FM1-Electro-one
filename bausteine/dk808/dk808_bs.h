/* SPDX-License-Identifier: GPL-3.0-only
 * dk808: Adapter für das Gerüst der Solo- und Kombi-Firmwares (bausteine/varianten/gemeinsam, docs/adr/0050).
 * Ein Drum-Part = ein dk808_t; sein Sound ist ein Kit aus DK_BS_NP Rohwerten (uint8), gespeichert wie ein Synth-Sound:
 *   0..16   Pegel je Instrument 0..100 (Standard 100)
 *   17..67  die drei Regler je Instrument 0..100 (Instrument * 3 + Regler, Standard wie im DrumKern)
 *   68..70  HAT/CONGA/TOM CHOKE 0..1
 *   71      DIST SEND 0..100 (Anteil des Parts in den Distortion-Send der Firmware, Standard 0)
 * Für das Gerät mit -DDK808_FLOAT=1 bauen (gemessen: make -C bausteine/dk808 float). Noten wählen das Instrument über
 * die Drum-Map des DrumKern (dk808_note_instrument); Note aus wird nicht gebraucht (Drums klingen aus). */
#ifndef DK808_BS_H
#define DK808_BS_H
#include <stdint.h>
#include "dk808.h"

#define DK_BS_LEVEL 0u
#define DK_BS_KNOB DK_COUNT
#define DK_BS_CHOKE (DK_BS_KNOB + DK_COUNT * DK808_KNOBS)
#define DK_BS_DIST (DK_BS_CHOKE + 3u)
#define DK_BS_NP (DK_BS_DIST + 1u)

typedef struct { dk808_t k; float dist; } dk808_bs_t;   /* der Kern und der Anteil in den Distortion-Send */

void dk808_bs_init(dk808_bs_t *d, double sample_rate);
int dk808_bs_note_on(dk808_bs_t *d, int note, int velocity);   /* das ausgelöste Instrument, -1 = keins */
void dk808_bs_all_off(dk808_bs_t *d);                          /* sofort still */
void dk808_bs_set_raw(dk808_bs_t *d, unsigned param, int value);
void dk808_bs_load_values(dk808_bs_t *d, const uint8_t *v);    /* DK_BS_NP Rohwerte */
int dk808_bs_param_max(unsigned param);
const char *dk808_bs_param_name(unsigned param, char *buf);   /* z. B. "BD LEVEL", "SD SNAPPY"; buf >= 16 Byte */
int dk808_bs_voices(const dk808_bs_t *d);                        /* klingende Instrumente */
extern const uint8_t dk808_bs_init_kit[DK_BS_NP];             /* INIT: Standardwerte des DrumKern */
/* Wie dk808_render (addiert mono auf l und r), dazu der Distortion-Send: dist += Signal * DIST SEND. */
void dk808_bs_render(dk808_bs_t *d, float *l, float *r, float *dist, int n);
#endif
