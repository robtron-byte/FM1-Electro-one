# 0050: dk808 im Gerät: float-Fassung, Gerüst-Adapter, Last

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28

## Kontext

Der Baustein `bausteine/dk808` (DrumKern, 17 Instrumente, ADR 0040, B19) rechnet in `double`, Sample für Sample wie
der Swift-Kern. Das FM-1 rechnet `double` nur in Software. Die 808 soll als Part im Gerüst der Solo- und
Kombi-Firmwares laufen: 808 solo, 808 + 101 und 808 + 101 + JX (ADR 0044, 0052). Die Distortion kommt als Send dazu,
TUBE zuerst. Der Ersatz der 808 in FM1-Electro (`engine/`, ADR 0041) bleibt beim Bau-Chat.

## Entscheidung

1. **Schalter `DK808_FLOAT`** (Standard 0 = `double` wie bisher). Mit 1 rechnet der Zustand in `float` (`dk_real`),
   die Mathematik über `sinf`/`expf` usw. Die Metallbank (sechs Pulsoszillatoren für Hats und Becken) zählt ihre Phasen
   dann in 32-Bit-Ganzzahlen. `vca()` nimmt nahe 0 eine Reihe statt `expm1`. Init und `set_knob` behalten `double`
   als Schnittstelle.
   Gemessen mit `make -C bausteine/dk808 float`: zwei Programme rendern alle 17 Instrumente bei Velocity 64 und 127, je
   2 s; `test/float_cmp.py` vergleicht sie. Die float-Seite ist wie im Gerät gebaut (einfach genaue Konstanten).
   Fehlerenergie unter dem Signal: schlechtestes Instrument das Becken mit −62 dB, Hi-Hats −73/−95 dB, BD −78 dB, die
   übrigen −96 bis −128 dB. Grenze im Test: −60 dB.
2. **Pegel je Instrument im Kern** (`dk808_set_level`, Standard 1). Bei 1 bleibt der Klang bitgleich, der
   Referenztest ist unverändert.
3. **Gerüst-Adapter `bausteine/dk808/dk808_bs.c/.h`:** Ein Drum-Part ist ein `dk808_bs_t`. Sein Sound ist ein Kit aus
   72 Rohwerten:
   - 17 Pegel 0..100
   - 51 Regler 0..100
   - 3 Choke-Schalter
   - DIST SEND 0..100

   Noten wählen das Instrument über die Drum-Map des DrumKern. Note aus entfällt. `dk808_bs_render` addiert mono auf
   l/r und schreibt den Send in einen eigenen Puffer. Getestet in `test/bs.c` (double und float): das INIT-Kit klingt
   Sample für Sample wie `dk808_init`; Pegel, Send, Namen und all_off werden geprüft.
4. **Messvariante `bausteine/varianten/dk808`** (808 solo, 0.0.1, nur Emulator, kein Paket):
   - Gerüst mit einem Part, Kennung vorerst FM-1_91 (geteilt mit 808 + 101).
   - Distortion als Send wie beim SH-101 (`gemeinsam/fx/gm_dist.c`, Kurven im Flash): Kern 0 schreibt den Send,
     Kern 1 rechnet TUBE/OD/DIST/RAT neben Delay und Hall.
   - Mit `DK_LAST=1` spielt sie ein eingebautes Messmuster.
5. **Last im Emulator** (Kern 0, Anteil eines Blocks von 1,45 ms, je Phase 1 s, alle 1/16 neu angeschlagen):

   | Phase | Spitze | Mittel |
   |---|---|---|
   | nichts (Rauschen und Metallbank laufen immer) | 11,9 % | 11,6 % |
   | BD, LG, SD, RS, CP | 15–16,5 % | 14–15,6 % |
   | Congas, Toms, Claves, Maracas | 12,8–13,7 % | 12,5–13,3 % |
   | CH / OH | 20,3 / 20,4 % | 19,8 % |
   | CY | 32,5 % | 31,1 % |
   | CB | 17,2 % | 16,6 % |
   | alle 17 zugleich | 85,3 % | 80,2 % |
   | volles Pattern (BD, SD, CP, CH/OH, CY, Toms, Congas, Perkussion über 16 Schritte) | 69,9 % | 44,9 % |

   Das Becken kostet allein etwa 20 % (zwei Bandpässe, zweifach überabgetastet). Jede Hi-Hat kostet etwa 8 %.

## Alternativen

- **double im Gerät:** in Software viel zu langsam.
- **Eigener float-Port ohne Schalter:** dann gäbe es keinen Vergleich mit dem geprüften double-Kern mehr.
- **Pegel nur im Adapter:** Der Kern gibt eine Summe aus, ein Pegel je Instrument geht nur im Kern.
- **Messen in einem Prozess:** Erst rendert `test/float.c` double und float in einem Programm (Kopie mit umbenannten
  Symbolen). Dabei wich das Becken um −40 dB ab, auch zwischen zwei gleichen double-Kopien. Das war also eine Störung
  der Messung. Darum rendern jetzt zwei getrennte Programme.

## Folgen

- 808 solo passt auf Kern 0, Delay, Hall und Distortion laufen auf Kern 1. Ein volles Pattern kostet in der Spitze
  70 %.
- Für die Kombis (808 + 101, 808 + 101 + JX) reicht Kern 0 nicht für 808 und Synth zusammen. Mögliche Wege:
  - die 808 auf Kern 1 neben die Effekte legen,
  - die Becken-/Hat-Pfade sparsamer machen (eigener Schalter, nach Gehör, ADR 0006),
  - die Instrumente abwählen (`DK808_INSTRUMENTS`).

  Das entscheidet der Kombi-Chat mit dem sh101-Chat.
- Die Bedienung der Messvariante ist vorläufig: eine Seite je Instrument (Pegel und drei Regler), Kits „808 KIT“ und
  „808 TUBE“. Die eigentliche Firmware 808 solo (B20) folgt als eigener Auftrag.

## Verlauf

- 2026-11-28: angelegt (float-Schalter, Adapter, Messvariante, Last im Emulator)
