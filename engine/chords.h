/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * FM1-Electro's chord memory and areas, shared by every synth engine (MKS-50, JP-6, JX-8P) and the
 * sequencer: the eight chords of the MKS-50 / JX-8P / JP-6 apps (ChordMemory.swift). Moved here from
 * engine/mks.h and mks.c (docs/adr/0039) so the sequencer does not depend on one engine; the names keep
 * their MKS_ prefix. */
#ifndef EL_CHORDS_H
#define EL_CHORDS_H
#include "el_config.h"

enum { MKS_CHORDS = 8, MKS_CHORD_NOTES = 5 };
enum { MKS_AREA_RHYTHM = 1, MKS_AREA_STRINGS = 2, MKS_AREA_BASS = 3 };

extern const char *const mks_chord_names[MKS_CHORDS];
extern const int8_t mks_chords[MKS_CHORDS][MKS_CHORD_NOTES]; /* offsets, -128 ends */
/* Chord type (1-based, 0 = none) of records up to version 15, when there
 * were 13 (m7 m9 MIN 5+8 4THS 7SUS4 MAJ7 m7b5 MAJ SUS2 ADD9 MAJ9 7), to
 * the nearest of today's eight. */
extern const uint8_t st_chord_v15[14];
#endif
