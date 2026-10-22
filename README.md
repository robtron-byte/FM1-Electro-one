# FM1-Electro-one

Eigene Firmware für den **M-VAVE FM-1**, aufgebaut auf dem offiziellen SDK [FM1 Open](https://github.com/m-vave/fm1_open). Ziel: die Klangerzeugung der iOS-Apps **MKS-50** und **JP-6** auf die Hardware bringen, dazu eine **808-Drum-Machine** als Electro-Begleitung, Arpeggiator, Step-Sequencer und Looper. Es läuft immer eine Synth-Engine (MKS-50 oder JP-6, umschaltbar) zusammen mit den Drums.

> Unabhängiges Projekt, nicht mit M-VAVE oder Roland verbunden. „MKS-50“, „Jupiter-6“ und „TR-808“ beschreiben nur die nachgebildete Architektur.

## Stand

| Teil | Status |
|---|---|
| Gemeinsame Clock (16tel, Swing, MIDI-Clock-Eingang) | fertig, getestet (Timing auf ±1 Sample) |
| 808-Drum-Machine: Klang, Sequencer, Patterns, Generator | fertig, getestet auf dem PC |
| Bedienung am FM-1 (Display, Tasten, Encoder) | als Nächstes |
| Einbindung ins FM1-Open-Build (Audio, MIDI, Speichern) | geplant |
| MKS-50-Engine, Arpeggiator, Step-Sequencer | geplant |
| JP-6-Engine | geplant |
| Noten-Looper, Delay und Reverb | geplant |

## 808-Drum-Machine

- **Klang:** das 808-Modell aus FM1 Open (DaisySP/Plaits, MIT) mit 11 Instrumenten: BD, SD, LT, MT, HT, RS, CP, CB, CY, OH, CH. Pro Instrument Level, Tune, Decay, Tone/Snappy, Drive mit 7 Verzerrungsarten, Pan, Reverb- und Delay-Send. Rimshot auch als Clave, Toms auch als Conga, Clap auch als Maracas, Hi-Hat-Choke.
- **4 Kits:** TR-808, ELECTRO (langer, gesättigter Kick), MIAMI (Sub-Kick), KRAFT (kurz, Claves, gecrushtes Metall).
- **16 Patterns** mit 1–32 Steps, 808-Akzent-Reihe, **32tel-Rolls** pro Step, Swing 50–75 %, Patternwechsel am Taktende oder sofort, Kopieren.
- **Live-Aufnahme:** Einspielen während das Pattern läuft, auf das nächste 16tel quantisiert (ohne Doppelanschlag). Mute und Solo pro Instrument.
- **14 Electro-Grooves ab Werk:** ELECTRO, BRONX 82, NUMBERS, ROTHER, MIAMI, BREAKER, FREESTYL, DROWNED, EBM, TRANS EUR, HALFTIME, CLAVES, BUILD UP (32 Steps mit Snare-Roll), SPACE. Dazu 2 leere Plätze.
- **Groove-Generator** in den Stilen ELECTRO, KRAFT, ROTHER, ROBOT und MIAMI, mit Dichte-Regler und Option „Kick behalten“. Bei 32 Steps variiert der zweite Takt den ersten.
- **MIDI:** General-MIDI-Drum-Noten (36 BD, 38 SD, 41/45/48 Toms, 37 RS, 39 CP, 56 CB, 49 CY, 46 OH, 42 CH).

## Geplante Bedienung am FM-1

Das FM-1 hat 27 Notentasten und 14 Funktionstasten (alle mit LED), 7 Encoder, einen Lautstärke-Regler und ein 240×240-Farbdisplay.

- **SEQ** wechselt zur Drum-Ansicht. Die 16 weißen Tasten sind die Steps, ihre LEDs zeigen gesetzte Schritte und das Lauflicht.
- Die 11 schwarzen Tasten wählen das Instrument (BD … CH). Ist ein Instrument gewählt, setzen die weißen Tasten seine Steps.
- **PLAY** startet und stoppt, **REC** schaltet die Live-Aufnahme. Dann spielen die schwarzen Tasten die Drums.
- Encoder: Pattern, Tempo, Swing und Länge sowie die Klangparameter des gewählten Instruments.
- Das Display zeigt das ganze Pattern als Raster (11 Reihen × Steps) mit Lauflicht, Akzenten und Rolls.

## Bauen und testen (PC)

```sh
make demo   # rendert alle Grooves und Generator-Stile nach build/wav/
make test   # Timing- und Pegeltests
```

Der Firmware-Build für das Gerät läuft unter Windows mit der JieLi-Toolchain 2.5.3 über das FM1-Open-SDK (siehe dort). Die Anleitung folgt, sobald die Geräteanbindung steht.

## Speicher und Rechenzeit

Das FM-1 hat rund 512 KB internes RAM. Laut SDK-Bibliothek belegt die Systemsoftware davon vor dem Linken etwa 196 KB fest, mindestens 48 KB braucht der Laufzeit-Heap (Bluetooth, USB). Die Drum-Machine braucht rund 6,5 KB Daten. Auf dem PC kostet ein dichter 808-Groove 2,6 % Echtzeit. Gemessen auf dem Gerät wird, sobald die Firmware gebaut ist.

## Lizenz

GPLv3 mit der Linking-Ausnahme von FM1 Open, siehe `LICENSE.md` und `THIRD_PARTY.md`.
