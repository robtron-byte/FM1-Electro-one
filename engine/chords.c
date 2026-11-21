/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * FM1-Electro's chord memory (engine/chords.h), moved unchanged from engine/mks.c (docs/adr/0039). */
#if defined(EL_PLATFORM)   /* own platform: the constants in SRAM with the audio code */
#pragma const_seg(".dsp_rodata")
#elif !defined(FM1_HOST_TEST)   /* FM1 Open SDK build */
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "chords.h"

/* The MKS-50 app's eight chords: the types measured in Rother's and
 * The Egyptian Lover's pads. */
const char *const mks_chord_names[MKS_CHORDS] = {"m7", "SUS2", "MIN", "5+8", "SUS4", "7TH", "MAJ", "m7b5"};
const int8_t mks_chords[MKS_CHORDS][MKS_CHORD_NOTES] EL_DSP_DATA = {
    {0, 3, 7, 10, -128}, {0, 2, 7, -128, 0}, {0, 3, 7, -128, 0}, {0, 7, 12, -128, 0},
    {0, 5, 7, -128, 0}, {0, 4, 7, 10, -128}, {0, 4, 7, -128, 0}, {0, 3, 6, 10, -128}};

const uint8_t st_chord_v15[14] = {0, 1, 1, 3, 4, 5, 5, 7, 8, 7, 2, 7, 7, 6};
