# Arbeitstafel Bausteine

Hier steht, **wer gerade woran arbeitet** und **was man noch tun kann**. Mehrere Claude-Chats können parallel arbeiten,
jeder an einem anderen Baustein. Ein neuer Chat liest zuerst `CLAUDE.md`, dann `bausteine/README.md`, dann diese Datei.

## Einen anderen Chat starten

Text zum Einfügen in einen neuen Chat (Baustein austauschen):

> Lies `CLAUDE.md`, `bausteine/README.md` und `bausteine/ARBEITSTAFEL.md` im Repository FM1-Electro-one. Übernimm den
> Baustein **sh101**: trag dich in der Arbeitstafel ein und pushe, dann arbeite den Auftrag in `bausteine/sh101/README.md` ab.
> Arbeite nur in `bausteine/sh101/` und in deinem Abschnitt der Arbeitstafel.

Jeder Baustein hat in seinem `README.md` einen vollständigen Auftrag (Ziel, Quellen mit Dropbox-Pfad, Vorgehen, „fertig,
wenn“, was man nicht anfasst). Neue Bausteine beginnen mit einer Kopie von `_vorlage/`; die Schnittstelle steht in
`baustein.h`; `bausteine/Makefile` findet jeden Baustein-Ordner selbst.

## Regeln für parallele Arbeit

1. **Vor dem Anfangen eintragen:** `git pull --rebase origin main`, beim Baustein „In Arbeit“ auf deinen Chat setzen
   (Name + Datum), sofort committen und pushen. Erst dann loslegen. Steht dort schon ein anderer Chat, nimm einen anderen
   Baustein oder frag den Nutzer.
2. **Nur im eigenen Ordner arbeiten:** Wer `dk808` hat, ändert nur `bausteine/dk808/` (und seinen Abschnitt hier).
   Gemeinsame Dateien (`bausteine/Makefile`, `bausteine/README.md`, `docs/FEATURES.md`, `docs/adr/README.md`) nur
   kurz anfassen, gleich committen, Konflikte beim Rebase zusammenführen, nie fremde Änderungen verwerfen.
3. **Nummern nur aus der Reserve unten nehmen** (ADR, FEATURES), damit zwei Chats nicht dieselbe Nummer vergeben.
   Wer eine nimmt, streicht sie hier unter „frei“.
4. **Fertig oder Pause:** „In Arbeit“ auf „frei“ setzen und „Stand“ und „Nächster Schritt“ so schreiben, dass ein anderer
   Chat ohne Rückfragen weitermachen kann.
5. Der **Bau-Chat** (Gerätebuilds, Versionen, Emulator) bleibt zuständig für alles in `firmware/`, `platform/`,
   `VERSION`. Bausteine werden erst über einen Adapter ins Gerät gebracht; das stimmt man mit ihm ab.

## Reservierte Nummern

| Art | Nummer | Für | Status |
|---|---|---|---|
| ADR | 0040 | Bausteine-Aufbau | vergeben |
| ADR | 0041 | dk808 (DrumKern) im Gerät | frei (reserviert) |
| ADR | 0042 | sh101 (MonoBass) im Gerät | frei (reserviert) |
| ADR | 0043 | mksultra im Gerät | frei (reserviert) |
| FEATURES | P13 | Bausteine-System | vergeben |
| FEATURES | B19 | dk808 | vergeben |
| FEATURES | Y10 | sh101 | vergeben |
| FEATURES | Y11 | mksultra | vergeben |

## Bausteine

### dk808: TR-808 (DrumKern)

- **In Arbeit:** Feature-Chat (session_01WM34DFe7NZGLWrVtNzsA4L), 28.11.2026
- **Vorlage:** Dropbox `Programmieren/808 Projekt/app/Sources/DrumKernDSP/` (`Voices.swift`, `DSPMath.swift`,
  `Engine.swift`, `Mixer.swift`, `Model.swift`), Beschreibung `app/SOUND-ENGINE.md`, Referenzen `app/reference-renders/`
  (Kopie von `manifest.json` und `SHA256SUMS.txt` in `dk808/ref/`)
- **Stand:** Gerüst angelegt, Port in Arbeit.
- **Nächster Schritt:** C-Port aller 17 Stimmen in `double`, Test gegen `ref/manifest.json` (erste 16 Samples, Spitze, RMS,
  Summe je Schlag).
- **Danach frei für andere:** float-Fassung und Lastmessung; Adapter für FM1-Electro (ersetzt `engine/tr808*` als Schalter,
  ADR 0041); Effekte des DrumKern bleiben vorerst weg (Wunsch des Nutzers).

### sh101: SH-101 (MonoBass)

- **In Arbeit:** frei
- **Vorlage:** Dropbox `Programmieren/REA/SH101/app/` — `docs/SOUND-ENGINE.md`, `Sources/MonoBassCore/` (`Oscillator`,
  `Voice`, `Engine`, `Halfband`, `Params`, `Preset`), Testvektoren `docs/test-vectors/` (`vectors.json`, WAVs)
- **Stand:** nicht begonnen.
- **Nächster Schritt:** wie dk808: C-Port in voller Genauigkeit (4× überabgetastete Ladder, 8× Clipper, BLEP), Test gegen
  `vectors.json`, eigener Ordner `bausteine/sh101/`.

### mksultra: Alpha Juno / MKS-50 „Ultra“ (TAL-Pha-Nachbau)

- **In Arbeit:** frei
- **Vorlage:** Dropbox `Programmieren/MKS50 App/TAL Pha Nachbau/app/` — `docs/SOUND_ENGINE_PORTING.md`, `engine/src/*.rs`,
  `spec/*.md`, `MECHANICS.md`, 520 Presets `presets/**/*.mks`, Golden-Renders `docs/golden/` (FNV-1a-Prüfsummen)
- **Stand:** nicht begonnen.
- **Nächster Schritt:** C-Port in voller Genauigkeit (12 Stimmen, DCO 2× Fs, VCF 4× mit zwei nichtlinearen SVFs), Test gegen
  die Golden-Renders. **Für das FM-1 zuerst an den Stimmen sparen, nicht am Filter** (Wunsch des Nutzers 28.11.).

### jx8p, mks50, jp6, tr808alt: vorhandene Engines herauslösen (später, je einzeln)

- **In Arbeit:** jx8p frei · mks50 frei · jp6 frei · tr808alt frei
- Die vorhandenen Engines liegen heute in `engine/`: JX-8P (`engine/jx8p.c`), MKS-50 (`engine/mks.c`), JP-6 (`engine/jp6.c`)
  und die bisherige 808 (`engine/tr808*.c`). Jede wird ein eigener Ordner in `bausteine/` mit demselben Aufbau und
  `baustein.h`. Das ist nur nach Absprache mit dem Bau-Chat möglich, weil die Engines im Gerätebuild stecken. Der Klang
  muss bitgleich bleiben (`make test` im Hauptordner). Erster Schritt ohne Risiko: ein Baustein-Ordner, der die Dateien aus
  `engine/` nur einbindet (keine Kopie) und einen Host-Test über `baustein.h` hat.

### Firmware-Varianten (später)

- **In Arbeit:** frei
- Je Variante eine Datei in `varianten/`; Bauen über Schalter. Braucht zuerst die Adapter (ADR 0041–0043).

## Weitere offene Punkte

- **Dropbox-Ordner `Programmieren` aufräumen:** Vorschlag liegt beim Nutzer, nichts wird verschoben, bevor er zustimmt
  (Feature-Chat, 28.11.).
