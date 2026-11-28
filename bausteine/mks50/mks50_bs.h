/* SPDX-License-Identifier: GPL-3.0-only
 * mks50: der MKS-50 / Alpha Juno von FM1-Electro als Baustein (docs/adr/0040, 0047). Der Klangkern ist engine/mks.c (eingebunden,
 * nicht kopiert); dieser Adapter stellt ihn über baustein.h bereit, dazu die Presets (eigene des Nutzers zuerst,
 * docs/adr/0051), das Tempo für die LFO-Synchronisation und die Sends für Delay/Hall der späteren Firmware.
 *
 * Abtastrate: der Kern rechnet fest mit EL_SAMPLE_RATE (44,1 kHz, sdk_include/fm1_block.h); init nimmt jede Rate an,
 * die Koeffizienten bleiben aber die von 44,1 kHz (mks50_bs_rate sagt, womit er rechnet). */
#ifndef MKS50_BS_H
#define MKS50_BS_H
#include "baustein.h"

/* Stimmen des Bausteins: voll wie der MKS-50 (6). Sparfassung für ein Gerät: -DMKS50_BS_VOICES=4 o. ä. */
#ifndef MKS50_BS_VOICES
#define MKS50_BS_VOICES 6
#endif

extern const baustein_t mks50_baustein;

double mks50_bs_rate(void);
/* Presets: zuerst die eigenen des Nutzers (mks50_eigene.c, aus den iPhone-JSON erzeugt), dann die Werks-Sounds aus
 * engine/mks_presets.c (56 Sounds der MKS-50-App). */
unsigned mks50_bs_preset_count(void);
unsigned mks50_bs_eigene_count(void);
const char *mks50_bs_preset_name(unsigned i);
const unsigned char *mks50_bs_preset_values(unsigned i);   /* MP_COUNT Rohwerte (engine/mks.h) */
void mks50_bs_load_preset(void *state, unsigned i);
/* Ein Parameter als Rohwert 0..mks_param_max (wie der Kern ihn speichert). */
void mks50_bs_set_raw(void *state, unsigned param, int value);
int mks50_bs_get_raw(const void *state, unsigned param);
void mks50_bs_set_bpm(void *state, double bpm);   /* für LFO SYNC, Standard 120 */
/* Wie render, zusätzlich die Sends (Delay, Hall) addiert, die der Kern je Preset ausgibt (MP_DELAY_SEND, MP_REVERB_SEND). */
void mks50_bs_render_sends(void *state, float *left, float *right, float *delay, float *reverb, int n);
#endif
