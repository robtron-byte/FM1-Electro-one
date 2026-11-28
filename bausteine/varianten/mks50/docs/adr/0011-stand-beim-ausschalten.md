# 0011: Immer der Stand vom Ausschalten

- **Status:** vorgeschlagen
- **Stand:** 2026-11-13 (gebaut, im Emulator geprüft; am Gerät noch nicht)

## Kontext

Der Nutzer wünscht: „immer auf dem Stand vom Ausschalten bleiben und beim Start wiederherstellen.“ Bisher wurden nur die Parts gesichert (Sound, Kanal, Pegel, Arp, Effekte, Tempo), und nur, wenn 2 s lang keine Stimme klang. Ungespeicherte Klangänderungen und die offene Seite gingen verloren. Bei laufendem Arp mit HOLD wurde nie gesichert.

Das FM-1 hat keinen Hinweis vor dem Ausschalten. Ein Flash-Sektor zu löschen sperrt die Interrupts etwa 50 ms (der Klang setzt aus); einen Teil zu schreiben nur Bruchteile einer Millisekunde.

## Entscheidung

- **Was:** alles Eingestellte: die vier Parts mit den aktuellen Werten ihrer Sounds (auch ungespeicherte Änderungen, die dann weiter als EDITED gelten), Kanäle, Pegel, Arp-Einstellungen, Effekte, Tempo, gewählter Part, Oktave und die offene Seite. Nicht: gehaltene oder per HOLD gelatchte Töne (das Gerät soll beim Einschalten nicht von selbst spielen).
- **Wann:** 1,5 s nach der letzten Änderung, **auch während gespielt wird**. Die Bedienung vergleicht alle 250 ms den Stand mit dem zuletzt gesicherten.
- **Wie:** ein Journal in 16 Sektoren der Slots der Plattform (`store.h`, 64 KB), 12 Einträge zu 320 Byte je Sektor (`jr_*` in `app/mk_ui.c`). Ein Eintrag wird in Stücken von höchstens 64 Byte geschrieben, jedes direkt nach einem Audioblock: Die Interrupts sind dann etwa 0,2 ms gesperrt, der nächste Block hat 1,45 ms. Erst der Inhalt, zuletzt der Kopf mit Folgenummer und CRC: Ein vom Ausschalten abgeschnittener Eintrag wird nie gelesen.
- **Löschen** nur, wenn eine Sekunde lang alles still war (keine Stimme, kein laufender Arp, Ausgang unter −80 dB, also auch keine Hall- oder Delayfahne), immer zwei Sektoren im Voraus; beim Einschalten sofort.
- **Beim Einschalten** gilt der neueste vollständige Eintrag. Gibt es keinen (erste Firmware mit Journal), werden die alten Einstellungen gelesen, sonst die Werkseinstellung.
- Eigene Sounds speichert weiter nur SAVE (ADR 0006).

## Prüfung (Emulator, Flash-Datei)

- Änderungen bei durchgehend gehaltenen Tönen (nie still): Part B, Seite ENV, T1 geändert, Oktave +1, Arp an → nach dem Neustart genau so, mit EDITED.
- Ausschalten 0,8 s nach einer Änderung: der Stand davor; nach 2,3 s: mit der Änderung.

## Alternativen

- **Nur in Stille sichern (bisher):** bei HOLD-Arp oder langen Flächen nie.
- **Jede Änderung sofort mit Löschen sichern:** Aussetzer beim Spielen, mehr Verschleiß.

## Folgen

- Verschleiß: Ein Sektor wird erst nach 192 Einträgen wieder gelöscht; bei 100.000 Löschzyklen reicht das für etwa 19 Millionen Sicherungen.
- Wer 1,5 s nach einer Änderung ausschaltet, verliert nur diese letzte Änderung.

## Verlauf

- 2026-11-13: angelegt auf Wunsch des Nutzers
- 2026-11-14: Kracher im Klang (Nutzer, MKS-50 und JX-8P): ein Stück von 64 Byte zu schreiben hält die Interrupts so lange aus, wie der Flash zum Programmieren braucht (typisch 0,4–0,7 ms, bis 3 ms); bei 55–70 % Last bleiben einem Block nur 0,4–0,6 ms. **Geschrieben wird jetzt nur noch in einer Spielpause** (keine Stimme, kein Arp, 1 s Stille am Ausgang), wie gelöscht wird. Der Stand wird in der nächsten Pause gesichert; wer ohne Pause spielt und ausschaltet, verliert die Änderungen seit der letzten. Die Seite SYSTEM zählt verspätete Audio-Interrupts als GAP (MKS-50 0.1.1-alpha, JX-8P 0.1.2-alpha)
