# 0021: Eigene Grundlage ohne das SDK von FM1 Open (Felucca-Plattform, Ausgangspunkt Dinghy)

- **Status:** vorgeschlagen
- **Stand:** 2026-11-05

## Kontext

Auf dem SDK von FM1 Open ist FM1-Electro beim Spielen etwa 1,75-fach überlastet (ADR 0019). Die Optimierungen ohne Klangänderung haben 9 % gebracht. Gründe:

- **Zweiter Kern:** Das SDK hält seine Task-Funktionen intern (`os_task_create`, `thread_fork` sind lokale Symbole). Der zweite Kern ist für uns nicht nutzbar.
- **Speicher:** Das SDK belegt viel SRAM (eigene Effekte `fm1_effects` 48 KB, Bluetooth-Stack, Puffer). Es bleiben 80 KB Heap, und der Start lag einmal nur 1 KB vor dem Absturz.
- **Lizenz:** Wegen des nicht-freien SDK steht FM1-Electro unter GPL-3.0-only mit Linking-Exception. Fremden GPL-Code ohne diese Ausnahme konnten wir deshalb nicht übernehmen (ADR 0006).

Die Firmwares Felucca, SLOOP, X0X, OberVave und Dinghy laufen ohne SDK auf einer gemeinsamen Plattform von Leo Kuroshita (Hügelton Instruments, GPL-3.0-only). Sie umfasst:

- `firmware/hal/fm1_*.h`: Register-Ebene für USB, LCD, Panel, ADC, Audio, Flash, Interrupts, Wächter, Timer;
- `firmware/src`: USB-MIDI/CDC/Audio, Display, Update-Weg, USB-Rettung;
- `firmware/loader`: der Update-Lader;
- `tools/build.py`, `fm1pkg_make.py`, `fm1_install.py`;
- dieselbe JieLi-Linux-Toolchain (clang 4.0.1 pi32v2), die FM1-Electro schon benutzt.

Die einzelnen Projekte:

- **Dinghy** (kurogedelic, 0.4, Commit 22acb91) ist ausdrücklich „die kleinste Firmware für das FM-1, um eigene darauf zu bauen“:
  - die Plattform ohne Instrument (ein Sinus in `dinghy.c`);
  - Update-Weg und USB-Rettung (OCT− beim Einschalten, nach zwei Abstürzen beim Start automatisch);
  - USB- und TRS-MIDI mit Clock, USB-Audio;
  - Panel, Display, Flash-Speicher (Settings, Blobs, Slots mit CRC).
- **SLOOP** (isod89, 2.8.5) und **X0X** (Charles Vestal, 1.1.0) bringen zusätzlich mit:
  - den **zweiten Kern** (`hal/fm1_cpu1.{h,S}`, in SLOOP 2.8.3 gehärtet: Prüfwort je Auftrag, Rückfall auf einen Kern bei einem fehlerhaften Kern 1);
  - Float-DSP (`-mcpu=r3 -mfprev1`).
  - X0X: 115 % → 68 % Last mit dem zweiten Kern.
- **Speicheraufteilung der Plattform** (`app.ld`):

  | Bereich | Größe |
  |---|---|
  | Code im Flash (XIP) | 568 KB |
  | Code im SRAM | 24 KB |
  | `.data` + `.bss` | 96 KB |
  | **Pool für große Puffer** | **336 KB** |
  | NOINIT | 15 KB |

  Die Engine (≈100 KB) und ihre Effektspeicher passen in den Pool.

## Entscheidung

Vorgeschlagen, das Projekt startet auf Zustimmung des Nutzers:

1. **Plattform übernehmen:** Dinghy 0.4 kommt als `platform/` ins Repository, mit Urheber, Herkunft und Lizenz in jeder Datei und in `LICENSES`. FM1-Electro steht damit insgesamt unter GPL-3.0-only. Unsere Dateien behalten ihren Kopf; die Linking-Exception wird nicht mehr gebraucht.
2. **Adapter** `fm1/el_platform.c`: die wenigen Aufrufe, die FM1-Electro heute ans SDK richtet, auf der Plattform:
   - `fm1_controls_read`, `fm1_led_set`;
   - `fm1_screen_submit`/`pending`;
   - `fm1_flash_*`, `fm1_millis`;
   - Audio- und MIDI-Rückruf.

   UI, Engine und Store bleiben unverändert. Den SDK-Build gibt es weiter, bis der neue auf dem Gerät besteht.
3. **Zweiter Kern** aus SLOOP (2.8.3+): Die Synth-Bereiche rechnen auf Kern 1, Drums, Effekte und Master auf Kern 0. Wenn Kern 1 Fehler macht, rechnet Kern 0 allein weiter.
4. **Heißer Code im SRAM:** Der CPU-Test zeigt die Schleife aus dem Flash halb so schnell wie aus dem SRAM (21,9 gegen 10,8 ns). Der SRAM-Code-Bereich wird deshalb aus dem Pool vergrößert, damit die Render-Schleifen dort liegen.
5. **Prüfen:**
   - im Emulator fm1-emu (die Plattform läuft dort, Felucca ist sein Hauptziel);
   - mit dem Kostenmodell (ADR 0019);
   - auf dem Gerät mit der Lastanzeige.
6. **Installieren** über den Update-Weg der Plattform (`fm1_install.py`, Web-Installer, FM-1 Firmware Hub). Zurück zur Original-Firmware geht es über denselben Weg oder mit dem RP2040 (Transporter).

## Alternativen

- **Auf dem SDK bleiben und beim Klang sparen** (Weg A): halbe Rate für Synth-Stimmen und tiefe Drums, Master aus. Das ergibt etwa 100 % Last, ohne Reserve.
- **SLOOP oder X0X als Ausgangspunkt statt Dinghy:** Beide haben den zweiten Kern schon eingebaut, tragen aber ein ganzes eigenes Instrument mit, das erst herausgeschnitten werden müsste. Dinghy ist die saubere Grundlage; der Kern-1-Teil kommt aus SLOOP dazu.

## Folgen

- Bluetooth (BLE-MIDI) und die SDK-eigenen Funktionen von FM1 Open entfallen, solange die Plattform sie nicht hat. Dinghy hat kein Bluetooth.
- Die Plattform wird von der Felucca-Familie gepflegt. Fehlerbehebungen dort (z. B. der neue Flash-Chip in SLOOP 2.8.5) übernehmen wir bewusst nach.
- `tools/package_fm1.py` und der SDK-Build werden nach dem Wechsel nicht mehr gebraucht.

## Verlauf

- 2026-11-05: angelegt nach der Prüfung von Dinghy, SLOOP und X0X, auf Wunsch des Nutzers (Weg B)
- 2026-11-06: Effekte und Master (des vorigen Blocks) laufen auf dem Kern, der im vorigen Block weniger zu tun hatte: auf Kern 1 nach den Synths, wenn diese weniger Zeit brauchten als die Drums auf Kern 0 (nur Drums, dichtes Kit), sonst wie bisher auf Kern 0 (`engine/electro.c` `render_sources`). Bitgleich (Emulator-Aufnahme). Emulator, nur Drums: Kern 0 64 → 42 %
