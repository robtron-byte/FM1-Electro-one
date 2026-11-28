/* SPDX-License-Identifier: GPL-3.0-only
 * jx8p: der JX-8P von FM1-Electro als Baustein (docs/adr/0040, 0046). Der Klangkern ist engine/jx8p.c (eingebunden,
 * nicht kopiert); dieser Adapter stellt ihn über baustein.h bereit, dazu die Presets (eigene des Nutzers zuerst,
 * docs/adr/0051), das Tempo für die LFO-Synchronisation und die Sends für Delay/Hall der späteren Firmware.
 *
 * Abtastrate: der Kern rechnet fest mit EL_SAMPLE_RATE (44,1 kHz, sdk_include/fm1_block.h); init nimmt jede Rate an,
 * die Koeffizienten bleiben aber die von 44,1 kHz (jx8p_bs_rate sagt, womit er rechnet). */
#ifndef JX8P_BS_H
#define JX8P_BS_H
#include "baustein.h"

/* Stimmen des Bausteins: voll wie der JX-8P (6). Sparfassung für ein Gerät: -DJX8P_BS_VOICES=4 o. ä. */
#ifndef JX8P_BS_VOICES
#define JX8P_BS_VOICES 6
#endif

extern const baustein_t jx8p_baustein;

double jx8p_bs_rate(void);
/* Presets: zuerst die eigenen des Nutzers (jx8p_eigene.c, aus den iPhone-JSON erzeugt), dann die Werks-Sounds aus
 * engine/jx8p_presets.c (64 Töne der JX-8P-ROM und INIT je Bereich). */
unsigned jx8p_bs_preset_count(void);
unsigned jx8p_bs_eigene_count(void);
const char *jx8p_bs_preset_name(unsigned i);
const unsigned char *jx8p_bs_preset_values(unsigned i);   /* JX_COUNT Rohwerte (engine/jx8p.h) */
void jx8p_bs_load_preset(void *state, unsigned i);
/* Ein Parameter als Rohwert 0..jx8p_param_max (wie der Kern ihn speichert). */
void jx8p_bs_set_raw(void *state, unsigned param, int value);
int jx8p_bs_get_raw(const void *state, unsigned param);
void jx8p_bs_set_bpm(void *state, double bpm);   /* für LFO SYNC, Standard 120 */
/* Wie render, zusätzlich die Sends (Delay, Hall) addiert, die der Kern je Preset ausgibt (JX_DELAY_SEND, JX_REVERB_SEND). */
void jx8p_bs_render_sends(void *state, float *left, float *right, float *delay, float *reverb, int n);
#endif
