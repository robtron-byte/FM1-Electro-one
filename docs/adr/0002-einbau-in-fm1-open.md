# 0002: Einbau in FM1 Open: eigene App-Schicht statt der SDK-App

- **Status:** angenommen
- **Stand:** 2026-10-26

## Kontext

FM1-Electro läuft auf der M-VAVE-SDK-Bibliothek von FM1 Open (Bluetooth, USB, Flash, Display, Update). Die App-Teile von FM1 Open (Bildschirm, Presets, Projekte, Sampler, FM-Bibliothek, Projektübertragung) würden die Synth-Engines, Effekte und Puffer des SDK mitlinken: rund 100 KB SRAM, die FM1-Electro selbst braucht.

## Entscheidung

- `tools/prepare_fm1.py` kopiert den Code nach `app/electro`, setzt zwei Einhängepunkte (Audio in `app/board/audio.c`, MIDI in `app/midi_router.c`) und verschiebt die ersetzten App-Teile nach `fm1_electro_unused/`.
- `fm1/el_sdk_app.c` übernimmt deren Einstiegspunkte und wird als **`app/screen.c`** abgelegt. Diese Datei übersetzt `Build.ps1` ohne LTO. Nur so gewinnen ihre Definitionen (z. B. `fm1_app_sysex`) gegen die schwachen Standardfunktionen des SDK; mit LTO verwarf der Linker sie.
- `el_sdk_app.c` hält die Symbole verlinkt, die `Build.ps1` verlangt (BLE-API, `groove_tuning_apply`, `groove_project_apply_edit`), und ergänzt `groove_project_metadata`.
- Die SDK-Dateien selbst bleiben unverändert (`Build.ps1` prüft ihre Prüfsummen).

## Alternativen

- FM1 Opens App behalten und daneben laufen: passt nicht ins SRAM.
- `Build.ps1` ändern: geht nicht, das SDK prüft seine eigenen Dateien.

## Folgen

- Neue FM1-Open-Versionen brauchen eine Prüfung der Einhängepunkte (das Skript bricht ab, wenn sie nicht passen). Getestet: Commit `4c7496d` (V14).
- Prüfungen aus `Build.ps1` lassen sich unter Linux nachstellen (Pflicht-Symbole, Audio-Pfad im SRAM, Heap ≥ 48 KB).

## Verlauf

- 2026-10-26: App-Teile ausgelagert, `el_sdk_app.c` eingeführt
- 2026-10-26: als `app/screen.c` ohne LTO; Pflicht-Symbole verankert
