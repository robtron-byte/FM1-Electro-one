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
- **Liste live aus dem Repository (Wunsch des Nutzers 28.11.: „immer die aktuellste“):** Die Seite liest beim Öffnen
  `firmware/` über die GitHub-API (Zweig `main`) und `web/installer/beschreibungen.json` von raw.githubusercontent.com.
  Je Firmware-Name erscheint nur die **neueste Version**; die Kennung kommt aus dem ersten KB des Pakets (Range-Abruf).
  Beim Wählen wird das Paket geladen, gegen die Git-Prüfsumme der API und mit `fm1pkg.js` geprüft. Ein neues Paket ist
  also sichtbar, sobald es auf `main` liegt, ohne Deploy. Rückfall, wenn GitHub nicht erreichbar ist (oder das Limit von
  60 API-Abrufen je Stunde und Adresse erreicht ist): `liste.json` und die Pakete aus dem letzten Deploy (`make_site.py`).
  Eine eigene `.fwsc` kann überall auf der Seite abgelegt werden.
- **Kennung ≠ Version:** Das Gerät meldet nur die Kennung (z. B. FM-1_972 für JX-8P 0.2.6 und 0.2.7). „Verbinden“
  zeigt deshalb die Firmware und dazu „Neueste Version hier: …“, nicht die installierte Version.
- **Deploy:** Pages-Quelle ist der Zweig `gh-pages` (Wurzel); er enthält nur die gebaute Seite (ein Commit,
  `make_site.py`). Der Workflow `web/installer/installer-pages.yml` baut bei jedem Push auf `main` mit Änderungen in
  `firmware/` oder `web/installer/` erst den Test, dann die Seite, und schreibt `gh-pages` neu. **Er liegt noch nicht in
  `.github/workflows/`:** der Git-Schlüssel des Chats hat keinen Scope `workflow`, GitHub lehnte den Push ab. Bis jemand
  ihn einmal nach `.github/workflows/installer-pages.yml` verschiebt (Weboberfläche oder Schlüssel mit Scope), wird
  `gh-pages` von Hand gebaut: `python3 web/installer/make_site.py /tmp/site`, dort `git init -b gh-pages`, Commit,
  `git push -f origin gh-pages`. Seit der Live-Liste braucht es das nur noch für Änderungen an der Seite selbst. Der Workflow ist noch nie gelaufen; nach dem Verschieben prüfen, dass `liste.json`
  auf der Seite sich ändert. Alles läuft im Browser, nichts geht an einen Server.
- **Test ohne Gerät:** `node web/installer/test/test.mjs` lässt `test/vectors.py` mit dem unveränderten `fm1_install.py`
  rechnen und vergleicht: Prüfergebnis und logisches Abbild jedes Pakets samt kaputter Abwandlungen (Bit gekippt, Kopf,
  abgeschnitten, zu kurz), Antwort-Bytes, und ganze simulierte Läufe (normal, Weiterschreiben im Lader, falsches Modell,
  fremder Lader, Abziehen beim Schreiben): jede gesendete Nachricht Byte für Byte gleich; dieselben Läufe zusätzlich durch die Web-MIDI-Schicht (`WebMidiBackend`/`WebMidiLink`) mit
  nachgebildetem `MIDIAccess`, der Lader unter neuem Portnamen. Stand 28.11.: 98 Prüfungen gleich (mit JXSH).

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

- 2026-11-28: angelegt (Web-Installer-Chat). Pages per Push des Zweigs `gh-pages` eingeschaltet (Quelle Zweig statt
  Actions, weil der Workflow ohne Scope `workflow` nicht gepusht werden konnte). Seite online, zwei Pakete bytegleich
  mit dem Repository geprüft. Seite im Browser mit nachgebildetem Web MIDI durchgespielt (Verbinden, SH-101 → JX-8P,
  Balken, „Fertig“, danach JX-8P 0.2.6-alpha erkannt).
- 2026-11-28: Liste live aus `main` (neueste Version je Firmware, Git-Prüfsumme), Ablegen überall auf der Seite,
  „Verbinden“ nennt Firmware + neueste Version statt einer Version, die das Gerät nicht meldet. JXSH in die Beschreibungen.
