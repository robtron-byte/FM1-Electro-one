# 0042: SH-101 (Baustein sh101) im FM-1: Gerätefassung, eine Stimme, Sequencer aus dem Gerüst

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28 (Solo-Firmware 0.1.0-alpha gebaut, im Emulator geprüft; auf dem Gerät nicht gemessen)

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
auf Kern 0; Delay und Hall auf Kern 1: 6,6 %. Keine verspäteten Blöcke. Am Gerät kann niemand messen; nach FM1-Electro
liegt das Gerät etwa 10 % über dem Emulator, fm1-synths schätzt Gleitkomma am Gerät teurer als im Emulator ein.

## Entscheidung

- **Gerätefassung als Schalter des Bausteins**, Referenz unverändert: `SH101_FLOAT=1` (Hüllkurve und Glide dann als
  gleichwertiger Schritt zum Ziel `v += (Ziel − v)/(c + 1)`, sonst verliert float im Abklingen bis 2 % Pegel),
  `SH101_FASTMATH=1` (exp2, tan, sin als Polynome, ~1e-7 relativ). Abweichung zur Vorlage: RMS je 100 ms ≤ 6e-5; Sample
  für Sample nur ein langsames Auseinanderlaufen der Oszillatorphase (bei LFO auf die Tonhöhe und Glide), nicht hörbar.
  Dritter Testlauf `test_fm1` in `bausteine/sh101`.
- **Eine Stimme** (`SH101_WITH_POLY=0`), wie das SH-101. Eine zweite Stimme passt nicht mit auf Kern 0 (2 × 39 % plus
  Drive); sie bräuchte Kern 1 und dafür eine Block-Schnittstelle im Baustein (offen, Arbeitstafel).
- **Kern 1 rechnet Delay und Hall** (aus fm1-synths), Kern 0 die Stimme.
- **Arp und Sequencer des Bausteins aus** (`SH101_WITH_ARPSEQ=0`): sie laufen samplegenau in der Stimme. Stattdessen
  Arpeggiator und Step-Sequencer des Gerüsts (fm1-synths ADR 0015, wie beim SH-101: Schritt-Aufnahme mit REC, Pause
  mit der obersten Taste, Bindung durch Halten, Transponieren mit den Tasten).
- **Werte** je Parameter 0..max als Byte; Regler 0..200 (angezeigt 0.0..10.0 wie die Schieber des SH-101), Schalter
  mit ihren Stufen. 46 Parameter; Feinstimmung 100 = genau Mitte.
- **14 Werkssounds** als eigene Einstellungen (keine Daten eines Originals), in den Gruppen BASS, LEAD, SEQUENCE, FX.

## Alternativen

- double behalten: 143 % Last, unmöglich.
- Ladder nur 2× überabtasten: spart viel, ändert aber den Klang; erst wenn Stimmen und gleich klingende Wege nicht
  reichen (nicht nötig).
- Arp/Seq des Bausteins im Gerät: zwingt die Stimme in den Sample-Takt der Steuerung und auf Kern 0 allein.

## Folgen

- Die Gerätefassung ist im Baustein abgesichert (`make -C bausteine sh101` prüft alle drei Fassungen).
- Für Poly mit zwei Kernen: Block-Schnittstelle (`sh101_block_*`) mit Host-Test „bitgleich zu `sh101_sample`“.
- Am Gerät die Last auf der Seite SYSTEM ablesen (GLO, zweite Seite), vor allem mit Drive.

## Verlauf

- 2026-11-28: angelegt (sh101-Chat); Messungen im Emulator.
