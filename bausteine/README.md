# Bausteine: Klangerzeuger zum Kombinieren

**Kurz:** Jeder Klangerzeuger (808-Drums, SH-101-Bass, Alpha-Juno/MKS-Ultra, JX-8P …) wird hier ein eigener, abgeschlossener
**Baustein**. Aus den Bausteinen lassen sich verschiedene Firmwares zusammenstellen: zum Beispiel nur 808 + SH-101, oder
808 + SH-101 + JX-8P, oder nur der Alpha Juno für den vollen Klang auf einem FM-1. Welche Kombination am besten passt,
entscheidest du nach Gehör.

Wer gerade woran arbeitet und was noch zu tun ist: **[ARBEITSTAFEL.md](ARBEITSTAFEL.md)**.
Die Entscheidung dahinter steht in [docs/adr/0040](../docs/adr/0040-bausteine.md).

## Was ist ein Baustein?

Ein Ordner mit genau einem Klangerzeuger:

| Datei im Ordner | Wofür |
|---|---|
| `README.md` | Was der Baustein ist, woher er stammt, Stand, wie man ihn prüft |
| `<name>.h`, `<name>.c` | Der Klangkern: reines C, ohne Bezug zum FM-1, zu Display oder Tasten |
| `test/` | Prüfprogramm für den PC: rendert WAV-Dateien zum Anhören und vergleicht mit Referenzen |
| `ref/` | Referenzdaten aus dem Original-Nachbau (Prüfsummen, erste Samples, Pegel) |

Regeln, damit Bausteine austauschbar bleiben:

1. **Klang zuerst, in voller Genauigkeit.** Der Kern rechnet wie die Vorlage (z. B. `double` wie der Swift-Code des DrumKern).
   So ist er die Referenz für stärkere Geräte in der Zukunft. Sparfassungen für das FM-1 (float, Festkomma, weniger
   Stimmen) kommen als eigener Schalter dazu und werden gegen diese Referenz verglichen.
2. **Kein Bezug nach außen.** Ein Baustein kennt weder `engine/` noch `ui/` noch die Plattform. Er bekommt Noten,
   Reglerwerte (0…1) und eine Abtastrate und liefert Samples. Was ihn mit FM1-Electro verbindet, ist ein **Adapter**
   (kommt später, eigene Datei je Baustein).
3. **Teile abschaltbar.** Was man nicht immer braucht (z. B. Toms, Congas, Laser beim 808), lässt sich beim Bauen mit einem
   Schalter weglassen. Im Baustein ist trotzdem alles drin.
4. **Einzeln prüfbar.** `make -C bausteine test` prüft alle Bausteine; `make -C bausteine dk808` nur einen.
   Die Prüfung ändert nichts am übrigen FM1-Electro (`make test` im Hauptordner bleibt wie es ist).

## Für Chats und Programmierer

- `baustein.h`: gemeinsame Schnittstelle (Init, Note an/aus, Parameter 0…1, Render).
- `_vorlage/`: Kopiervorlage für einen neuen Baustein.
- Jeder Baustein-Ordner hat in `README.md` einen vollständigen Auftrag; `ARBEITSTAFEL.md` zeigt, wer woran arbeitet.

## Die Bausteine

| Ordner | Klangerzeuger | Vorlage | Stand |
|---|---|---|---|
| [`dk808/`](dk808/) | TR-808, alle 17 Instrumente | DrumKern (Swift, „808 Projekt“) | siehe Arbeitstafel |
| [`sh101/`](sh101/) | SH-101, monophon | MonoBass (Swift, REA/SH101) | siehe Arbeitstafel |
| [`mksultra/`](mksultra/) | Alpha Juno / MKS-50 „Ultra“ | MKS Ultra (Rust, TAL-Pha-Nachbau) | siehe Arbeitstafel |

Schon vorhanden in FM1-Electro, noch nicht als Baustein herausgelöst: JX-8P (`engine/jx8p.c`), MKS-50 (`engine/mks.c`),
JP-6 (`engine/jp6.c`), unsere bisherige 808 (`engine/tr808*.c`). Sie bleiben, wo sie sind, bis jemand sie hier
herauslöst (siehe Arbeitstafel).

## Firmware-Varianten

In [`varianten/`](varianten/) steht für jede geplante Firmware, welche Bausteine sie enthält und mit welchen Schaltern sie
gebaut wird. Eine Variante ist eine Liste von Bausteinen plus Einstellungen, kein eigener Code.

## Prüfen

```sh
make -C bausteine test        # alle Bausteine
make -C bausteine dk808       # nur die 808
```

Die Prüfung schreibt WAV-Dateien nach `bausteine/build/` zum Anhören.
