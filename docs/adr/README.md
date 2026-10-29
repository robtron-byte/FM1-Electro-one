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
| [0011](0011-live-effekte.md) | Live-Effekte auf den schwarzen Tasten 5–11 | angenommen | 2026-10-26 |
| [0012](0012-cowbell.md) | Cowbell an die echte Cowbell der Aufnahme angepasst | angenommen | 2026-10-29 |
| [0013](0013-gated-reverb.md) | Gated Reverb: echter Hall mit Gate (Variante C), Clap trocken | angenommen | 2026-10-29 |
| [0014](0014-hats.md) | Hi-Hats: eigener Bandpass / X0X-Bauweise (A bleibt) | verworfen | 2026-10-29 |

Neue Records: `TEMPLATE.md` kopieren, nächste freie Nummer, Zeile in diese Tabelle.
