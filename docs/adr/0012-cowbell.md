# 0012: Cowbell an die echte Cowbell der Aufnahme angepasst

- **Status:** angenommen (Modell 1, Standard)
- **Stand:** 2026-10-29

## Kontext

Rückmeldung: „Die Cowbell klingt noch falsch.“ Ursache: Die bisherige Cowbell (Modell 0) war an Treffer 9 der Aufnahme „808 Basics Kit A“ angepasst, und der ist **keine Cowbell** (geräuschhaft, unharmonisch). Die echte Cowbell ist Treffer 20: ein sauberer Zweiklang aus 573 und 859 Hz mit Obertönen.

| | Original (Treffer 20) | Modell 0 (bisher) |
|---|---|---|
| Filter | eng um ~1 kHz, darüber schnell leiser | breit um 2,9 kHz (Q 0,4), viel zu hell |
| tiefer Ton | −17 dB unter dem hohen | fast gleich laut (−3 dB) |
| Ausklang (dB unter Spitze bei 20 / 100 / 200 / 400 ms) | −16 / −29 / −38 / −53 | −20 / −38 / −60 / Stille |

Das Gerät der Aufnahme läuft 6 % über der 808-Norm (540 / 800 Hz), seine Töne stehen genau 3:2.

## Entscheidung

Schalter `TR808_CB_MODEL`, **Standard 1**, Modell 1 (`cowbell_fitted`, `tr808_cb_cal`), angepasst mit `host/calibration/fit_cowbell.py` (gerendert mit TUNE 69, damit die Tonhöhe dem Gerät entspricht):

- die beiden Rechtecke durch einen engen Bandpass 1033 Hz, Q 7,1, dazu ein wenig ihrer Flanken (Hochpass 900 Hz);
- Klank 89 % mit 21 ms Abfall, Körper mit 139 ms; DECAY skaliert beide;
- schwache Asymmetrie des VCA (gerade Obertöne), DC-Sperre;
- Pegel so laut wie Modell 0 über die ersten 0,1 s.

Die Norm-Tonhöhe 540 / 800 Hz bleibt (TUNE stimmt um).

Fehler gegen die Aufnahme (rms dB; Hüllkurve 5 ms / 20 ms, Spektrum in drei Zeitfenstern, Teiltöne): Modell 1: 1,35 / 1,7 / 4,6 / 6,7, Modell 0: 13,6 / 13,7 / 17,2 / 13,0. Ausklang Modell 1: −15 / −30 / −38 / −50 dB.

## Alternativen

- Sättigung im VCA (tanh): vom Fit verworfen (auf null gedrückt).
- Tonhöhe auf das Gerät der Aufnahme setzen (573 / 859 Hz, 3:2): würde auch Hats und Becken verstimmen (gemeinsame Oszillatoren); per TUNE erreichbar.

## Folgen

- Der Rest-Fehler bei den Teiltönen kommt vom 3:2-Verhältnis des Aufnahme-Geräts: Dort fallen die Mischprodukte des VCA auf Obertöne, bei 540 / 800 Hz (1,48) knapp daneben.
- Standard umgestellt; Simulator und Testbuild werden neu gebaut, sobald der Sound fertig ist.
- Die Cowbell-Aufnahme (573 / 859 Hz) war vermutlich hochgestimmt: Am Becken derselben Aufnahme laufen die Oszillatoren auf Norm (542 / 800 Hz, siehe 0014).

## Verlauf

- 2026-10-29: angelegt; falscher Referenz-Treffer der alten Kalibrierung gefunden, Modell 1 angepasst
- 2026-10-29: „Die neue Cowbell ist super“: Standard
