# mksultra: Alpha Juno / MKS-50 „Ultra“ – Auftrag und Stand

Wer daran arbeitet: [../ARBEITSTAFEL.md](../ARBEITSTAFEL.md). Wer übernimmt, liest nur diese Datei, `../README.md` und
`../baustein.h`, und beginnt mit einer Kopie von `../_vorlage/`.

## Ziel

Der Alpha-Juno/MKS-50-Klang des Rust-Nachbaus „MKS Ultra“ (TAL-Pha-Nachbau) als Baustein. Er soll klingen wie die Vorlage.
Vorgesehen sind die Firmware „Alpha Juno solo“ und später Kombinationen (`../varianten/`).

## Vorlage (Dropbox des Nutzers, `Programmieren/MKS50 App/TAL Pha Nachbau/app/`)

| Datei | Inhalt |
|---|---|
| `docs/SOUND_ENGINE_PORTING.md` (`id:bJM3TYf_3HoAAAAAABvb4g`) | Anleitung zum Portieren (zuerst lesen) |
| `engine/src/*.rs` | Code: 12 Stimmen, DCO bei 2× Fs, VCF 4× mit zwei nichtlinearen SVFs, `fastpow2`-Bit-Trick |
| `spec/*.md`, `MECHANICS.md` | Herleitung |
| `presets/**/*.mks` | 520 Presets (Patch-Daten sind erlaubt) |
| `docs/golden/` | 7 Golden-Renders mit FNV-1a-Prüfsummen |

## Vorgehen

1. In die Arbeitstafel eintragen und pushen.
2. `../_vorlage/` nach `mksultra/` kopieren und `name` durch `mksultra` ersetzen.
3. Den Rust-Code in C portieren, mit genau den Typen der Vorlage (`f32`/`f64` wie dort, sonst stimmen die
   Prüfsummen nicht). Die Stimmenzahl wird ein Laufzeit- oder Bauparameter (`MKSULTRA_VOICES`, Standard 12).
4. `docs/golden/` nach `ref/` kopieren und den Test gegen die FNV-1a-Prüfsummen schreiben.
5. Die Presets mit einem Skript in eine C-Tabelle umwandeln (`tools/`-Skript im Ordner `mksultra/`).

## Fürs FM-1 (späterer Auftrag, ADR 0043)

**Zuerst an den Stimmen sparen, nicht am Filter** (Wunsch des Nutzers 28.11.). Erst wenn das nicht reicht, über andere
Wege reden.

## Fertig, wenn

- `make -C bausteine mksultra` besteht: Die Golden-Prüfsummen stimmen, und die WAVs liegen in `bausteine/build/mksultra/`.
- Die Arbeitstafel ist nachgeführt, ebenso FEATURES Y11.

## Nicht anfassen

`engine/` (auch nicht `engine/mks.c`, der bisherige MKS-50 bleibt), `ui/`, `platform/`, `firmware/` und das
Haupt-`Makefile`.
