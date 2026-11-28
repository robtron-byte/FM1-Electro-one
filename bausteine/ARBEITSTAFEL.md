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

## Reihenfolge (Wunsch des Nutzers 28.11.)

1. **sh101** (SH-101) zuerst.
2. Dann die anderen Synths: **mksultra**, danach das Herauslösen von jx8p, mks50 und jp6.
3. **dk808**: die Folgeaufgaben (float-Fassung, Adapter fürs Gerät) erst zum Schluss.

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
| ADR | 0042 | sh101 (MonoBass) im Gerät | vergeben (sh101-Chat) |
| ADR | 0043 | mksultra im Gerät | vergeben (mksultra-Chat) |
| FEATURES | P13 | Bausteine-System | vergeben |
| FEATURES | B19 | dk808 | vergeben |
| FEATURES | Y10 | sh101 | vergeben |
| FEATURES | Y11 | mksultra | vergeben |
| ADR | 0044 | Solo-Firmware-Gerüst (gemeinsam: Plattform, Sequencer, Speichern, MIDI, Raster) + SH101 solo | vergeben (sh101-Chat) |
| ADR | 0045 | mksultra solo | frei (reserviert) |
| ADR | 0046 | jx8p als Baustein + solo | frei (reserviert) |
| ADR | 0047 | mks50 als Baustein + solo | frei (reserviert) |
| ADR | 0048 | jp6 als Baustein + solo | frei (reserviert) |
| ADR | 0049 | Kombi-Firmwares 808 + 101, 808 + 101 + JX | frei (reserviert) |
| ADR | 0050 | dk808 float, Adapter, solo | frei (reserviert) |
| ADR | 0051–0055 | Reserve des Koordinators (auf Anfrage) | frei (reserviert) |
| FEATURES | P14 | Solo-Firmware-Gerüst | vergeben (sh101-Chat) |
| FEATURES | P15 | Kombi-Firmwares | frei (reserviert) |
| FEATURES | Y12 | SH101 solo | vergeben (sh101-Chat) |
| FEATURES | Y13 | mksultra solo | frei (reserviert) |
| FEATURES | Y14 | jx8p solo | frei (reserviert) |
| FEATURES | Y15 | mks50 solo | frei (reserviert) |
| FEATURES | Y16 | jp6 solo | frei (reserviert) |
| FEATURES | B20 | dk808 solo | frei (reserviert) |

## Koordination (Koordinator-Chat "Synth Produktions Werkstatt", 28.11.2026)

Der Koordinator vergibt die Aufträge (höchstens zwei Chats gleichzeitig), nimmt ab und hält diese Tafel aktuell.

**Wo Firmware-Code liegt (der "eigene Bereich"):**
- Solo-Firmware `<name>`: `bausteine/varianten/<name>/` (Code, Adapter, Bedienung, `VERSION` je Firmware, eigene
  Speicherversion). Paket: `firmware/<NAME>-<Version>.fwsc` (z. B. `firmware/SH101-0.1.0-alpha.fwsc`), alte eigene Datei entfernen.
- Gemeinsames Gerüst aller Solo-/Kombi-Firmwares (beide Kerne, Audio im SRAM, Sequencer, Speichern, MIDI, Seitenraster
  nach fm1-synths ADR 0008): `bausteine/varianten/gemeinsam/`. Baut der sh101-Chat; spätere Chats nutzen es und ändern
  es nur abwärtskompatibel.
- `platform/`, `engine/`, `ui/`, Haupt-`Makefile`, `VERSION` und `firmware/FM1_ELECTRO-*` bleiben beim Bau-Chat.
  Ausnahme: das Herauslösen von jx8p/mks50/jp6 (bitgleich, `make test`).
- Werkzeuge auf dem Mac des Nutzers (einmal einrichten, alle nutzen dieselben): JieLi-Toolchain `~/.jieli/toolchain`
  (`platform/tools/get_toolchain.sh`), SDK `~/fw-AC79_AIoT_SDK` (siehe `platform/README.md`), Emulator `~/fm1-emu`
  (siehe `tools/fm1-emu/check.sh`), Docker Desktop.

## Bausteine

### dk808: TR-808 (DrumKern)

- **In Arbeit:** frei (Port fertig, Feature-Chat session_01WM34DFe7NZGLWrVtNzsA4L, 28.11.2026)
- **Vorlage:** Dropbox `Programmieren/Musik/808 Projekt/app/Sources/DrumKernDSP/`, Referenzen `app/reference-renders/`
- **Stand:** Alle 17 Instrumente in `double` portiert (`dk808/dk808.c`). Der Test vergleicht jeden Schlag (Velocity 64
  und 127) mit den Referenz-Renders des Swift-Kerns. Bei Velocity 127 Sample für Sample gegen die Referenz-WAVs geprüft (Abweichung ≤ 2e-24). Bei Velocity 64 lagen die
  WAVs nicht vor; dort stimmen die Kennwerte (erste 16 Samples genau, Spitze, RMS, Summe). Mit der Maske
  `DK808_MASK_FM1_ELECTRO` ohne Laser, Congas, Toms, Claves und Maracas gebaut und geprüft.
- **Frei für andere (je ein eigener Auftrag):**
  1. float-Fassung als Schalter (`DK808_FLOAT`) und Abweichung gegen `double` messen.
  2. Adapter für FM1-Electro (ADR 0041): die 808 von FM1-Electro (`engine/tr808*`) als Schalter ersetzen. Last im
     Emulator und auf dem Gerät messen, mit dem Bau-Chat abstimmen.
  3. Pattern-Vergleich mit `reference-renders/pattern-demo.wav` (braucht den Sequencer-Ablauf aus `Engine.swift`).

### sh101: SH-101 (MonoBass)

- **In Arbeit:** sh101-Chat (Unterchat des Koordinators, Worktree heuristic-easley-bfab0d), seit 28.11.2026: Kern, dann Solo-Firmware + Gerüst `varianten/gemeinsam/`
- **Vorlage:** Dropbox `Programmieren/Musik/REA/SH101/app/` — `docs/SOUND-ENGINE.md`, `Sources/MonoBassCore/` (`Oscillator`,
  `Voice`, `Engine`, `Halfband`, `Params`, `Preset`), Testvektoren `docs/test-vectors/` (`vectors.json`, WAVs)
- **Stand:** nicht begonnen.
- **Nächster Schritt:** wie dk808: C-Port in voller Genauigkeit (4× überabgetastete Ladder, 8× Clipper, BLEP), Test gegen
  `vectors.json`, eigener Ordner `bausteine/sh101/`.

### mksultra: Alpha Juno / MKS-50 „Ultra“ (TAL-Pha-Nachbau)

- **In Arbeit:** mksultra-Chat (Unterchat des Koordinators), 28.11.2026: C-Kern + Test, noch keine Firmware
- **Vorlage:** Dropbox `Programmieren/Musik/MKS50 App/TAL Pha Nachbau/app/` — `docs/SOUND_ENGINE_PORTING.md`, `engine/src/*.rs`,
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

- **Dropbox-Ordner `Programmieren` aufgeräumt (28.11., Wahl des Nutzers):** `Musik/` (M-Vave FM1, 808 Projekt, MKS50 App,
  REA, Rea-Repo), `Apps/` (Timing App, Nebel App, Seed3 Desktop), `Spiele/` (Mc Spiel), `Processing/`. Übersicht in
  `Programmieren/LIES-MICH.md`. Die Pfade in diesem Repository sind nachgeführt. Dropbox-IDs (`id:…`) bleiben beim Verschieben gleich.
