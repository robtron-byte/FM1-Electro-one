# Knöpfe und Tastenkappen zum Drucken

Aufsteckbare Drehknöpfe und Tastenkappen für das M-VAVE FM-1, in Farben passend zur FM1-Electro-Bedienung. Das Gerät wird dafür **nicht geöffnet**: Die Knöpfe ersetzen die vorhandenen, die Kappen stecken über den vorhandenen Tasten.

> Die Maße des FM-1 sind nirgends veröffentlicht. Die STL-Dateien in `stl/` nutzen übliche Werte (6-mm-D-Achse, Taste 10,4 × 5,4 mm). **Erst messen, dann den Passungstest drucken, dann die Teile.**

![Vorschau](preview.png)

## Teile

| Datei | Teil | Anzahl |
|---|---|---|
| `stl/knob_large.stl` | Drehknopf Ø 16 mm für KNOB 1–4 (Klang) | 4 |
| `stl/knob_small.stl` | Drehknopf Ø 13 mm für SELECT, ALGORITHM, PRESETS (Struktur) | 3 |
| `stl/button_cap.stl` | Tastenkappe zum Überstecken | 12 |
| `stl/fit_test.stl` | Passungstest: 7 Bohrungen 5,8–6,4 mm (Punkte zählen die Größe) | 1 |

MASTER bleibt, wie er ist: Das ist der Lautstärkeregler des Geräts.

## Farben

Die Farben folgen der Logik von Display und Anleitung: links Struktur, rechts Klang, die Bereiche in ihren Farben.

| Teil | Farbe | Vorschlag |
|---|---|---|
| SELECT, ALGORITHM, PRESETS | Anthrazit | Struktur |
| KNOB 1–4 | Weiß | Klang |
| FX (BEAT) | Orange | RAL 2004 Reinorange |
| SEL (RHYTHM) | Gelb | RAL 1023 Verkehrsgelb |
| ENV (STRINGS) | Violett | RAL 4005 Blaulila |
| LFO (BASS) | Türkis | RAL 5018 Türkisblau |
| EDIT, GLO | Mittelgrau | Umschalter |
| HOME, SAVE | Weiß | |
| ARP, SEQ | Schwarz | |
| PLAY | Grün | |
| REC | Rot | |

Tipp: Die Zeigerrille oben auf den Knöpfen mit etwas weißer (oder bei weißen Knöpfen schwarzer) Acrylfarbe füllen und überstehende Farbe abwischen.

## 1. Messen

Mit einer Schieblehre, an einer Achse, von der der Knopf abgezogen ist:

| Maß | Wo | Variable in `fm1_caps.scad` |
|---|---|---|
| Achsendurchmesser | über die runde Seite | `shaft_d` |
| Maß über die Abflachung | von der flachen Seite zur runden gegenüber (bei runder Achse = Durchmesser) | `shaft_flat` |
| Achslänge über dem Gehäuse | minus 0,5 mm | `shaft_depth` |
| Abstand der Achsmitten | zwischen benachbarten Knöpfen | Knopf-Ø höchstens Abstand − 2 mm (`large_d`, `small_d`) |
| Tastenbreite und -länge | die vorhandene Taste | `btn_w`, `btn_l` |
| Tastenhöhe über dem Gehäuse | minus 0,3 mm, damit die Kappe nicht aufsitzt | `btn_h` |
| Spalt zur Nachbartaste | muss größer als 2 × `wall` + `btn_fit` sein (ab Werk 1,75 mm) | |

Hat die Achse statt einer Abflachung eine Riffelung (Rändel), nimm den äußeren Durchmesser als `shaft_d` und setze `shaft_flat` = `shaft_d`. Der Knopf hält dann nur durch Klemmung; stelle `fit` auf 0 oder −0,05.

## 2. Passung testen

`stl/fit_test.stl` drucken und auf eine Achse stecken. Die Bohrung, die stramm sitzt, ohne zu knacken, ergibt `fit = (Bohrung − shaft_d) / 2`. Beispiel: 6,2 mm passt bei 6,0-mm-Achse → `fit = 0.1`. Jede Druckerkombination ist etwas anders, darum lohnt sich der Test.

## 3. Anpassen und exportieren

`fm1_caps.scad` in [OpenSCAD](https://openscad.org) öffnen (gibt es kostenlos für Mac), die Werte oben eintragen, `part` auf das gewünschte Teil stellen, F6, dann *Datei → Exportieren → STL*. Oder ohne OpenSCAD: in `make_stl.py` die Werte in `P` ändern und `python3 make_stl.py` ausführen (braucht `pip install manifold3d trimesh numpy matplotlib`).

## 4. Drucken

- PLA oder PETG, Düse 0,4 mm, Schichthöhe 0,12–0,16 mm.
- Knöpfe: 3 Wände, 100 % Füllung, **mit der Oberseite aufs Bett** (die Bohrung zeigt nach oben). So wird die Oberseite glatt, und es braucht keinen Support.
- Kappen ebenfalls mit der Oberseite aufs Bett, die Öffnung zeigt nach oben.
- Die Bohrung nicht nachträglich aufbohren; bei zu engem Sitz lieber `fit` um 0,05 erhöhen und neu drucken.

## 5. Alte Knöpfe abnehmen

1. Gerät ausschalten und USB abziehen.
2. Knopf **gerade nach oben** abziehen, ohne zu kippen. Wenn er fest sitzt: ein Stück Stoff oder eine Zahnseide-Schlaufe unter den Knopfrand legen und gleichmäßig nach oben ziehen, oder zwei Teelöffel als Hebel links und rechts, mit einem Tuch als Schutz für das Gehäuse.
3. Hat der Knopf eine kleine Madenschraube an der Seite, diese erst lösen.
4. Nicht mit Schraubenzieher hebeln und die Achse nicht mit Gewalt drehen: Unter den Knöpfen sitzen Drehgeber, deren Achsen nicht abbrechen sollen.
5. Die alten Knöpfe aufheben, falls du zurückbauen willst.

## 6. Neue Teile anbringen

- **Knöpfe:** mit der Abflachung zur Abflachung der Achse ausrichten und gerade aufdrücken, bis sie knapp über dem Gehäuse stehen (nicht aufsitzen lassen, sonst schleifen sie).
- **Tastenkappen:** einfach überstecken. Sitzen sie zu locker, innen einen Punkt wiederablösbaren Klebstoff (z. B. UHU patafix oder dünnes doppelseitiges Klebeband) setzen. Kein Sekundenkleber: Er kann in die Taste laufen und sie blockieren.
- Danach jede Taste einmal drücken und prüfen, dass sie sauber auslöst und nicht klemmt.

Die Tasten des FM-1 sind fest in das Bedienfeld eingesetzt; sie werden nicht abgenommen, die Kappen kommen darüber. Wer das Gehäuse öffnet, riskiert die Garantie, und für die Kappen ist das nicht nötig.
