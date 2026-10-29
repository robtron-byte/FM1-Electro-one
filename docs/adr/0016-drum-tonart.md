# 0016: Drum-Tonart: tonale Drums auf eine Tonart gestimmt

- **Status:** angenommen
- **Stand:** 2026-10-29

## Kontext

Wunsch: „Die Drums sollen sich an das Tuning anpassen, damit alle in einer Tonlage klingen, einstellbar mit einem Befehl.“ Die 808-Drums haben feste Grundtöne (BD ~44 Hz ≈ F1, Toms 93 / 185 Hz, Snare-Körper 172 Hz, Cowbell 540 / 800 Hz …), die zu keiner Tonart passen. Rückfragen beantwortet: Quelle eigene Einstellung **und** „folgt dem Bass“; Verteilung musikalisch; alle tonalen Drums; TUNE in Halbtönen.

## Entscheidung

- Zwei neue KIT-Parameter (KIT-Seite 2): **Key** (OFF, C … B) und **Follow** (OFF, BASS). Follow BASS addiert die Transposition des BASS-Bereichs.
- Jede tonale Drum geht auf den Ton ihrer Stufe, der ihrer Werkstonhöhe am nächsten liegt:
  - **Grundton:** BD, High Tom / High Conga, Snare-Körper (beide Resonatoren), Claves, Cowbell (unterer Ton; der obere eine reine Quinte darüber), Zap (Start), Pulse.
  - **Quinte:** Low Tom / Low Conga, Rimshot.
  - Keine Terz (es gibt kein Dur/Moll).
- TUNE dieser Drums rastet dann in Halbtönen (±12) um den zugewiesenen Ton. Key OFF: alles wie vorher (stufenlos, Bauteil-Toleranzen).
- Hats, Becken, Clap und Maracas bleiben (Rauschen / Metall ohne klaren Grundton).
- Kit-Wechsel behalten Key und Follow.
- Speicher: in dem bisher unbenutzten Feld `reserved` des Speicherstands (Key im unteren, Follow im oberen Byte). Ältere Speicherstände haben dort 0 = Key OFF; keine neue Version nötig.
- Umsetzung: `key_update` in `engine/tr808.c` berechnet je Block das Verhältnis zur Werkstonhöhe; die Stimmen nutzen es statt `spread(TUNE)`.

## Folgen

- Gemessen (Tonart C): BD C1, Snare-Körper C3, Low Tom G2, High Tom C4, Rimshot G4, Cowbell C5 / G5 (jeweils ±11 Cent oder besser; Toms ohne ihren Anfangs-Bend). Tonart G: BD G1, Snare G3, High Tom G3, Rimshot D5, Cowbell G4 / D5.
- Weil jede Drum zum nächstgelegenen Ton geht (bis ±6 Halbtöne), können BD und Snare deutlich tiefer oder höher werden als ab Werk (z. B. Tonart C: BD 33 Hz, Snare-Körper 131 Hz). TUNE gleicht in Halbtönen aus.
- Pro Block wenige log2/exp2-Rechnungen, nur bei eingeschalteter Tonart.
- Hörbeispiel: `build/wav/808_tonart.mp3` (BD, Low Tom, High Tom, Snare, Rimshot, Cowbell je zweimal: AUS, C, G).

## Verlauf

- 2026-10-29: Rückfragen beantwortet, umgesetzt
