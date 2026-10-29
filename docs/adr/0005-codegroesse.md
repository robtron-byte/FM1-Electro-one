# 0005: Codegröße: Mathe-Helfer und 808-Stimmen ausgelagert

- **Status:** angenommen
- **Stand:** 2026-10-26

## Kontext

Die Firmware war 545 KB groß. Der Großteil des Überhangs kam aus Inlining: `dsp_exp2`, `dsp_sin`/`dsp_tan`, `dsp_sqrt`, `dsp_log2` wurden an jeder Aufrufstelle eingebettet. Code auf dem Audio-Pfad liegt zusätzlich im SRAM.

## Entscheidung

- Nur auf dem Gerät: die Mathe-Helfer als eigene Funktionen im SRAM (`DSP_MATH_FN` in `third_party/daisy808/dsp_math.h`), eine Kopie pro Datei.
- Die 808-Stimmen als eigene Funktionen (`EL_DSP_VOICE` in `engine/el_config.h`).
- Host und Simulator unverändert.

## Folgen

- 545 → 513 KB, rund 30 KB mehr Heap.
- Rechenlast im Emulator unverändert (78,8 % beim Spielen).
- Die 808-Stimmen brachten nur 2 KB (statt geschätzter 17 KB): Die Stimmen selbst sind größer als angenommen.

## Verlauf

- 2026-10-26: angelegt
