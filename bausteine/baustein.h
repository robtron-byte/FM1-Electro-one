/* SPDX-License-Identifier: GPL-3.0-only
 * Gemeinsame Schnittstelle aller Bausteine (docs/adr/0040). Jeder Baustein stellt ein baustein_t bereit, z. B.
 * `extern const baustein_t dk808_baustein;`. Adapter (FM1-Electro, spätere Geräte) und Varianten sprechen Bausteine nur
 * über diese Tabelle an; so lassen sie sich austauschen und kombinieren.
 *
 * Regeln:
 *  - Reines C99, nur <stdint.h>, <stddef.h>, <math.h>. Kein malloc im Audio-Pfad: der Speicher für den Zustand kommt vom
 *    Aufrufer (state_size Bytes, ausgerichtet auf 16).
 *  - Abtastrate frei (der Baustein rechnet seine Koeffizienten daraus).
 *  - Noten: MIDI-Nummer 0..127; Drums wählen das Instrument über die Note (Belegung im README des Bausteins).
 *  - Velocity 0..127 wie MIDI (der Baustein rechnet sie wie seine Vorlage um).
 *  - Parameter: Nummer laut Tabelle des Bausteins, Wert normiert 0..1 (Schalter: Stufe / (Stufen-1)).
 *  - render ADDIERT n Stereo-Samples auf left/right (so lassen sich Bausteine mischen); n beliebig.
 *  - Ereignisse gelten ab dem nächsten render-Aufruf; für samplegenaue Noten den Block an der Stelle teilen.
 */
#ifndef BAUSTEIN_H
#define BAUSTEIN_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;        /* Kurzname, z. B. "dk808" */
    const char *title;       /* Anzeige, z. B. "TR-808 (DrumKern)" */
    size_t state_size;       /* Bytes für den Zustand */
    unsigned param_count;
    const char *(*param_name)(unsigned param);
    void (*init)(void *state, double sample_rate);
    void (*note_on)(void *state, int note, int velocity);
    void (*note_off)(void *state, int note);
    void (*set_param)(void *state, unsigned param, double value);   /* 0..1 */
    void (*render)(void *state, float *left, float *right, int n);  /* addiert */
    int (*active)(const void *state);                               /* klingt noch etwas? */
} baustein_t;

#endif
