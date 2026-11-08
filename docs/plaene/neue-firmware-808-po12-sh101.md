# Plan: neue Firmware „808 + PO-12 + SH-101“

Stand: 08.11.2026 · Status: **Plan, nicht begonnen** – Start erst, wenn der Nutzer „los“ sagt.

## Ziel

Eine schlanke, eigene Firmware für das M-VAVE FM-1 mit genau drei Instrumenten:

1. **TR-808**: das kalibrierte 808-Modell aus FM1-Electro (Festkomma, B1–B9).
2. **PO-12**: die PO-12-Drums als zweites Kit (vorhanden: `engine/po12.c`).
3. **SH-101**: ein monophoner Analog-Synth nach Roland SH-101 mit Sequencer und Arpeggiator (neu).

Von Anfang an ausgelegt auf **flüssige Bedienung** (Display und Tasten ohne Hänger) und **guten Klang** mit sicherer Reserve: Ziel-Last auf dem Gerät **unter 60 % pro Kern** bei voller Belegung, nie Aussetzer.

## Was wir übernehmen (fertig und gemessen)

| Teil | Quelle | Zustand |
|---|---|---|
| Plattform ohne SDK, zwei Kerne, Audio im SRAM | Dinghy/Felucca (`platform/`, ADR 0021) | läuft |
| 808 in Festkomma, Hi-Hats, BD, Clap, Cowbell … | `engine/tr808*.c/h` (ADR 0006–0017, 0022) | kalibriert |
| PO-12-Kit | `engine/po12.c` | läuft |
| Drum-Sequencer: Locks, Varianz, Wahrscheinlichkeit, Fills, Mute-Modus | `engine/drums.c`, ADR 0025, 0028, 0032 | läuft |
| Display-Painter (Bänder, 30 MHz, Bild in einem Zug) | `ui/gfx.c`, ADR 0020 | läuft |
| Speichern ohne Aussetzer | `fm1/el_store.c`, ADR 0023 | läuft |
| Emulator-Prüfung, Lastmessung | `tools/fm1-emu`, ADR 0018/0019 | läuft |
| Fremdcode erlaubt (GPL-kompatibel, mit Herkunft) | `CLAUDE.md` | z. B. Elixir, Felucca, SLOOP |

Neu-Repository oder Unterordner: Vorschlag **eigenes Repository** (z. B. `fm1-808-101`), das diese Teile als Kopie mit Herkunftsangabe übernimmt. Bis das Repository angelegt ist, liegt dieser Plan hier.

## Leistungs-Budget (Gerät ~340 MHz, eine Addition je Takt, ADR 0019)

| Kern | Aufgabe | Ziel |
|---|---|---|
| Kern 0 | 808/PO-12 (Festkomma), Master (Kompressor, Limiter), Effekte | ≤ 55 % |
| Kern 1 | SH-101-Stimme (Oszillatoren mit BLEP, IR3109-Filter, Hüllkurve), Delay | ≤ 40 % |
| Steuerung | Display, Tasten, Speicher (nie im Audio-Pfad) | flüssig, 25 Bilder/s |

Regeln von Anfang an:
- **Festkomma** dort, wo es auf dem Gerät messbar schneller ist (808 ja; Master blieb in Float schneller – nur nach Messung).
- **Blockweise** rechnen (Koeffizienten pro Block, nicht pro Sample), keine Division im Audio-Pfad, keine Bibliotheksaufrufe aus dem Flash.
- **Jede Stufe wird gemessen** (Profil im Emulator, Lastzeilen auf dem Gerät per Schalter `EL_DEBUG_PANEL`).
- Nur **ein** Synth: keine Polyphonie, keine Akkord-Stimmen → viel Reserve für Klang.

## SH-101 – Klangentwurf

- **VCO**: Sägezahn, Puls mit PWM (LFO/Hüllkurve/manuell), Sub-Oszillator (−1 Okt Rechteck, −2 Okt Rechteck, −2 Okt Puls 25 %), Rauschen; Mixer für alle vier. BLEP-Kanten (wie die MKS-50-„chop“-Wellen, bitgenau getestet).
- **VCF**: IR3109-Tiefpass 24 dB mit Resonanz bis zur Selbstoszillation; Hüllkurve, LFO, Keyboard-Tracking, Velocity; als Festkomma- oder Float-Ladder (A/B-Vergleich, ADR).
- **VCA**: Hüllkurve oder Gate; ADSR wie das Original (schnelle Attack, „Snappy“-Kurven).
- **LFO**: Dreieck, Rechteck, Random (S&H), Rauschen; Modulation von Pitch, PWM, Filter.
- **Portamento**, Glide nur bei Legato (wie 101 „Auto“).
- **Kalibrieren** an Aufnahmen eines echten SH-101 (falls vorhanden) wie bei der 808.

## Bedienung (erste Skizze)

- Zwei Bereiche: **BEAT** (808/PO-12) und **SH-101**, HOME als Übersicht.
- 808: wie FM1-Electro (Instrumente auf den schwarzen Tasten, Steps auf den weißen, Akzent-Zeile, Fills, Mute-Modus).
- SH-101: Tastatur spielt live; **Sequencer wie beim 101** (Step-Aufnahme mit Pausen und Ties, 100 Steps) und Live-Aufnahme (Anfang auf 1/16, Länge wie gehalten, N16); Arpeggiator Up/Down/Up-Down; Transpose.
- Die vier Knöpfe: Seiten VCO / VCF / ENV / LFO wie auf dem Bedienfeld des 101 (FX, SEL, ENV, LFO).
- Leitlinie aus `CLAUDE.md`: links = weniger, rechts = mehr; Regler starten am aktuellen Wert; was passiert, ist sichtbar; grünes PLAY.

## Speicher

- 4 Plätze je Bereich, Kits und Sounds pro Platz; eigene SH-101-Sounds (Bibliothek, N14/N19).
- Ein Format mit Version von Anfang an; Umrechnen nur innerhalb des einen Puffers (Lehre aus Version 26).

## Phasen

1. **Gerüst** (1): Repository, Plattform, Build, Emulator-Prüfung, leere UI mit Lastanzeige. Messen: Leerlast.
2. **808 + PO-12** (2): Engine und Drum-Sequencer übernehmen, Bedienung BEAT. Messen gegen FM1-Electro (bitgleiche Demo-WAVs).
3. **SH-101-Stimme** (3): VCO, VCF, ENV, LFO auf Kern 1; A/B-WAVs, Kalibrierung. Last ≤ 40 %.
4. **SH-101-Sequencer und Arp** (4): Step- und Live-Aufnahme, Transpose.
5. **Master und Effekte** (5): Kompressor, Limiter, Delay, ein Hall; Live-Effekte (Punch-ins, MONO DELAY N15).
6. **Speichern, Bibliothek, Feinschliff** (6): Projekte, Sounds, Display-Animationen ohne Flackern, Gerätetests.

Jede Phase endet mit `make test`, Emulator-Prüfung, Lastmessung und einem Testbuild.

## Offene Fragen an den Nutzer

- Eigenes Repository anlegen (Name?) oder als Unterordner hier?
- SH-101-Referenz: gibt es Aufnahmen eines echten SH-101 zum Kalibrieren?
- Sollen weitere FM1-Electro-Funktionen mit (STRINGS/RHYTHM fallen weg – bewusst)?
- MIDI: SH-101 auf einem Kanal, Drums auf Kanal 10 wie bisher?
