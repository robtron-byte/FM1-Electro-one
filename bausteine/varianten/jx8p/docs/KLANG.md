# Klang: was von der JX-8P-iOS-App abweicht

Grundlage ist die iOS-App des Nutzers (Branch `cursor/ios-jx8p-grundlage-76ee`, Commit `9687574`). Die Stimme ist
Zeile für Zeile übernommen; dieselben Bausteine wie in der MKS-50-Firmware (Hüllkurve, Ladder, Chorus, Chop-Oszillator)
kommen aus `../shared/dsp/`, die in `mks50` bitgleich zu FM1-Electros geprüftem Port der MKS-50-App sind.

## Bewusst weggelassen (wie in der MKS-50-Firmware)

| App | hier | Grund |
|---|---|---|
| Pitch Bend, Mod Wheel, Aftertouch (Vibrato, Brilliance, Volume) | aus | das FM-1 hat sie nicht; die Werte der Sounds bleiben gespeichert |
| zusätzlicher durchstimmbarer Hochpass (`hpfFreq`) | aus | wie `mks50`; der 4-stufige HPF ist da |
| Distortion (OD / DIST / RAT) | aus | wie `mks50`; Delay und Hall kommen aus `../shared/fx/` |
| Voice Shedding (10-ms-Ausblenden) | aus | wie `mks50`: der Pool nimmt zuerst losgelassene Stimmen |

## Rechnerisch anders, klanglich gleich gemeint

- `exp2f`, `powf`, `tanf`, `sinf`: die Näherungen aus `dsp_math.h` (wie `mks50`), nicht libm. Die Stimmung ist
  geprüft: DCO-1 auf A4 = 440,000 Hz (`make osc`).
- Hüllkurven-Einstellungen werden pro Stimme nur neu gerechnet, wenn sich Knöpfe, Tonhöhe oder Drift ändern
  (dasselbe Ergebnis).

## Noch nicht mit der App verglichen

Am Rechner gibt es kein Swift; die App selbst konnte nicht als Referenz laufen. Der Vergleich ist bisher das Gehör
(`make demo`). Möglicher nächster Schritt: die App mit demselben Ablauf (`host/scenario.h`) auf dem Mac rendern und
Sample für Sample vergleichen.
