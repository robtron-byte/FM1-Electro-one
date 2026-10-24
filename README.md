# FM1-Electro-one

Eigene Firmware für den **M-VAVE FM-1**, aufgebaut auf dem offiziellen SDK [FM1 Open](https://github.com/m-vave/fm1_open). Ziel: die Klangerzeugung der iOS-Apps **MKS-50** und **JP-6** auf die Hardware bringen, dazu eine **808-Drum-Machine** als Electro-Begleitung, Arpeggiator und Step-Sequencer. Es läuft immer eine Synth-Engine (MKS-50 oder JP-6, umschaltbar) zusammen mit den Drums.

> Unabhängiges Projekt, nicht mit M-VAVE oder Roland verbunden. „MKS-50“, „Jupiter-6“ und „TR-808“ beschreiben nur die nachgebildete Architektur.

## Branch `sound-808`: authentische 808 und fertiger Mix

Auf diesem Branch steckt die Arbeit am Sound:

- **TR-808, neu aus der Schaltung nachgebaut** (`engine/tr808.c`). Jede Stimme besteht wie im Original aus Resonatoren, Oszillatoren, Filtern und Hüllkurven. Angeregt werden sie vom 1-ms-Triggerimpuls, dessen Höhe der Akzent ist (4–14 V). Der Akzent verändert also auch den Klang, nicht nur die Lautstärke.
- **Grundstimmung wie beim Original:**

  | Instrument | Stimmung |
  |---|---|
  | Bassdrum | 50 Hz, mit Attack-Punch, Retrigger-Impuls, leichtem Tonhöhenabfall und Klick über TONE |
  | Snare | 173 + 336 Hz |
  | Toms | 93 / 185 Hz (Sinus, sackt in 43 ms um 28 % ab, nach tachyonbeats) |
  | Congas | 185 / 400 Hz |
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
| **BEAT** | FX | 808-Drum-Machine (10 Instrumente + ZAP + PULSE, 5 Kits inkl. PO-12; Toms/Congas tief und hoch) | 16 Patterns, bis 32 Steps, Akzent, Rolls, Swing |
| **RHYTHM** | SEL | MKS-50 oder JP-6, 3 Stimmen: Zaps, Stabs, Bleeps, Sequenzen, Leads, Perc (30 + 44 Sounds) | Noten-Steps mit Akzent und Tie |
| **STRINGS** | ENV | MKS-50 oder JP-6, 4 Stimmen: Strings, Pads, Brass (16 + 20 Sounds) | Akkord-Sequenzer für lange Sequenzen: Schrittlänge 1/16 bis 2 Takte (Standard 1 Takt), gehaltene Akkorde, pro Step Akkordtyp (m7, m9, MIN, 5+8, 4THS, 7SUS4, MAJ7, m7b5, MAJ, SUS2, ADD9, MAJ9, 7) und Umkehrung |
| **BASS** | LFO | MKS-50 oder JP-6, 2 Stimmen (Mono/Unison, Glide): Robo, Acid, Sub, Wobble (10 + 10 Sounds) | Noten-Steps mit Akzent und Slide (Tie) |

**PO-12-Kit.** Als Alternative zur 808 gibt es das Kit **PO-12** (PRESETS im BEAT-Bereich, oder Kit-Parameter ENGINE): die 16 Sounds des Teenage Engineering PO-12 rhythm, nachsynthetisiert (keine Samples) mit einer allgemeinen digitalen Drum-Stimme (`engine/po12.c`: Oszillator mit Tonhöhen-Hüllkurve und FM, gefiltertes Rauschen, Clap-Bursts, Gate, Sättigung, Lo-Fi-Abtastung). Jeder Sound ist an eine Aufnahme des echten Geräts in Grundeinstellung angepasst (Hüllkurve, Spektrum früh und spät, Tonhöhenverlauf; im Mittel 2–7 dB Abweichung im Spektrum), die Lautstärken folgen dem Verhältnis der Aufnahme. Belegung: BD Bassdrum, SD Snare (SOUND: Synth-Snare), LT Low Tom (SOUND: Bass Tone), HT Hi Tom (SOUND: Tone), RS Sticks (SOUND: Click), CP Clap (SOUND: Noise), CB Cowbell, CY Cymbal, OH/CH Hats, ZP Blip; PU bleibt der 808-Trigger. TUNE und DECAY wirken wie A und B am PO-12 (Tonhöhe ×½…×2, Länge ×¼…×4); wie die echten Regler genau wirken, ist noch nicht vermessen, dafür bräuchte es Aufnahmen mit verschiedenen A/B-Stellungen. Nebenbei hat die 808-Snare jetzt einen SOUND-Schalter: späte (172/339 Hz) oder frühe Geräte (238/476 Hz).

**ZAP und PULSE.** Zwei Stimmen, die die 808 nicht hat, die im Electro aber dazugehören. **ZAP** ist der Laser-Zap: ein Sinus, der exponentiell nach unten fällt; TUNE setzt die Starthöhe (300 Hz bis 5 kHz), SWEEP die Fallzeit (5 bis 400 ms), DEPTH wie tief er fällt (bis 7 Oktaven), DECAY die Länge, TONE fährt ihn von Sinus bis zum kratzigen Rechteck; Akzent macht ihn schärfer. **PULSE** ist der 808-Trigger-Ausgang als Klang, wie bei Egyptian Lover: Bei ihm landete der TRIGGER OUT zufällig direkt im Mischpult, er hat ihn per EQ angepasst und als Klick programmiert. Das Modell ist an eine Aufnahme eines echten 808-Triggers angepasst (Wellenform-Restfehler −8,5 dB, Spektrum im Mittel 4,5 dB genau): ein 15,2-ms-Impuls mit weicher Anstiegs- und harter Abfallflanke und einem kurzen Glitch davor, durch die Eingangskopplung und drei Hochpässe (341 Hz, 97 Hz und 92 Hz, die beiden tiefen resonant). Das ergibt eine weiche positive Spitze und 15 ms später einen doppelt so lauten negativen Knack. Den tiefen „Plopp“ der Aufnahme (Nachschwingen um 100 Hz) nimmt ein steiler Hochpass bei 250 Hz heraus, damit er sich nicht mit dem Bass beißt. WIDTH 3,8 bis 60 ms (Abstand der beiden Spitzen), TUNE verschiebt den Hochpass (/4 bis ×4, ganz links kommt der Plopp zurück), RING gibt ihm Resonanz, DECAY die Länge der Spitzen. Der Trigger-Ausgang hängt nicht vom Akzent ab, PULSE darum kaum. Der Generator setzt PULSE vor allem in EGYPT, FREAKY, LOVE und den Miami-Stilen, ZAP gelegentlich in den Electro-Stilen. MIDI: ZAP Note 67, PULSE Note 75.

**Verzerrung als Send-Effekt.** Wie Delay und Hall gibt es eine gemeinsame Verzerrung (die der Apps: OD, DIST, RAT, TUBE, 2-fach überabgetastet), die nur einmal läuft, in Mono. Jeder Bereich hat einen eigenen Send-Regler: **HOME + EDIT halten**, dann KNOB 1–4 = Send von BEAT, RHYTHM, STRINGS und BASS, PRESETS = Typ (OFF, OD, DIST, RAT, TUBE), ALGORITHM = Stärke (DRIVE), SELECT = Klang (TONE). Das Original bleibt im Mix, die Verzerrung kommt dazu (Parallel-Verzerrung), ihr Rücklauf liegt auf dem Drum-Bus. Stehen alle Sends auf 0, rechnet sie nicht mit; offen kostet sie auf dem PC rund 0,4 % Rechenzeit. Die Einstellungen werden mit gespeichert. Die JP-6-Sounds behalten zusätzlich ihre eigene Verzerrung aus der App (nur dort, wo ein Preset sie nutzt).

**Zwei Synth-Engines, pro Bereich wählbar, multitimbral.** Jeder Synth-Bereich spielt entweder mit der MKS-50-Engine oder mit der JP-6-Engine (`engine/jp6.c`, aus der JP-6-App portiert: zwei VCOs mit Cross-Mod und Sync, HPF, 24-dB-VCF mit LPF/BPF/HPF, zwei Hüllkurven, Chorus und die Verzerrung der App). Die Engine kommt mit dem Sound: PRESETS blättert erst durch die MKS-50-Sounds, dann durch die JP-6-Sounds des Bereichs, EDIT + PRESETS springt direkt zur anderen Engine. So laufen z. B. JP-6-Strings, MKS-50-Bass und JP-6-Sequenz gleichzeitig. Beide Engines teilen sich den Speicher eines Bereichs, die Wahl kostet also keinen zusätzlichen RAM; eine JP-6-Stimme braucht auf dem PC etwa 30–40 % weniger Rechenzeit als eine MKS-50-Stimme. Engine und Sound werden pro Bereich gespeichert. Über MIDI spielen Kanal 1, 2 und 3 die Bereiche RHYTHM, STRINGS und BASS (Kanal 10 die Drums). Die Sounds sind die Factory-Sounds beider Apps, automatisch übernommen (`tools/convert_presets.py`, `tools/convert_jp6_presets.py`).

**Generierte Grooves statt fester Patterns.** 24 Pattern-Plätze in zwei Bänken. Die Plätze 1–16 sind Stil-Plätze: Dort liegt je ein generierter Groove in seinem Stil (`engine/drum_factory.c`): ELECTRO, BRONX 82, NUMBERS, TRANS EUR, TOUR, COMPUTER, ROTHER, MIAMI, MIAMI 2, BOOM BASS, DROWNED, ROBOT, PERFECT, EGYPT, FREAKY, LOVE. Jeder Stil bringt Tempo, Swing, Kit, Länge, typische Varianten pro Instrument und eine Akkordfamilie mit. Der Generator wählt daraus und variiert leicht (Geister-Kick, versetzte Hat, Open-Hat-Auftakt); Downbeats, Backbeat und die Figur des Stils bleiben. Zu jedem Beat entstehen passende RHYTHM-, STRINGS- und BASS-Spuren, alles in einer gemeinsamen Moll-Tonart (C, D, A, E, G, F, B♭ oder E♭). **HOME + ARP + weiße Taste** erzeugt für diesen Platz einen neuen Groove. Neu generiert wird nur auf diesen Befehl hin, nie beim Einschalten: Neue Grooves (auch einzelne neue Spuren aus dem Generator) werden zwei Sekunden nach dem letzten Generator-Tastendruck von selbst im Flash abgelegt und sind nach einem Neustart wieder da. Die Plätze 17–24 sind User-Plätze: **SAVE und danach weiße Taste 1–8** legt den laufenden Groove (Beat und alle drei Synth-Spuren) dort ab und speichert ihn mit allen Einstellungen im Flash. SAVE zweimal speichert alles ohne Kopie in einen User-Platz. Im Flash liegen alle 24 Plätze mit Klängen, Pegeln, Effekten, Tempo und Kit.

**Akkordfolgen im Stil von Anthony Rother und Kraftwerk.** Die Streicher spielen Vier-Akkord-Schleifen mit einem oder zwei Takten pro Akkord (PADS: ein Step pro Takt, SLOW: ein Step pro zwei Takte, PULSE: Achtel bzw. Viertel, STABS: 16tel), mit automatischer Stimmführung über die Umkehrungen. Jeder Stil wählt eine von drei Familien mit je acht Schleifen (`engine/synth_track.c`):

- **ROTHER** (ELECTRO, ROTHER, DROWNED): dunkles Moll, lange Akkorde über dem Grundton, phrygische ♭II, Sus- und Quarten-Klänge, fallende parallele Quarten, Quint-Oktav-Drones.
- **KRAFT** (NUMBERS, TRANS EUR, TOUR, COMPUTER, ROBOT, LOVE): schlichte Dreiklänge, Zwei-Akkord-Pendel (i–v wie „The Model“), Moll mit Sus2 und ein Maj7-Zwischenspiel wie in „Computer Love“, Moll-♭VI wie bei „The Robots“, Mediant-Rückungen wie „Europe Endless“.
- **ELECTRO** (BRONX 82, MIAMI, MIAMI 2, BOOM BASS, PERFECT, EGYPT, FREAKY): Detroit- und Miami-Moll-Schleifen mit m7, m9, Maj7 und 7sus4.

**Arpeggiator.** Jeder Synth-Bereich hat einen Arpeggiator, aus den iOS-Apps übernommen (`engine/arp.c`): Er sammelt die gehaltenen Tasten, bei STRINGS samt Live-Akkord, und spielt sie im Takt als UP, DOWN, UP/DN, RAND oder ORDER (Spielreihenfolge) über 1–4 Oktaven, mit Rate 1/32 bis 1/1 (auch Triolen und punktiert), Gate und HOLD. Die erste Note kommt sofort, die weiteren auf dem Raster. Läuft der Sequencer nicht, spielt der Arpeggiator im eingestellten Tempo weiter. Mit REC landen seine Noten im Pattern. Die Einstellungen werden pro Bereich gespeichert.

Ein Noten-Looper (wie der Loop-Recorder der Apps, aber mit Noten statt Audio) ist fertig gebaut und getestet, aber vorerst herausgenommen, um Bedienung und RAM schlank zu halten. Er liegt auf dem Branch `looper` und lässt sich bei Bedarf wieder einbauen.

Grooves und Akkordfolgen sind Rekonstruktionen aus den typischen Merkmalen und aus Akkord-Sheets der Community, keine Transkriptionen der Platten. Der Bass bleibt als Ostinato auf dem Grundton, die Akkorde bewegen sich darüber. Dahinter liegen ein gemeinsames Delay (tempo-synchron, Ping-Pong) und ein Hall. Jeder Sound bringt seine eigenen Delay- und Hall-Anteile mit.

### Bedienung

Die Belegung folgt dem Bedienfeld des FM-1 (Lage laut der Workbench von FM1 Open, bestätigt durch ein Foto des Geräts):

```
MASTER   SELECT              Display        KNOB 1  KNOB 2  KNOB 3  KNOB 4
PRESETS  ALGORITHM                          FX   SEL  ENV  LFO  EDIT GLO
OCT −    OCT +                              HOME SAVE ARP  SEQ  PLAY/STOP REC
27 Tasten in zwei Reihen: 11 schwarze (3-2-3-2-1) über 16 weißen
```

**Links die Struktur, rechts der Klang.** SELECT wählt das Pattern, ALGORITHM das Tempo (GLO: Pattern-Länge, EDIT: Swing bzw. Noten- oder Schrittlänge), PRESETS den Sound eines Bereichs, in BEAT das Kit. KNOB 1–4 rechts stellen immer den Klang ein. In HOME sind es die Pegel (bzw. Verzerrungs-Sends) der vier Bereiche, deren Tasten FX, SEL, ENV und LFO direkt darunter liegen. EDIT und GLO sind die beiden Umschalter.

**Überall**

| Bedienung | Funktion |
|---|---|
| FX / SEL / ENV / LFO | Bereich BEAT / RHYTHM / STRINGS / BASS. Nochmal drücken: nächste Klangseite |
| GLO + Bereichstaste | Bereich stumm / wieder an |
| HOME | Übersicht: alle vier Spuren, Pegel, Mute. Weiße Tasten wählen das Pattern, ARP + weiße Taste erzeugt dort einen neuen Groove, OCT − / + schaltet die Bank (Stile 1–16 / User 17–24), schwarze Tasten 1–4 schalten die Bereiche stumm. KNOB 1–4 Pegel, GLO + KNOB 1–4 Delay-Zeit, Feedback, Hallgröße, Hallzeit; PRESETS Hall-Anteil (GLO: Delay-Anteil), GLO + ALGORITHM Tempo fein |
| HOME + EDIT | Verzerrung: KNOB 1–4 Send von BEAT, RHYTHM, STRINGS, BASS; PRESETS Typ (OD, DIST, RAT, TUBE), ALGORITHM Drive, SELECT Ton |
| PLAY | Start/Stop |
| REC | Aufnahme in den aktuellen Bereich. EDIT + REC löscht dessen Pattern |
| ARP halten | Generator für den aktuellen Bereich: KNOB 1 Stil, KNOB 2 Dichte, weiße Taste erzeugt eine neue Spur |
| ARP antippen | RHYTHM / STRINGS / BASS: Arpeggiator an/aus, die Tasten spielen dann den Arpeggiator. EDIT + ARP: HOLD |
| SAVE | danach weiße Taste 1–8: laufenden Groove in User-Platz 17–24 und alle Einstellungen speichern. SAVE nochmal: alles speichern ohne Kopie |
| SELECT | Pattern (alle Bereiche, wechselt am Taktende) |
| ALGORITHM | Tempo. GLO + ALGORITHM: Länge des Patterns im aktuellen Bereich. EDIT + ALGORITHM: Swing (BEAT), Notenlänge (RHYTHM, BASS), Schrittlänge 1/16 … 2 Takte (STRINGS) |
| PRESETS | Sound des Bereichs (EDIT: andere Engine), in BEAT das Kit (808, ELECTRO, MIAMI, KRAFT, PO-12) |

**BEAT:** schwarze Taste wählt und spielt ein Instrument (BD SD LT HT RS CP CB CY OH CH ZAP; SEQ schaltet auf die zweite Ebene mit PULSE auf schwarzer Taste 1), weiße Tasten setzen Steps. EDIT + weiß Akzent, GLO + weiß 32tel-Roll, EDIT + schwarz Instrument löschen, GLO + schwarz Instrument stumm. KNOB 1–4 Klang des Instruments, GLO + SELECT wählt das Instrument ohne es zu spielen (auch PULSE).

**RHYTHM, STRINGS, BASS** haben zwei Modi, SEQ schaltet um:

| | KEYS (SEQ aus) | STEP (SEQ an) |
|---|---|---|
| Tasten | alle 27 Tasten spielen den Sound, die LEDs zeigen, was die Sequenz gerade spielt | weiße Tasten schalten Steps an/aus und wählen sie aus. EDIT + weiß Akzent, GLO + weiß Tie |
| OCT − / + | Oktave | Steps 1–16 / 17–32, GLO + SELECT wählt den Step |
| KNOB 1–4 | Klangseite: FILTER, ENVELOPE, OSC, MOD, FX (je nach Engine), dann ARP (Modus, Rate, Oktaven, Gate) | Note, Oktave, Akkord (STRINGS) bzw. Akzent, Tie des gewählten Steps |
| REC | läuft: live einspielen (quantisiert). Steht: Noten Schritt für Schritt eingeben, EDIT + Taste = Pause | – |
| STRINGS | GLO + weiße Taste 1–14 wählt den Akkord fürs Live-Spiel (1 = Einzelnoten) | KNOB 3 Akkordtyp, KNOB 4 Umkehrung, GLO + weiß: halten/neu anschlagen |

Die Lage der Encoder stammt aus der Workbench von FM1 Open (`sdk/Simulator.js`) und deckt sich mit dem FM-1-Emulator von Joakim Eriksson und der Verwendung in FM1 Open selbst (SELECT = 0, ALGORITHM = 1, KNOB 1–4 = 2–5, PRESETS = 6). Die Bereichstasten FX/SEL/ENV/LFO tragen am Gerät ihre alten Namen. Das Display zeigt die Bereiche als Reiter.

### Stand

| Teil | Status |
|---|---|
| Clock, Bedienung (vier Bereiche + HOME), Flash-Speicher | fertig, auf dem PC und im Panel-Simulator getestet |
| 808-Drum-Machine (12 Stimmen inkl. ZAP und PULSE), an Aufnahmen echter 808 abgeglichen | fertig, auf dem PC getestet |
| PO-12-Kit (16 Sounds, nachsynthetisiert) | fertig, auf dem PC getestet; Regler A/B noch nicht vermessen |
| MKS-50-Engine (DCO, HPF, 24-dB-Filter, Hüllkurve, LFO, Chorus, Mono/Unison/Glide) | fertig, auf dem PC getestet |
| JP-6-Engine, pro Bereich wählbar, multitimbral (MIDI-Kanal 1–3) | fertig, auf dem PC getestet; nicht gegen die App gehört |
| Drei Synth-Bereiche mit Step-Sequencer, Akkorden (Rother/Kraftwerk-Folgen) und Generator | fertig, auf dem PC getestet |
| Live-Aufnahme in die Patterns (REC, quantisiert), Groove-Generator, User-Plätze | fertig, auf dem PC getestet |
| Delay, Hall, Gated Reverb, Verzerrung als Send, Mix- und Master-Bus | fertig, auf dem PC getestet |
| Firmware auf dem Gerät | **noch nicht gebaut** (braucht die JieLi-Toolchain unter Windows) |
| Arpeggiator der Apps (UP, DOWN, UP/DN, RAND, ORDER, 1/32–1/1, 1–4 Oktaven, Hold), pro Synth-Bereich | fertig, auf dem PC und im Simulator getestet |
| Noten-Looper (1/2/4/8 Takte, Overdub, frei über den Patterns) | fertig, aber vorerst herausgenommen; liegt auf dem Branch `looper` |

## Simulator im Browser

`make web` übersetzt die echte Firmware (Engine, Bedienung, Speicher) mit clang nach WebAssembly und baut daraus `build/web/fm1-electro-simulator.html`: Display, Encoder, Tasten mit LEDs, Klaviatur und Ton im Browser, auch auf dem Handy. Die FM1-Open-Schnittstelle (Display, LEDs, Bedienelemente, Flash) ist in `host/web/sim.c` nachgebildet, gespeicherte Grooves bleiben im Browser. Rechenzeit und Speicher des echten Geräts bildet der Simulator nicht ab.

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

**Speicherplatz im Flash:** FM1-Electro speichert alle Plätze und Einstellungen (rund 10 KB) in zwei Kopien zu je drei 4-KB-Sektoren hinter dem Bereich von FM1 Open. Ist dort kein Platz, nutzt es die Projekt-Slots 9–14 von FM1 Open. Die gehen beim Zurückwechseln zur Original-Firmware verloren.

## Speicher und Rechenzeit

Das FM-1 hat rund 512 KB internes RAM. Laut SDK-Bibliothek belegt die Systemsoftware davon vor dem Linken etwa 196 KB fest, mindestens 48 KB braucht der Laufzeit-Heap (Bluetooth, USB). Mit der neuen 808 und der Mischkette braucht dieser Branch geschätzt **rund 210 KB** (156 KB Daten plus Code im RAM): etwa 135 KB Daten (davon 90 KB Delay und Hall), dazu rund 50 KB Code im RAM. Die drei Arpeggiatoren brauchen zusammen unter 0,3 KB. Das ist knapp. Falls es auf dem Gerät nicht passt, gibt es drei Stellschrauben: Delay-Länge (`FX_DELAY_CAP`), Hall-Leitungen (`FX_REVERB_CAP`) und Stimmenzahl pro Bereich (`electro.c`).

Auf dem PC kosten alle vier Bereiche mit Mischkette zusammen 5,9 % Echtzeit. Wie viel davon auf dem 240-MHz-Chip übrig bleibt, zeigt erst die Messung am Gerät. Für Delay und Hall habe ich sparsamere Versionen als in den iOS-Apps gebaut: ein Ping-Pong-Delay mit halber Abtastrate und 16 Bit (bis 0,6 s, klingt etwas dunkler, eher wie Band oder Eimerkette) und einen Hall mit 4 statt 8 Leitungen.

## Lizenz

GPLv3 mit der Linking-Ausnahme von FM1 Open, siehe `LICENSE.md` und `THIRD_PARTY.md`.
