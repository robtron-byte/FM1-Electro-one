# 0023: Automatisch speichern erst bei STOP

- **Status:** vorgeschlagen
- **Stand:** 2026-11-06

## Kontext

Auf der eigenen Plattform (ADR 0021) sperrt das Löschen eines 4-KB-Flash-Sektors alle Interrupts für etwa 50 ms (`platform/firmware/hal/fm1_flash.h`, `fl_erase4k`): Während der Flash löscht, ist der XIP-Zugriff abgeschaltet, und Teile des Audio-Interrupts liegen im Flash. Der Store schreibt mehrere Sektoren (`fm1/el_store.c`), das sind über 100 ms ohne Ton. Ein Audio-Halbpuffer reicht 5,8 ms. Der Nutzer hörte ein Knacksen beim Verlassen von OPTIONS (dort wurde jedes Mal gespeichert) und sah `LATE BLOCKS` steigen.

## Entscheidung

`ui/el_ui.c`:

- **OPTIONS verlassen:** gespeichert wird nur, wenn sich eine Option geändert hat (Vergleich mit dem Stand beim Öffnen).
- **Läuft der Groove**, wartet jedes automatische Speichern (geänderte Optionen, zwei Sekunden nach einem Generator-Einsatz) auf STOP; das Display sagt dann „OPTIONS SET: SAVED AT STOP“.
- **SAVE** (ausdrücklich) speichert weiter sofort, auch während der Groove läuft.
- **Vorab löschen** (`el_store_prepare`, seit 2026-11-07): Die Kopie, die das nächste Speichern beschreibt (die ältere der beiden), wird gelöscht, sobald der Groove nach dem Start oder nach einem Speichern 1,5 s stillsteht. Ein SAVE während des Spielens schreibt dann nur noch Seiten (Interrupts zwischen den 256-Byte-Seiten an) und hat keine Lücke mehr. Die neueste gültige Kopie bleibt dabei immer unangetastet. Erst ein zweites Speichern im selben Durchlauf muss wieder löschen.

## Alternativen

- **Audio-Interrupt komplett ins RAM, Interrupts während des Löschens an:** Dann liefe der Ton weiter. Das braucht alle Funktionen und Konstanten des Audio-Pfads im RAM (auch Timer-, Tasten- und USB-Teile der Plattform) und einen Lösch-Ablauf, der nur pollt. Größerer Eingriff in die Plattform; später möglich.
- **Ein Protokoll in vorgelöschten Sektoren** (nur Seiten schreiben, Löschen im Stillstand): weniger Lücken, aber ein neues Speicherformat.

## Folgen

- Wird das Gerät ausgeschaltet, während der Groove läuft, gehen ungespeicherte Generator-Ergebnisse und Optionen verloren. SAVE sichert sofort.
- Host-Test `host/ui_sim.c`: Optionen und neue Grooves sind erst nach STOP im Flash.

## Verlauf

- 2026-11-06: angelegt nach der Rückmeldung „beim Zurückgehen von OPTIONS auf HOME knackst es“
- 2026-11-07: vorab löschen im Stillstand, damit SAVE während des Spielens keinen Aussetzer mehr hat (Rückmeldung „beim Speichern gibt es einen kleinen Aussetzer“); Host-Test „store: erased ahead“
