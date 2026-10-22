# FM1-Electro-one

Eigene Firmware für den **M-VAVE FM-1**, aufgebaut auf dem offiziellen SDK [FM1 Open](https://github.com/m-vave/fm1_open). Ziel: die Klangerzeugung der iOS-Apps **MKS-50** und **JP-6** auf die Hardware bringen, dazu eine **808-Drum-Machine** als Electro-Begleitung, Arpeggiator, Step-Sequencer und Looper. Es läuft immer eine Synth-Engine (MKS-50 oder JP-6, umschaltbar) zusammen mit den Drums.

> Unabhängiges Projekt, nicht mit M-VAVE oder Roland verbunden. „MKS-50“, „Jupiter-6“ und „TR-808“ beschreiben nur die nachgebildete Architektur.

## Stand

| Teil | Status |
|---|---|
| Gemeinsame Clock (16tel, Swing, MIDI-Clock-Eingang) | fertig, getestet (Timing auf ±1 Sample) |
| 808-Drum-Machine: Klang, Sequencer, Patterns, Generator | fertig, getestet auf dem PC |
| Bedienung am FM-1 (Display, Tasten, LEDs, Encoder) | fertig, im Panel-Simulator getestet |
| Einbindung ins FM1-Open-Build (Audio, MIDI, Speichern im Flash) | fertig, **noch nicht auf dem Gerät gebaut** |
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

## Bedienung am FM-1

Das FM-1 hat 27 Notentasten und 14 Funktionstasten (alle mit LED), 7 Encoder, den MASTER-Regler und ein 240×240-Farbdisplay. Das Display zeigt das Pattern als Raster (11 Instrumente × 16 Steps) mit Lauflicht, Akzent-Reihe und Rolls, darunter die Klangwerte des gewählten Instruments.

| Bedienung | Funktion |
|---|---|
| Schwarze Taste | Instrument wählen und anhören (BD, SD, LT, MT, HT, RS, CP, CB, CY, OH, CH) |
| Weiße Taste | Step des gewählten Instruments setzen/löschen. LEDs zeigen die Steps und das Lauflicht |
| EDIT + weiße Taste | Akzent auf diesem Step |
| SCL + weiße Taste | 32tel-Roll auf diesem Step |
| EDIT + schwarze Taste | Instrument im Pattern löschen. EDIT + REC löscht das ganze Pattern |
| GLO + schwarze Taste | Mute. HOME + schwarze Taste: Solo |
| PLAY | Start/Stop (übernimmt die interne Clock) |
| REC | Live-Aufnahme an/aus: schwarze Tasten und MIDI-Drums werden quantisiert aufgenommen |
| OCT − / OCT + | Steps 1–16 / 17–32 bei langen Patterns |
| ENV | nächste Seite der Klangparameter |
| LFO halten | Groove-Generator: E3 Stil, E4 Dichte, E5 Kick behalten, weiße Taste erzeugt einen neuen Groove |
| SAVE | Patterns, Kit-Klang und Tempo im Flash speichern |
| E1 | Pattern wählen (wechselt am Taktende). GLO + E1: Kit |
| E2 | Tempo (GLO + E2: in 0,1-BPM-Schritten) |
| E3–E6 | Klangparameter des gewählten Instruments |
| E7 | Pattern-Länge. GLO + E7: Swing |
| MASTER | Gesamtlautstärke |

MIDI: Start/Stop/Continue und MIDI-Clock von außen (nach MIDI-Start folgt das Tempo der Clock), Drum-Noten auf Kanal 10.

Die Belegung der Encoder E1–E7 folgt der Nummerierung im FM1-Open-Code. Wo sie physisch sitzen, prüfe ich am ersten Geräte-Build.

## Bauen und testen (PC)

```sh
make demo      # rendert alle Grooves und Generator-Stile nach build/wav/
make screens   # Panel-Simulator: Screenshots mit LED-Zustand nach build/screens/
make test      # Timing-, MIDI-Clock- und Speichertests
```

## Firmware bauen (Windows)

FM1-Electro wird in eine Kopie von FM1 Open eingesetzt. Dabei kommen drei Einhängepunkte dazu: Audio, Panel und MIDI. Die M-VAVE-Bibliothek (Bluetooth, USB, Flash, Display) bleibt unverändert.

1. [FM1 Open](https://github.com/m-vave/fm1_open) klonen. Getestet ist Commit `4c7496d` (V14).
2. `python tools\prepare_fm1.py C:\pfad\zu\fm1_open` ausführen. Das Skript kopiert den Code nach `app\electro` und setzt die Einhängepunkte. Es bricht ab, wenn die FM1-Open-Version nicht passt.
3. Die JieLi-Toolchain 2.5.3 installieren (siehe README von FM1 Open) und im FM1-Open-Ordner `.\sdk\Build.ps1 -FirmwareOnly -Toolchain 'C:\JL\pi32'` ausführen.
4. Die erzeugte `.fwsc`-Datei mit der FM1-Workbench oder dem offiziellen Updater flashen. Zurück zur Original-Firmware V15 geht jederzeit über die Workbench.

Der Code kompiliert für einen 32-Bit-Testprozessor ohne Fehler. Auf dem Gerät gebaut und gemessen ist er noch nicht.

**Speicherplatz im Flash:** FM1-Electro speichert in zwei 4-KB-Sektoren hinter dem Bereich von FM1 Open. Ist dort kein Platz, nutzt es die Projekt-Slots 14 und 15 von FM1 Open. Die gehen beim Zurückwechseln zur Original-Firmware verloren.

## Speicher und Rechenzeit

Das FM-1 hat rund 512 KB internes RAM. Laut SDK-Bibliothek belegt die Systemsoftware davon vor dem Linken etwa 196 KB fest, mindestens 48 KB braucht der Laufzeit-Heap (Bluetooth, USB). FM1-Electro braucht derzeit rund 40 KB RAM (Code im RAM, Daten, Display-Puffer). Auf dem PC kostet ein dichter 808-Groove 2,6 % Echtzeit. Gemessen auf dem Gerät wird, sobald die Firmware gebaut ist.

## Lizenz

GPLv3 mit der Linking-Ausnahme von FM1 Open, siehe `LICENSE.md` und `THIRD_PARTY.md`.
