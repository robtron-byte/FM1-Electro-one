# 3D-Modell des FM-1

3D-Scan des M-VAVE FM-1, aufgenommen mit einem Revopoint-Scanner und als Volumenmodell aufbereitet. Das STEP-Modell (`FM1.stp`) ist nach dem Ausmessen wieder entfernt worden; es liegt in der Git-Historie (Commit 56a9aab). Die Maße dienen als Referenz für eigene Teile wie die Knöpfe und Kappen in [`../caps`](../caps): Maße abnehmen, Passungen prüfen, Teile am Gerät ausrichten.

| Datei | Inhalt |
|---|---|
| `FM1.png`, `FM1-2.png`, `FM1-3.png` | Renderbilder des Modells |
| [`MASSE.md`](MASSE.md) | Alle Maße von Gehäuse, Tasten, Tastern, Encodern, Display und Anschlüssen, aus dem Modell gemessen |
| `fm1_dimensions.json` | dieselben Maße maschinenlesbar |
| `fm1_panel.scad` | OpenSCAD-Bibliothek mit den Maßen, Umriss, Bedienelementen und einer Beispiel-Frontplatte |
| `fm1_top.png`, `fm1_back.png` | bemaßte Draufsicht und Rückseite |

Für eigene Teile brauchst du das Modell nicht hochzuladen: Die Maße stehen in `MASSE.md`, und mit `fm1_panel.scad` kannst du direkt in OpenSCAD konstruieren.

Das Modell ist ein Scan: Kleine Teile wie Encoder-Achsen und Tastenkanten sind nur so genau wie der Scanner. Für die Passung der Knöpfe gilt weiter: den Passungstest aus `../caps` drucken.
