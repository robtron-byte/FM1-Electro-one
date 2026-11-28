# Näher ans Original: Stand und Plan

Der Nutzer besitzt kein MKS-50. Diese Datei hält fest, wo die Engine vom Original abweicht oder schätzt, und woran sich jede Stelle prüfen lässt. Jede Klangänderung kommt zuerst als Schalter mit Standard „aus“ und einem A/B-Rendering (ADR 0001).

## Abweichungen und Annahmen in der Engine

| Stelle | Heute (`engine/mks.c`) | Offen |
|---|---|---|
| Filter-Rate | volle Rate (Schalter `vcf_half` aus) | erledigt, sofern die Messung es trägt (ADR 0003) |
| Release | endet wie eingestellt (Schalter `release_hurry` aus) | erledigt |
| Hochpass 2/3 | Eckfrequenz höchstens der Grundton der Stimme (`render_step`) | im Original fest; Werte aus dem Schaltplan |
| Hochpass 0 | Bass-Shelf +6 dB bei 70 Hz; die iOS-Doku nennt +3 dB | Schaltplan |
| Filter | Ladder mit einer Sättigung am Eingang, Resonanz bis 3,8, Basskorrektur `1 + 0,55 k` | Original: IR3R05, keine Selbstoszillation; Verhalten geschätzt |
| Chorus | 3,5 ms ± 1,9 ms, zwei 8-kHz-Tiefpässe, Mischung 0,7 trocken / 0,75 nass | alles Annahmen; MN3009/MN3101 aus dem Schaltplan |
| Aftertouch, Bender | Parameter vorhanden (`MP_DCO_AFTER`, `MP_VCF_AFTER`, `MP_VCA_AFTER`, `MP_BENDER`), nicht ausgewertet | fehlt |
| Hüllkurve, LFO | pro Sample bzw. alle 16 Samples, glatt | Original rechnet im Prozessor in Stufen; Rate unbekannt |
| Zeiten und Raten | Tabellen des Emulators june-21 (am Juno-2 gemessen, laut Autor schwankend) | Tabellen im ROM des Originals |
| Pegel Sub, Noise | 6-dB-Stufen bzw. drei Stufen | Annahme; Schaltplan |
| Pulsbreite | stufenlos | Original vergleicht 7 Bit (128 Stufen) |
| Werks-Sounds | 56 eigene Sounds aus der iOS-App | die Roland-Werksbänke fehlen |

## Quellen, ohne ein Gerät zu haben

1. **Service-Manual (Schaltplan):** Bauteilwerte für Hochpass, Mischer, Chorus-Takt und Filter.
2. **Firmware-ROM des Originals:** Tabellen für Hüllkurvenzeiten, LFO-Raten und die Update-Rate. Nur auslesen, keine Roland-Daten ins Repository. Ob ein Abbild auffindbar ist, ist offen.
3. **Werksbänke als SysEx:** llamamusic.com (MKS-50 ZONE) und vintagesynth.com. Die Engine nutzt die originalen Parameternummern 0–35.
4. **Aufnahmen echter Geräte:** denselben Werks-Tone rendern und Spektrum, Hüllkurvenzeiten und Chorus-Rate vergleichen. Eine freie Sample-Sammlung wurde nicht gefunden; Aufnahmen des Nutzers oder Links helfen.

Nicht gefunden: der Quellcode von june-21 und gemessene Chorus-Werte für den Alpha Juno.

## Reihenfolge

1. Messung am Gerät (ADR 0003), dann Pool-Größe.
2. Werksbänke importieren (Kategorie „Roland Werk“, ADR 0006).
3. Aftertouch und Bender.
4. Hochpass, Chorus und Pegel nach Schaltplan, je als Schalter mit A/B.
5. Hüllkurven- und LFO-Stufen, falls das ROM die Rate hergibt.
