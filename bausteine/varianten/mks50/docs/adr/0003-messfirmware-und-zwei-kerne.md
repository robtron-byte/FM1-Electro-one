# 0003: Messfirmware, zwei Kerne, Klang-Schalter aus FM1-Electro

- **Status:** vorgeschlagen
- **Stand:** 2026-11-10

## Kontext

Der Emulator zeigt bei Gleitkomma-Code etwa die Hälfte der Last des Geräts (FM1-Electro `docs/adr/0019`). Gemessen wurde dort: 3 MKS-Stimmen mit Chorus kosten etwa 55 % eines Kerns. Die Größe des Pools und die Zahl der Parts lassen sich deshalb nur am Gerät festlegen.

FM1-Electro hat auf dem Gerät zwei Klangkompromisse eingebaut, die der Auftrag ausschließt („ohne Klangkompromisse“):

- das Filter mit halber Abtastrate (`MKS_VCF_DECIM 2`);
- ein Release, das weit abgeklungen 8-mal schneller endet (`MKS_RELEASE_HURRY`).

## Entscheidung

- **Beide Kompromisse sind Schalter mit Standard „aus“** (`mks_t.vcf_half`, `mks_t.release_hurry`). Das Filter rechnet jedes Sample. Die halbe Rate bleibt nur, um in der Messfirmware zu zeigen, was die volle Rate kostet.
- **Ein Block in vier Schritten** (`engine/mks.h`):
  1. `mks_begin`: die Steuerwerte jedes Parts für alle 16-Sample-Schritte des Blocks (LFO, Pulsbreite, Wellenform) und die Verteilung der Arbeit.
  2. `mks_voices(kern)`: jeder Kern rechnet seine Stimmen. Die Stimmen gehen abwechselnd an Kern 1 und Kern 0.
  3. `mks_finish(kern)`: jeder Kern summiert seine Parts, rechnet Pegel und Chorus. Der Kern mit weniger Stimmen bekommt den ersten Part.
  4. `mks_mix`: die Summe.
- **Ein Auftrag an Kern 1 pro Block** (`app/mk_app.c`): Er rechnet seine Stimmen, wartet auf die von Kern 0 und rechnet dann seine Chorus-Einheiten. Meldet er sich 4 ms nicht, hält Kern 0 ihn an und rechnet bis zum Ausschalten allein.
- **Aller Audio-Code im SRAM** (`.dsp_text`, Objekte `dsp_*.o`).
- **Die Messfirmware zeigt** je halbe Sekunde Mittel und Spitze in Prozent eines Blocks: gesamt, Kern 0, Kern 1, Stimmen je Kern, Chorus je Kern, dazu den Höchstwert und die Zahl verspäteter Blöcke. Umschaltbar: Zahl der Stimmen (1–8), Sound, Filter voll/halb, 1 oder 2 Kerne.
- **Prüfung:** Die Aufteilung auf zwei Kerne ändert nur die Reihenfolge der Summen. `make identity`: schlimmste Abweichung −132 dB.

## Alternativen

- **Stimmen eines Parts immer auf einem Kern:** nur eine Übergabe ohne Warten, aber ein Part allein mit 6 Stimmen läge ganz auf einem Kern.
- **Lastanzeige aus dem Emulator:** widerlegt durch FM1-Electro.

## Folgen

- Nach der Messung werden Pool-Größe und Zahl der Parts festgelegt. Danach folgen nur Optimierungen, die `make identity` bestehen.
- Bekannter Kandidat: `dco_tick` ist mit 8 KB zu groß zum Einbetten und wird pro Sample und Stimme aufgerufen.

## Messung am Gerät (2026-11-10, Block 64, Sound DARK STR, 6 Stimmen auf 3 Parts, jeder mit Chorus)

Prozent der Blockzeit, Mittel / Spitze, über die letzte halbe Sekunde (Fotos des Nutzers):

| Filter | Kerne | Gesamt | Kern 0 | Kern 1 | Stimmen K0 / K1 | Chorus K0 / K1 | MAX | SPÄT |
|---|---|---|---|---|---|---|---|---|
| voll | 2 | 69 / 71 | 68 / 71 | 46 / 46 | 45 / 40 | 12 / 5 | 72 | 0 |
| halb | 2 | 67 / 68 | 67 / 68 | 45 / 45 | 44 / 39 | 12 / 5 | 70 | 0 |
| voll | 1 | 119 / 122 | 119 / 120 | (alt) | 91 / (alt) | 17 / (alt) | 122 | 1809 |
| voll | 1 (TRANS EURO) | 96 / 96 | 96 / 96 | (alt) | 69 / (alt) | 16 / (alt) | 101 | 2 |

Die Werte für Kern 1 bei einem Kern waren alte Zahlen der Anzeige (behoben).

- **Das Filter in voller Rate kostet nur etwa 2 Punkte mehr** als mit halber Rate. Der Klangkompromiss lohnt sich nicht; der Schalter `vcf_half` bleibt aus.
- **Ein Kern reicht nicht** (119 %, 1809 späte Blöcke bei DARK STR), zwei Kerne ergeben 69 %.
- Der teuerste Sound aus FM1-Electro mit 6 Stimmen und 3 Chorus-Einheiten bleibt bei 71 % Spitze ohne späte Blöcke. Ein Chorus kostet etwa 5–6 Punkte bei 64 Samples.
- **Ungleich verteilt:** Kern 0 68 %, Kern 1 46 %. Kern 0 rechnet nebenbei `mks_begin`, Mischen und Ausgabe (etwa 11 Punkte).
Zweite Messung, voll, 2 Kerne, DARK STR (4 Parts ab 7 Stimmen):

| Stimmen | Gesamt | Kern 0 | Kern 1 | Stimmen K0 / K1 | Chorus K0 / K1 | MAX |
|---|---|---|---|---|---|---|
| 7 | 81 / 81 | 72 / 73 | 65 / 66 | 46 / 54 | 12 / 11 | 91 |
| 8 | 88 / 88 | 88 / 88 | 65 / 66 | 61 / 54 | 12 / 11 | 92 |

Der Nutzer sieht beim Umschalten des Sounds und bei anderen Eingriffen kurze Sprünge über 100 %. Vermutung: Der Test lädt dabei alle 4 Parts und schlägt 6–8 Noten im selben Block an; jede neue Stimme rechnete ihre Hüllkurvenzeiten (exp2/log2) neu.

- Noch nicht gemessen: Block 128, Latenz am Gerät.

## Änderung nach der zweiten Messung (bitgleich, `make test` besteht)

- **Hüllkurven-Grundzeiten** (`mks_part_t.base_t`) werden beim Laden des Sounds und beim Drehen von T1–T4 gerechnet, nicht bei jedem Anschlag. Die Zahlen sind dieselben.
- **Verteilung auf die Kerne nach Last** (`mks_begin`): Eine Stimme zählt 10, ein Chorus 5, Kerns 0 eigene Arbeit (Begin, Mischen, Ausgabe) 9; jede Einheit geht an den Kern mit der kleineren Summe. Vorher gingen die Stimmen abwechselnd, die Parts nach der Stimmenzahl.
- **Neue Zeile 3 der Messfirmware:** Spitze von EREIGNIS (MIDI und Tasten annehmen, Soundwechsel, Anschläge) und BEGIN (Steuerwerte aller Parts), je in Prozent eines Blocks. Sie zeigt, woher ein Sprung über 100 % kommt.

## Dritte Messung (Sound MACHINE PD, 2 Kerne, voll, Block 64) und Folgerung

Die Anzeige „A2 B3 C3“ bzw. „A2 B2 C2 D2“ zeigt, dass im Test 8 Stimmen klangen: 6 gehaltene und 2 ausklingende (der Pool hat 8). Ergebnis: Gesamt 91 / 92, Kern 0 80 / 81, Kern 1 70 / 71, Stimmen 60 / 54, Chorus 6 / 16, BEGIN-Spitze 13 %, MAX 94 bzw. 98 %, keine späten Blöcke.

- **EREIGN springt beim Soundwechsel nur auf 5 %** eines Blocks (4 Sounds laden, Noten anschlagen). Das ist nicht die Ursache für Last über 100 %.
- **BEGIN (13 %) ist der größte feste Posten** und lag ganz auf Kern 0.
- **Pool:** Mit 6 Stimmen im Pool klingen höchstens 6 (der Pool nimmt zuerst die losgelassenen), das ergibt etwa 70–80 %. Vorschlag: Pool von 6, wie das Original. 8 bleibt eine Option nach dem Ausgleich der Kerne.

Änderung (bitgleich): Die Steuerwerte der Parts (`mks_control`) werden auf beide Kerne verteilt. `mks_plan` verteilt vorher Stimmen, Steuerwerte (Einheit 3) und Chorus nach Last; ein Kern wartet auf den anderen, bevor die Stimmen laufen. Fällt Kern 1 aus, rechnet Kern 0 dessen Anteil nach.

## Verlauf

- 2026-11-10: angelegt
- 2026-11-10: erste Messung am Gerät, siehe oben; Anzeige für Kern 1 bei einem Kern behoben
- 2026-11-10: 7 und 8 Stimmen gemessen (81 % und 88 %); Grundzeiten pro Sound, Verteilung nach Last, Zeile für Ereignisse und Begin
- 2026-11-10: MACHINE PD gemessen; Steuerwerte auf beide Kerne verteilt; Pool von 6 vorgeschlagen
