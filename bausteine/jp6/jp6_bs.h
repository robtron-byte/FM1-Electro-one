/* SPDX-License-Identifier: GPL-3.0-only
 * jp6: der Jupiter-6 von FM1-Electro als Baustein (docs/adr/0040, 0048). Der Klangkern ist engine/jp6.c (mit engine/dist.c) (eingebunden,
 * nicht kopiert); dieser Adapter stellt ihn über baustein.h bereit, dazu die Presets (eigene des Nutzers zuerst,
 * docs/adr/0051), das Tempo für die LFO-Synchronisation und die Sends für Delay/Hall der späteren Firmware.
 *
 * Abtastrate: der Kern rechnet fest mit EL_SAMPLE_RATE (44,1 kHz, sdk_include/fm1_block.h); init nimmt jede Rate an,
 * die Koeffizienten bleiben aber die von 44,1 kHz (jp6_bs_rate sagt, womit er rechnet). */
#ifndef JP6_BS_H
#define JP6_BS_H
#include "baustein.h"

/* Stimmen des Bausteins: voll wie der Jupiter-6 (6). Sparfassung für ein Gerät: -DJP6_BS_VOICES=4 o. ä. */
#ifndef JP6_BS_VOICES
#define JP6_BS_VOICES 6
#endif

extern const baustein_t jp6_baustein;

double jp6_bs_rate(void);
/* Presets: zuerst die eigenen des Nutzers (jp6_eigene.c, aus den iPhone-JSON erzeugt), dann die Werks-Sounds aus
 * engine/jp6_presets.c (74 Sounds der JP-6-App). */
unsigned jp6_bs_preset_count(void);
unsigned jp6_bs_eigene_count(void);
const char *jp6_bs_preset_name(unsigned i);
const unsigned char *jp6_bs_preset_values(unsigned i);   /* JP_COUNT Rohwerte (engine/jp6.h) */
void jp6_bs_load_preset(void *state, unsigned i);
/* Ein Parameter als Rohwert 0..jp6_param_max (wie der Kern ihn speichert). */
void jp6_bs_set_raw(void *state, unsigned param, int value);
int jp6_bs_get_raw(const void *state, unsigned param);
void jp6_bs_set_bpm(void *state, double bpm);   /* für LFO SYNC, Standard 120 */
/* Wie render, zusätzlich die Sends (Delay, Hall) addiert, die der Kern je Preset ausgibt (JP_DELAY_SEND, JP_REVERB_SEND). */
void jp6_bs_render_sends(void *state, float *left, float *right, float *delay, float *reverb, int n);
#endif
