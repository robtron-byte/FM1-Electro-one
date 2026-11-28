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
- 2026-11-28: kleines Bild je Firmware in der Liste (Wunsch des Nutzers): eigene schlichte SVG-Zeichnungen in
  `web/installer/bilder/` (keine Herstellerlogos), Rückfall `allgemein.svg` für neue Namen; kommen mit dem Deploy der Seite.
- 2026-11-28: Bilder erschienen beim Nutzer nicht: der Browser nahm das alte `app.js` aus dem Cache (GitHub Pages
  `max-age=600`, Module werden beim Neuladen nicht neu geprüft). `make_site.py` hängt jetzt eine Fassungskennung
  (`?v=…`, Hash der Seitendateien) an Skripte und Bilder.
- 2026-11-28: Display-Bild je Firmware (Wunsch des Nutzers): Startbildschirm aus dem Emulator fm1-emu (`--snap`),
  `web/installer/bildschirme/<NAME>.png`, erzeugt mit `bildschirme/make_bildschirme.sh`. Fehlt ein Bild, entfällt es.
  Die Bilder zeigen die Version zum Zeitpunkt der Aufnahme; bei neuen Versionen neu aufnehmen.
- 2026-11-28: Anleitung je Firmware (Wunsch des Nutzers über den Koordinator): Quelle
  `bausteine/varianten/<name>/ANLEITUNG.md` + `.en.md` (FM1-Electro: `web/installer/anleitungen/`), aus dem Code
  geschrieben; `anleitung.py` erzeugt beim Bau `anleitung/<NAME>.html` (Deutsch/Englisch, druckbar, Handy), Link
  „Anleitung“ an jedem Listeneintrag. Pflicht zum Nachführen in `bausteine/varianten/README.md`.
- 2026-11-28: Versionsvergleich nach „Verbinden“ (Wunsch des Nutzers über den Koordinator). Ohne Firmware-Änderung
  meldet das Gerät nur die Kennung (cmd 0x11, `FM-1_9NN`, je Firmware fest, nicht je Version); mehr Text in den
  27 Bytes der Kennung würde Felucca-/Hub-Installer brechen (sie lesen das ganze Feld). Deshalb ein **neues Kommando
  cmd 0x12 „Info“** in `bausteine/varianten/gemeinsam/platform/firmware/src/ota.c`: Antwort `APP_VER \0 APP_DATE`
  (≤ 32 Byte). `gemeinsam/tools/build.py` setzt `APP_DATE` (Build-Datum, `GM_APP_DATE` überschreibt). Gilt für alle
  Firmwares auf dem Gerüst (SH-101, MKS Ultra, JP-6, JXSH, JX-8P, MKS-50) ab ihrem nächsten Paket; Firmwares ohne
  es (FM1-Electro, ältere Pakete) schweigen, die Seite zeigt „Version unbekannt, ältere Firmware“. Die Seite zeigt
  „Auf dem FM-1: <Firmware> <Version> vom <Datum> · Neueste hier: <Version> vom <Datum> → Update möglich/aktuell“,
  hebt den Eintrag hervor und bietet „Update auf X“. Datum je Paket: `daten.json` (git log beim Bau), für neuere Pakete
  Commit-Datum über die API. Test: `ota.c` wird mit dem Host-Compiler übersetzt und beantwortet die Abfrage selbst
  (`test/ota_info_host.c`); mit und ohne `APP_DATE`; 104 Prüfungen.
- 2026-11-28: Fehler nach Rückmeldung des Nutzers (JXSH 0.1.1 installiert, Seite schlug „installieren“ vor):
  **Ist die Version nicht abfragbar, schlägt die Seite nie ein Update vor**, sie zeigt neutral „Auf dem FM-1: <Firmware>
  (Version nicht abfragbar)“. Ohne cmd 0x12 lässt sich die Version nicht bestimmen: die Kennung ist über alle Versionen
  gleich (SH101 0.1.0–0.1.4 alle FM-1_941, JXSH 0.1.0/0.1.1 FM-1_991, aus der Git-Geschichte geprüft), und M-UPGRADE
  liest nichts vom Gerät (das Gerät fragt, der Rechner antwortet). Kein großer Knopf oben mehr: oben nur die
  Vergleichszeile, am Eintrag der installierten Firmware ein kleiner Knopf „Update auf X“ bzw. „Installiert“
  (ausgegraut). Feste Reihenfolge der Liste über das Feld `reihe` in `beschreibungen.json` (FM1-Electro, Kombis,
  Solos), nach dem Verbinden die installierte Firmware oben. Logik in `web/installer/logik.js`, im Test geprüft (113).
- 2026-11-28: Bedienung vereinfacht (Präzisierung des Nutzers): jeder Eintrag hat seinen Knopf in derselben Zeile:
  „Installieren“ (nicht drauf, nicht verbunden oder Version unbekannt, ohne Update-Behauptung), „Update auf X“ (drauf,
  neuere Version hier), „Installiert“ ausgegraut (gleiche Version). Der getrennte Auswahl- und Installierschritt
  entfällt; eine abgelegte eigene `.fwsc` erscheint als eigene Karte mit „Installieren“. Ist noch nicht verbunden, fragt
  der Knopf selbst den MIDI-Zugriff an. `buttonFor` in `logik.js`, Test 120 Prüfungen; im Browser mit simuliertem FM-1
  von JXSH auf JX-8P durchinstalliert.
- 2026-11-28: Gestaltung überarbeitet (Rückmeldung des Nutzers): retro-modern nach dem Display der Firmwares,
  Farben aus `gemeinsam/app/gm_theme.h` (Schwarz, Orange #FF5E21 für Knöpfe mit dunkler Schrift, Grün für „auf dem
  FM-1“), Pixelschrift (Google Fonts „Silkscreen“) nur für Überschriften und Firmware-Namen, „VT323“ für Versionen,
  Kennungen und Kennzeichen, Fließtext in der Systemschrift. 8-px-Raster, Karte je Firmware (Icon und Display,
  Name und Version, Beschreibung, Kennzeichen, darunter die Knöpfe mit Abstand), hell/dunkel, Handy/Desktop; die
  Anleitungsseiten im gleichen Stil.
- 2026-11-28: Geteilte Kennungen (Hinweis jx8p-/dk808-Chat: DK808 und DK101 melden beide FM-1_911): die Info-Antwort
  cmd 0x12 trägt zusätzlich den Paketnamen, `APP_VER \0 APP_DATE \0 APP_NAME` (`gemeinsam/tools/build.py` setzt
  `APP_NAME` = Name der Variante, nur wenn alles in 32 Byte passt). Die Seite erkennt die laufende Firmware zuerst am
  Namen (`installedName` in `logik.js`), sonst an der Kennung, aber nur wenn genau ein Eintrag sie hat; bei geteilter
  Kennung ohne Namen gilt keiner als installiert (nur „Installieren“, keine Behauptung). Test 135 Prüfungen.
- 2026-11-28: Farben je Bereich, kein Orange mehr (Wunsch des Nutzers): 1 Verbinden blau (T_BLUE), 2 Firmware
  installieren grün (T_GREEN), „Wenn etwas schiefgeht“ und Anleitungen violett (T_VIOLET), jeweils als Nummer, Rand
  links und Knopffarbe; im Hellmodus dunklere Töne mit weißer Schrift, im Dunkelmodus die Display-Farben mit dunkler
  Schrift. Installieren-Knöpfe (auch für eine abgelegte Datei) sind bis „Verbinden“ ausgegraut („Erst verbinden“),
  „Anleitung“ bleibt aktiv.
- 2026-11-28: 808 (DK808): Icon „Mini-TR-808“ (`bilder/make_bilder.py`: dunkles Gehäuse, zwei Reglerreihen, 16
  Step-Tasten in Rot/Orange/Gelb/Weiß, Schrift nur „808“; das Orange gehört zur Nachbildung, die Seite bleibt ohne)
  und Display aus dem Emulator (Seite STEPS mit Pattern).
  **So entstehen Bild und Display für neue Pakete (DK101, DKJXSH …):** Der Chat, der ein neues Paket nach
  `firmware/` legt, (1) ergänzt in `web/installer/bilder/make_bilder.py` eine Zeichnung `B["<NAME>"]` und lässt das
  Skript laufen, (2) erzeugt das Display mit `web/installer/bildschirme/make_bildschirme.sh ~/fm1-emu
  firmware/<Paket>.fwsc`; soll das Bild eine bestimmte Seite zeigen, legt er `bausteine/varianten/<name>/BILDSCHIRM`
  an (`KEYS='…'` für fm1-emu `--keys`, `MS=…`; Beispiel `dk808/BILDSCHIRM`), (3) trägt Titel, Text, Stimmen und
  `reihe` in `beschreibungen.json` ein. Fehlt etwas, erscheint das Paket trotzdem (allgemeines Icon, kein Display);
  `make_site.py` meldet beim Bau „fehlt: Bild/Display“. Automatisch im Pages-Bau geht es nicht, weil dort kein
  Emulator läuft.
- 2026-11-28: MKS-50 (solo) nicht mehr gelistet (Wunsch des Nutzers, er baut mit MKS Ultra eine neuere): Feld
  `versteckt: true` in `beschreibungen.json`, beachtet in der Live-Liste (`visible`/Filter in `app.js`) und in
  `make_site.py`. Das Paket bleibt in `firmware/`, eine abgelegte eigene Datei lässt sich weiter installieren.
