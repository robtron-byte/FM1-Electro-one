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
- **Die ganze 808 in Festkomma** (`engine/tr808_metal_fixed.h`, `tr808_bd_fixed.h`, `tr808_rest_fixed.h`; `make metal-fixed-check`, 36 Fälle). Gerät (7. Build): DR49 RH8 ST43 BA6 FX20 MB15, PEAK 100 %, MAX 103 %, C1 83 %, LATE BLOCKS 11. 8. Build verbilligt (Emulator 91 → 82 %). **Profil:** `FM1_COST_DUMP=datei fm1-emu …`, PCs auf `nm -n dinghy.elf` legen; größte Posten fx_render ~22 %, mb_process ~19 %, Drums ~27 %, Tasten-Timer ~9 %. Nächste Ideen: Hüllkurven pro Block, Hat-Ketten, ungenutzten Felucca-Code raus, ggf. Arbeit auf Kern 1. Danach Abgleich an den Aufnahmen des Nutzers (BD, OH, Becken, SD) als A/B.
- **Referenzaufnahmen** einer echten 808: Dropbox-Ordner des Nutzers „808 best of“ (BD, SD, HH = Open Hats, Cymbal,
  Low Tom/Conga), per Dropbox-Erweiterung zu finden; Download als ZIP über den geteilten Link (`dl=1`) braucht
  uneingeschränktes Netz (danach wieder einschränken). Closed Hats: vier AIF-Dateien vom Nutzer (CH-Abgleich, ADR 0014). Vorgehen
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
- Gerät (8. Build): DR34, PEAK 91 %, MAX 102 %, LATE BLOCKS steigen vor allem beim Hall-Wurf und beim Verlassen von OPTIONS (Flash-Löschen sperrt Interrupts ~50 ms/Sektor). 9. Build: Speichern im Lauf verschoben (ADR 0023), Hall-Wurf billiger.
- 10. Build: Sequencer-Balken ohne Versatz (Position je Bild festgehalten, `ui->show_pos`), Verzerrung billiger (Kennlinien-Tabellen `DIST_TABLE`, gespiegelte Ringpuffer). Emulator: Groove 82 %, mit Verzerrung 94 %, mit Hall-Wurf 87 % (über 5 s gemittelt).
- Gerät (10. Build): ohne Verzerrung DR41 RH8 ST19 BA12 FX16 MB15, PEAK 83 %, C1 62 %; mit voller Verzerrung MAX 122 %, LATE BLOCKS 930. 11. Build: Verzerrung in Festkomma (`DIST_FIXED`, `make dist-fixed-check`). Falls noch zu teuer: die Verzerrung auf Kern 1 verlegen (der hat mehr Luft), mit einem Block Verzögerung ihres Rücklaufs.
