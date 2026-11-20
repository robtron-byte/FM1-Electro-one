# 0038: Stand bleibt beim Firmware-Update erhalten

- **Status:** vorgeschlagen
- **Stand:** 2026-11-20

## Kontext

Wunsch vom 20.11. (N30): Nach einem Firmware-Update soll der aktuelle Stand da sein. Geprüft wurde, was ein Update der eigenen Plattform (ADR 0021) schreibt:

- Der Update-Loader (`platform/firmware/loader/ldr_core.c`) schreibt nur den App-Bereich 0x4000–0x92FFF. Im Bereich 0x93000–0xFBFFF löscht er nur Sektoren, an deren Ende ein gültiger Update-Datensatz steht (Kennung und CRC).
- Die App legt das Update-Paket bei 0xE0000–0xE4FFF ab (`ota.c`).
- Die Grooves und eigenen Sounds liegen bei 0x97000–0x9EFFF (`fm1/el_app.c`, `EL_STORE_AT`). Das Update berührt sie also nicht. Jede neuere Firmware lädt alle Speicherstände ab Version 19.

Verloren ging bisher nur, was seit dem letzten SAVE geändert wurde.

## Entscheidung

Fragt der Installer ein Update an (`usb.ota_req`), ruft `platform_poll` vor `ota_session()` den neuen Hook `APP_BEFORE_UPDATE` auf. FM1-Electro (`el_before_update` in `fm1/el_app.c`) zeigt dann „SAVING BEFORE UPDATE“ und speichert alles wie SAVE (`el_glue_store` → `el_store_save`). Danach läuft das Update wie bisher. Bricht es ab, ist trotzdem nur gespeichert worden.

## Alternativen

- Automatisch speichern bei jedem STOP oder in festen Abständen: Flash-Verschleiß, kurze Tonlücken beim Löschen (ADR 0023), und ungewollte Zwischenstände würden SAVE überschreiben.
- Nichts ändern und im Hinweis „vor dem Update SAVE“ sagen: leicht zu vergessen.

## Folgen

- Vor dem Update steht der Ton kurz (Löschen und Schreiben, unter 1 s).
- Der Speicherstand muss außerhalb von 0x4000–0x92FFF und 0xE0000–0xE4FFF bleiben.
- Neue Speicherversionen müssen ältere weiter laden (wie bisher, `fm1/el_store.c`).
- `platform/firmware/src/platform.c` (Dinghy) hat dafür einen eigenen Hook bekommen, nach dem Muster von `APP_BOOT`.

## Verlauf

- 2026-11-20: angelegt
