# 0003: App und Oberfläche aus der MKS-50-Firmware, Seiten nach dem JX-8P-Panel

- **Status:** vorgeschlagen
- **Stand:** 2026-11-14 (gebaut, im Emulator geprüft; am Gerät noch nicht)

## Kontext

Die Engine hat die Schnittstelle der MKS-50-Engine (ADR 0001). Die MKS-50-Firmware hat eine fertige, mit dem Nutzer
abgestimmte Bedienung (`../../mks50/docs/adr/0008`), MIDI mit kurzer Latenz (0004), Delay und Hall (0009), Chord und Arp
(0010), den Stand vom Ausschalten (0011) und MIDI-Sync (0012). Der Auftrag (`../../AUFTRAG-JX8P.md`) verlangt diese App
mit den JX-8P-Seiten, eigener Kennung und den Farben der JX-8P-iOS-App.

## Entscheidung

- **`app/jx_app.c`, `app/jx_ui.c`** sind Kopien von `mks50/app/mk_app.c` und `mk_ui.c`, auf `jx_*` / `JP_*` umgestellt.
  Gleich bleiben: Raster, Bedienregeln, Halte-Ebenen, MIDI im Audio-Interrupt, Arp, Chord, MIDI-Sync, Delay und Hall
  auf Kern 1, Speichern, Journal. Der gemeinsame Teil wandert in der zweiten Stufe nach `shared/` (Repo-ADR 0001).
- **Seiten in der Reihenfolge des JX-8P-Panels:**

  | Taste | Seiten (KNOB 1–4) |
  |---|---|
  | HOME | PLAY (CUTOFF, RESO, ENV, RELEASE von ENV-2), MIX (Pegel A–D) |
  | EDIT | DCO-1 (RANGE, WAVE, TUNE, LFO) · DCO-2 (RANGE, WAVE, X-MOD, TUNE) · DCO-2 (FINE, LFO) · DCO ENV (Tiefe DCO-1, DCO-2, Modus, DYNA) · MIXER (DCO-1, DCO-2, ENV, Modus) · VCF (CUTOFF, RESO, ENV, HPF) · VCF (LFO, KEY, Modus, DYNA) · VCA (LEVEL, MODE, DYNA, VOLUME) |
  | ENV | ENV-1 (A, D, S, R) · ENV-2 (A, D, S, R) · ENV KEY (Key-Follow beider, SNAP, RETRIG) |
  | LFO | LFO (RATE, DELAY, WAVE, SYNC; mit SYNC ist RATE der Notenwert) · LFO (SHAPE, KEYSYNC) |
  | FX | CHORUS (CHORUS, MODE, Sends DELAY, REVERB) · CHORUS (RATE für FREE) · DELAY ×2 · REVERB ×2 |
  | ARP | ARP ×2 · CHORD |
  | GLO | TEMPO · MIDI · VOICE ×2 · SYSTEM (Last und Lasttest, ADR 0002) |
  | SEL | Soundliste · SETS (8 ganze Aufbauten, `../../mks50/docs/adr/0013`) |

- **Bilder:** DCO-1 und DCO-2 zeigen ihre Wellen (DCO-2 über DCO-1, mit Sync und X-MOD), MIXER die Summe mit den
  Anteilen, DCO ENV die Hüllkurve mit ihrer Richtung, VCF die Kurve mit HPF-Stufe, ENV-1/ENV-2 ihr ADSR, VCA GATE oder
  ENV-2, LFO die Form, CHORUS die Bewegung.
- **Halte-Ebenen:** EDIT: X-MOD, DCO-2 TUNE, MIX DCO-1, MIX DCO-2 · ENV: ENV-1 ATTACK, DECAY, ENV-2 ATTACK, RELEASE ·
  LFO: RATE, DELAY, Tiefe DCO-1, Tiefe VCF · FX, ARP, GLO wie MKS-50.
- **INIT TONE** (Grundwerte der JX-8P-iOS-App: DCO-1 Sägezahn 8', DCO-2 stumm, Filter offen, VCA über ENV-2) zuerst in der Gruppe INIT, vor PG8X INIT; wie in der MKS-50-Firmware hinten an der Werksliste, ein geänderter INIT TONE öffnet bei SAVE den Dialog, der Gruppenregler hält an den Enden an.
- **Kategorien** aus `jx_cat_names` (12, leere werden übersprungen), dazu EIGENE. 40 eigene Sounds (ein Sound hat hier
  83 Werte statt 54).
- **Eigene Identität:** Kennung `FM-1_97Y` (Y aus der Version; `FM-1_96Y` gehört FM1-Electro, `../../mks50/docs/adr/0014`), Ladebildschirm „JX-8P“ in LED-Rot, `STORE_MAGIC` „JX8P“, Datenfamilie `JX8P`,
  Journal „JXST“ mit 512 Byte je Eintrag. Die Speicher der MKS-50-Firmware werden nicht gelesen.
- **Farben** der JX-8P-iOS-App (`app/jx_theme.h`): Schwarz `#000000` (die App: Blauschwarz `#0D0F12`; Wunsch des Nutzers nach mehr Kontrast, 14.11.2026), Blau `#3D7DD8` (Klangquelle), Hellblau
  `#7FC4E8` (Modulation), Rot der LED-Anzeige `#E8382D` (Filter, Akzent, Scope), Grau der Tastenschrift `#E6E2D6`
  (Hüllkurven, Auswahl), Grau der Schieberkappen `#8E959E` (Effekte).
- **Startsounds:** A POLY SYNTH, B E. BASS, C STRINGS 1, D PIANO 1.
- **Platz für Delay und Hall auf Kern 1** nur, solange ein Part einen Send offen hat. Ohne Send schlafen die Effekte
  (etwa 1 %), und Kern 1 bekommt seinen Anteil an Stimmen (Lasttest 6 Stimmen: 59 % → 49 % im Emulator).

## Prüfung (Emulator)

Alle 30 Seiten angesehen; Spielen über MIDI, Halte-Ebenen, Speichern und Neustart mit dem letzten Stand. `make test`
der Engine unverändert grün.

## Alternativen

- **App gleich nach `shared/` verschieben:** zweite Stufe des Auftrags; erst wenn die JX-App am Gerät läuft.

## Verlauf

- 2026-11-14: angelegt
- 2026-11-14: LFO-Sync wie in der MKS-50-Firmware (`../../mks50/docs/adr/0012`): SYNC auf Seite LFO, RATE = Notenwert, Bild mit Phase und Clock (JX8P_MESS_02)
- 2026-11-14: INIT TONE in der Gruppe INIT, Gruppenregler ohne Umlauf (JX8P_MESS_03)
- 2026-11-14: Sets wie in der MKS-50-Firmware (ADR 0013 dort), JX8P_MESS_04
- 2026-11-14: Seitengedächtnis je Taste und LFO RATE in Hz wie in der MKS-50-Firmware (JX8P_MESS_05)
- 2026-11-14: LFO-Bild nach der Engine, MIDI-Punkt je Part wie in der MKS-50-Firmware (JX8P_MESS_06)
- 2026-11-14: Versionsnummer statt MESS-Zählung (0.1.0-alpha, Kennung FM-1_961)
- 2026-11-14: Kennung FM-1_97Y statt FM-1_96Y (die nutzt FM1-Electro), 0.1.1-alpha
- 2026-11-14: Journal schreibt nur in Spielpausen, Zähler GAP auf SYSTEM (wie MKS-50 ADR 0011), 0.1.2-alpha
- 2026-11-14: Hintergrund reines Schwarz (mehr Kontrast), 0.1.3-alpha
- 2026-11-14: Sequencer wie MKS-50 (`../../mks50/docs/adr/0015`), Oszilloskop ruhig bei Stille, 0.2.0-alpha
- 2026-11-14: Hüllkurvenbild zeigt einen Punkt, der beim Spielen mit der jüngsten Stimme des Parts über die Kurve läuft; LFO-Punkt läuft auch ohne Note mit der eingestellten Rate (oder Clock und Notenwert); Filterbild zeigt bei Modulation eine grüne Linie am wirklichen Cutoff der jüngsten Stimme (wie MKS-50 ADR 0008), 0.2.1-alpha
- 2026-11-14: Hüllkurvenpunkt sitzt auf der gezeichneten Linie (Anteil an der Zeit der Stufe statt des Pegels der Engine, der gekrümmt verläuft und das Release beim aktuellen Wert beginnt); auf der Doppelseite nur der Punkt der hellen Kurve; 0.2.2-alpha
- 2026-11-14: Sequencer: REC nimmt auch MIDI-Noten auf dem Kanal des Parts als Schritte auf (Legato bindet, GLO + Note verlängert, Pause mit der obersten Taste am Gerät); steht SEQ beim Drücken von REC auf ON, läuft die neue Sequenz wie beim FM1-Electro weiter und wächst mit jedem Schritt, die Tasten klingen dabei statt zu transponieren (Wunsch des Nutzers); 0.2.3-alpha
- 2026-11-14: Cutoff-Anzeige: statt Strich und Punkt wandert die Tiefpasskurve selbst (Schulter, Spitze, Flanke) in der Akzentfarbe zum wirklichen Cutoff, die orange Kurve bleibt beim Regler (Wunsch des Nutzers: sah seltsam aus); 0.2.4-alpha
