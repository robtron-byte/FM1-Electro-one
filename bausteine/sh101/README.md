# sh101: SH-101 (MonoBass) – Auftrag und Stand

Wer daran arbeitet: [../ARBEITSTAFEL.md](../ARBEITSTAFEL.md). Wer übernimmt, liest nur diese Datei, `../README.md` und
`../baustein.h`, und beginnt mit einer Kopie von `../_vorlage/`.

## Ziel

Ein monophoner Synth nach Roland SH-101, als Baustein. Der Klang soll dem Swift-Nachbau „MonoBass“ des Nutzers entsprechen.
Das ist zugleich die Grundlage für die Firmware „808 + 101“ und für „101 solo“ (`../varianten/`).

## Vorlage (Dropbox des Nutzers, `Programmieren/REA/SH101/app/`)

| Datei | Inhalt |
|---|---|
| `docs/SOUND-ENGINE.md` (`id:bJM3TYf_3HoAAAAAABvbPg`) | Beschreibung der Klangerzeugung |
| `Sources/MonoBassCore/Oscillator.swift`, `Voice.swift`, `Engine.swift`, `Halfband.swift`, `Params.swift`, `Preset.swift` | Code |
| `docs/test-vectors/vectors.json`, `01-init.wav` … `07-poly-chord.wav` | Referenzen |

Finden: Dropbox-Suche nach dem Dateinamen im Pfad `/Ron Zöllner/Privat/PP/Programmieren/REA/SH101`.

## Vorgehen

1. In die Arbeitstafel eintragen und pushen.
2. `../_vorlage/` nach `sh101/` kopieren, `name` durch `sh101` ersetzen.
3. Swift-Quellen und `SOUND-ENGINE.md` lesen. Port in `double` mit denselben Formeln, Konstanten und derselben Reihenfolge:
   BLEP-Oszillatoren, 4× überabgetastete nichtlineare Ladder, 8× Clipper. Parameter als Tabelle 0…1 nach `baustein.h`.
4. `vectors.json` nach `ref/` kopieren und den Test gegen die Vektoren schreiben. Die WAVs ins Repository nur, wenn sie klein sind.
5. Teile, die man nicht immer braucht (z. B. Poly-Modus, Arpeggiator), über Schalter `SH101_WITH_*` abschaltbar machen.

## Fertig, wenn

- `make -C bausteine sh101` besteht: Die Testvektoren stimmen, und die WAVs liegen in `bausteine/build/sh101/`.
- Die Arbeitstafel ist nachgeführt, ebenso FEATURES Y10.

## Nicht anfassen

`engine/`, `ui/`, `platform/`, `firmware/` und das Haupt-`Makefile`. Ins Gerät kommt der Baustein erst mit einem eigenen
Auftrag (Adapter, ADR 0042).
