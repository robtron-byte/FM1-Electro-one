/* SPDX-License-Identifier: GPL-3.0-only
 * mksultra: Alpha Juno / MKS-50 „Ultra“ (TAL-Pha-Nachbau „MKS Ultra“). Vorlage: Dropbox
 * `Programmieren/Musik/MKS50 App/TAL Pha Nachbau/app/engine/src/` (alle .rs) (Stand 28.11.2026), Zeile für Zeile nach C
 * übertragen, mit denselben Zahlentypen (f32/f64) und derselben Rechenreihenfolge. Siehe README.md.
 *
 * Bauschalter:
 *   MKSULTRA_VOICES   Stimmenzahl (Standard 12 wie die Vorlage; Sparfassung z. B. 4 oder 6). Die Zufallswerte der
 *                     weggelassenen Stimmen werden trotzdem gezogen, damit die übrigen Stimmen bitgleich bleiben.
 *   MKSULTRA_MAX_SR   größte Abtastrate (Standard 48000); bestimmt die Größe der Effekt-Puffer im Zustand.
 *
 * Der Zustand enthält Zeiger in sich selbst (Effekt-Puffer): nach init nicht mehr verschieben.
 */
#ifndef MKSULTRA_H
#define MKSULTRA_H
#include "baustein.h"

#ifndef MKSULTRA_VOICES
#define MKSULTRA_VOICES 12
#endif
#ifndef MKSULTRA_MAX_SR
#define MKSULTRA_MAX_SR 48000
#endif

/* MKSULTRA_FM1 1: Rechenwege fürs FM-1, die nicht bitgleich, aber gleich klingend sind (LFO-Phase in Festkomma statt
 * double). Standard 0 = wie die Vorlage. */
#ifndef MKSULTRA_FM1
#define MKSULTRA_FM1 0
#endif

/* Speicher der Effekte (fürs FM-1; Standard wie die Vorlage), Einzelheiten in mksultra_fx.h:
 *   MKSULTRA_DELAY_TENTHS 40, MKSULTRA_PREDELAY_TENTHS 10, MKSULTRA_FX_INT16 0 */
#ifndef MKSULTRA_DELAY_TENTHS
#define MKSULTRA_DELAY_TENTHS 40
#endif
#ifndef MKSULTRA_PREDELAY_TENTHS
#define MKSULTRA_PREDELAY_TENTHS 10
#endif
#ifndef MKSULTRA_FX_INT16
#define MKSULTRA_FX_INT16 0
#endif

#define MKSULTRA_PARAMS 85

extern const baustein_t mksultra_baustein;

/* Vollständige Schnittstelle (wie `Synth` der Vorlage). `s` zeigt auf mksultra_baustein.state_size Bytes. */
void mksultra_init(void *s, float sample_rate);
void mksultra_note_on(void *s, int note, int velocity);   /* velocity 0 = Note aus */
void mksultra_note_off(void *s, int note);
void mksultra_pitch_bend(void *s, float v);               /* -1..1 */
void mksultra_mod_wheel(void *s, float v);                /* 0..1 */
void mksultra_aftertouch(void *s, float v);               /* 0..1 */
void mksultra_sustain(void *s, int on);
void mksultra_all_notes_off(void *s);
void mksultra_set_tempo(void *s, float bpm);
/* Parameter in natürlichen Einheiten der Vorlage (Bereich laut mksultra_param_range), Nummer = Reihenfolge PARAMS. */
void mksultra_set_param(void *s, int id, float value);
float mksultra_get_param(const void *s, int id);
const char *mksultra_param_id(int id);
void mksultra_param_range(int id, float *min, float *max, float *def, int *is_int);
int mksultra_param_find(const char *id);
/* SCHREIBT n Stereo-Samples (die Baustein-Schnittstelle addiert dagegen). */
void mksultra_process(void *s, float *left, float *right, int n);
int mksultra_active_voices(const void *s);

/* Presets (520 aus der Vorlage, Tabelle in mksultra_presets.c, erzeugt von tools/mks2c.py). */
typedef struct {
    float cutoff, detune, attack, decay, release, pan;
} mksultra_vtune_t;

typedef struct {
    const char *path;         /* relativ zu presets/, z. B. "Factory Bank A/000 PolySynth1.mks" */
    const char *name;
    int version;
    unsigned have[3];         /* Bit k gesetzt: Parameter k steht in der Datei */
    float v[MKSULTRA_PARAMS]; /* Wert aus der Datei (natürliche Einheiten) */
    int ntuning;              /* Einträge der Stimmungstabelle (0 = gleichstufig) */
    const float *tuning;
    int nchord;               /* 0 = kein <Chord>-Element */
    signed char chord[6];
    int nvt;
    mksultra_vtune_t vt[6];
} mksultra_preset_t;

extern const mksultra_preset_t mksultra_presets[];
extern const int mksultra_preset_count;
void mksultra_load_preset(void *s, const mksultra_preset_t *p);
int mksultra_preset_find(const char *path);   /* -1 = nicht gefunden */

#endif
