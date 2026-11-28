# 0044: Gemeinsames Gerüst der Solo- und Kombi-Firmwares (`bausteine/varianten/gemeinsam`), erste Variante SH-101 solo

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28 (SH-101 solo 0.1.1-alpha baut und besteht `tools/fm1-emu/check.sh`; am Gerät nicht geprüft)

## Kontext

Jeder Baustein soll eine eigene Firmware bekommen, die das FM-1 ganz nutzt, später auch Kombinationen (808 + 101,
808 + 101 + JX). Das private Repository fm1-synths hat dafür schon fertige Solo-Firmwares (MKS-50, JX-8P, MKS + JX) auf
der Plattform Dinghy / Felucca, mit Bedienraster (dort ADR 0008), Sequencer, Speichern, MIDI-Sync und Lastmessung. Die
Plattform in FM1-Electros `platform/` ist eine abweichende Fassung (Bau-Chat) und kennt z. B. keine wählbare Blockgröße.

## Entscheidung

- **Übernehmen statt neu erfinden:** `gemeinsam/platform/` und `gemeinsam/shared/` sind Kopien aus fm1-synths
  (Commit 2a7b4fa). `gemeinsam/app/gm_app.c` und `gm_ui.c` sind aus `mks50/app/mk_app.c` und `mk_ui.c` abgeleitet und
  kennen keinen Klangerzeuger: Kern-1-Start mit Selbsttest, Ereignis-Warteschlange zum Audio-Interrupt, MIDI (Kanal je
  Part, Pitch Bend, CC 1), Takt intern / MIDI-Clock mit Start/Stop, Arpeggiator, Step-Sequencer wie beim SH-101,
  Delay und Hall auf Kern 1, Werks- und eigene Sounds, Stand beim Ausschalten (Journal), Lastmessung, Raster
  (KNOB 1–4 = vier Felder, SELECT Seite, PRESETS Sound, ALGORITHM Part, Taste tippen = Seiten, halten = Ebene).
  Weggelassen gegenüber MKS-50: Akkorde, Sets, die MKS-eigenen Bilder.
- **Eine Variante** ist eine App-Datei, die Tabellen und Haken festlegt und `gm_app.c` einbindet (eine Übersetzungseinheit
  mit `platform.c`): Parameter, Grundklang, Werkssounds, Seiten, Ebenen, `kern_*` (Laden, Setzen, Noten, Rendern,
  Stimmen, eigene Bilder). Klangdateien werden als `dsp_*.o` ins SRAM gelegt. Bauen: `gemeinsam/tools/build.py`
  (nach fm1-synths `mks50/tools/build.py`); auf dem Mac startet es Docker über `tools/mac_docker.py` mit dem ganzen
  Repository unter demselben Pfad.
- **Speicherversionen:** eigene Sounds in Blob 0, immer 4000 Bytes (die Plattform lädt nur genau die gespeicherte
  Länge), mit Kopf (Kennung, Formatversion, Anzahl Parameter); ein älterer Stand mit weniger Parametern wird geladen
  und aus dem Grundklang ergänzt. Journal je Variante mit eigener Kennung (`GM_JR_MAGIC`); jeder Eintrag beginnt mit
  den Längen (Einstellungen, Parameter, Parts), so lädt eine spätere Fassung ältere Einträge. Sequenzen in Blob 3.
  Die Umwandlung ist bisher nur gebaut, nicht mit echten Altständen geprüft (es gibt noch keine).
- **Kennungen** (Update-Lader, Hub): FM-1_9x; vergeben 95 MKS-50, 96 FM1-Electro, 97 JX-8P, 98 MKS+JX, 99 Dinghy;
  **94 SH-101 solo**. Frei: 90–93 (Tabelle in `gemeinsam/README.md`). Es gibt nur noch vier.
- **Versionen** je Variante (`<variante>/VERSION`, X.Y.Z-alpha); Paket `firmware/<NAME>-<Version>.fwsc`. Die
  Release-Aktion (`.github/workflows/release.yml`) reagiert nur auf `FM1_ELECTRO-*`; für die Varianten legt sie keinen
  Release an (Sache des Bau-Chats).

## Alternativen

- FM1-Electros `platform/` mitbenutzen: gehört dem Bau-Chat, weicht ab (keine Blockgröße 64, eigener Update-Haken).
- Das MKS-50-Gerüst je Variante kopieren (wie fm1-synths jx8p): jede Änderung an der Bedienung in jeder Kopie
  nachziehen; gemeinsam ist billiger.

## Folgen

- Spätere Varianten (mksultra, jx8p, mks50, jp6, dk808, Kombis) schreiben nur ihre App-Datei und `tools/build.py`.
  Änderungen an `gemeinsam/` nur abwärtskompatibel; SH-101 neu bauen und prüfen.
- Kombis mit mehreren Parts: `GM_PARTS` > 1 ist vorgesehen (Kopfzeile mit Parts, ALGORITHM), aber noch nicht gebaut.
- Kennungen werden knapp (vier frei): Kombis eventuell unter einer Kennung bündeln.

## Verlauf

- 2026-11-28: angelegt (sh101-Chat).
- 2026-11-28: Blob 0 mit fester Länge, Journal-Einträge mit Längen vorn (vor der ersten Auslieferung).
- 2026-11-28 (jx8p-Chat): abwärtskompatibel ergänzt: `app.ld` nimmt `.ctrl_text` (EL_CTRL_CODE der Kerne aus `engine/`) in `.text`; `GM_SEND_FIRST` und `GM_FX_FIRST_ON` (erster Start: Part-Sends, Effekte an/aus), für JX-8P solo.
