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

- **Seit 2026-11-06: Becken, Open Hat und Closed Hat sind unser kalibriertes Modell in Festkomma** (`engine/tr808_metal_fixed.h`, Schalter `TR808_METAL_FIXED`): dieselbe Rechnung wie `tr808.c` (sechs Oszillatoren mit PolyBLEP und ihrem Wandern, Bandpässe, Swing-VCAs, Hats-Modell 2), Signale Q20, Koeffizienten Q28, Hüllkurven Q30, Produkte 32 × 32 → 64 Bit (ein Befehl auf dem FM-1). Koeffizienten einmal pro Render-Aufruf aus den Gleitkomma-Funktionen von `tr808.c`. Prüfung `make metal-fixed-check`: zwölf Einzelschläge (Akzent, TUNE, DECAY, TONE, Choke) liegen 58–70 dB unter dem Signal neben dem Gleitkomma-Modell, Spektrum und Hüllkurve innerhalb 0,14 dB. Die übrigen Stimmen folgen auf demselben Weg.
- **Seit 2026-11-06 auch die BD** (`engine/tr808_bd_fixed.h`, Schalter `TR808_BD_FIXED`): Triggerimpuls, Resonator (Rohspannung des SVF, Frequenz und Güte alle 8 Samples wie im Gleitkomma-Code), Nachschlag, Seufzer, Oktav-Körper, Nichtlinearität, TONE, DC-Sperre; Signale Q24. `make metal-fixed-check` mit neun BD-Fällen (Akzent, DECAY 0/127, TUNE, TONE, PUNCH, DRIVE, Neuanschlag): 53–71 dB unter dem Signal.
- **Seit 2026-11-06 die ganze 808:** SD, Toms/Congas, Rim/Claves, Clap/Maracas (`engine/tr808_rest_fixed.h`, `TR808_REST_FIXED`; nur die Standardmodelle SD 0, CP 3) und die Cowbell (im Metall-Teil, aus denselben Oszillatoren). Gleiches Rauschen wie das Gleitkomma-Modell (derselbe Generator, Sample für Sample); tanh und cos aus Tabellen (Fehler unter −100 dB). `make metal-fixed-check`: 36 Einzelschläge, alle 49–92 dB unter dem Signal. Feluccas Stimmen spielen damit nicht mehr (der Code ist noch im Build, ungenutzt).
- **Last:** Im Emulator (Kostenmodell, Groove ROTHER) steigt Kern 0 von 71 % (Feluccas Stimmen) auf 91 %. Das Gerät entscheidet; danach wird der Nachbau billiger gemacht oder Arbeit auf Kern 1 verschoben.
- Vorher spielten auf der eigenen Plattform (`TR808_FIXED`, `engine/el_config.h`) SD, LT, HT, RS, CP und CB über Feluccas `drum_voice.c` (`third_party/felucca/`, mit Herkunft):
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
- 2026-11-06: Rückmeldung des Nutzers am Gerät: Becken und Closed Hat klingen nicht wie die 808. Gegen ADR 0006 (das kalibrierte Modell bleibt die Grundlage, Klangänderungen erst als A/B mit Standard aus) war dieser Record gleich als Standard eingeschaltet worden. Der Nutzer will den Klang des kalibrierten Modells; offen ist, wie die Last dafür sinkt (eigenes Modell in Festkomma oder verbilligt)
- 2026-11-06: Auf Wunsch des Nutzers wird unser Modell selbst in Festkomma nachgebaut, Stimme für Stimme; zuerst Becken und Hats (`engine/tr808_metal_fixed.h`)
- 2026-11-06: BD in Festkomma nachgebaut (der Nutzer: „die BD war vorher besser“). Referenzaufnahmen des Nutzers („808 best of“, Dropbox, nicht im Repository): BD-Grundton dieser 808 fällt von ~62 auf 48,4–51 Hz (unser Modell: 44,5–46 Hz, kalibriert an anderen Aufnahmen), DECAY-Stufen −40 dB nach 0,29 / 0,45 / 0,9 / 1,5 s. Ein Abgleich daran käme als A/B-Schalter.
- 2026-11-06: alle übrigen Stimmen in Festkomma nachgebaut (auf Wunsch des Nutzers erst alle, dann die Last am Gerät testen)
