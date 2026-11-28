# 0054: Web-Installer im Browser (Web MIDI, GitHub Pages)

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28

## Kontext

Die Firmwares (FM1-Electro, SH-101, JX-8P, MKS-50, JP-6, MKS Ultra, später Kombis) liegen als `.fwsc` in `firmware/`.
Bisher installiert der Nutzer mit dem M-Flasher, der die Presets löscht. Der FM-1 Firmware Hub installiert nur Firmwares
aus seiner eigenen Liste. `platform/tools/fm1_install.py` schreibt über USB-MIDI (M-UPGRADE) nur den Programmbereich,
braucht aber ein Terminal; der Nutzer kann nicht mit dem Terminal umgehen.

## Entscheidung

Eine öffentliche Seite `web/installer/` auf GitHub Pages: https://robtron-byte.github.io/FM1-Electro-one/

- **Ablauf:** Chrome/Edge, FM-1 per USB. „Verbinden“ zeigt die laufende Firmware (Kennung → Name und Version aus der
  Liste, sonst Familie nach Kennung). Firmware aus der Liste wählen oder eigene `.fwsc` hineinziehen. „Installieren“ mit
  Rückfrage, Fortschrittsbalken, „Fertig“. Hinweis „Während des Schreibens nicht abziehen“, Rettungsweg (OCT− beim
  Einschalten) und Weiterschreiben nach Abbruch erklärt. Deutsch, Englisch umschaltbar. Ohne Web MIDI ein klarer Hinweis.
- **Update:** `fm1ota.js` ist der Web-Installer der Felucca-Vorlage (github.com/hugelton/Felucca, `web/fm1ota.js`,
  Leo Kuroshita / Hügelton Instruments, GPL-3.0), angeglichen an `fm1_install.py`: Port-Suche (`PORT_RE`) und Paarung
  (`pair_output`), Modellprüfung (`same_keyboard`), Weiterschreiben nur im eigenen Lader (`OWN_LOADER_RE`), fremder Lader
  wird abgelehnt, Fortschritt in Prozent, Wartezeiten einstellbar. Schreibt wie das Python-Werkzeug nur den Programmbereich.
- **Paketprüfung vor dem Schreiben** (`fm1pkg.js`, Port von `load_package`/`package_damage`): Kennung, Felucca-Lader im
  Paket (`FELUCCA-LOADER-1`), Kopf-, Dateilisten- und Datei-CRC. Fremde oder kaputte Dateien: Abbruch, nichts geschrieben.
  Pakete aus der Liste zusätzlich per SHA-256 gegen `liste.json`.
- **Liste:** `make_site.py` liest beim Deploy jede `firmware/NAME-VERSION.fwsc`, prüft sie mit `fm1_install.py` und
  schreibt `liste.json`; Beschreibung und Stimmenzahl aus `beschreibungen.json`. Neue Pakete erscheinen von selbst.
- **Deploy:** `.github/workflows/installer-pages.yml` (Push auf `main` bei Änderungen in `firmware/`, `web/installer/`),
  erst Test, dann Pages (Quelle: GitHub Actions). Alles läuft im Browser, nichts geht an einen Server.
- **Test ohne Gerät:** `node web/installer/test/test.mjs` lässt `test/vectors.py` mit dem unveränderten `fm1_install.py`
  rechnen und vergleicht: Prüfergebnis und logisches Abbild jedes Pakets samt kaputter Abwandlungen (Bit gekippt, Kopf,
  abgeschnitten, zu kurz), Antwort-Bytes, und ganze simulierte Läufe (normal, Weiterschreiben im Lader, falsches Modell,
  fremder Lader, Abziehen beim Schreiben): jede gesendete Nachricht Byte für Byte gleich. Stand 28.11.: 77 Prüfungen gleich.

## Alternativen

- M-Flasher: löscht die Presets. FM-1 Firmware Hub: nur seine eigene Liste. `fm1_install.py`: braucht Terminal.
- Eine eigene App (Electron o. ä.): Installation nötig, mehr Pflege; Web MIDI in Chrome/Edge reicht.

## Folgen

- Safari und Firefox (kein Web MIDI mit SysEx) gehen nicht; die Seite sagt das.
- **Kein Gerätetest:** Der Ablauf ist nur gegen `fm1_install.py` und ein simuliertes Gerät geprüft, nicht an einem echten
  FM-1 (in dieser Sitzung nicht möglich). Der erste echte Lauf sollte mit einer Firmware geschehen, die sich schon mit
  `fm1_install.py` installieren ließ; der Rettungsweg OCT− bleibt.
- Ob gespeicherte Daten nach dem Wechsel auf eine andere Firmware lesbar sind, hängt von der Firmware ab; gelöscht werden
  sie nicht. Die Seite verspricht nicht mehr.
- Stimmenzahl und Beschreibung in `beschreibungen.json` bei neuen Versionen nachführen.
- Änderungen an `fm1_install.py` (Plattform) sollten in `fm1ota.js`/`fm1pkg.js` nachgezogen werden; der Test zeigt Abweichungen.

## Verlauf

- 2026-11-28: angelegt (Web-Installer-Chat)
