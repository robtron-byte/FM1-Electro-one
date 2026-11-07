# Architecture Decision Records

Hier steht, **was** an FM1-Electro grundsätzlich entschieden wurde, **warum**, und welche Alternativen es gab. Jede Entscheidung hat eine eigene Datei. Die Records werden bei jeder neuen oder geänderten Entscheidung im selben Commit nachgeführt (siehe [0001](0001-adrs-fuehren.md) und `CLAUDE.md`).

**Status:** *vorgeschlagen* (offen, z. B. A/B-Hörvergleich läuft), *angenommen*, *abgelöst durch NNNN*, *verworfen*.

| Nr. | Entscheidung | Status | Stand |
|---|---|---|---|
| [0001](0001-adrs-fuehren.md) | Entscheidungen als ADRs festhalten | angenommen | 2026-10-29 |
| [0002](0002-einbau-in-fm1-open.md) | Einbau in FM1 Open: eigene App-Schicht statt der SDK-App | angenommen | 2026-10-26 |
| [0003](0003-eigener-packer.md) | Eigener Packer mit 384-KB-Benutzerbereich | angenommen | 2026-10-26 |
| [0004](0004-sdk-start.md) | SDK-Start: Standardspuren aus, Master-Pegel voll | angenommen | 2026-10-26 |
| [0005](0005-codegroesse.md) | Codegröße: Mathe-Helfer und 808-Stimmen ausgelagert | angenommen | 2026-10-26 |
| [0006](0006-808-modell-und-fremde-ideen.md) | 808: eigenes kalibriertes Modell, fremde Ideen nur als eigene Umsetzung | angenommen | 2026-10-29 |
| [0007](0007-bass-mono.md) | BASS einstimmig und mono | angenommen | 2026-10-26 |
| [0008](0008-clap.md) | Clap: Ausklang nach dem Original (gefittete Variante D) | angenommen | 2026-10-29 |
| [0009](0009-808-metall.md) | 808-Metall: wandernde Oszillatoren, linearer Open Hat (Variante D) | angenommen | 2026-10-29 |
| [0010](0010-bd-ausklang.md) | Bassdrum-Ausklang: Oktav-Körper (Variante C) | angenommen | 2026-10-26 |
| [0011](0011-live-effekte.md) | Live-Effekte auf den schwarzen Tasten 5–11 | angenommen | 2026-11-06 |
| [0012](0012-cowbell.md) | Cowbell an die echte Cowbell der Aufnahme angepasst | angenommen | 2026-10-29 |
| [0013](0013-gated-reverb.md) | Gated Reverb: echter Hall mit Gate (Variante C), Clap trocken | angenommen | 2026-10-29 |
| [0014](0014-hats.md) | Hi-Hats in X0X-/8W8-Bauweise (Variante D), Closed Hat an echter 808 nachkalibriert (B) | angenommen | 2026-11-06 |
| [0015](0015-snare.md) | Snare: A bleibt, Körper länger | angenommen | 2026-10-29 |
| [0016](0016-drum-tonart.md) | Drum-Tonart: tonale Drums auf eine Tonart gestimmt | angenommen | 2026-10-29 |
| [0017](0017-zap.md) | Zap nach der Referenz des Nutzers (Modell 3), ohne Bass | angenommen | 2026-10-29 |
| [0018](0018-divisions-trap.md) | Divisions-Trap aus, keine Division durch null, Hardware-Prüfung vor jedem Flashen | angenommen | 2026-11-04 |
| [0019](0019-rechenlast.md) | Rechenlast: Lastanzeige, CPU-Test, Master schläft bei Stille | vorgeschlagen | 2026-11-05 |
| [0020](0020-tasten-leds.md) | Funktionstasten leuchten, aktive Zustände blinken, inaktive Bereiche gedimmt | vorgeschlagen | 2026-11-06 |
| [0021](0021-eigene-grundlage.md) | Eigene Grundlage ohne das SDK von FM1 Open (Felucca-Plattform, Dinghy) | vorgeschlagen | 2026-11-05 |
| [0022](0022-drums-festkomma.md) | Drums in Festkomma auf der eigenen Plattform (Felucca-Drumstimmen) | vorgeschlagen | 2026-11-06 |
| [0023](0023-speichern-im-stillstand.md) | Automatisch speichern erst bei STOP (Flash-Löschen stoppt den Ton) | vorgeschlagen | 2026-11-06 |
| [0024](0024-sound-pro-platz.md) | Jeder Pattern-Platz behält seinen Sound (Drums und Synths) | vorgeschlagen | 2026-11-06 |
| [0025](0025-step-varianz.md) | Step-Varianz: ein Step spielt nur in jedem zweiten oder vierten Durchlauf | vorgeschlagen | 2026-11-06 |
| [0026](0026-eigene-sounds-zuerst.md) | Eigene Sounds vor den Werks-Sounds, GLO + PRESETS als Schnellspur | vorgeschlagen | 2026-11-07 |
| [0027](0027-eigene-sounds-und-lange-streicher.md) | Eigene schlichte Sounds und Pads, Bässe neben der Bassdrum, STRINGS bis 8 Takte pro Step, Live-Aufnahme in Achteln | vorgeschlagen | 2026-11-07 |
| [0028](0028-master-kompressor-club.md) | Master-Kompressor CLUB (Programm-Release, parallel) | vorgeschlagen | 2026-11-07 |

Neue Records: `TEMPLATE.md` kopieren, nächste freie Nummer, Zeile in diese Tabelle.
