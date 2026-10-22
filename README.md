# FM1-Electro-one

Eigene Firmware für den **M-VAVE FM-1**, aufgebaut auf dem offiziellen SDK [FM1 Open](https://github.com/m-vave/fm1_open). Ziel: die Klangerzeugung der iOS-Apps **MKS-50** und **JP-6** auf die Hardware bringen, dazu eine **808-Drum-Machine** als Electro-Begleitung, Arpeggiator, Step-Sequencer und Looper. Es läuft immer eine Synth-Engine (MKS-50 oder JP-6, umschaltbar) zusammen mit den Drums.

> Unabhängiges Projekt, nicht mit M-VAVE oder Roland verbunden. „MKS-50“, „Jupiter-6“ und „TR-808“ beschreiben nur die nachgebildete Architektur.

## Branch `sound-808`: authentische 808 und fertiger Mix

Auf diesem Branch steckt die Arbeit am Sound:

- **TR-808, neu aus der Schaltung nachgebaut** (`engine/tr808.c`). Jede Stimme besteht wie im Original aus Resonatoren, Oszillatoren, Filtern und Hüllkurven. Angeregt werden sie vom 1-ms-Triggerimpuls, dessen Höhe der Akzent ist (4–14 V). Der Akzent verändert also auch den Klang, nicht nur die Lautstärke.
- **Grundstimmung wie beim Original:**

  | Instrument | Stimmung |
  |---|---|
  | Bassdrum | 50 Hz, mit Attack-Punch, Retrigger-Impuls, leichtem Tonhöhenabfall und Klick über TONE |
  | Snare | 173 + 336 Hz |
  | Toms | 90 / 135 / 185 Hz |
  | Congas | 185 / 280 / 400 Hz |
  | Rimshot | 455 + 1667 Hz |
  | Claves | 2,5 kHz |
  | Metall-Sounds | sechs Rechteck-Oszillatoren bei 205,3–800 Hz mit Bandpässen bei 3,44 und 7,1 kHz |
  | Clap | drei Sägezahn-Impulse plus Hallfahne |

  Die Pegel normal/Akzent folgen der Tabelle im Roland-Servicehandbuch. Alle Werte und Quellen stehen in [`docs/TR808.md`](docs/TR808.md).
- **Mischung im Stil von Anthony Rother** (`engine/mixbus.c`):
  - Die Kick läuft unkomprimiert am Drum-Bus vorbei.
  - Drum-Bus: Kompressor 2:1 plus Bandsättigung.
  - Die Kick duckt Bass und Flächen.
  - Hochpass und Präsenz auf den Synths.
  - Snare, Clap und Rimshot bekommen einen nichtlinearen Gated-Reverb (nach TC NonLin2), alles andere den Hall.
  - Summe nach Art des SPL Kultube: Kompressor 1,8:1 und Röhrensättigung, danach ein Limiter bei −1 dBFS.
  - Ergebnis: rund −9 bis −10 LUFS, die Lautheit eines fertigen Club-Tracks.
- **Soundcheck:**
  - `make soundcheck` rendert jedes Instrument einzeln (normal, Akzent, Tune- und Decay-Verlauf) nach `build/sc/` und misst Frequenz und Ausklingzeit.
  - `make demo` erzeugt unter anderem `ab_rother_raw.wav` und `ab_rother_mixed.wav` für den Vorher/Nachher-Vergleich der Mischkette.
  - `python3 host/loudness.py datei.wav` misst die Lautheit in LUFS.

## Variante „Vier Bereiche“ (Branch `bedienung-vier-bereiche`)

Das FM-1 wird zur Electro-Groovebox mit vier Bereichen, die zusammen auf einer Clock laufen und gemeinsam das Pattern wechseln:

| Bereich | Taste | Klang | Sequencer |
|---|---|---|---|
| **BEAT** | FX | 808-Drum-Machine (11 Instrumente, 4 Kits) | 16 Patterns, bis 32 Steps, Akzent, Rolls, Swing |
| **RHYTHM** | SCL | MKS-50-Engine, 3 Stimmen: Zaps, Stabs, Bleeps, S&H-Sequenzen, Perc (30 Sounds) | Noten-Steps mit Akzent und Tie |
| **STRINGS** | ENV | MKS-50-Engine, 4 Stimmen: JX-Strings, Pads, Brass (16 Sounds) | Akkord-Sequenzer für lange Sequenzen: Schrittlänge 1/16 bis 2 Takte (Standard 1 Takt), gehaltene Akkorde, pro Step Akkordtyp (m7, m9, MIN, 5+8, 4THS, 7SUS4, MAJ7, m7b5, MAJ, SUS2, ADD9, MAJ9, 7) und Umkehrung |
| **BASS** | LFO | MKS-50-Engine, 2 Stimmen (Mono/Unison, Glide): Robo, Acid, Sub, Wobble (10 Sounds) | Noten-Steps mit Akzent und Slide (Tie) |

Die Sounds sind die Factory-Sounds der MKS-50-App, automatisch übernommen (`tools/convert_presets.py`).

**Generierte Grooves statt fester Patterns.** 24 Pattern-Plätze in zwei Bänken. Die Plätze 1–16 sind Stil-Plätze: Bei jedem Einschalten erzeugt der Generator dort neue Grooves, jeder Platz in seinem Stil (`engine/drum_factory.c`): ELECTRO, BRONX 82, NUMBERS, TRANS EUR, TOUR, COMPUTER, ROTHER, MIAMI, MIAMI 2, BOOM BASS, DROWNED, ROBOT, PERFECT, EGYPT, FREAKY, LOVE. Jeder Stil bringt Tempo, Swing, Kit, Länge, typische Varianten pro Instrument und eine Akkordfamilie mit. Der Generator wählt daraus und variiert leicht (Geister-Kick, versetzte Hat, Open-Hat-Auftakt); Downbeats, Backbeat und die Figur des Stils bleiben. Zu jedem Beat entstehen passende RHYTHM-, STRINGS- und BASS-Spuren, alles in einer gemeinsamen Moll-Tonart (C, D, A, E, G, F, B♭ oder E♭). **HOME + ARP + weiße Taste** erzeugt für diesen Platz sofort einen neuen Groove. Die Plätze 17–24 sind User-Plätze: **SAVE und danach weiße Taste 1–8** legt den laufenden Groove (Beat und alle drei Synth-Spuren) dort ab und speichert ihn mit allen Einstellungen im Flash. SAVE zweimal speichert nur die Einstellungen. Gespeichert werden nur die User-Plätze, Klänge, Pegel, Effekte, Tempo und Kit, nicht die Stil-Plätze.

**Akkordfolgen im Stil von Anthony Rother und Kraftwerk.** Die Streicher spielen Vier-Akkord-Schleifen mit einem oder zwei Takten pro Akkord (PADS: ein Step pro Takt, SLOW: ein Step pro zwei Takte, PULSE: Achtel bzw. Viertel, STABS: 16tel), mit automatischer Stimmführung über die Umkehrungen. Jeder Stil wählt eine von drei Familien mit je acht Schleifen (`engine/synth_track.c`):

- **ROTHER** (ELECTRO, ROTHER, DROWNED): dunkles Moll, lange Akkorde über dem Grundton, phrygische ♭II, Sus- und Quarten-Klänge, fallende parallele Quarten, Quint-Oktav-Drones.
- **KRAFT** (NUMBERS, TRANS EUR, TOUR, COMPUTER, ROBOT, LOVE): schlichte Dreiklänge, Zwei-Akkord-Pendel (i–v wie „The Model“), Moll mit Sus2 und ein Maj7-Zwischenspiel wie in „Computer Love“, Moll-♭VI wie bei „The Robots“, Mediant-Rückungen wie „Europe Endless“.
- **ELECTRO** (BRONX 82, MIAMI, MIAMI 2, BOOM BASS, PERFECT, EGYPT, FREAKY): Detroit- und Miami-Moll-Schleifen mit m7, m9, Maj7 und 7sus4.

Grooves und Akkordfolgen sind Rekonstruktionen aus den typischen Merkmalen und aus Akkord-Sheets der Community, keine Transkriptionen der Platten. Der Bass bleibt als Ostinato auf dem Grundton, die Akkorde bewegen sich darüber. Dahinter liegen ein gemeinsames Delay (tempo-synchron, Ping-Pong) und ein Hall. Jeder Sound bringt seine eigenen Delay- und Hall-Anteile mit.

### Bedienung

**Überall**

| Bedienung | Funktion |
|---|---|
| FX / SCL / ENV / LFO | Bereich BEAT / RHYTHM / STRINGS / BASS. Nochmal drücken: nächste Klangseite |
| GLO + Bereichstaste | Bereich stumm / wieder an |
| HOME | Übersicht: alle vier Spuren, Pegel, Mute. Weiße Tasten wählen das Pattern, ARP + weiße Taste erzeugt dort einen neuen Groove, OCT − / + schaltet die Bank (Stile 1–16 / User 17–24), schwarze Tasten 1–4 schalten die Bereiche stumm. E3–E6 Pegel, GLO + E3–E6 Delay-Zeit, Feedback, Hallgröße, Hallzeit; E7 Hall-Anteil (GLO: Delay-Anteil) |
| PLAY | Start/Stop |
| REC | Aufnahme in den aktuellen Bereich. EDIT + REC löscht dessen Pattern |
| ARP halten | Generator für den aktuellen Bereich: E3 Stil, E4 Dichte, weiße Taste erzeugt eine neue Spur |
| SAVE | danach weiße Taste 1–8: laufenden Groove in User-Platz 17–24 und alle Einstellungen speichern. SAVE nochmal: nur Einstellungen |
| E1 / E2 | Pattern (alle Bereiche, wechselt am Taktende) / Tempo (GLO: fein) |

**BEAT:** schwarze Taste wählt und spielt ein Instrument, weiße Tasten setzen Steps. EDIT + weiß Akzent, GLO + weiß 32tel-Roll, EDIT + schwarz Instrument löschen, GLO + schwarz Instrument stumm. E3–E6 Klang, E7 Länge (GLO: Swing), GLO + E1 Kit.

**RHYTHM, STRINGS, BASS** haben zwei Modi, SEQ schaltet um:

| | KEYS (SEQ aus) | STEP (SEQ an) |
|---|---|---|
| Tasten | alle 27 Tasten spielen den Sound, die LEDs zeigen, was die Sequenz gerade spielt | weiße Tasten schalten Steps an/aus und wählen sie aus. EDIT + weiß Akzent, GLO + weiß Tie |
| OCT − / + | Oktave | Steps 1–16 / 17–32 |
| E3–E6 | Klangseite: FILTER, ENVELOPE, OSC, MOD, FX | Note, Oktave, Akkord (STRINGS) bzw. Akzent, Tie des gewählten Steps |
| REC | läuft: live einspielen (quantisiert). Steht: Noten Schritt für Schritt eingeben, EDIT + Taste = Pause | – |
| E7 | Sound wählen (GLO: Pattern-Länge) | Sound wählen |
| GLO + E1 | Notenlänge (Gate) | Notenlänge |
| STRINGS | GLO + weiße Taste 1–14 wählt den Akkord fürs Live-Spiel (1 = Einzelnoten). GLO + E1: Schrittlänge 1/16 … 2 Takte | E5 Akkordtyp, E6 Umkehrung, GLO + weiß: halten/neu anschlagen |

Die Encoder-Nummern E1–E7 folgen der Nummerierung im FM1-Open-Code. Wo sie physisch sitzen, prüfe ich am ersten Geräte-Build. Die Bereichstasten FX/SCL/ENV/LFO tragen am Gerät ihre alten Namen. Das Display zeigt die Bereiche als Reiter.

### Stand

| Teil | Status |
|---|---|
| Clock, 808-Drum-Machine, Bedienung, Flash-Speicher | fertig, auf dem PC getestet |
| MKS-50-Klangkern in C (DCO, HPF, 24-dB-Filter, Hüllkurve, LFO, Chorus, Mono/Unison/Glide) | fertig, auf dem PC getestet |
| Drei Synth-Bereiche mit Step-Sequencer, Akkorden und Generator | fertig, auf dem PC getestet |
| Delay und Hall | fertig, sparsame Variante (siehe unten) |
| Firmware auf dem Gerät | **noch nicht gebaut** |
| JP-6-Engine, Noten-Looper | geplant |

## Bauen und testen (PC)

```sh
make demo      # rendert alle Grooves (Beat allein und mit allen vier Bereichen) nach build/wav/
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

**Speicherplatz im Flash:** FM1-Electro speichert die User-Plätze und Einstellungen (3,5 KB) in zwei Kopien zu je einem 4-KB-Sektor hinter dem Bereich von FM1 Open. Ist dort kein Platz, nutzt es die Projekt-Slots 13–14 von FM1 Open. Die gehen beim Zurückwechseln zur Original-Firmware verloren.

## Speicher und Rechenzeit

Das FM-1 hat rund 512 KB internes RAM. Laut SDK-Bibliothek belegt die Systemsoftware davon vor dem Linken etwa 196 KB fest, mindestens 48 KB braucht der Laufzeit-Heap (Bluetooth, USB). Mit der neuen 808 und der Mischkette braucht dieser Branch geschätzt **rund 210 KB** (156 KB Daten plus Code im RAM): etwa 135 KB Daten (davon 90 KB Delay und Hall), dazu rund 50 KB Code im RAM. Das ist knapp. Falls es auf dem Gerät nicht passt, gibt es drei Stellschrauben: Delay-Länge (`FX_DELAY_CAP`), Hall-Leitungen (`FX_REVERB_CAP`) und Stimmenzahl pro Bereich (`electro.c`).

Auf dem PC kosten alle vier Bereiche mit Mischkette zusammen 5,9 % Echtzeit. Wie viel davon auf dem 240-MHz-Chip übrig bleibt, zeigt erst die Messung am Gerät. Für Delay und Hall habe ich sparsamere Versionen als in den iOS-Apps gebaut: ein Ping-Pong-Delay mit halber Abtastrate und 16 Bit (bis 0,6 s, klingt etwas dunkler, eher wie Band oder Eimerkette) und einen Hall mit 4 statt 8 Leitungen.

## Lizenz

GPLv3 mit der Linking-Ausnahme von FM1 Open, siehe `LICENSE.md` und `THIRD_PARTY.md`.
