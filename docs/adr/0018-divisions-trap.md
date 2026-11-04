# 0018: Divisions-Trap aus, keine Division durch null, Hardware-Prüfung vor jedem Flashen

- **Status:** angenommen
- **Stand:** 2026-11-04

## Kontext

Der erste Build auf einem echten FM-1 (29.10.2026) startete in einer Schleife neu. Das Display blinkte etwa im Sekundentakt, und kurz war oben ein Streifen der Oberfläche zu sehen. Im Emulator fm1-emu lief derselbe Build fehlerfrei.

Die Suche im Emulator hat der Reihe nach ausgeschlossen:

- unausgerichtete Speicherzugriffe;
- Integer-Division durch null;
- Überlauf eines Stacks (Interrupt-Stack ~1 KB von 4 KB, Tasks mit viel Reserve, die Stack-Fenster des SDK nie verletzt);
- Schreiben in den RAM-Code und Zugriffe auf die Nullseite;
- zu wenig Heap (nach dem Start ~48 KB frei);
- Watchdog (längste Lücke 147 ms, Timeout 4 s);
- alter oder zufälliger Inhalt im Flash außerhalb des Abbilds und zufälliges RAM beim Einschalten;
- Code im Flash aus dem Audio-Interrupt (Vanilla macht das genauso).

Gefunden hat es dann die Prüfung der FPU-Operationen.

- **Der Ablauf im Gerät:** Das SDK von FM1 Open startet mit eingeschaltetem Divisions-Trap der CPU (`EMU_CON` = 0x1EEF0D0, Bit 2; der Startwert ist 0x10C). Auf dem AC79 löst er auch bei **Float**-Divisionen aus (beobachtet von X0X auf echter Hardware, `fm1-x0x-rmx/firmware/hal/fm1_irq.h`).
- **Die Rolle des Compilers:** Er rechnet eine bewachte Division vor ihrem Test aus und wirft das Ergebnis weg, wenn der Test nicht zutrifft. Aus `peak > ceiling ? ceiling / peak : 1` wird erst `ceiling / peak` und dann die Auswahl.
- **Die Stelle in unserem Code:** Der Master-Limiter (`engine/mixbus.c`) teilte so bei Stille 0,89 durch 0.
  - Das passiert ab dem ersten Audio-Block nach `ready = 1`. Im Emulator ist es der erste Treffer bei 294 ms, gleich nach den ersten Bändern des Displays (292–295 ms).
  - Die CPU springt in `exception_irq_handler`, und `exception_analyze` startet das Gerät über P33 neu. Daher kommen der Streifen und die Schleife.
- **Vergleich:** Vanilla FM1 Open und die Original-Firmware V16 erzeugen im Emulator kein einziges Inf/NaN und teilen nie durch null.

## Entscheidung

- **Trap aus:**
  - `el_fm1_trap_off()` löscht Bit 2 von `EMU_CON`. Das geschieht in `groove_screen_init()` (`fm1/el_sdk_app.c`) und noch einmal unmittelbar vor `ready = 1` (`fm1/el_fm1.c`).
  - Das SDK setzt das Register nur beim Start, vorher. Danach schaltet es nur Bit 3 (Stack-Wächter) mit `|=`/`&=` um, das Löschen bleibt also bestehen.
  - Genauso halten es X0X, SLOOP, Felucca und Dinghy.
- **Keine Division durch null im Limiter:**
  - Der Limiter rechnet jetzt `ceiling / max(peak, ceiling)`. Das Ergebnis ist bitgleich: Unter der Decke ergibt `ceiling / ceiling` genau 1.
  - Ein A/B-Schalter ist nicht nötig, der Klang ändert sich nicht.
- **Hardware-Prüfung vor jedem Flashen:**
  - `tools/fm1-emu/check.sh` lässt ein Paket in fm1-emu laufen, mit `tools/fm1-emu/hardware-checks.patch` (gegen fm1-emu 0bebe54).
  - Die Prüfung schlägt fehl bei:
    - Division durch null bei aktivem Trap;
    - NaN/Inf;
    - Float-zu-Int außerhalb des Bereichs;
    - unausgerichteten Zugriffen;
    - Stack außerhalb seines Fensters;
    - eingeschaltetem Trap.
  - Sie läuft mit zufälligem RAM und Flash-Inhalt beim Start.
  - Der alte Build fällt durch, der neue besteht.

## Alternativen

- **Nur die Stelle im Limiter reparieren, Trap an lassen:**
  - Dagegen spricht, dass der Compiler jede bewachte Float-Division so umstellen darf. Jede neue Formel mit `x > 0 ? a / x : …` wäre wieder ein möglicher Neustart, und der Host-Test sieht ihn nicht.
  - Die Prüfung im Emulator deckt nur die Wege ab, die sie durchläuft.
- **Nur den Trap ausschalten:**
  - Das ginge, aber das Inf bliebe im Code. Ohne Inf ist der Limiter auch auf einem Gerät oder SDK sicher, das den Trap wieder einschaltet.

## Folgen

- **Divisionen durch null:** Eine Division durch null stürzt nicht mehr ab, sie liefert Inf (Float) bzw. 0 (Integer).
  - Float-Divisionen mit möglichem Nenner 0 trotzdem vermeiden. Ein Inf, das nicht weggeworfen wird, macht den Ton kaputt.
  - Die FPU-Prüfung in `check.sh` meldet solche Stellen.
- **Vor jedem neuen Testbuild in `firmware/`:**
  1. `make test`;
  2. die Prüfungen aus `Build.ps1`;
  3. `tools/fm1-emu/check.sh`.
- **Weitere Grenzen des Emulators:** Auch nach dieser Prüfung kann das echte Gerät abweichen. USB und Funk sind im Emulator nicht nachgebildet, und Rechenzeiten zählt er als ein Takt pro Befehl.

## Verlauf

- 2026-11-04: angelegt nach der Neustart-Schleife des ersten Gerätetests
