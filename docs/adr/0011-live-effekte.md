# 0011: Live-Effekte auf den schwarzen Tasten 5–11

- **Status:** angenommen
- **Stand:** 2026-10-26

## Kontext

In HOME sollen Live-Effekte (Punch-ins) spielbar sein, ohne die Mute-Tasten zu verlieren.

## Entscheidung

- Schwarze Tasten 1–4: Mute der vier Bereiche. 5–11: STUTTER, THROW (Hall-Wurf, der Hauptklang blendet aus, der Hall bleibt), TAPE (Bandstopp), REPEAT, DELAY, CRUSH, PHASER.
- Effekte sind kombinierbar. Solange einer gehalten wird, wählen die weißen Tasten seinen Wert (z. B. 8tel/16tel). TAPE: 1/16 bis 1/2 Takt.
- REPEAT wiederholt einen Schritt (1–16, immer 16tel) und startet erst, wenn eine weiße Taste gedrückt wird; die Abspielköpfe laufen darunter weiter.
- Hoch- und Tiefpass wurden verworfen.
- Die Effekte teilen sich einen gemeinsamen Speicher (`fx_memory_t`) mit Hall und Delay.

## Folgen

- Alle sieben Effekte zugleich kosten im Emulator ~112 % Rechenzeit (beim Spielen); einzeln 68–93 %.

## Verlauf

- 2026-10-26: angelegt (Stand der Umsetzung)
