# 0055: Grafik-Kopf je Seite in den Gerüst-Firmwares

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28 (SH-101 0.1.5, JXSH 0.1.3, JP-6 0.1.3, MKS Ultra 0.1.2 gebaut, Emulator-Bilder geprüft; am Gerät offen)

## Kontext

Wunsch des Nutzers: Auf jeder Seite oben ein grafisches Element, das zeigt, wo man ist, und live auf die gerade bediente
Einstellung antwortet: LFO als Wellenform mit Rate, Tiefe und laufendem Punkt; Hüllkurve als ADSR mit dem Punkt, wo die
gespielte Note steht; VCF als Filterkurve aus Cutoff und Reso; Effekte (Delay, Hall, Distortion) einfach nach Einstellung.
Bisher zeigten die meisten Seiten nur ihren Namen groß. Das RAM ist in den Firmwares knapp (SH-101), gezeichnet wird in
Streifen (`GM_BAND_H`).

## Entscheidung

- **Modul im Gerüst:** `gemeinsam/app/gm_viz.c` (Teil von gm_ui.c). Neue Stages `ST_VLFO`, `ST_VENV`, `ST_VVCF`,
  `ST_VDLY`, `ST_VREV`, `ST_VDIST` (vor `ST_USER`). Die Variante setzt sie als Stage ihrer Seiten und liefert die Werte
  über `GM_VIZ(stage, part, float v[8])` (normiert 0…1, dazu Welle, Abschnitt und Pegel der Hüllkurve, Phase des LFO).
  Delay und Hall kommen ohne Haken aus den Effekten des Gerüsts.
- **Bilder:** LFO (Welle, Tiefe = Höhe, mehr Schwingungen bei höherer Rate, laufender Punkt aus der Phase des Klangerzeugers
  oder aus der Zeit), ENV (ADSR; gedrehter Abschnitt orange; Punkt am Pegel der klingenden Note), VCF (Kurve mit
  Resonanzbuckel, 24/12 dB, Hochpass; Hüllkurven-Anteil als Pfeil), DELAY (Echos als abnehmende Striche, Ping-Pong oben/
  unten), REVERB (Abklingen nach Größe, Decay, Dämpfung), DIST (Kennlinie nach Modus TUBE/OD/DIST/RAT und Drive, Tone).
- **Rückmeldung überall:** Gedrehter Regler steht 1,5 s oben links mit Wert (orange), die Kurve färbt sich; auf Seiten
  ohne Bild zeigt die Mitte 1,5 s Regler und Wert groß statt des Namens.
- **Kein Speicher:** nur Linien und Rechtecke in die Streifen; eine Zeit je Bild (`viz_now`), damit Punkte nicht zwischen
  Streifen springen. Keine Division mit möglichem Nenner 0 (Nenner geklemmt). RAM unverändert, Flash +3–4 kB.
- **Varianten:** SH-101 (LFO, ENV mit Punkt der Stimme, VCF, DIST, DELAY, REVERB), JXSH (dasselbe je Erzeuger; JX: ENV-1/2
  mit der jüngsten Stimme des Parts, LFO-Phase des Parts), JP-6 (LFO, VCF, DELAY, REVERB; Hüllkurvenbilder hatte sie),
  MKS Ultra (LFO, VCF, DELAY, REVERB aus den Effekten des Bausteins; ENV-Bild hatte sie).

## Alternativen

- Eigene Bilder je Variante (`kern_stage`): mehrfacher Code; die Arten wiederholen sich in allen Synths.
- Puffer für Kurven: kostet RAM, das SH-101 nicht hat.

## Folgen

- Neue Varianten bekommen die Bilder mit `ST_V*` und einem `GM_VIZ`-Haken.
- Am Gerät prüfen: Bildrate und Lesbarkeit; die Last des Audios ist nicht betroffen (Zeichnen in der Hauptschleife).

## Verlauf

- 2026-11-28: angelegt (sh101-Chat), Wunsch des Nutzers über den Koordinator
