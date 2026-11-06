# 0022: Drums in Festkomma auf der eigenen Plattform (Felucca-Drumstimmen)

- **Status:** vorgeschlagen
- **Stand:** 2026-11-06

## Kontext

Auf dem Gerät brauchte die Gleitkomma-808 (`engine/tr808.c`) allein 67 % eines Kerns (OPTIONS-Zeile `DR67`, 2026-11-06), im Emulator 28 %. Der Code lag schon im SRAM; das half nicht. Andere Firmwares zeigen den Grund: Ihre Stimmen rechnen pro Sample viel weniger.

- **Felucca:** in den Drums keine einzige Kommazahl. Gedämpfte Sinus-Oszillatoren aus einer Tabelle, Rauschen durch ein Festkomma-SVF, Hi-Hats aus sechs Rechtecken, Hüllkurven und Koeffizienten je Block von 32 Samples.
- **FM-106** (Juno-106, gleiche Plattform): Gleitkomma, aber das Filter nur mit 11 kHz; 6 Stimmen bei etwa 30 %.
- **X0X/BB1-808** (8W8): Gleitkomma, schaltungsgenau, ähnlich teuer wie unsere 808.

## Entscheidung

Vorgeschlagen, auf Wunsch des Nutzers („start“, 2026-11-06):

- Auf der eigenen Plattform (`TR808_FIXED`, `engine/el_config.h`) spielen BD, SD, LT, HT, RS, CP, CB, CY, OH und CH über Feluccas `drum_voice.c` (`third_party/felucca/`, mit Herkunft):
  - BD, SD, Toms, RS, CP, CB (Glocke), OH, CH: Feluccas Modell-Kit „80“;
  - CY: Feluccas eigenes Becken;
  - CONGA und CLAVE (Schalter SOUND): Feluccas eigene.
- Unsere Regler wirken weiter: TUNE (±12 Halbtöne), DECAY, TONE, SNAPPY/BEND, LEVEL, PAN, Sends, Akzent, Choke.
- ZAP, PULSE und das PO-12-Kit bleiben wie sie sind.
- Host und SDK-Build behalten die Gleitkomma-808 (Tests, Vergleich).
- Auslöser liegen auf einem Raster von 32 Samples (0,7 ms).

## Alternativen

- **Unsere 808 behalten und verschlanken:** Klang bleibt näher, Ersparnis unsicher.
- **X0X/BB1-808 übernehmen:** genauer an der 808, aber Gleitkomma und ähnlich teuer.
- **Halbe Abtastrate für die Drums:** halbe Last, Hi-Hats und Becken dumpfer.

## Folgen

- Der Drum-Klang ändert sich (Lautstärke im Mittel ±3 dB gegenüber vorher, Klangfarbe anders). Der Nutzer prüft ihn, wenn das Gerät rund läuft.
- Zwei Host-Tests, die genaue Werte der alten 808 messen, gelten nur für den Gleitkomma-Build.

## Verlauf

- 2026-11-06: angelegt nach der Messung `DR67` auf dem Gerät
