# Herkunft der Plattform

`platform/` ist **Dinghy** von Leo Kuroshita (@kurogedelic), Hügelton Instruments, mit der Plattform von
[Felucca](https://github.com/hugelton/Felucca), wie [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) und OberVave sie
tragen.

- Quelle: https://github.com/zvenson/dinghy, Commit `f77bf03` (2026-11-05)
- Lizenz: GPL-3.0-only (siehe `LICENSE` und `LICENSING.md` in diesem Ordner; die Dateiköpfe nennen Urheber und Lizenz)
- Übernommen für FM1-Electro nach `docs/adr/0021`. Eigene Änderungen an Dateien der Plattform stehen in deren Kopf und in
  der Git-Historie dieses Repositorys.

Für den Bau braucht die Plattform drei Dateien des JieLi-AC79-SDK (`AC79_SDK`). Ohne Zugang zu dessen Repository nimmt
FM1-Electro: `uboot.boot` aus FM1 Open (`sdk/tools/pack`, im SDK-Format kodiert; derselbe Bootlader wie M-VAVEs V16),
`cfg_tool.bin` aus FM1 Open (identisch mit der SDK-Version) und `cfg/eq_cfg_hw.bin` aus M-VAVEs Firmware.

## fm1-synths

Dieser Ordner kam aus FM1-Electro (github.com/robtron-byte/FM1-Electro-one, Commit `e7b3b14`, dort `platform/`): Dinghy
`f77bf03` mit den Änderungen von FM1-Electro (zweiter Kern aus SLOOP/X0X in `firmware/hal/fm1_cpu1.{h,S}`, die Abschnitte
`.c1_text`/`.dsp_text` in `firmware/app.ld`, `APP_BOOT` und das Kopieren beim Start in `firmware/src/platform.c`).

Änderungen in fm1-synths:

- `firmware/src/platform.c`: `HALF_FRAMES` kann die App vorgeben (MKS-50: 64 Frames statt 256, kürzere Latenz).
- `firmware/app.ld`: Audio-Code im SRAM sind alle Objekte `dsp_*.o` (vorher die Dateinamen von FM1-Electro).
- `firmware/src/usb.c`, `firmware/src/midi_uart.c`: Song Position Pointer (F2) erreicht die App über USB und TRS (vorher verworfen; MKS-50 ADR 0012).
- `firmware/src/gfx.c`: der Abschnitt der Zeichenfläche (`CV_SECTION`, Standard `.pool`) kann die App vorgeben (combo: `.noinit.cv`).
- `firmware/app.ld`: `.ram_bss` (NOLOAD) im Rest des RAM-Code-Bereichs für Puffer, die ihr Code selbst löscht (combo: die Diffusoren des Halls).
