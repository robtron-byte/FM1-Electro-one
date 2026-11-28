# Firmware-Varianten

Eine Variante = welche Bausteine in einer Firmware stecken, auf welchen Bereichen, mit welchen Schaltern. Alle bauen auf
dem Gerüst [`gemeinsam/`](gemeinsam/) auf (ADR 0044); eine Variante ist ein Ordner mit App-Datei und `tools/build.py`.

| Variante | Bausteine | Gedacht für | Stand |
|---|---|---|---|
| FM1-Electro (heute) | bisherige 808, JX-8P, JP-6 | wie bisher | läuft (`firmware/`) |
| 808 + 101 | dk808, sh101 | Drums und Bass, viel Reserve | geplant |
| 808 + 101 + JX | dk808, sh101, JX-8P | Drums, Bass, Flächen | geplant |
| JX + 101 | jx8p, sh101 | Flächen und Bass, Bedienung wie fm1-synths MKS+JX, Sequencer der iOS-Apps | geplant |
| Alpha Juno solo | mksultra (mehr Stimmen) | voller Synth-Klang | geplant |
| 808 solo | dk808 mit allen 17 Instrumenten | volle Drum-Machine | gebaut: [`dk808/`](dk808/), `firmware/DK808-0.1.0-alpha.fwsc` (FM-1_91, zwei Kerne, Sequencer 16 Steps, Emulator ok, Gerät ungeprüft) |
| 101 solo | sh101 | voller Bass/Lead | gebaut: [`sh101/`](sh101/), `firmware/SH101-0.1.1-alpha.fwsc` (Emulator ok, Gerät ungeprüft) |
| JX-8P solo | jx8p (alle Stimmen) | voller JX-8P | gebaut: [`jx8p/`](jx8p/), `firmware/JX8P-0.2.8-alpha.fwsc` (fm1-synths fortgeführt, FM-1_97, Pool 4, Emulator ok, Gerät ungeprüft) |
| MKS-50 solo | mks50 (alle Stimmen) | voller MKS-50 | gebaut: [`mks50/`](mks50/), `firmware/MKS50-0.2.8-alpha.fwsc` (fm1-synths fortgeführt, FM-1_95, Pool 4, Emulator ok, Gerät ungeprüft) |
| JP-6 solo | jp6 (alle Stimmen) | voller JP-6 | gebaut: [`jp6/`](jp6/), `firmware/JP6-0.1.3-alpha.fwsc` (Gerüst, FM-1_92, 4 Stimmen, ohne Distortion, Emulator ok, Gerät ungeprüft) |

**Solo-Firmware je Baustein (Wunsch 28.11.):** Jeder Baustein bekommt eine eigene Firmware, die das FM-1 ganz für ihn nutzt:
so viele Stimmen und so viel Qualität (Überabtastung, alle Instrumente/Parameter), wie die Last auf dem Gerät erlaubt,
mit eigener Bedienung nach dem Raster der vorhandenen Synths.

In der Firmware des Nutzers fehlen vorerst Toms, Congas, Claves, Maracas und Laser (Wunsch 28.11.). In den
Solo-Varianten sind sie dabei.

## Anleitung je Firmware (Pflicht, Wunsch des Nutzers 28.11.)

Jede Variante mit Paket in `firmware/` hat eine kurze Anleitung **`<name>/ANLEITUNG.md`** (Deutsch) und
**`<name>/ANLEITUNG.en.md`** (Englisch, gleicher Inhalt). Daraus erzeugt `web/installer/make_site.py` die Seite
`anleitung/<NAME>.html`, die der Web-Installer an jedem Listeneintrag als „Anleitung“ verlinkt (ADR 0054).
**Wer die Bedienung einer Firmware ändert, führt im selben Commit diese eine Datei nach.**

- Inhalt nach dem Code, nichts erfinden: was die Firmware ist, Stimmen, Tasten und Regler, die Seiten der Reihe nach
  mit KNOB 1–4, Presets (eigene vorn), Sequencer und Live-Aufnahme LOOP, Speichern, Effekte.
- Nur einfaches Markdown: `#`/`##`/`###`, Absätze, `-`/`1.` Listen, Tabellen mit `|`, `**fett**`, `` `Code` ``.
- FM1-Electro (kein Ordner hier): `web/installer/anleitungen/FM1_ELECTRO.md` und `.en.md`.
- Ordnername = Paketname klein (`firmware/JXSH-…` → `jxsh/ANLEITUNG.md`).
