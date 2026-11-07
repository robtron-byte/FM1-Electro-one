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
- Gerät (11. Build): nur Drums, dichtes Kit (MIAMI, mit ZAP): über 100 %. 12. Build: ZAP und PULSE in Festkomma; Effekte und Master auf Kern 1, wenn der im vorigen Block weniger zu tun hatte (Emulator, nur Drums: 64 → 42 % auf Kern 0, bitgleich).
- **Strenger Emulator** (github.com/joakimeriksson/fm1-emu, neuer Stand, `cargo build --release -p fm1-live`, braucht libasound2-dev): `FM1_STRICT=1 FM1_BENCH_KEYS='1000:PLAY,1100:PLAY^,6000:HOME,6100:SAVE,6200:SAVE^,6300:HOME^' FM1_BENCH_PANEL=p.ppm fm1-live --fwsc … --bench 12000` zeigt OPTIONS mit der Last, wie das Gerät sie misst (MB 14 gegen Gerät 15). Nicht zusammen mit `FM1_BENCH_NOKEYS` (löscht das Skript). Profil: `FM1_TPROF=datei,4000,8000 … --shot 8100`, PCs auf `nm -n dinghy.elf`. Ergebnis 12. Build: DR31 RH7 ST38 BA5 FX18 MB14, PEAK 84 %. Master in Festkomma dort teurer (MB 18) → `MB_FIXED 0` (ADR 0019). Plattform bauen: `JIELI_DOCKER=0 python3 tools/build_platform.py` (nicht `platform/build.sh`).
- 13. Build: jeder Pattern-Platz behält seinen Sound (ADR 0024, Store-Version 21; Rückmeldung „Drum-Einstellungen setzen sich zurück von A zu B“). Offen für Performance: Effekte (fx_render) und STRINGS-Stimmen (Kern 1) sind die größten Posten; Festkomma nur dort, wo das strenge Modell es bestätigt.
- Gerät (13. Build) mit Verzerrung: FX34, PEAK 104 %, LATE 140. 14. Build: Bus geteilt auf beide Kerne (`bus_place`, ADR 0021), Bus im längsten Abschnitt des Blocks, Verzerrungs-Filter ausgerollt. Emulator mit Verzerrung PEAK 83 %. **Block-Protokoll:** vorübergehend `uint32_t el_block_log[512][8]` in `render_bus` (Gesamt, D, S, P, X, Q, Platz), `FM1_DUMPRAM=ram.bin fm1-live … --shot 7000`, Adresse per `nm` (RAM ab 0x01c00000); 32 KB passen nicht ins RAM, 16 KB schon. Gleichmäßig verteilt: Kern 0 ≈ D + Q (61 %), Kern 1 ≈ P + X + S (61 %). Größte Brocken: Synths (S, schwankt 30–67 % mit den STRINGS-Akkorden), Hall/Delay/Master (Q 32 %).
- 808-Quellen des Nutzers für spätere Abgleiche: `docs/TR808.md`, letzter Abschnitt (Mz808 ist AGPL: nur vergleichen, nicht übernehmen).
- 15. Build: Step-Varianz (ADR 0025; Drums `dr_machine_t.cond` Bitebenen, Synths Flag-Bits 3–5, Store-Version 22; Bedienung: Step halten + PRESETS), Tape-Stop-Standard 1/4, Regler-Beschleunigung in `edit_knob` (nur bei mehr als 16 Werten). ReplicantTr8s laut Nutzer für den 808-Klang verwenden (`docs/TR808.md`), aber keine Roland-Daten ins Repo.
- 16. Build: `MKS_CHOP_GRID` (Raster-Weg für Saw 5 / Sub 1–3, 5; `make chop-grid-check` bitgleich). Messung aller STRINGS-Sounds: Skript-Idee in ADR 0019 (PRESETS-Rasten per `e6:+1` nach ENV, dann HOME+SAVE). Teuerste jetzt ST45; Gerät liegt ~10 % über dem Emulator.
- Überlastschutz (bei hoher Last sparsamer rechnen): auf Wunsch des Nutzers vorerst nicht bauen (06.11.2026). Last weiter über Optimierungen ohne Klangänderung senken.
- Erledigt im 17. Build (war in Arbeit): `dsp_math.h` ohne Bibliotheks-`memcpy` (`__builtin_memcpy`), Hüllkurven-Zeiten der MKS-50 pro Stimme gemerkt, `chop_enabled` inline (alle 56 MKS-Sounds bitgleich; Emulator DARK STR ST 45 → 39 %, C1 81 → 68 %). Hall/Delay/Gate: eine Schleife je Effekt, Zustände in lokalen Variablen, `soft`/`delay_read` inline (70 Demo-WAVs bytegleich; Emulator FX 18 → 16, die Gate-Änderung noch nicht gemessen). Noch kein neuer Testbuild in `firmware/`: als Nächstes bauen, `check.sh`, messen, 17. Build.
- 18. Build: SAVE ohne Aussetzer (Vorab-Löschen, ADR 0023, `el_store_prepare`, `ui->prepare`), Display-SPI 20 MHz (`-DLCD_BAUD=2` in `tools/build_platform.py`). Falls das Display Fehler zeigt: zurück auf 4.
- 19. Build: Master-Schleife mit lokalen Zuständen (bitgleich). Weitere Kandidaten: Drums (dr_render/dm_render/db_render), Tasten-Timer (fm1_timer5_irq ~3.6 %).
- 20. Build: Step-Wahrscheinlichkeit (ADR 0025, Store-Version 23, Synths in Flag-/Akkord-Bits 6–7), große Anzeige beim Halten (`draw_step_overlay`).
- 21. Build: Step-Regler: rechts Durchläufe, links Prozent (ein Step hat eins), Anzeige nach ½ s getrennt.
- 22. Build: Step: PRESETS = Durchläufe, ALGORITHM = Prozent (kombinierbar), Anzeige nach ¼ s, zeigt den zuletzt gedrehten.
- 23. Build: Step-Aufnahme Pause = letzte Taste (UI_KEYS-1), Hervorhebung des gerade gedrehten Werts (`knob_at`, 700 ms).
- 24. Build: korrigiert; 20.–23. waren wegen eines unbemerkten Linkfehlers (`strlen` fehlt auf der Plattform) noch Build 19. Bauen jetzt nur mit Prüfung von Rückgabewert, frischer `dinghy.elf` und Log (kein 'undefined'/'failed'/'truncated').
- 25. Build: JP-6 inline (vco_tick/vcf), tr808 key_update ohne memcmp. Bauen mit Skript, das Linkfehler erkennt (siehe 24. Build).
- 26. Build: Branch claude/loving-hopper-0mxdty (ADR 0026, 0027) zusammengeführt (Merge), make test/web, check.sh ok.
