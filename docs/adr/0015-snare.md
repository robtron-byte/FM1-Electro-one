# 0015: Snare: Vergleich mit der X0X-Snare (8W8)

- **Status:** vorgeschlagen (Hörvergleich `build/wav/snare_original_A_B.wav`: Original 13, Original 14, A, B)
- **Stand:** 2026-10-29

## Kontext

Wunsch: „Schaue dir die Snare aus X0X genauer an.“ Unsere Snare (A) ist an Aufnahmen bei 16 TONE/SNAPPY-Stellungen gefittet. X0X-RMX nutzt die Snare der 8W8-Engine, ein Schaltungsmodell:

| | A (unsere) | 8W8 / X0X |
|---|---|---|
| Körper | zwei Resonatoren 172 / 339 Hz, leicht gesättigt (tanh) | 173 / 336 Hz, Q 17,4 / 10,7, so hart angeschlagen, dass der OpAmp clippt (Schiene 40): gestauchter, knackiger Körper |
| TONE | lineare Überblendung | leistungsgleich (Sinus / Kosinus) |
| Rauschen | weiß, Hochpass 2,1 kHz, Tiefpass 7,7 kHz | einweg-gleichgerichtet (VCA-Transistor), nur Hochpass 1,76 kHz |
| Rausch-Hüllkurve | zwei Neigungen (29 / 40 ms) | eine, 75 ms, Höhe SNAPPY × Triggerspannung |

## Messung gegen die Aufnahme (808 Basics Kit A, Snares 13–15)

| | Körper fällt | Rauschen zu Körper (0–100 ms) | 8–16 kHz | Rausch-Abfall |
|---|---|---|---|---|
| Original 13 / 14 | ~2,7 dB je 10 ms | −1,7 / −1,4 dB | −27 … −35 dB | ~3 dB je 10 ms |
| A | ~3,0 | −1,7 dB | −23 … −26 dB | ~3 dB je 10 ms |
| B | ~2,4 | −8,9 dB | −26 dB (bis 16 kHz offen) | ~1 dB je 10 ms |

A trifft Spektrum und Rauschanteil des Originals; der Körper klingt etwas zu kurz aus. B hat den längeren, gestauchten Körper, aber zu wenig, zu langes und zu helles Rauschen.

## Entscheidung (vorgeschlagen)

Schalter `TR808_SD_MODEL` (Standard 0 = A); 1 = B, eigene Umsetzung der 8W8-Struktur (`snare_circuit`), Pegel wie A über 0,1 s (`TR808_SD1_GAIN`). Das Ohr entscheidet; denkbar ist auch eine Mischung (A mit dem gestauchten, etwas längeren Körper von B).

## Verlauf

- 2026-10-29: angelegt
