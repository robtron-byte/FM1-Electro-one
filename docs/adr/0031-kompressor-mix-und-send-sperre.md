# 0031: Mix-Regler für den Master-Kompressor, Verzerrungs-Send nur mit einem Typ

- **Status:** vorgeschlagen
- **Stand:** 2026-11-07

## Kontext

Rückmeldung vom Nutzer:

- Ein **Mix-Regler** für die Kompression, um parallel zu komprimieren.
- Steht die Verzerrung auf **OFF**, soll man nichts in den Send schicken können: das ergibt keinen Sinn und gibt Doppelungen.

Der Master-Kompressor (OPTIONS) hatte keinen Mischanteil; nur CLUB (ADR 0029) hielt fest 30 % des unkomprimierten Signals. Bei Typ OFF kommt aus der Verzerrung nichts zurück (`fx_distortion_process`), die vier Send-Regler ließen sich aber trotzdem verstellen und zeigten Werte.

## Entscheidung

**COMP MIX** (OPTIONS, hinter COMP MAKEUP):

- Der Anteil des komprimierten Signals, 100 % bis 0 % in 5er-Schritten. Das Gegenstück ist das unkomprimierte Signal ohne Make-up: Verstärkung = `dry + (1 − dry) × Kompressorgewinn` (`mb_comp_out` in `engine/mixbus.h`, beide Mixbus-Pfade: Float und Festkomma). 100 % ist der Kompressor wie eingestellt, 0 % ist bitgleich zu keinem Kompressor.
- Gespeichert wird der trockene Anteil (`EL_OPT_COMP_DRY`, 0..20 Stufen): so ist der Wert 0 in älteren Ständen und in `el_init` „alles komprimiert“, wie es vorher war.
- **CLUB:** Die festen 30 % trocken sind kein Teil des Kompressors mehr, sondern die Einstellung. Wählt man CLUB im Menü und steht der trockene Anteil auf 0, setzt das Menü ihn auf 30 % (MIX 70 %). Ein Stand aus einer Version vor 25 mit CLUB bekommt ihn beim Laden ebenfalls (`el_store_load`), damit CLUB so klingt wie bisher.
- **Speicher:** Version 25. `EL_OPTIONS` (im Speicher) wird 16; die ersten zwölf (`EL_OPTIONS_STORED`) liegen wie bisher im Stand, die weiteren hängen hinten an (`option_ext`, 4 Byte). Ältere Stände laden unverändert.
- Menü: Die Zeilen sind 9 statt 10 Pixel hoch; es sind jetzt 16 und alle passen über die Fußzeile (HELP LINE war vorher halb abgeschnitten).

**Send-Sperre:** Steht der Typ der Verzerrung auf OFF, sind die vier Send-Regler (HOME + EDIT, KNOB 1–4) gesperrt: sie zeigen „OFF“ in dunkler Farbe, Drehen tut nichts und die Meldung „DISTORTION OFF: PICK A TYPE (PRESETS)“ erscheint (`edit_knob` in `ui/el_ui.c`). Bereits gespeicherte Send-Werte bleiben stehen und gelten wieder, sobald ein Typ gewählt ist.

## Alternativen

- **Send-Werte beim Wechsel auf OFF auf 0 setzen:** verliert die Einstellung, ohne dass sie klingt; so bleibt sie.
- **Sperre auch in der Engine (`EL_CMD_FX` lehnt Sends bei OFF ab):** würde Stände und Tests (die Sends vor dem Typ setzen) überraschen. Die Engine rechnet bei OFF ohnehin nichts.
- **Den gespeicherten Wert als Mix-Anteil halten (100 % = Standard):** bräuchte einen Standardwert ≠ 0, den ältere Stände nicht haben.
- **Einen Regler außerhalb von OPTIONS:** alle freien Regler sind belegt; der Kompressor gehört zu den Einstellungen, die man einmal macht.

## Folgen

- Mit weniger als 100 % MIX wirkt die Kompression schwächer; wer die Pegelabsenkung (COMP GR) im Menü sieht, sieht sie unverändert (sie zeigt den Kompressor, nicht den Mix).
- Die Master-Kette (Röhrensättigung, Limiter) macht den Pegel zu einem schlechten Maß; der Test vergleicht daher die Änderung der Ausgabeenergie.

## Verlauf

- 2026-11-07: angelegt; `make test` (Tests „comp mix“, „dist“), `make mb-fixed-check` und `make web` laufen.
