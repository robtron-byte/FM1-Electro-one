# 0013: Gated Reverb dicht statt flatternd

- **Status:** angenommen: Variante C (Standard), Clap in allen Kits trocken
- **Stand:** 2026-10-29

## Kontext

Snare, Clap und Rimshot gehen in einen Gated Reverb (nach TC/AMS „NonLin“). Die Werks-Grooves schicken die Clap mit Send 56 hinein. Rückmeldung: „Clap ohne Hall ist viel besser, durch den Hall kommt so etwas wie ein Doppler dazu, evtl. klingt der Hall einfach nicht gut.“

Ursache: Der Hall (A) besteht aus 15 einzelnen Echos im fast gleichmäßigen Abstand von ~15 ms (20–330 ms) hinter nur zwei kurzen Diffusoren (5 / 6 ms). Jedes Echo wiederholt die ganze Clap; die regelmäßigen Abstände ergeben ein hörbares Flattern (Hüllkurven-Modulation um 64 Hz, +1,7 dB über dem Umfeld) und eine deutliche erste Wiederholung bei 20 ms.

## Entscheidung

Schalter `FX_GATE_DENSE` in `engine/fx.h`, **Standard 2 (Variante C, siehe unten)**. Zuerst gebaut: Variante B (1):

- 24 Echos in **unregelmäßigen** Abständen (5–22 ms), Seiten gemischt, gleiche Gesamtenergie;
- vier längere Diffusoren (5,1 / 7,1 / 9,0 / 10,9 ms, Koeffizient `FX_GATE_K` 0,7) statt zwei: die Echos verschmelzen zu einer Fläche;
- schwillt über die ersten drei Echos an (keine eigene „zweite Clap“), fällt im letzten Fünftel, dann das Gate.

Gemessen (Clap D, Send 56): Hallpegel gleich (−37,9 / −38,2 dB), Flattern um 64 Hz −0,5 dB statt +1,7 dB (keine Spitze mehr). Die Stärke der Diffusion (0,62–0,8) änderte daran kaum etwas.

**Variante C** (`FX_GATE_DENSE` 2, **gewählt**), nach „B ist besser, aber nicht gut“: Statt einzelner, gleichsinniger Echos (die immer etwas metallisch klingen) ein echter, dichter Hall, den ein Gate abschneidet, wie die 80er es mit Hallplatte und Noise-Gate gemacht haben:

- vier Leitungen im Gate-Puffer (27,8 / 35,7 / 44,0 / 54,1 ms), Hadamard-Rückkopplung 0,94, leichter Tiefpass in der Schleife;
- ein Schlag (schnelle Hüllkurve dreimal über der langsamen) öffnet das Gate für 300 ms, erneut erst nach 100 ms; dann schließt es in ~13 ms und nimmt die Rückkopplung mit: kein Nachklang;
- Pegel wie A und B (`FX_GATE_OUT` 0,57).

Gemessen (Clap D, Send 56): Hallpegel −38,2 dB wie A; kein Flattern (−0,2 dB); Verlauf −27 / −23 / −26 dB bei 50 / 150 / 250 ms, dann −51 dB bei 330 ms und −80 dB bei 400 ms (A: −57 bei 400, B: −41).

**Clap trocken:** In allen Kits hat die Clap keinen Hall-Send mehr (vorher TR-808 24, ELECTRO 56, MIAMI 40; `engine/drums.c`). Gespeicherte Grooves behalten ihre Werte.

## Alternativen

- Mehr Echos ohne Diffusion: mehr Rechenzeit, Flattern bleibt bei regelmäßigen Abständen.

## Folgen

- Variante C: kein zusätzlicher Puffer (die vier Leitungen teilen sich den Gate-Puffer), Rechenzeit: vier Leitungen mit Tiefpass und Hadamard je zwei Samples statt 15 Abgriffe, nur solange der Hall läuft. Auf dem Gerät noch nicht gemessen.
- Betrifft auch Snare und Rimshot.

## Verlauf

- 2026-10-29: angelegt
- 2026-10-29: „Hall B ist besser, aber nicht gut; Hall-Send auf jeden Fall trocken“: Clap-Send in allen Kits 0, Variante C (gegatetes Feedback-Netzwerk) zum Vergleich
- 2026-10-29: C gewählt, Standard. A und B bleiben als Schalter.
