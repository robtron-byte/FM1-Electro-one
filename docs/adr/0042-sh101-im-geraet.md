# 0042: SH-101 (Baustein sh101) im FM-1: Gerätefassung, eine Stimme, Sequencer aus dem Gerüst

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28 (Solo-Firmware 0.1.1-alpha gebaut, im Emulator geprüft; auf dem Gerät nicht gemessen)

## Kontext

Der Baustein `bausteine/sh101` rechnet wie seine Vorlage MonoBass (Swift): Audio in float, Hüllkurven, Glide, LFO-Phase
und Clipper in double, 4× überabgetastete nichtlineare Ladder, 8× überabgetasteter Clipper, BLEP-Oszillator. Er ist
bitgleich mit den Referenz-WAVs der Vorlage. Das FM-1 rechnet nur float in Hardware, double in Software, und seine
Bibliotheksfunktionen (`exp2f`, `tanf`, `sinf`, `fabsf` …) liegen im Flash. Ziel des Nutzers: so viele Stimmen und so
viel Qualität wie sicher, unter 70 % je Kern, keine Aussetzer; bei zu viel Last zuerst Stimmen, dann billigere gleich
klingende Rechenwege, zuletzt Klang.

Gemessen im Emulator (fm1-emu, Block 64 = 1,45 ms, eine gehaltene Note, Laufzeit im Audio-Interrupt):

| Fassung | Last je Block |
|---|---|
| wie die Vorlage (double, Bibliothek) | 143 % |
| float statt double (`SH101_FLOAT`) | 80 % |
| + Polynome statt Bibliothek (`SH101_FASTMATH`) | 65 % |
| + keine versteckten double-Konstanten mehr in der Hüllkurve | 48 % |
| + `fabsf`/`fminf`/`floorf` inline, Halbband-Ketten fester Länge | **39 %** |
| dasselbe mit Drive (8× Clipper) | 51 % |

In der fertigen Firmware (mit Gerüst, Bild, MIDI): eine Note 44 %, schlimmster Fall (Drive + hohe Resonanz) 58,5 % Spitze
auf Kern 0; Delay und Hall auf Kern 1: 6,6 %. Keine verspäteten Blöcke. Am Gerät kann niemand messen.

**Einordnung der Emulator-Zahlen:** Die Prozente oben sind die Messung der Firmware selbst (Zeitgeber je Block, wie die
Seite SYSTEM am Gerät). Das Kostenmodell des Emulators (`MODEL … % of real time`, ADR 0019) liegt höher: mit gehaltener
Note 98 % Laufzeit gegen 34 % ohne. Zum Abgleich dieselben Läufe mit der MKS-50-Firmware aus fm1-synths (6 Stimmen):
Zeitgeber 32 %, Modell 80 % gegen 30 %. Das Verhältnis Modell zu Zeitgeber ist bei beiden gleich (1,45 SH-101,
1,56 MKS-50); für die MKS-50 stimmte die Zeitgeber-Zahl mit dem Gerät überein (fm1-synths: Emulator 56 %, Gerät 56 %
Spitze). Schätzung fürs Gerät deshalb: wie der Zeitgeber, 44 % mit einer Note, um 60 % mit Drive. Unsicher bleibt der
Anteil der FPU-Befehle (SH-101 37 Mio./s gegen MKS-50 18 Mio./s).

## Entscheidung

- **Gerätefassung als Schalter des Bausteins**, Referenz unverändert: `SH101_FLOAT=1` (Hüllkurve und Glide dann als
  gleichwertiger Schritt zum Ziel `v += (Ziel − v)/(c + 1)`, sonst verliert float im Abklingen bis 2 % Pegel),
  `SH101_FASTMATH=1` (exp2, tan, sin als Polynome, ~1e-7 relativ). Abweichung zur Vorlage: RMS je 100 ms ≤ 6e-5; Sample
  für Sample nur ein langsames Auseinanderlaufen der Oszillatorphase (bei LFO auf die Tonhöhe und Glide), nicht hörbar.
  Dritter Testlauf `test_fm1` in `bausteine/sh101`.
- **Eine Stimme** (`SH101_WITH_POLY=0`), wie das SH-101 (Wunsch des Nutzers: bleibt mono).
- **Kern 1 rechnet Delay und Hall** (aus fm1-synths), Kern 0 die Stimme.
- **Arp und Sequencer des Bausteins aus** (`SH101_WITH_ARPSEQ=0`): sie laufen samplegenau in der Stimme. Stattdessen
  Arpeggiator und Step-Sequencer des Gerüsts (fm1-synths ADR 0015, wie beim SH-101: Schritt-Aufnahme mit REC, Pause
  mit der obersten Taste, Bindung durch Halten, Transponieren mit den Tasten).
- **Werte** je Parameter 0..max als Byte; Regler 0..200 (angezeigt 0.0..10.0 wie die Schieber des SH-101), Schalter
  mit ihren Stufen. 46 Parameter; Feinstimmung 100 = genau Mitte.
- **14 Werkssounds** als eigene Einstellungen (keine Daten eines Originals), in den Gruppen BASS, LEAD, SEQUENCE, FX.

## 0.1.2: Distortion-Send, mono, Delay 0,6 s (Wünsche des Nutzers 28.11.)

- **Der SH-101 bleibt mono** (`SH101_WITH_POLY=0`), auch in den Kombis. Poly über Kern 1 ist als offener Punkt gestrichen;
  freie Last bleibt Reserve.
- **Der eingebaute Drive (8× Clipper) bleibt** wie im Baustein.
- **Zusätzlich ein Distortion-Send wie in FM1-Electro:** `gemeinsam/fx/gm_dist.c` ist FM1-Electros `engine/dist.c`
  (Festkomma auf dem Gerät, Rechnung unverändert), nur die Kurventabellen liegen halb im Pool, halb im RAM. Modi in der
  Reihenfolge **TUBE, OD, DIST, RAT**, Start TUBE. Werte im Sound (Formatversion 2; ältere Sounds laden mit Send 0):
  Seite FX/DIST (SEND, MODE, DRIVE, TONE) und DIST 2 (LEVEL); gehaltene FX-Taste: DLY SEND, REV SEND, DST SEND, TIME.
  Er läuft auf Kern 1 neben Delay und Hall (Gerüst-Haken `GM_FX_EXTRA`), auf den Send des Blocks davor (1,45 ms).
- **Delay höchstens 0,6 s** (`SYN_FX_DELAY_MAX_S=0.6f`).
- Last im Emulator (SQUELCH mit Drive, Distortion, Delay, Hall): Kern 0 57,5 % (Spitze 58,2 %), Kern 1 13,9 %
  (Distortion allein 7,4 %), keine verspäteten Blöcke.
- Speicher: knapp. RAM 98 292 von 98 304 Bytes, Pool 3 kB frei (Zeichenfläche in Streifen von 20 Zeilen, `GM_BAND_H`).

## Alternativen

- double behalten: 143 % Last, unmöglich.
- Ladder nur 2× überabtasten: spart viel, ändert aber den Klang; erst wenn Stimmen und gleich klingende Wege nicht
  reichen (nicht nötig).
- Arp/Seq des Bausteins im Gerät: zwingt die Stimme in den Sample-Takt der Steuerung und auf Kern 0 allein.

## Folgen

- Die Gerätefassung ist im Baustein abgesichert (`make -C bausteine sh101` prüft alle drei Fassungen).
- Am Gerät die Last auf der Seite SYSTEM ablesen (GLO, zweite Seite), vor allem mit Drive.

## Verlauf

- 2026-11-28: angelegt (sh101-Chat); Messungen im Emulator.
- 2026-11-28: 0.1.2-alpha: Distortion-Send (TUBE, OD, DIST, RAT) auf Kern 1, Drive bleibt, mono, Delay 0,6 s.
- 2026-11-28: 0.1.1-alpha: Clipper ohne bewachte Division (Nenner nie 0), Abgleich der Lastzahlen mit der MKS-50.
