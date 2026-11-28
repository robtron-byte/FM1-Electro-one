# 0002: Pool-Größe und Zahl der Parts nach der Messung am Gerät

- **Status:** angenommen
- **Stand:** 2026-11-14 (am Gerät gemessen, vom Nutzer entschieden)

## Kontext

Eine JX-Stimme hat zwei Oszillatoren mit einfachen Formen (MKS: einer mit bis zu drei gehackten Formen), zwei
Hüllkurven statt einer und keinen Sub, sonst dasselbe (Ladder in voller Rate, HPF, Chorus pro Part). Am Gerät kosten
6 MKS-Stimmen mit 3 Chorus-Einheiten etwa 70 % auf zwei Kernen (`../mks50/docs/adr/0003`). Der Emulator rechnet
Gleitkomma zu billig; die Messung am Gerät entscheidet.

## Entscheidung (vorgeschlagen)

- Erst eine **Messfirmware** wie bei `mks50`: 6 JX-Stimmen, 3 Parts mit Chorus, beide Kerne, Lastanzeige je Kern und Teil.
- Danach Pool (6, wie der JX-8P, oder weniger) und Zahl der Parts festlegen; nur Optimierungen, die den Klang nicht
  ändern.
- Bis dahin: Pool 6 (`JX_POOL`), 4 Parts, Lasteinheiten wie MKS.
- **Die Messfirmware ist die normale Firmware** mit dem Lasttest auf der Seite SYSTEM (GLO, letzte Seite): KNOB 1 TEST
  hält 1–8 Töne über die Parts A, B, C (jeder mit seinem Chorus; ab 7 wächst der Pool auf diese Zahl), KNOB 2 SOUND lädt
  einen Werks-Sound in A–C (Vorgabe PG8X INIT: gehalten, mit Chorus, am Rechner unter den teuersten), KNOB 3 CORES 1
  oder 2. Die Seite zeigt Mittel und Spitze je halbe Sekunde: gesamt, Kern 0, Kern 1, Stimmen je Kern, Chorus,
  Delay + Hall, Höchstwert, späte Blöcke.

## Messung am Gerät (2026-11-14, Block 64, PG8X INIT auf Part A–C mit Chorus, Fotos des Nutzers)

| Fall | Gesamt | Kern 0 | Kern 1 | Stimmen K0 / K1 | Chorus | Delay + Hall | MAX | spät |
|---|---|---|---|---|---|---|---|---|
| 6 Stimmen, 2 Kerne | 54 / 55 | 45 / 45 | 45 / 45 | 33 / 29 | 17 | 1 | 59 | 0 |
| 8 Stimmen, 2 Kerne | 65 / 67 | 56 / 58 | 55 / 55 | 45 / 38 | 17 | 1 | 72 | 0 |
| 8 Stimmen, Sends offen | ≈ 77 / 78 | ≈ 75 / 76 | 61 / 61 | ≈ 50 / 25 | 15 | 23 | ≈ 78 | 0 |
| 8 Stimmen, 1 Kern | 144 / 144 | 144 | – | 97 / – | 18 | 27 | 199 | 28811 |

(Mittel / Spitze in Prozent eines Blocks; das Foto mit offenen Sends ist unscharf.)

- Eine JX-Stimme kostet am Gerät etwa 10–11 % eines Blocks, so viel wie die 10 Lasteinheiten des Planers annehmen.
  Die JX-8P-Firmware ist leichter als die MKS-50-Firmware (6 Stimmen: 55 % gegen 69–71 %).
- Ein Kern reicht nicht; beide Kerne sind nötig.
- **Entscheidung des Nutzers: 6 Stimmen wie der JX-8P**, 4 Parts. Mit Delay und Hall gibt der Planer Kern 0 vier und
  Kern 1 zwei Stimmen neben den Effekten (etwa 56 / 57 %, aus den Werten oben geschätzt); eine Optimierung ist nicht
  nötig. Das Ungleichgewicht mit offenen Sends tritt erst bei 8 Stimmen auf.

## Emulator (2026-11-14, Block 64, PG8X INIT, nur zum Vergleich; das Gerät rechnet Gleitkomma teurer)

| Fall | Gesamt | Kern 0 | Kern 1 | MAX | spät |
|---|---|---|---|---|---|
| 6 Stimmen, 3 Parts, ohne Sends | 49 / 52 | 40 / 42 | 42 / 43 | 53 | 0 |
| 6 Stimmen, Sends offen (Delay, Hall) | 61 / 62 | 58 / 58 | 49 / 50 | 63 | 0 |
| 8 Stimmen, ohne Sends | 59 / 61 | 50 / 51 | 50 / 51 | 65 | 0 |
| 6 Stimmen, ein Kern | 85 / 86 | 85 / 86 | – | 88 | 0 |

Zum Vergleich MKS-50: Emulator 56 % mit Effekten, Gerät 69–71 % ohne Effekte.

## Verlauf

- 2026-11-14: angelegt
- 2026-11-14: Lasttest auf der Seite SYSTEM gebaut, Emulator-Werte eingetragen; Messung am Gerät offen
- 2026-11-14: am Gerät gemessen; der Nutzer wählt 6 Stimmen, 4 Parts; angenommen
