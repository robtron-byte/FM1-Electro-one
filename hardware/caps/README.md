# Knöpfe und Tastenkappen zum Drucken

Aufsteckbare Drehknöpfe und Tastenkappen für das M-VAVE FM-1, in Farben passend zur FM1-Electro-Bedienung. Das Gerät wird dafür **nicht geöffnet**: Die Knöpfe kommen auf die vorhandenen Encoder-Achsen, die Kappen stecken über den vorhandenen Tasten.

> Die Maße stammen aus dem CAD-Modell des FM-1 ([`../fm1_model/MASSE.md`](../fm1_model/MASSE.md)). Die Encoder haben **gerändelte Achsen mit 20 Zähnen** (EC11, Ø 6, Rändel außen 5,88, Grund 5,08), die Knöpfe haben eine passende Rändelbohrung. **Erst den Passungstest drucken, dann die Teile.**

![Vorschau](preview.png)

## Teile

| Datei | Teil | Anzahl |
|---|---|---|
| `stl/knob_large.stl` | Drehknopf Ø 16 mm für KNOB 1–4 (Klang) | 4 |
| `stl/knob_small.stl` | Drehknopf Ø 13 mm für SELECT, ALGORITHM, PRESETS (Struktur) | 3 |
| `stl/button_cap.stl` | Kappe für die 12 quadratischen Tasten rechts vom Display | 12 |
| `stl/fit_test.stl` | Passungstest: 7 Rändelbohrungen mit Spiel −0,10 bis +0,20 mm (Punkte zählen die Stufe) | 1 |

MASTER bleibt, wie er ist: Das ist der Lautstärkeregler des Geräts, mit geschlitzter Achse und eigenem Zeiger.

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

## 1. Maße

Alle Werte stehen schon in `fm1_caps.scad` und kommen aus dem Modell:

| Maß | Wert | Variable |
|---|---|---|
| Rändel: Zähne, außen, Grund | 20, 5,88, 5,08 | `knurl_teeth`, `knurl_od`, `knurl_rd` |
| Glatter Bund unter dem Rändel | Ø 6,0, bis 1,5 über dem Panel | `collar_d`, `collar_above` |
| Achse über dem Panel | 6,7 | `shaft_above` |
| Abstand Knopf zum Panel | 0,5 | `knob_gap` |
| Achsabstand | 18 (KNOB 1–4), 17 (links) | Knopf-Ø höchstens Abstand − 2 (`large_d`, `small_d`) |
| Taste rechts: Fuß, oben, Höhe | 8,4, 6,9, 1,26 über dem Panel | `btn_base`, `btn_top`, `btn_h` |
| Tastenraster | 10 | `btn_pitch` |

**Selbst messen musst du nur den Tastenhub:** Drück eine der Tasten rechts, bis sie auslöst, und schätze, wie weit sie dabei nach unten geht. Die Kappe endet `cap_gap` über dem Panel (ab Werk 0,8 mm). Dieser Wert muss größer sein als der Hub, sonst stößt die Kappe auf, bevor die Taste schaltet. Weil die Tasten nur 1,26 hoch sind, greift die Kappe nur auf den oberen 0,46 mm. Sie hält deshalb vor allem mit etwas Klebemasse (siehe unten).

## 2. Passung testen

`stl/fit_test.stl` drucken und nacheinander jede Bohrung auf eine Encoder-Achse stecken. Die Bohrung, die stramm sitzt, ohne zu knacken, ergibt `fit`:

| Punkte | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|
| `fit` | −0,10 | −0,05 | 0 | 0,05 | 0,10 | 0,15 | 0,20 |

Die STL-Dateien sind mit `fit = 0.10` erzeugt. Die Zähne der Bohrung sind nur 0,4 mm hoch und damit fein für eine 0,4-mm-Düse. Greifen sie bei keiner Stufe sauber, stell `bore = "round"` ein: Dann bekommt der Knopf eine glatte Bohrung, die auf den Zahnspitzen klemmt (Ø = `knurl_rd` + 2 · `fit`, dann eher `fit` 0,2–0,3).

## 3. Anpassen und exportieren

`fm1_caps.scad` in [OpenSCAD](https://openscad.org) öffnen (gibt es kostenlos für Mac), bei Bedarf `fit` und `cap_gap` ändern, `part` auf das gewünschte Teil stellen, F6, dann *Datei → Exportieren → STL*. Ohne OpenSCAD geht es auch: in `make_stl.py` die Werte in `P` ändern und `python3 make_stl.py` ausführen (braucht `pip install manifold3d trimesh numpy matplotlib`).

## 4. Drucken

- PLA oder PETG, Düse 0,4 mm, Schichthöhe 0,12–0,16 mm.
- Knöpfe: 3 Wände, 100 % Füllung, **mit der Oberseite aufs Bett** (die Bohrung zeigt nach oben). So wird die Oberseite glatt, es braucht keinen Support, und die Zähne der Bohrung entstehen sauber in der Ebene.
- Kappen ebenfalls mit der Oberseite aufs Bett, die Öffnung zeigt nach oben.
- Die Bohrung nicht nachträglich aufbohren; bei zu engem Sitz lieber `fit` um 0,05 erhöhen und neu drucken.

## 5. Knöpfe aufsetzen

1. Gerät ausschalten und USB abziehen.
2. Laut Modell stecken auf den Encodern keine Knöpfe, nur die gerändelten Achsen. Falls doch Knöpfe darauf sitzen: **gerade nach oben** abziehen, ohne zu kippen, notfalls mit einer Zahnseide-Schlaufe unter dem Rand. Nicht mit dem Schraubenzieher hebeln: Die Achsen der Drehgeber sollen nicht abbrechen.
3. Den Knopf gerade auf die Achse drücken, bis er etwa 0,5 mm über dem Gehäuse steht. Er darf nicht aufsitzen, sonst schleift er. Die Bohrung ist so tief, dass die Achse oben nicht anstößt.
4. Der Zeiger lässt sich in 18°-Schritten ausrichten (20 Zähne): Knopf abziehen, versetzt wieder aufstecken.

## 6. Tastenkappen anbringen

- Einen kleinen Punkt wiederablösbare Klebemasse (z. B. UHU patafix) oder ein Stück dünnes doppelseitiges Klebeband innen auf den Kappenboden setzen und die Kappe gerade auf die Taste drücken.
- Kein Sekundenkleber: Er kann in die Silikonmatte laufen und die Taste blockieren.
- Danach jede Taste einmal drücken und prüfen, dass sie sauber auslöst und die Kappe nicht aufs Gehäuse stößt. Stößt sie auf, `cap_gap` erhöhen und neu drucken.

Die Tasten des FM-1 sind fest in das Bedienfeld eingesetzt; sie werden nicht abgenommen, die Kappen kommen darüber. Wer das Gehäuse öffnet, riskiert die Garantie, und für die Kappen ist das nicht nötig.
