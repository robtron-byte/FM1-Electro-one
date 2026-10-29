# 0013: Gated Reverb dicht statt flatternd

- **Status:** vorgeschlagen (Hörvergleich `make gate-ab`: `gate_A`, `gate_B`, `gate_dry`); offen auch: Hall-Send der Clap in den Werks-Grooves
- **Stand:** 2026-10-29

## Kontext

Snare, Clap und Rimshot gehen in einen Gated Reverb (nach TC/AMS „NonLin“). Die Werks-Grooves schicken die Clap mit Send 56 hinein. Rückmeldung: „Clap ohne Hall ist viel besser, durch den Hall kommt so etwas wie ein Doppler dazu, evtl. klingt der Hall einfach nicht gut.“

Ursache: Der Hall (A) besteht aus 15 einzelnen Echos im fast gleichmäßigen Abstand von ~15 ms (20–330 ms) hinter nur zwei kurzen Diffusoren (5 / 6 ms). Jedes Echo wiederholt die ganze Clap; die regelmäßigen Abstände ergeben ein hörbares Flattern (Hüllkurven-Modulation um 64 Hz, +1,7 dB über dem Umfeld) und eine deutliche erste Wiederholung bei 20 ms.

## Entscheidung (vorgeschlagen)

Schalter `FX_GATE_DENSE` in `engine/fx.h` (Standard 0), Variante B (1):

- 24 Echos in **unregelmäßigen** Abständen (5–22 ms), Seiten gemischt, gleiche Gesamtenergie;
- vier längere Diffusoren (5,1 / 7,1 / 9,0 / 10,9 ms, Koeffizient `FX_GATE_K` 0,7) statt zwei: die Echos verschmelzen zu einer Fläche;
- schwillt über die ersten drei Echos an (keine eigene „zweite Clap“), fällt im letzten Fünftel, dann das Gate.

Gemessen (Clap D, Send 56): Hallpegel gleich (−37,9 / −38,2 dB), Flattern um 64 Hz −0,5 dB statt +1,7 dB (keine Spitze mehr). Die Stärke der Diffusion (0,62–0,8) änderte daran kaum etwas.

## Alternativen

- Hall-Send der Clap in den Werks-Grooves auf 0 (trocken wie die echte 808) oder leiser: unabhängig davon möglich, Entscheidung offen.
- Mehr Echos ohne Diffusion: mehr Rechenzeit, Flattern bleibt bei regelmäßigen Abständen.

## Folgen

- RAM: +1 KB (zwei weitere Diffusor-Puffer), Rechenzeit: 9 Lesezugriffe und zwei Allpässe mehr je zwei Samples, nur solange der Hall läuft.
- Betrifft auch Snare und Rimshot.

## Verlauf

- 2026-10-29: angelegt
