# dk808: TR-808 (DrumKern) – Auftrag und Stand

**Stand 28.11.2026: fertig.** Alle 17 Instrumente stimmen Sample für Sample mit den Referenz-Renders des Swift-Kerns
(`make -C bausteine dk808`). Zum Anhören: `bausteine/build/dk808/hit-XX-v127.wav`.

Wer daran arbeitet: [../ARBEITSTAFEL.md](../ARBEITSTAFEL.md). Wer übernimmt, liest nur diese Datei, `../README.md` und
`../baustein.h`.

## Ziel

Alle 17 Instrumente des DrumKern als Baustein, Klang wie der Swift-Kern (der Nutzer hat ihn gehört: näher an der 808 als
unsere jetzige). Instrumente: BD, Laser Gun, SD, RS, Clap, Low/Mid/Hi Conga, Low/Mid/Hi Tom, CH, OH, Cymbal, Claves,
Maracas, Cowbell. Dazu gehören der gemeinsame Rauschgenerator und die Bank aus sechs Pulsoszillatoren für Hi-Hats und
Becken. **Ohne** Effekte, Kanalzug, Busse und Master des DrumKern (Wunsch des Nutzers).

## Vorlage (Dropbox des Nutzers, `Programmieren/Musik/808 Projekt/app/`)

| Datei | Dropbox-ID | Inhalt |
|---|---|---|
| `Sources/DrumKernDSP/Voices.swift` | `id:bJM3TYf_3HoAAAAAABudRQ` | alle Stimmen, Rauschen, Metallbank, Halbband |
| `Sources/DrumKernDSP/DSPMath.swift` | `id:bJM3TYf_3HoAAAAAABudNg` | Einpol-Filter, Biquad, SVF |
| `Sources/DrumKernDSP/Engine.swift` | `id:bJM3TYf_3HoAAAAAABudOw` | Ablauf je Sample, Velocity, Choke |
| `Sources/DrumKernDSP/Model.swift` | `id:bJM3TYf_3HoAAAAAABudNw` | Reglernamen, Standardwerte, Drum-Map |
| `Sources/DrumKernDSP/Mixer.swift` | `id:bJM3TYf_3HoAAAAAABudOg` | Kanalzug (nur für den Referenzvergleich) |
| `SOUND-ENGINE.md` | `id:bJM3TYf_3HoAAAAAABvb5A` | Beschreibung aller Formeln |
| `reference-renders/` | Ordner `id:bJM3TYf_3HoAAAAAABvbnw` | WAVs (32-bit float, 48 kHz, 2 s je Schlag) |

`ref/manifest.json` und `ref/SHA256SUMS.txt` sind Kopien. Die WAVs kommen nicht ins Repository (15 MB). Liegen sie in
`ref/wav/`, vergleicht der Test jedes Sample; sonst die Kennwerte aus dem Manifest: erste 16 Samples, Spitze, RMS, Summe.

## Wichtig beim Port

- `double` überall, Konstanten genau abschreiben. Die Velocity wird wie im Original gerechnet:
  `u = (double)((float)vel * 0.007874015718698502f) * 1.377`.
- Je Sample: zuerst fällige Trigger auslösen, dann Rauschen und Metallbank weiterschalten (immer, auch wenn keine
  Stimme klingt), dann die aktiven Stimmen rechnen.
- Zur Choke-Logik: CH und OH dämpfen die offene Hi-Hat (Standard an). Für Congas und Toms gibt es je einen eigenen
  Schalter, Standard aus.
- Die Referenz lief durch den Kanalzug. Bei Standardeinstellungen multipliziert er nur mit `cos(π/4)·1.41421356237`. Der
  Test rechnet diesen Faktor dazu, der Baustein selbst nicht.
- **Abschaltbar:** `-DDK808_INSTRUMENTS=<Bitmaske>` (Bit = Nummer in `dk808_instrument`), Standard alle 17.
  `DK808_MASK_FM1_ELECTRO` lässt Toms, Congas, Claves, Maracas und Laser weg. Das ist die Auswahl für die Firmware des Nutzers.

## Fertig, wenn

1. `make -C bausteine dk808` besteht: jedes Instrument stimmt bei Velocity 64 und 127 mit `ref/manifest.json` überein
   (Abweichung < 1e-5), und es klingt mit jeder Maske.
2. Die WAVs in `bausteine/build/dk808/` sind zum Anhören da.
3. Die Arbeitstafel ist nachgeführt, ebenso FEATURES B19.

## Danach (eigene Aufträge, in die Arbeitstafel eintragen)

- float-Fassung als Schalter, gemessen gegen die `double`-Referenz.
- Adapter für FM1-Electro (ADR 0041): ersetzt `engine/tr808*` als Schalter nach ADR 0006, mit Last im Emulator und auf
  dem Gerät, abgestimmt mit dem Bau-Chat.
