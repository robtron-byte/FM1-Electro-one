# 0010: Bassdrum-Ausklang: Oktav-Körper (Variante C)

- **Status:** angenommen
- **Stand:** 2026-10-26

## Kontext

Der Ausklang der 808-Bassdrum klang zu dünn. Vier Varianten wurden verglichen (`make kick-ab`):

- **A:** durch die Master-Kette (alt);
- **B:** trockener Ausklang;
- **C:** mit Oktav-Körper `TR808_BD_BODY=0.2`;
- **D:** zusätzlich sinkende Tonhöhe (`TR808_BD_SINK=0.06`, Tonhöhe folgt der Lautstärke).

## Entscheidung

Variante **C**. Die Kick läuft an Kompressor und Röhre der Master-Kette vorbei (außer `MB_KICK_THROUGH_MASTER`). A, B und D bleiben als Schalter zum späteren Testen.

## Folgen

- Die Idee der 8W8-Engine (Tonhöhe hängt an der Lautstärke) entspricht Variante D; sie wurde damit schon gehört und nicht gewählt.

## Verlauf

- 2026-10-26: C gewählt
- 2026-10-29: Abgleich mit X0X-RMX vermerkt
