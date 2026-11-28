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
| ADR | 0045 | mksultra solo | vergeben (sh101-Chat) |
| ADR | 0046 | jx8p als Baustein + solo | vergeben (jx8p-Chat) |
| ADR | 0047 | mks50 als Baustein + solo | vergeben (jx8p-Chat) |
| ADR | 0048 | jp6 als Baustein + solo | vergeben (jx8p-Chat) |
| ADR | 0049 | Kombi-Firmwares 808 + 101, 808 + 101 + JX | frei (reserviert) |
| ADR | 0050 | dk808 float, Adapter, solo | vergeben (jx8p-Chat) |
| ADR | 0051 | Eigene Presets des Nutzers aus den iPhone-Apps in allen Bausteinen | vergeben (jx8p-Chat) |
| ADR | 0052 | Kombi JX-8P + SH-101 (Bedienung wie fm1-synths MKS+JX, Sequencer der iOS-Apps) | vergeben (sh101-Chat) |
| ADR | 0053 | Live-Aufnahme nach RK-008 in jeder Firmware | vergeben (jx8p-Chat) |
| ADR | 0054 | Web-Installer (Browser, Web MIDI, GitHub Pages) | vergeben (Web-Installer-Chat) |
| ADR | 0055 | Grafik-Kopf je Seite (LFO, Hüllkurve, Filter, Effekte live), sh101-Chat | frei (reserviert) |
| FEATURES | P14 | Solo-Firmware-Gerüst | vergeben (sh101-Chat) |
| FEATURES | P15 | Kombi-Firmwares | frei (reserviert) |
| FEATURES | Y12 | SH101 solo | vergeben (sh101-Chat) |
| FEATURES | Y13 | mksultra solo | vergeben (sh101-Chat) |
| FEATURES | Y14 | jx8p solo | vergeben (jx8p-Chat) |
| FEATURES | Y15 | mks50 solo | vergeben (jx8p-Chat) |
| FEATURES | Y16 | jp6 solo | vergeben (jx8p-Chat) |
| FEATURES | B20 | dk808 solo | vergeben (jx8p-Chat) |
| FEATURES | P16 | Kombi JX-8P + SH-101 | vergeben (sh101-Chat) |
| FEATURES | S20 | Live-Aufnahme nach RK-008 | vergeben (jx8p-Chat) |
| FEATURES | P17 | Web-Installer | vergeben (Web-Installer-Chat) |

## Koordination (Koordinator-Chat "Synth Produktions Werkstatt", 28.11.2026)

Der Koordinator vergibt die Aufträge (höchstens zwei Chats gleichzeitig), nimmt ab und hält diese Tafel aktuell.

**Wo Firmware-Code liegt (der "eigene Bereich"):**
- Solo-Firmware `<name>`: `bausteine/varianten/<name>/` (Code, Adapter, Bedienung, `VERSION` je Firmware, eigene
  Speicherversion). Paket: `firmware/<NAME>-<Version>.fwsc` (z. B. `firmware/SH101-0.1.3-alpha.fwsc`), alte eigene Datei entfernen.
- Gemeinsames Gerüst aller Solo-/Kombi-Firmwares (beide Kerne, Audio im SRAM, Sequencer, Speichern, MIDI, Seitenraster
  nach fm1-synths ADR 0008): `bausteine/varianten/gemeinsam/`. Baut der sh101-Chat; spätere Chats nutzen es und ändern
  es nur abwärtskompatibel.
- `platform/`, `engine/`, `ui/`, Haupt-`Makefile`, `VERSION` und `firmware/FM1_ELECTRO-*` bleiben beim Bau-Chat.
  Ausnahme: das Herauslösen von jx8p/mks50/jp6 (bitgleich, `make test`).
- **Kennungen (Update-Lader) für die neuen Firmwares, vom Koordinator vergeben (28.11.):** FM-1_94 SH-101 solo,
  FM-1_95 MKS-50 solo (löst die fm1-synths-Fassung ab), FM-1_97 JX-8P solo (löst die fm1-synths-Fassung ab),
  FM-1_93 MKS Ultra solo, FM-1_92 JP-6 solo, FM-1_91 808 + 101, FM-1_90 808 + 101 + JX, FM-1_99 JX-8P + SH-101 (99 war nur die Dinghy-Vorlage, keine Firmware des Nutzers). Für 808 solo ist keine 9x
  mehr frei: der dk808-Chat prüft im Lader/Hub, ob FM-1_8x geht, sonst teilt sich 808 solo eine Kennung mit 808 + 101.
- **Feste Werte für alle Firmwares (Wunsch des Nutzers 28.11.):** Delay höchstens **0,6 s** in jeder Firmware.
  Polyphone Synths (MKS Ultra, JX-8P, MKS-50, JP-6, auch in Kombis) spielen mit **4 Stimmen** (Pool), SH-101 mono.
  Damit entfällt das Einmessen der Stimmenzahl; gemessen wird nur noch, dass 4 Stimmen sicher < 70 % je Kern bleiben.
- **Keine Distortion in den Synth-Engines (Wunsch des Nutzers 28.11.):** Keine Synth-Engine rechnet eine eigene
  Distortion (JP-6 `dist_process` je Part in `engine/jp6.c`, SH-101 Drive/8×-Clipper). Distortion gibt es, wenn überhaupt,
  nur als **Send-Effekt der Firmware für alle Parts** (wie heute in FM1-Electro `engine/fx.c`, Send DIST). Regeln:
  - Firmwares mit SH-101 (SH-101 solo, JX-8P + SH-101): **der eingebaute Drive des SH-101 bleibt** (Ausnahme, Wunsch des
    Nutzers), zusätzlich Distortion als Send (aus FM1-Electro `engine/fx.c`/`dist.c`).
  - **Reihenfolge der Modi in jedem Distortion-Send: TUBE zuerst** (TUBE, OD, DIST, RAT), TUBE ist der Startwert.
    Gespeicherte Werte bleiben gültig (Anzeige-Reihenfolge umsortieren, gespeicherte Nummer bleibt oder wird beim Laden
    umgerechnet). In FM1-Electro über den Bau-Chat.
  - Übrige Synth-Firmwares (MKS Ultra, JX-8P, MKS-50, JP-6 solo): keine Distortion, auch kein Send; Speicher und Last gehen
    in Stimmen.
  - Firmwares mit 808 (FM1-Electro, 808 solo, 808 + 101, 808 + 101 + JX): Distortion als Send für alle Parts.
  - FM1-Electro: die interne JP-6-Distortion als Schalter nach ADR 0006 ausbauen (Bau-Chat); der Send DIST bleibt.
  - Bausteine behalten sie für die Prüfung gegen die Vorlage (Schalter `*_WITH_DIST`/`SH101_WITH_DRIVE`, Firmwares bauen mit 0).
    Presets mit Distortion klingen in den Firmwares ohne.
- **Eigene Presets des Nutzers (Pflicht, Wunsch 28.11.):** Jeder Baustein und jede Firmware bringt die eigenen Presets des
  Nutzers mit, vor den Werks-Sounds. Quellen (Dropbox): `Programmieren/Musik/MKS50 App/Eigene Presets iPhone/` (`mks50/`,
  `jx8p/`, `jp6/`: JSON der iOS-Apps, Format siehe `LIES MICH.txt` dort) und für mksultra
  `Programmieren/Musik/MKS50 App/MKS-Ultra-Vorlage/app/presets/Ron/` (6 Presets, dem Nutzer am wichtigsten). Neue Presets kommen
  laufend dazu: Jeder Baustein bekommt ein Werkzeug (`tools/` im Baustein), das **alle** Dateien im Quellordner einliest und
  die Preset-Tabelle neu erzeugt (Ergebnis wird eingecheckt, damit der Build ohne Dropbox geht), und ein gemeinsames Skript
  `tools/iphone_presets_pull.sh` holt sie per `xcrun devicectl` vom iPhone in diesen Ordner. Presets, die nur eine
  unveränderte Kopie von INIT sind, auslassen. Dokumentiert in ADR 0051.
- Werkzeuge auf dem Mac des Nutzers (einmal einrichten, alle nutzen dieselben): JieLi-Toolchain `~/.jieli/toolchain`
  (`platform/tools/get_toolchain.sh`), SDK `~/fw-AC79_AIoT_SDK` (siehe `platform/README.md`), Emulator `~/fm1-emu`
  (siehe `tools/fm1-emu/check.sh`), Docker Desktop.

## Bausteine

### dk808: TR-808 (DrumKern)

- **In Arbeit:** jx8p-Chat (Worktree nervous-shamir-666aca), seit 28.11.2026: Firmware 808 solo (B20, ADR 0050; Punkte 1–2 fertig)
- **Stand Gerät (ADR 0050):**
  - `DK808_FLOAT=1`: schlechtestes Instrument das Becken mit −62 dB gegen double (`make float`).
  - Pegel je Instrument im Kern (bitgleich bei 1).
  - Gerüst-Adapter `dk808/dk808_bs.c` (Kit aus 72 Werten, DIST SEND).
  - Messvariante `varianten/dk808` (FM-1_91, kein Paket).
  - Last Kern 0 im Emulator: Leerlauf 12 %, CY +20 %, CH/OH je +8 %, volles Pattern 70 % (Spitze), alle 17
    zugleich 85 %. Delay, Hall und Distortion-Send TUBE laufen auf Kern 1.
- **Nächster Schritt:** Firmware 808 solo (B20: Bedienung, Kits, Paket). Für die Kombis die 808 auf Kern 1 legen oder
  Becken/Hats sparsamer machen (Schalter, nach Gehör); das entscheidet der Kombi-Chat.
- **Vorlage:** Dropbox `Programmieren/Musik/808 Projekt/app/Sources/DrumKernDSP/`, Referenzen `app/reference-renders/`
- **Stand:** Alle 17 Instrumente in `double` portiert (`dk808/dk808.c`). Der Test vergleicht jeden Schlag (Velocity 64
  und 127) mit den Referenz-Renders des Swift-Kerns. Bei Velocity 127 Sample für Sample gegen die Referenz-WAVs geprüft (Abweichung ≤ 2e-24). Bei Velocity 64 lagen die
  WAVs nicht vor; dort stimmen die Kennwerte (erste 16 Samples genau, Spitze, RMS, Summe). Mit der Maske
  `DK808_MASK_FM1_ELECTRO` ohne Laser, Congas, Toms, Claves und Maracas gebaut und geprüft.
- **Frei für andere (je ein eigener Auftrag):**
  1. ~~float-Fassung als Schalter~~ (erledigt, ADR 0050).
  2. Adapter für FM1-Electro (ADR 0041): die 808 von FM1-Electro (`engine/tr808*`) als Schalter ersetzen. Last im
     Emulator und auf dem Gerät messen, mit dem Bau-Chat abstimmen.
  3. Pattern-Vergleich mit `reference-renders/pattern-demo.wav` (braucht den Sequencer-Ablauf aus `Engine.swift`).

### sh101: SH-101 (MonoBass)

- **In Arbeit:** frei (sh101-Chat fertig, 28.11.2026)
- **Vorlage:** Dropbox `Programmieren/Musik/REA/SH101/app/` — `docs/SOUND-ENGINE.md`, `Sources/MonoBassCore/`, Testvektoren `docs/test-vectors/`
- **Stand Baustein** (`bausteine/sh101/`, Y10): C-Port, alle 7 Testvektoren bitgleich mit den Referenz-WAVs. Schalter
  `SH101_WITH_POLY`, `SH101_WITH_ARPSEQ`, dazu die Gerätefassung `SH101_FLOAT` + `SH101_FASTMATH` (RMS ≤ 6e-5 neben der
  Vorlage, eigener Testlauf).
- **0.1.2 (28.11.):** Distortion-Send TUBE/OD/DIST/RAT (FM1-Electros dist.c, Kern 1), Drive bleibt, mono, Delay ≤ 0,6 s; Kern 0 57,5 %, Kern 1 13,9 %.
- **Stand Solo-Firmware** (`varianten/sh101/`, Y12, ADR 0042): `firmware/SH101-0.1.4-alpha.fwsc` (Delay/Hall-Fehler aus 0.1.3 behoben), Kennung FM-1_94, eine
  Stimme, 46 Parameter, 14 Werkssounds, Arp, SH-101-Sequencer, Delay/Hall auf Kern 1. `check.sh` ok. Last im Emulator:
  eine Note 44 %, mit Drive 58,5 % Spitze (Kern 0), Effekte 6,6 % (Kern 1). Am Gerät nicht gemessen.
- **Stand Gerüst** (`varianten/gemeinsam/`, P14, ADR 0044): Plattform, shared und App-Rahmen aus fm1-synths (2a7b4fa),
  Variante = Tabellen + `kern_*`-Haken, `tools/build.py` (Mac: Docker über `mac_docker.py`). Anleitung und Kennungen in
  `varianten/gemeinsam/README.md`.
- **Offen:** Gerätetest (Last mit Drive und Distortion, Pegel `SH_GAIN`, Bedienung, Speichern über einen Neustart); Speicher knapp (RAM 4 kB frei, Pool 2 kB; 0.1.3); nur noch Kennungen frei laut Tafel; `release.yml` legt für `firmware/SH101-*` keinen Release an (Bau-Chat). Poly gestrichen (bleibt mono).

### mksultra: Alpha Juno / MKS-50 „Ultra“ (Rust-Nachbau)

- **In Arbeit:** frei (Solo-Firmware 0.1.0-alpha gebaut, sh101-Chat, 28.11.2026)
- **Solo-Firmware** (`varianten/mksultra/`, ADR 0045, Y13): A = 1 Stimme mit vollem Filter (`firmware/MKSULTRA-0.1.1-alpha.fwsc`),
  B = 2 Stimmen mit VCF 2× (`MKU_B=1`, nur `build/`). Stimmen auf Kern 1, Master auf Kern 0 im Fließband. 6 Stimmen passen
  in keiner Fassung (A ~300 %, B ~220 % bei 140 %). Offen: Frage A/B an den Nutzer; Delay 0,5 s (Micro Ticks braucht
  1,17 s); 307 Presets fehlen (Gerüst zählt Werkssounds in einem Byte); Knacks beim Preset-Wechsel.
- **Vorlage:** Dropbox `Programmieren/Musik/MKS50 App/MKS-Ultra-Vorlage/app/` — `docs/SOUND_ENGINE_PORTING.md`, `engine/src/` (Rust),
  `spec/`, `MECHANICS.md`, 520 Presets in `presets/`, Golden-Renders `docs/golden/` (FNV-1a-Prüfsummen)
- **Stand:** C-Kern fertig, alle 7 Golden-Renders bitgenau (`make -C bausteine mksultra`). 520 Presets als C-Tabelle,
  alle gegen den Rust-Kern geprüft (`make sweep`, bitgleich).
  Sparfassung `MKSULTRA_VOICES` (weniger Stimmen, Filter unverändert): bei 6 und 4 Stimmen alle Golden-Fälle gleich.
  Rechenzeit auf dem PC ≈ 100 ns je Stimme und Sample (Unisono ≈ 330 ns). Details in `mksultra/README.md`, ADR 0043.
  **Ron-Presets** (`presets/Ron/`, ADR 0051): `tools/mks2c.py` liest den ganzen Ordner neu ein und stellt sie an den
  Anfang der Tabelle (Index 0–5). Jedes hat einen eigenen Render gegen den Rust-Kern (`ref/ron.txt`), alle bitgleich.
  Sparfassung mit 6 Stimmen: alle 6 Ron-Presets bitgleich. Mit 4 Stimmen weichen R2 Random, Bass Hit 01 und Stringer
  ab (sie spielen 6-stimmig).
- **Nächster Schritt:** Solo-Firmware „Alpha Juno solo“ (ADR 0045, Y13), sobald `varianten/gemeinsam/` abgenommen ist:
  Stimmenzahl und Puffergröße (`MKSULTRA_MAX_SR`, Delay 2 × 4 s ≈ 1,5 MB bei 48 kHz) im Emulator einmessen.
  **Pflicht für die Firmware:**
  - Die Ron-Presets stehen an vorderster Stelle (Preset 1–6).
  - Mindestens 6 Stimmen, damit die Ron-Presets klingen wie im Baustein. Weniger nur nach Rücksprache mit dem Nutzer.
  - Die Ron-Prüfung (`make -C bausteine mksultra`) muss auch mit den Schaltern der Firmware grün bleiben.

### jx8p, mks50, jp6, tr808alt: vorhandene Engines herauslösen (später, je einzeln)

- **In Arbeit:** jx8p: jx8p-Chat (Worktree nervous-shamir-666aca), seit 28.11.2026: Solo-Firmware JX-8P (FM-1_97), danach MKS-50 solo und JP-6 solo · mks50 frei (Baustein fertig, jx8p-Chat, 28.11.2026) · jp6 frei (Baustein fertig, jx8p-Chat, 28.11.2026) · tr808alt frei
- **jx8p Stand:** `bausteine/jx8p/` bindet `engine/jx8p.c` und Co. ein (keine Kopie, `engine/` unverändert), Adapter
  `jx8p_bs.c` auf `baustein.h`. Alle 74 Presets (7 eigene des Nutzers vorn, dann 64 Werks-Töne und 3 INIT) über den
  Baustein Sample für Sample gleich mit dem Kern, Prüfsummen in `ref/`. Last auf dem PC ≈ 18 ns je Stimme und Sample.
  Eigene Presets: `tools/eigene2c.py` (ADR 0051), gemeinsames Skript `tools/iphone_presets_pull.sh`. ADR 0046, Y14.
- **JX-8P solo** (`varianten/jx8p/`, Y14, ADR 0046): `firmware/JX8P-0.2.7-alpha.fwsc` (Pool 4, Delay ≤ 0,6 s) = fm1-synths-Fassung fortgeführt
  (Wahl A: FM-1_97, `STORE_MAGIC` „JX8P“, 4 Parts, Pool 6 Stimmen auf 2 Kernen, SETS; Speicher bleibt), 7 eigene Presets
  angehängt, EIGENE zuerst. Pool-Kern bitgleich mit dem Baustein (`make -C bausteine jx8p`). Emulator ≤ 23 % je Kern.
  **Am Gerät ungeprüft.** (0.1.0 auf dem Gerüst: zurückgezogen.)
- **mks50 Stand:** `bausteine/mks50/` bindet `engine/mks.c` und Co. ein (mit `EL_SYNTH_JX=0`, `engine/` unverändert),
  Adapter `mks50_bs.c` wie jx8p. Alle 79 Presets (23 eigene vorn, dann 56 Werks-Sounds) bitgleich mit dem Kern, Prüfsummen
  in `ref/`. Last auf dem PC ≈ 20 ns je Stimme und Sample (WOBBLE 01 ≈ 40 ns). ADR 0047, Y15.
- **MKS-50 solo** (`varianten/mks50/`, Y15, ADR 0047): `firmware/MKS50-0.2.7-alpha.fwsc` (Pool 4, Delay ≤ 0,6 s) = fm1-synths fortgeführt (FM-1_95,
  „MK50“, 4 Parts, Pool auf 2 Kernen, SETS; Speicher bleibt), 23 eigene Presets angehängt, EIGENE zuerst, Schalter
  `MK_RELEASE_HURRY` (aus). Pool-Kern bitgleich mit dem Baustein (mit Hurry, `make -C bausteine mks50`). Emulator ≤ 35 %.
  **Am Gerät ungeprüft.**
- **jp6 Stand:** `bausteine/jp6/` bindet `engine/jp6.c`, `dist.c` und Co. ein (`engine/` unverändert), Adapter `jp6_bs.c`
  wie jx8p. Alle 79 Presets (5 eigene aus der Sicherung vorn, dann 74 Werks-Sounds) bitgleich mit dem Kern, Prüfsummen in
  `ref/`. Last auf dem PC ≈ 13 ns je Stimme und Sample (12 VCO 01 ≈ 23 ns). ADR 0048, Y16.
- **JP-6 solo** (`varianten/jp6/`, Y16, ADR 0048): `firmware/JP6-0.1.2-alpha.fwsc`, Gerüst, FM-1_92 („JP01“), 4 Stimmen,
  ohne Distortion (`JP6_BS_DIST=0`), Delay ≤ 0,6 s, 5 eigene Presets vorn. Emulator ≤ 41 % (Kern 0). **Am Gerät ungeprüft.**
- **In Arbeit:** sh101-Chat (Worktree heuristic-easley-bfab0d), seit 28.11.2026. **Stand:** `firmware/JXSH-0.1.2-alpha.fwsc`
  (FM-1_99): Gerüst mit `GM_ENGINES` (abwärtskompatibel, SH-101/MKS Ultra/JP-6 bauen), Seiten PARTS/MIX/MIDI, ein SH-101,
  JX-Pool 4 auf Kern 1, Distortion global + Send je Part, eigene Presets vorn, Sequencer und LOOP des Gerüsts. Emulator:
  Kern 0 60 %, Kern 1 36 % (mit Distortion RAT), RAM 13,1 kB frei; JX und SH gleich laut (Gain-Staging), Sequencer der iOS-Apps (`GM_SEQ_IOS`), `check.sh` ok. Gerüst: Kern-1-Wettlauf um `fx_in` behoben (`fx_src`).
  ADR 0052, P16. Am Gerät ungeprüft.
- **Vorbild Bedienung:** die vorhandene Firmware MKS-50 + JX-8P aus fm1-synths (`combo/`, `MKSJX-*.fwsc`, Kennung FM-1_98):
  Parts, Seite PARTS, Raster nach fm1-synths ADR 0008. Hier: Parts spielen JX-8P oder SH-101 (Bausteine `jx8p`, `sh101`).
- **Sequencer:** wie in den iOS-Apps des Nutzers (Dropbox `Programmieren/Musik/MKS50 App/JX8P/ios/Shared/UI/SequencerViews.swift`,
  `PatternBank.swift`, `SynthViewModel.swift`; dieselbe Logik in `Mks50-iOS-/`). Umfang und Bedienung von dort übernehmen,
  auf das Gerüst `varianten/gemeinsam/` (dessen Sequencer erweitern, abwärtskompatibel). Eigene Patterns des Nutzers aus den
  Apps (falls in den App-Daten auf dem iPhone, siehe `tools/iphone_presets_pull.sh`) wie die eigenen Presets mitnehmen.
- **Nummern/Kennung:** ADR 0052, FEATURES P16, FM-1_99, Paket `firmware/JXSH-<Version>.fwsc`, Code in `bausteine/varianten/jxsh/`.

### Live-Aufnahme nach RK-008 in jeder Firmware (Wunsch des Nutzers 28.11.)

- **In Arbeit:** frei (fertig, jx8p-Chat, 28.11.2026)
- **Stand:** Modul `varianten/gemeinsam/shared/seq/syn_live.h` (Host-Test in `make -C bausteine test`), im Gerüst (REC, Seite
  LOOP über `ST_LIVE`/`LVK`, Blob 4) und in den fm1-synths-Apps. Pakete JX-8P 0.2.7, MKS-50 0.2.7, JP-6 0.1.1 (Emulator ok,
  Gerät offen). ADR 0053, S20. **Offen:** SH-101 und MKS Ultra: Seite LOOP eintragen und neu bauen (sh101-Chat);
  FM1-Electro (Bau-Chat).
- **Ziel:** spontan kurze Sequenzen aufnehmen, standardmäßig in jeder Firmware (Gerüst `varianten/gemeinsam/`, die
  fm1-synths-abgeleiteten Solo-Firmwares jx8p/mks50/jp6 und FM1-Electro; dort als Schalter nach ADR 0006 über den Bau-Chat).
- **Verhalten (Logik des Retrokits RK-008, Handbuch; Code ist nicht öffentlich):**
  1. REC einmal drücken: Aufnahme startet sofort, ohne Einzähler. REC doppelt drücken: mit 1 Takt Einzähler
     (Entscheidung des Nutzers, umgekehrt zum RK-008).
  2. Ist das Pattern leer, legt die erste Aufnahme die Länge fest, **gerundet auf ganze Takte** (Entscheidung des Nutzers).
  3. Danach läuft die Schleife; weitere Durchgänge nehmen dazu auf (Overdub). REC übernimmt, STOP verwirft den letzten Durchgang.
  4. **Quantisierung immer fest auf 1/16** beim Aufnehmen (Entscheidung des Nutzers), Notenlänge wie gehalten.
  5. Länge nachträglich: verlängern (Inhalt wiederholen oder leer), kürzen (Anfang oder Ende behalten), in ganzen Takten.
  6. Höchstlänge nach Speicher des FM-1 festlegen (Ziel 8–16 Takte), Display zeigt Takt und Länge groß.
- Bestehende STRINGS-Live-Aufnahme in FM1-Electro (N6) geht in diesem Verhalten auf, kein zweiter Weg.
- **Nummern:** ADR 0053, FEATURES S20 (Quelle: RK-008-Handbuch, Retrokits).

### Firmware-Varianten

- **In Arbeit:** frei
- Gerüst `varianten/gemeinsam/` steht (ADR 0044); erste Variante `varianten/sh101/`. Neue Variante: App-Datei mit Tabellen und `kern_*`-Haken plus `tools/build.py` (siehe `varianten/gemeinsam/README.md`).

### Web-Installer (Browser, Web MIDI, GitHub Pages)

- **In Arbeit:** frei (Web-Installer-Chat fertig, 28.11.2026)
- **Stand:** Seite online: https://robtron-byte.github.io/FM1-Electro-one/ (`web/installer/`, ADR 0054, P17). Web MIDI,
  M-UPGRADE wie `platform/tools/fm1_install.py`, Paketprüfung, Liste live aus `firmware/` auf `main` (neueste Version je Firmware,
  Beschreibungen in `beschreibungen.json`). Test `node web/installer/test/test.mjs`: 98 Prüfungen gleich mit Python.
  **Am Gerät ungeprüft.**
- **Offen:** `web/installer/installer-pages.yml` nach `.github/workflows/` verschieben (Chat-Schlüssel ohne Scope
  `workflow`); nötig nur noch für Änderungen an der Seite selbst (sonst `gh-pages` von Hand, ADR 0054). **Neue Pakete
  erscheinen von selbst**, sobald sie als `NAME-X.Y.Z-alpha.fwsc` auf `main` liegen; neuer Name: Zeile in
  `web/installer/beschreibungen.json`.

## Weitere offene Punkte

- **Dropbox-Ordner `Programmieren` aufgeräumt (28.11., Wahl des Nutzers):** `Musik/` (M-Vave FM1, 808 Projekt, MKS50 App,
  REA, Rea-Repo), `Apps/` (Timing App, Nebel App, Seed3 Desktop), `Spiele/` (Mc Spiel), `Processing/`. Übersicht in
  `Programmieren/LIES-MICH.md`. Die Pfade in diesem Repository sind nachgeführt. Dropbox-IDs (`id:…`) bleiben beim Verschieben gleich.
