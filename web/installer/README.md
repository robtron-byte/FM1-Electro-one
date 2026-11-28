# Web-Installer für das FM-1

**Seite:** https://robtron-byte.github.io/FM1-Electro-one/

Firmware auf das M-VAVE FM-1 aufspielen, in Chrome oder Edge, ohne Terminal und ohne dass Presets gelöscht werden:
Verbinden, Firmware wählen (oder eigene `.fwsc` hineinziehen), Installieren. Entscheidung: [ADR 0054](../../docs/adr/0054-web-installer.md).

| Datei | Wofür |
|---|---|
| `index.html`, `app.js` | Seite und Bedienung (Deutsch, Englisch umschaltbar) |
| `fm1ota.js` | USB-MIDI-Update (M-UPGRADE) über Web MIDI; aus Felucca `web/fm1ota.js`, angeglichen an `platform/tools/fm1_install.py` |
| `fm1pkg.js` | Paketprüfung (Kennung, Felucca-Lader, Prüfsummen), Port von `fm1_install.py` |
| `bilder/` | kleines Bild je Firmware (`<NAME>.svg`, 120 × 80, erzeugt mit `bilder/make_bilder.py`); fehlt eins, zeigt die Seite `allgemein.svg` |
| `bildschirme/` | Display des FM-1 je Firmware (`<NAME>.png`, 240 × 240, Emulator fm1-emu: `bildschirme/make_bildschirme.sh ~/fm1-emu firmware/<Paket>.fwsc`); fehlt eins, entfällt es |
| `beschreibungen.json` | Titel, ein Satz Beschreibung und Stimmenzahl je Paketname (`SH101`, `JX8P` …) |
| `make_site.py` | baut die Seite: jede gültige `firmware/NAME-VERSION.fwsc` landet automatisch in `liste.json` |
| `test/test.mjs`, `test/vectors.py` | Test ohne Gerät: gleiche Bytes und Abläufe wie `fm1_install.py` |

- **Neue Firmware:** `.fwsc` als `NAME-X.Y.Z-alpha.fwsc` nach `firmware/` legen und auf `main` pushen: die Seite liest
  den Ordner live aus dem Repository und zeigt je Firmware die neueste Version, ohne Deploy. Neuer Name: Zeile in
  `beschreibungen.json` (wirkt ebenfalls sofort). Nur Änderungen an der Seite selbst brauchen ein neues `gh-pages`
  (Workflow `installer-pages.yml`, der noch nach `.github/workflows/` muss, sonst von Hand, siehe ADR 0054). Für Beschreibung und Stimmenzahl eine Zeile in `beschreibungen.json` (sonst
  erscheint nur Name, Version, Kennung). Pakete ohne Felucca-Lader (z. B. `FM1_ELECTRO.fwsc` aus dem SDK-Build) fehlen.
- **Prüfen:** `node web/installer/test/test.mjs` (braucht Python 3).
- **Lokal ansehen:** `python3 web/installer/make_site.py /tmp/site` und dort `python3 -m http.server`; dann
  `http://localhost:8000` in Chrome.
