# Übergabe: Stand 2026-11-06

Für eine neue Claude-Sitzung (anderer Account). Zuerst `CLAUDE.md` lesen, dann diese Datei, dann `docs/adr/0019`, `0021`, `0022`.

## Ziel

FM1-Electro soll auf dem M-VAVE FM-1 flüssig laufen (Sequencer im Tempo, kein Krachen). Klang ist zweitrangig, bis es rund
läuft; danach prüft der Nutzer den Klang. Weniger Stimmen sind erlaubt. Fremder GPL-3-Code darf übernommen werden (mit
Urheber, Herkunft, Lizenz).

## Stand

- **Eigene Plattform** statt SDK von FM1 Open: `platform/` (Dinghy, Felucca-Plattform), Adapter `fm1/el_app.c` +
  `fm1/el_glue.c`, ADR 0021. Build: `JIELI_TOOLCHAIN=… AC79_SDK=… JIELI_DOCKER=0 python3 tools/build_platform.py`
  (Toolchain und die drei SDK-Dateien: `platform/ORIGIN.md`). Ergebnis nach `firmware/FM1_ELECTRO_PLATTFORM.fwsc` kopieren.
- **Installieren:** M-Flasher → FLASH FIRMWARE → Datei → REVIEW & FLASH (findet er das Gerät nicht: OCT− und OCT+ 5 s
  halten). Rettung: OCT− beim Einschalten, oder RP2040 (Transporter).
- **Zweiter Kern:** Synths (RHYTHM, STRINGS, BASS) auf Kern 1; Kern 0 rechnet Drums und – einen Block versetzt – Effekte und
  Master des vorigen Blocks (`engine/electro.c`, `bus_set`, Host-Test „two cores“).
- **Audio-Code im SRAM:** alle Klang-Dateien (`e_*.o`) in `.dsp_text` (`platform/firmware/app.ld`), beim Start kopiert.
- **Drums in Festkomma:** Feluccas `drum_voice.c` (`third_party/felucca/`, `engine/dr_fixed.c`, ADR 0022). ZAP, PULSE,
  PO-12 unverändert.
- **STRINGS 3 Stimmen** auf der Plattform (`engine/electro.c`).
- **MKS-50:** Filter mit halber Rate (`MKS_VCF_DECIM` 2 auf der Plattform), Pulse 2/3 als ein Gatter (gleicher Klang).
- **Tasten-LEDs** (ADR 0020): inaktive Bereichstasten gedimmt (`fm1_led_bg`, 8 µs Puls), OCT blinkt am Ende der Oktave.
- Gerät, 2. Build vom 06.11.: DR13 RH4 ST43 BA3 FX20 MB15, läuft viel besser, aber noch Knackser (Überlastung, sagt der
  Nutzer). PO-12-Kit 120 %. **Becken und Closed Hat klingen nicht wie die 808**: Der Nutzer will das kalibrierte Modell
  (ADR 0006); Feluccas Stimmen (ADR 0022) waren gegen diese Regel Standard geworden.
- 4. Build vom 06.11.: PO-12 vorberechnet, MKS Saw 2–4 schneller Weg, OPTIONS zeigt PEAK/MAX/C1/LATE BLOCKS,
  **CY/OH/CH als unser Modell in Festkomma** (`engine/tr808_metal_fixed.h`, `make metal-fixed-check`). Repeat wie
  früher (die Änderung „nur solange Weiß gehalten“ hat der Nutzer zurückgenommen). Noch nicht auf dem Gerät.
- **Nächste Stimmen in Festkomma:** BD (der Nutzer: „die BD war vorher besser“), dann SD, Toms, RS, CP, CB. Vorgehen
  wie bei den Metall-Stimmen: gleiche Rechnung in Q20/Q28/Q30, Koeffizienten aus den Float-Funktionen, Vergleich je
  Einzelschlag gegen den Float-Build.
- **Cloud-Umgebung „FM1-Electro-one“** erlaubt pkgman.jieliapp.com, jl-update.oss-cn-shenzhen.aliyuncs.com (Toolchain) und
  gitee.com (AC79-SDK, reicht `--sparse` mit `cpu/wl82/tools`). fm1-emu: von GitHub klonen, `0bebe54`, Patch anwenden.

## Messungen auf dem Gerät (OPTIONS, Groove ROTHER läuft)

| Build | Last | DR | ST | FX | MB |
|---|---|---|---|---|---|
| SDK (alt) | 170–180 % | | | | |
| Plattform, 2 Kerne | 110 % | 66 | 71 | 20 | 15 |
| + Code im SRAM, STRINGS 3 | 112 % | 67 | 55 | 20 | 15 |
| + Festkomma-Drums | 102 % | 13 | 55 | 20 | 15 |

Das Gerät ist bei Gleitkomma-Code etwa 2× langsamer, als der Emulator fm1-emu zeigt (der modelliert FPU-Wartezeiten nicht).
Jetzt bremst Kern 1 (STRINGS 55 %).

## Nächste Schritte

1. Nutzer testet den Build vom 06.11. (Foto der OPTIONS-Zeile, Helligkeit der gedimmten Tasten).
2. STRINGS (MKS-50 `render_voice` in `engine/mks.c`) weiter verbilligen nach FM-106 (github.com/patttf/fm-106: Juno-Filter
   mit 11 kHz, 6 Stimmen ~30 %): Hüllkurve pro Block, Filter ¼ Rate prüfen, Sättigung billiger; ggf. Saw 2/3/5 ohne
   `chop_general`.
3. Lastanzeige je Kern (wie FM-106).
4. Effekte (FX 20 %) und Master (MB 15 %) verbilligen.
5. Danach: Klangabgleich Drums, ADR 0021/0022 und README nachführen, deutsche Installationsanleitung in `firmware/README.md`.

## Nützliche Quellen

Felucca (github.com/hugelton/Felucca, alles Festkomma), FM-106, SLOOP/X0X (zweiter Kern), Firmware-Liste
github.com/aroum/fm1-custom-fw.
