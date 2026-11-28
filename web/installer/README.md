# Web-Installer für das FM-1

**Seite:** https://robtron-byte.github.io/FM1-Electro-one/

Firmware auf das M-VAVE FM-1 aufspielen, in Chrome oder Edge, ohne Terminal und ohne dass Presets gelöscht werden:
Verbinden, Firmware wählen (oder eigene `.fwsc` hineinziehen), Installieren. Entscheidung: [ADR 0054](../../docs/adr/0054-web-installer.md).

| Datei | Wofür |
|---|---|
| `index.html`, `app.js` | Seite und Bedienung (Deutsch, Englisch umschaltbar) |
| `fm1ota.js` | USB-MIDI-Update (M-UPGRADE) über Web MIDI; aus Felucca `web/fm1ota.js`, angeglichen an `platform/tools/fm1_install.py` |
| `fm1pkg.js` | Paketprüfung (Kennung, Felucca-Lader, Prüfsummen), Port von `fm1_install.py` |
| `beschreibungen.json` | Titel, ein Satz Beschreibung und Stimmenzahl je Paketname (`SH101`, `JX8P` …) |
| `make_site.py` | baut die Seite: jede gültige `firmware/NAME-VERSION.fwsc` landet automatisch in `liste.json` |
| `test/test.mjs`, `test/vectors.py` | Test ohne Gerät: gleiche Bytes und Abläufe wie `fm1_install.py` |

- **Neue Firmware:** `.fwsc` nach `firmware/` legen und pushen; der Workflow `.github/workflows/installer-pages.yml`
  baut und veröffentlicht die Seite. Für Beschreibung und Stimmenzahl eine Zeile in `beschreibungen.json` (sonst
  erscheint nur Name, Version, Kennung). Pakete ohne Felucca-Lader (z. B. `FM1_ELECTRO.fwsc` aus dem SDK-Build) fehlen.
- **Prüfen:** `node web/installer/test/test.mjs` (braucht Python 3).
- **Lokal ansehen:** `python3 web/installer/make_site.py /tmp/site` und dort `python3 -m http.server`; dann
  `http://localhost:8000` in Chrome.
