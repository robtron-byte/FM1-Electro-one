# 0033: Live-Effekt MONO DELAY (nach dem Korg Monotron Delay)

- **Status:** vorgeschlagen (Plan, wartet auf „los“; Arbeitszweig `claude/n15-mono-delay`)
- **Stand:** 2026-11-08

## Kontext

Nutzerwunsch (N15): ein neuer Live-Effekt, mit einem Drehregler steuerbar und überall nutzbar: ein Delay mit Filter-Cutoff wie in `mdtest.c` des Nutzers (Dropbox: `/Ron Zöllner/Privat/PP/Programmieren/M-Vave FM1/Notizen/mdtest.c`, Stand 08.11.2026) und wie im Korg Monotron Delay.

`mdtest.c` beschreibt: in → (+ Feedback) → Hochpass 100 Hz → Softclip (Padé-tanh) → Korg-35-Tiefpass (ZDF/TPT) → Delay (N Samples) → Feedback. Feedback 1,0, Nass 0,7, kein Rauschen. Ein Regler p 0..1: unteres Fünftel = Kill-Zone (Cutoff bleibt 100 Hz, Pegel und Feedback blenden mit Smoothstep auf 0), darüber Cutoff 100 Hz … 10 kHz exponentiell. Im Test N = 96 Samples bei 48 kHz (2 ms).

Recherche Delayzeit (08.11.2026): Das Monotron Delay nutzt laut DIY-Analyse einen PT2399 (kein BBD), gemessener Takt 2–20 MHz. Nach der Datenblatt-Formel von Electric Druid (Delay ms ≈ 683,21 / Takt MHz + 0,08) ergibt 20 MHz ≈ 34 ms (kürzeste Zeit), 2 MHz ≈ 340 ms. Reviews: „wenige ms bis ~1 s“ (MusicRadar), gemessen gut 1 s (Sound On Sound). Datenblattkopien sind beim Taktbereich uneins; die 20 MHz sind eine Hobby-Messung.

## Entscheidung (geplant)

- **Klang:** `mdtest.c` fast 1:1 als `engine/fx_mono.c` (Urheber Ron Zöllner im Dateikopf), 44,1 kHz; Delayzeit fest **34 ms** (die kürzeste des Monotron Delay; Nutzer erinnerte ~30 ms). Koeffizienten des Korg-35 nur bei Reglerbewegung. Auf der Master-Summe: trocken + nass (0,7).
- **Bedienung:** **GLO + ALGORITHM** in jeder Ansicht; zählt vom aktuellen Wert weiter (kein Sprung). Unteres Fünftel blendet aus, darüber Cutoff 100 Hz – 10 kHz. Bisherige Belegung zieht um: Pattern-Länge → **EDIT + SELECT**, Feintempo in HOME → **EDIT + ALGORITHM**.
- **Nicht gespeichert:** beim Start aus.
- **Display:** beim Drehen groß „MONO DELAY“, Cutoff in Hz, Balken mit Kill-Zone; „MD“ in der Kopfzeile, solange aktiv; Kachel in HOME (wie N10).
- **Erst hören:** Schalter `FX_MONO_DELAY` (Standard aus), `make monodelay-ab` mit Cutoff-Sweep (Groove und nur nass).

## Alternativen

- 2 ms wie in `mdtest.c` (resonanter Kammfilter-Ton statt Echo) – auf Wunsch nach 30 ms geändert.
- Taktsynchrone oder regelbare Zeit (Monotron: bis ~340 ms / ~1 s) – später möglich.
- GLO + MASTER (Poti, springt) oder GLO + SELECT – Nutzer wählte GLO + ALGORITHM.

## Folgen

- GLO + ALGORITHM ist nicht mehr Pattern-Länge / Feintempo (Doku, Hilfezeile, Tests anpassen).
- Rechenzeit: ein Korg-35 und ein Hochpass pro Sample auf der Summe, Puffer 34 ms (~1500 Samples, mono, ~6 KB float oder 3 KB int16).

## Verlauf

- 2026-11-08: angelegt (Plan, Recherche Delayzeit; Nutzer: 30 ms → 34 ms laut PT2399, GLO + ALGORITHM, nicht speichern)
- 2026-11-08: `mdtest.c` des Nutzers nach der Recherche angepasst und als `tools/mdtest.c` abgelegt: Delayzeit in ms (Standard 34 ms, erlaubt 1–340 ms = Bereich des PT2399), Länge in Samples aus der Abtastrate (48 kHz im Test, 44,1 kHz auf dem Gerät klingen gleich). Noch keine Umsetzung im Gerät (Nutzer: erst Code anpassen)
