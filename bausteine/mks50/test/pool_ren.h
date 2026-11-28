/* SPDX-License-Identifier: GPL-3.0-only
 * mks50: der Pool-Kern der Firmware (varianten/mks50/engine/mks.c) heißt wie engine/mks.c; für test/pool.c bekommen
 * seine globalen Namen ein fm_ davor (mit -include eingebunden). */
#define mks_active fm_mks_active
#define mks_all_off fm_mks_all_off
#define mks_begin fm_mks_begin
#define mks_control fm_mks_control
#define mks_finish fm_mks_finish
#define mks_init fm_mks_init
#define mks_lfo_hz fm_mks_lfo_hz
#define mks_mix fm_mks_mix
#define mks_note_off fm_mks_note_off
#define mks_note_on fm_mks_note_on
#define mks_param_max fm_mks_param_max
#define mks_param_name fm_mks_param_name
#define mks_param_value_name fm_mks_param_value_name
#define mks_part_load fm_mks_part_load
#define mks_part_set fm_mks_part_set
#define mks_part_voices fm_mks_part_voices
#define mks_plan fm_mks_plan
#define mks_preset_count fm_mks_preset_count
#define mks_presets fm_mks_presets
#define mks_render fm_mks_render
#define mks_voices fm_mks_voices
#define mks_chords fm_mks_chords
#define mks_chord_names fm_mks_chord_names
#define mks50_eigene fm_mks50_eigene
#define mks50_eigene_count fm_mks50_eigene_count
