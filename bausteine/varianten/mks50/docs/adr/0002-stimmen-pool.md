# 0002: Ein Stimmen-Pool für alle Parts

- **Status:** vorgeschlagen
- **Stand:** 2026-11-10

## Kontext

Der Auftrag verlangt mehrere Parts mit je eigenem Sound und eigenem Chorus. Spielt nur ein Part, soll er alle Stimmen bekommen. Mit mehr Parts dürfen Stimmen pro Part wegfallen, mindestens 3 Parts sollen je 2 Stimmen haben.

In FM1-Electro besitzt jeder Part seine Stimmen fest (`mks_part_t.voice[6]`). Drei Parts mit je 6 Stimmen sind auf dem Gerät nicht bezahlbar, und drei Parts mit je 2 Stimmen lassen einen einzelnen Part nie mehr als 2 spielen.

## Entscheidung

- **Ein Pool** (`mks_t.voice[]`, `engine/mks.h`): **6 Stimmen** (`MKS_POOL`, `engine/mk_config.h`), wie beim echten MKS-50; vom Nutzer nach den Messungen festgelegt (10.11.2026, ADR 0003). Jede Stimme trägt die Nummer ihres Parts. Die Engine kann bis zu 8 (`MKS_POOL_MAX`).
- **4 Parts** (`MKS_PARTS`), jeder mit Sound, LFO und Chorus.
- **Vergabe einer Stimme** (`pool_take` in `engine/mks.c`), in dieser Reihenfolge:
  1. eine freie Stimme;
  2. die älteste losgelassene Stimme, egal von welchem Part;
  3. die älteste gehaltene Stimme des Parts mit den meisten Stimmen.
- **Garantie:** Ein Part behält 2 Stimmen (`MKS_GUARANTEE`) gegen die anderen. Ein Part, der seine 2 schon hat, nimmt bei vollem Pool seine eigene älteste, außer ein anderer Part hat mehr als 2 und mehr als er selbst.
- **Mono** nimmt die Stimme, die den Part gerade spielt. **Unison** nimmt die Stimmen des Parts und alle freien, höchstens 6.
- Der Klang einer Stimme ist der von FM1-Electro, Zeile für Zeile. `make identity` rendert alle 56 Sounds durch die unveränderte Engine von FM1-Electro (`host/ref/`) und durch diese und vergleicht Sample für Sample: bitgleich.

## Alternativen

- **Feste Stimmen pro Part:** einfacher, erfüllt aber „ein Part allein bekommt alle“ nicht.
- **Ohne Garantie, nur die älteste Stimme:** Ein Part mit vielen langen Noten könnte einen anderen ganz verdrängen.

## Folgen

- Eine gestohlene Stimme wechselt den Part mitten im Ton. Die Hüllkurve startet vom aktuellen Wert, das Filter behält seinen Zustand, damit es nicht knackt.
- Mit 4 Parts und 6 Stimmen sind 2 Stimmen pro Part nicht für alle gleichzeitig möglich. Ein Part ohne Stimme bekommt dann trotzdem eine.
- `make pool` prüft die Regeln.

## Verlauf

- 2026-11-10: angelegt
- 2026-11-10: Pool auf 6 Stimmen festgelegt (Nutzer)
