# 0006: Sounds speichern und Kategorien

- **Status:** vorgeschlagen
- **Stand:** 2026-11-10

## Kontext

Der Nutzer wünscht eine einfache Speicherfunktion für Sounds und Kategorien für die Presets. Gebaut (ADR 0007), im Emulator geprüft, am Gerät noch nicht.

## Entscheidung

Geplant:

- **INIT TONE** (Werte der iOS-App: Sägezahn 8', Filter offen, Orgel-Gate, kein Chorus) in der Gruppe INIT, die in der Liste vorne steht. Er hängt hinten an der Werksliste, damit die Nummern der anderen Sounds bleiben. Ein geänderter INIT TONE öffnet bei SAVE kurz den Dialog (er braucht Name und Gruppe). Der Gruppenregler hält an den Enden an (INIT links, EIGENE rechts).
- **SAVE kurz** speichert den Sound auf seinen eigenen Platz. **SAVE lang** legt einen neuen Sound an; das Display fragt nach Platz, Name und Kategorie.
- **Ein geänderter Sound ist im Display markiert,** bis er gespeichert ist. Beim Wechsel bleibt der letzte ungespeicherte Stand zum Zurückholen erhalten.
- **Werks-Sounds sind unveränderlich.** Speichern auf einem Werks-Sound legt eine eigene Kopie an.
- **Kategorien:** Bass, Lead, Sequence, Zaps, Strings, Stab, Keys, Pad, FX & Perc, Risers, dazu „Roland Werk“ (die Original-Tones) und „Eigene“. Ein Regler wählt die Kategorie, einer den Sound darin.
- **Kein Aussetzer beim Speichern:** Ein Flash-Sektor wird vorab gelöscht (ein Löschen sperrt die Interrupts etwa 50 ms; FM1-Electro `docs/adr/0023`).

## Alternativen

- **Speichern mit Rückfrage bei jedem Wechsel:** sicher, aber stört beim Spielen.

## Folgen

- Die Presets brauchen ein Kategorie-Feld; `mks_preset_t.area` von FM1-Electro reicht dafür nicht.

## Gebaut (2026-11-10)

- **Eigene Sounds:** bis zu 48 in einem Flash-Sektor (Blob 0 der Plattform), je 67 Byte: Name (10 Zeichen), Kategorie, 54 Parameter. Sie erscheinen in ihrer Kategorie hinter den Werks-Sounds und in EIGENE.
- **Die vier Parts** und alles andere Eingestellte kommen beim Einschalten zurück, auch ungespeicherte Änderungen: seit 2026-11-13 über das Journal aus ADR 0011 (vorher: 2 s nach der letzten Änderung, nur wenn nichts klang).
- **SAVE** speichert sofort: Ein Löschen sperrt die Interrupts etwa 50 ms, der Klang setzt dann kurz aus. Ein Vorab-Löschen (zwei Sektoren im Wechsel, gelöscht im Stillstand) ist nicht gebaut.
- Ein Wechsel des Sounds mit ungespeicherten Änderungen legt sie beiseite; HOME lang gedrückt holt sie zurück (ein Stand je Part).
- Nicht gebaut: Import der Roland-Werksbänke (Kategorie ROLAND bleibt leer).

## Verlauf

- 2026-11-10: angelegt (Wunsch des Nutzers, noch nicht gebaut)
- 2026-11-10: gebaut: eigene Sounds, Speichern kurz und unter, Kategorien, Parts im Flash
- 2026-11-13: der Stand der Parts über das Journal aus ADR 0011
- 2026-11-14: INIT TONE und Gruppe INIT (Wunsch des Nutzers, auch JX-8P); Gruppenregler ohne Umlauf (TEST_08)
