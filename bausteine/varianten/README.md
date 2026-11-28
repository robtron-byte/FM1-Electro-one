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
| 808 solo | dk808 mit allen 17 Instrumenten | volle Drum-Machine | geplant |
| 101 solo | sh101 | voller Bass/Lead | gebaut: [`sh101/`](sh101/), `firmware/SH101-0.1.1-alpha.fwsc` (Emulator ok, Gerät ungeprüft) |
| JX-8P solo | jx8p (alle Stimmen) | voller JX-8P | in Arbeit: [`jx8p/`](jx8p/) (0.1.0 nicht abgenommen, Gleichstand mit fm1-synths nötig) |
| MKS-50 solo | mks50 (alle Stimmen) | voller MKS-50 | geplant |
| JP-6 solo | jp6 (alle Stimmen) | voller JP-6 | geplant |

**Solo-Firmware je Baustein (Wunsch 28.11.):** Jeder Baustein bekommt eine eigene Firmware, die das FM-1 ganz für ihn nutzt:
so viele Stimmen und so viel Qualität (Überabtastung, alle Instrumente/Parameter), wie die Last auf dem Gerät erlaubt,
mit eigener Bedienung nach dem Raster der vorhandenen Synths.

In der Firmware des Nutzers fehlen vorerst Toms, Congas, Claves, Maracas und Laser (Wunsch 28.11.). In den
Solo-Varianten sind sie dabei.
