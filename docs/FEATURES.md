# FM1-Electro: Feature-Liste

Zum Abgleichen, Bestellen und Streichen. Jedes Feature hat eine feste Nummer; sie wird nie neu vergeben und nichts wird gelöscht: Umgesetztes wandert nach „Umgesetzt“, Gestrichenes nach „Gestrichen“. Ansprechen einfach mit der Nummer, z. B. „S7 streichen“, „N3 umsetzen“.

**Status:** ○ offen · ◔ angefangen (wo der Stand liegt, steht dabei; nicht doppelt anfangen) · ◐ umgesetzt, wartet auf Gerätetest/Urteil · ✓ umgesetzt und bestätigt · ✗ gestrichen
**Bereiche:** **B** Beat/Drums · **S** Sequencer · **Y** Synths · **F** Effekte/Master · **U** Bedienung/Display · **P** Plattform/Leistung · **N** neu
Neue Nummern: im Bereich die nächste freie (B18, S18, Y9, F11, U15, P11, N26).

Stand: 51. Build (12.11.2026). ADR = Entscheidung in `docs/adr/`. Erste Runde (Auswahl des Nutzers 07.11., Checkliste): S8, N5, N8, N9, N11, B12, N1, N12, N13, N6. Reihenfolge der Umsetzung: N5/N8/N9/N11 → N13 → N1 → B12 → N6 + S8 → N12.

## Offen und angefangen

| Nr | Bereich | Feature | Status | ADR |
|---|---|---|---|---|
| B15 | Beat/Drums | Abgleich mit Referenzen (Mz808, ReplicantTr8s nur für 808-Klang) | ○ | `docs/TR808.md` |
| F8 | Effekte/Master | EP-133-Kompressor nachbauen (braucht Zugriff auf teenage.engineering) | ○ | 0029 |
| U5 | Bedienung/Display | Echtes Vsync (braucht TE-Leitung des Displays, unklar ob verdrahtet) | ○ | – |
| B17 | Beat/Drums | 808-Bassdrum: zu wenig Attack, geht im Mix oft unter (Feedback 12.11.); mehr Klick/Anschlag (z. B. kürzerer Pitch-Sweep, Klick-Anteil, Transient), als A/B-Schalter (ADR 0006) | ○ für später | `docs/TR808.md` |
| Y8 | Synths | Bass-Sounds fast nicht hörbar (Feedback 12.11.): Messung `make bass-check`: die meisten Bässe (FAT BASS, PLAIN BASS, SAW BASS, JUPI BASS …) haben den Grundton bei 32 Hz unter dem Lowcut des Mix und sind leise (RMS 0,02–0,06); anheben (Oktave höher bzw. Sub-Anteil statt 16'), Lowcut prüfen, Pegel angleichen, als A/B | ○ für später | 0027 |
| N14 | Neu | Projekt-Liste: ganze Projekte (alle Plätze A–D aller Bereiche, Sounds, Kit, Tempo, Effekte) unter einem Namen speichern und aus einer scrollbaren Liste laden (Nutzerwunsch 07.11.); ebenso eine scrollbare Liste für Sound-Presets (eigene Sounds speichern, benennen, laden) | ○ | – |
| N15 | Neu | Live-Effekt „MONO DELAY“ nach `mdtest.c` des Nutzers und dem Korg Monotron Delay: Delay 34 ms (kürzeste Zeit des Monotron, PT2399 bei 20 MHz), Feedback 1, Hochpass 100 Hz, Softclip, Korg-35-Tiefpass; Regler GLO + ALGORITHM überall (unteres Fünftel blendet aus, darüber Cutoff 100 Hz – 10 kHz); nicht gespeichert; Pattern-Länge → EDIT + SELECT, Feintempo → EDIT + ALGORITHM | ○ geplant, wartet auf „los“ (Zweig `claude/n15-mono-delay`) | 0033 |
| N17 | Neu | DENSITY: ein Regler dünnt das ganze Pattern aus oder verdichtet es (alle Bereiche oder der gewählte), live und ohne die Steps zu verändern (Idee aus Elixir, Dan Majka, GPL-3.0; Code darf übernommen werden) | ○ | – |
| N18 | Neu | Notenlänge pro Step im STEP-Modus der Synths einstellbar (das Längenfeld aus N6 auch von Hand), z. B. KNOB bei gehaltenem Step (Idee aus Elixir, Dan Majka, GPL-3.0; Code darf übernommen werden) | ○ | – |
| N19 | Neu | Bibliothek im Flash: viele Kits und Sounds (Elixir: je 128) zum Blättern, Speichern und Laden; hängt mit N14 zusammen (Idee aus Elixir, Dan Majka, GPL-3.0; Code darf übernommen werden) | ○ | – |
| N20 | Neu | Umschalt-Hilfe: solange GLO oder EDIT gehalten wird, zeigt das Display alle Kombinationen dieser Taste in der aktuellen Ansicht (Idee aus Elixir, Dan Majka, GPL-3.0; Code darf übernommen werden) | ○ | – |
| N21 | Neu | Farbschemata für den Bildschirm (z. B. GREY, GREEN, AMBER, ICE, VIOLET, PAPER, HI-CON, NIGHT) in OPTIONS (Idee aus Elixir, Dan Majka, GPL-3.0; Code darf übernommen werden) | ○ | – |
| N22 | Neu | Plattenhall nach Mutable Instruments Clouds als zweite Hallart (Elixir hat ihn in Festkomma; MIT-Lizenz) (Idee aus Elixir, Dan Majka, GPL-3.0; Code darf übernommen werden) | ○ | – |
| N23 | Neu | Neue Klangquellen nach Mutable Instruments Rings (Resonator: modal, String, FM) und Plaits (Engines), z. B. als Drum- oder Synth-Modell (Rechenlast prüfen) (Idee aus Elixir, Dan Majka, GPL-3.0; Code darf übernommen werden) | ○ | – |
| N24 | Neu | Neue Firmware „808 + PO-12 + SH-101“, von Anfang an auf flüssige Bedienung und Klang ausgelegt; Plan in `docs/plaene/neue-firmware-808-po12-sh101.md` | ○ Plan, wartet auf „los“ | – |

## Umgesetzt

| Nr | Bereich | Feature | Status | ADR |
|---|---|---|---|---|
| B1 | Beat/Drums | Kalibriertes 808-Modell (BD, SD, RS, CP, CB, CY, OH, CH), in Festkomma auf dem Gerät | ✓ | 0006, 0022 |
| B2 | Beat/Drums | Clap-Ausklang nach Original (D) | ✓ | 0008 |
| B3 | Beat/Drums | Metall: wandernde Oszillatoren, linearer Open Hat | ✓ | 0009 |
| B4 | Beat/Drums | Bassdrum-Ausklang mit Oktav-Körper | ✓ | 0010 |
| B5 | Beat/Drums | Cowbell nach Aufnahme | ✓ | 0012 |
| B6 | Beat/Drums | Hi-Hats X0X/8W8-Bauweise, Closed Hat an echter 808 kalibriert | ✓ | 0014 |
| B7 | Beat/Drums | Snare mit längerem Körper | ✓ | 0015 |
| B8 | Beat/Drums | Drum-Tonart (tonale Drums gestimmt, FOLLOW BASS) | ✓ | 0016 |
| B9 | Beat/Drums | ZAP nach Referenz, PULSE | ✓ | 0017 |
| B10 | Beat/Drums | 5 Kits inkl. PO-12 | ✓ | – |
| B11 | Beat/Drums | Tom und Conga aus der Bedienung genommen (Code noch drin, ~3,5 KB) | ✓ (Gerät 07.11.) | 0028 |
| B13 | Beat/Drums | Drum-Locks pro Step: Tonhöhe (KNOB 1, ±12 Halbtöne, nur BD SD RS CB ZP PU), Velocity (KNOB 2, 7 Stufen); zwei Felder auf dem Display | ✓ (Gerät 07.11.) | 0028 |
| B14 | Beat/Drums | Akzent-Zeile über die 11. schwarze Taste | ✓ (Gerät 07.11.) | – |
| S1 | Sequencer | 4 Plätze A–D je Bereich, Wechsel am Takt | ✓ | – |
| S2 | Sequencer | Generator (Variation / neu) je Bereich und für alles | ✓ | – |
| S3 | Sequencer | Jeder Platz behält seinen Sound (Kit, Regler, Synth-Sound) | ✓ (Gerät 07.11.) | 0024 |
| S4 | Sequencer | Step-Varianz 1:2 … 4:4 (PRESETS bei gehaltenem Step) | ✓ (Gerät 07.11.) | 0025 |
| S5 | Sequencer | Step-Wahrscheinlichkeit 100 … 10 % (ALGORITHM bei gehaltenem Step) | ✓ (Gerät 07.11.) | 0025 |
| S6 | Sequencer | Große Anzeige beim Halten eines Steps (nach ¼ s) | ✓ (Gerät 07.11.) | 0025 |
| S7 | Sequencer | Step-Aufnahme SH-101: Pause mit der letzten Taste | ✓ (Gerät 07.11.) | – |
| S10 | Sequencer | Erster Start ohne Speicher: leere Plätze, Zufalls-Kit und -Sounds | ✓ (Gerät 07.11.) | – |
| Y1 | Synths | MKS-50- und JP-6-Engine je Bereich | ✓ | – |
| Y2 | Synths | BASS einstimmig, mono | ✓ | 0007 |
| Y3 | Synths | Eigene Sounds (32 Plätze) vor den Werks-Sounds, GLO + PRESETS nur eigene | ◐ 45. Build: GLO + SAVE speichert den aktuellen Sound als eigenen (auch ohne Editor); Emulator mit Neustart geprüft | 0026 |
| Y4 | Synths | 23 zusätzliche schlichte Sounds/Pads | ✓ (Gerät 07.11.) | 0027 |
| F1 | Effekte/Master | Punch-in-Effekte (Stutter, Hall-Wurf, Tape-Stop, Repeat, Delay, Crusher, Phaser) | ✓ | 0011 |
| F2 | Effekte/Master | Tape-Stop Standard 1/4 | ✓ (Gerät 07.11.) | – |
| F3 | Effekte/Master | Gated Reverb (echter Hall mit Gate) | ✓ | 0013 |
| F4 | Effekte/Master | Verzerrung als Send (OD, DIST, RAT, TUBE), phasengleich | ✓ (Gerät 07.11.) | 0030 |
| F5 | Effekte/Master | Send-Regler gesperrt bei Verzerrung OFF | ✓ (Gerät 07.11.) | 0031 |
| F6 | Effekte/Master | Master-Kompressor CLUB | ✓ (Gerät 07.11.) | 0029 |
| F7 | Effekte/Master | COMP MIX (Parallel-Kompression) | ✓ (Gerät 07.11.) | 0031 |
| U1 | Bedienung/Display | Tasten-LEDs: inaktive Bereiche gedimmt, OCT blinkt am Ende | ✓ | 0020 |
| U2 | Bedienung/Display | Regler mit vielen Werten beschleunigen beim schnellen Drehen | ✓ (Gerät 07.11.) | – |
| U3 | Bedienung/Display | Gerade gedrehter Wert hervorgehoben | ◐ 36. Build: Amber-Fläche, 0,5 s (Wunsch 07.11.) | – |
| U4 | Bedienung/Display | Display 30 MHz, Bild in einem Zug | ✓ (Gerät 07.11.) | – |
| U6 | Bedienung/Display | Lastanzeige in OPTIONS (DR…PU, PEAK, MAX, C1, LATE) – seit 43. Build aus (Schalter `EL_DEBUG_PANEL`, Wunsch 08.11.) | ✓ | 0019 |
| U8 | Bedienung/Display | OPTIONS: alle 16 Zeilen sichtbar (9 statt 10 Pixel, HELP LINE war abgeschnitten) | ✓ (Gerät 07.11.) | 0031 |
| P1 | Plattform/Leistung | Eigene Plattform ohne SDK, zwei Kerne | ✓ | 0021 |
| P2 | Plattform/Leistung | Bus dynamisch auf beide Kerne (Verzerrung auf Kern 1) | ✓ | 0021 |
| P3 | Plattform/Leistung | Speichern ohne Aussetzer (vorab löschen, Autosave bei STOP) | ◐ noch nicht getestet | 0023 |
| P4 | Plattform/Leistung | Bitgleiche Optimierungen (STRINGS, Hall, Master, JP-6, Saw 5 …) | ✓ | 0019 |
| P5 | Plattform/Leistung | Takt messen (`ADDS …/US` in OPTIONS): Gerät ADD 336, IND 348/µs: eine Addition je Takt, also etwa 340 MHz | ✓ | 0019 |
| P8 | Plattform/Leistung | Web-Simulator | ✓ | – |
| P10 | Plattform/Leistung | Leise Releases der MKS-50-Stimmen enden früher (`MKS_RELEASE_HURRY`, Standard an): Stimmen schneller frei, v. a. bei vollen Akkorden | ◐ im 35. Build | 0019 |
| N5 | Neu | REC gedimmt, solange nicht aufgenommen wird; voll (blinkend) beim Aufnehmen | ◐ umgesetzt (37. Build), wartet auf Gerätetest | – |
| N8 | Neu | Kurzer Druck auf EDIT (ohne etwas anderes) öffnet den Editor des gerade aktiven Bereichs, noch ein kurzer Druck geht zurück. EDIT gehalten bleibt die Umschalttaste wie bisher (EDIT + …) | ◐ umgesetzt (37. Build), wartet auf Gerätetest | – |
| N9 | Neu | Im Editor im Kreis blättern: von der ersten Seite nach links auf die letzte, von der letzten nach rechts auf die erste (Seiten und Abschnitte) | ◐ umgesetzt (37. Build), wartet auf Gerätetest | – |
| N11 | Neu | GLO + ALGORITHM (Pattern-Länge) ändert in ganzen Takten; die Taktzahl erscheint groß im Display, solange gedreht wird. Ungerade Längen in einzelnen Steps nur im STEP-Modus der Synth-Bereiche | ◐ umgesetzt (37. Build), wartet auf Gerätetest | – |
| N13 | Neu | Mute-Modus in BEAT: GLO kurz tippen schaltet ihn an (bleibt an, auch nach dem Loslassen), dann schalten die Instrumenten-Tasten stumm/laut; GLO noch einmal tippen schaltet ihn aus. GLO gehalten bleibt Umschalter für Kombinationen. GLO-LED und Display zeigen „MUTE“. Im Mute-Modus leuchten die Instrumenten-Tasten nach Zustand: voll = hat Schläge und spielt, gedimmt = im Pattern nichts aufgenommen, aus/blinkend = stummgeschaltet | ◐ umgesetzt (37. Build): GLO blinkt, Fußzeile MUTE, stumme Taste blinkt | – |
| B12 | Beat/Drums | Tom/Conga-Code komplett entfernen (spart ~3,5 KB, keine Rechenzeit): Instrumentenliste ohne LT/HT, alle Tabellen verkürzt, alte Stände beim Laden umgerechnet (Speicherversion 26) | ◐ umgesetzt (37. Build): zehn Drum-Teile, Speicherversion 26, ~3,1 KB RAM frei | – |
| N1 | Neu | Drum-Kits merken sich deine Einstellungen: Kit wechseln und zurück bringt das Kit so, wie du es eingestellt hattest (nicht das Werks-Kit) | ◐ umgesetzt (37. Build): gespeichert (Version 26); EDIT + PRESETS holt den Werksklang des Kits | – |
| S8 | Sequencer | STRINGS-Schritte bis 8 Takte, Generator-Stil LONG, Live-Aufnahme in Achteln | ◐ umgesetzt (37. Build) mit N6: Anfang auf 1/8, Länge wie gehalten | 0027 |
| N6 | Neu | STRINGS-**Live**-Aufnahme ohne feste Länge: Das Pattern wächst, bis REC gestoppt wird, gerundet auf ganze Takte, höchstens 4 Takte (32 Achtel). Jede Note wird als **eine** Note mit ihrer ganzen Haltedauer gespeichert (Anfang + Länge, keine aneinander gebundenen Schritte), Anfang und Länge auf Achtel gerundet (mindestens eine Achtel); dafür bekommt ein Step ein Längenfeld (neues Speicherformat) | ◐ umgesetzt (37. Build): Länge in 32steln wie gehalten (nicht auf Achtel gerundet, Wunsch S8), Pattern 1–4 Takte | – |
| N12 | Neu | Fill-ins in BEAT: je Platz A–D ein eigener Fill, immer ein ganzer Takt; manuell (SEQ tippen: im nächsten Takt einmal; halten: wiederholen, solange gehalten) oder automatisch alle 8/16/32 Takte im letzten Takt der Phrase (GLO + SEQ: AUS/8/16/32, Countdown im Display); bearbeiten mit EDIT + SEQ (leerer Fill startet als Kopie des Grooves); der Generator legt einen passenden Fill mit an | ◐ umgesetzt (37. Build): SEQ tippen/halten, GLO + SEQ 8/16/32, EDIT + SEQ bearbeiten, Generator legt Fills an | – |
| B16 | Beat/Drums | Open Hat und Closed Hat auf demselben Step wie bei der TR-808: Statt dass die CH die OH abwürgt, klingt eine kurze OH (Nutzerwunsch 07.11.) | ◐ umgesetzt (38. Build): kurze OH (90 ms), dann abgewürgt | – |
| N4 | Neu | PLAY leuchtet grün, solange der Groove läuft (im Viertel dunkel/hell), die weiße LED aus; gestoppt gedimmt weiß. Die grüne LED sitzt an der freien Matrix-Stelle Spalte 8 / Zeile PA5 (gefunden in der Firmware Elixir von Dan Majka, thefirmwarehub.com) | ◐ umgesetzt (46. Build) | 0020 |
| N2 | Neu | Drum-Vorschau (Instrument klingt beim Drücken seiner Taste) mit OCT+ in BEAT an/aus; OCT+ leuchtet, solange sie an ist; bei REC wird ohne Ton weiter aufgenommen. Die Step-Seiten (1–16 / 17–32) schaltet in BEAT dann OCT− um (hin und her) | ◐ umgesetzt (38. Build): OCT+ Vorschau an/aus (gedimmt = aus), OCT− Seiten | – |
| S11 | Sequencer | Zurücksetzen auf leeren Start per Tastenkombination | ◐ umgesetzt (38. Build): HOME, ARP + 16. weiße Taste zweimal | – |
| N7 | Neu | STRINGS-Aufnahmen länger als 4 Takte: läuft eine Aufnahme über 4 Takte hinaus, rastet sie auf Viertel um und wächst bis 8 Takte (Entscheidung Nutzer 07.11.: Viertel-Raster statt mehr Speicher) | ◐ umgesetzt (38. Build): über 4 Takte → Viertel-Raster, bis 8 Takte | – |
| N3 | Neu | Gedrehter Wert hebt sich ab: etwas größer und in Highlight-Farbe, nach kurzer Pause zurück zum Normalzustand, beides mit kurzer Animation (Ausbau von U3; gilt auch für Synth-Editor und STEP-Modus, vgl. U7) | ◐ 40. Build (Feedback: sprang/flackerte): ohne Animation, Fläche steht sofort und bleibt ruhig stehen, Schrift mit Abstand zum Rand, Wert in gleicher Größe; schließt 0,5 s nach dem letzten Schritt | – |
| U7 | Bedienung/Display | Hervorhebung auch im Synth-Editor und STEP-Modus | ◐ umgesetzt (38. Build): STEP-Modus zeigt die Step-Werte auf KNOB 1–4, hervorgehoben; Editor und Drum-Locks ebenso | – |
| N10 | Neu | Live-Effekte (Punch-ins) im Display: solange gedrückt eine Kachel je Effekt mit Name und kleiner passender Animation (Stutter: Balken im Takt, Hall-Wurf: auslaufende Wellen, Tape-Stop: sinkende Kurve, Repeat: gewählter Step blinkt, Delay: Echos, Crusher: Treppe, Phaser: wandernde Kerbe); mehrere gleichzeitig nebeneinander (bis 7) | ◐ umgesetzt (38. Build): Kacheln über dem unteren Teil von HOME, ~12 Bilder/s | – |
| U9 | Bedienung/Display | Speichern beim Spielen ohne Hänger: die nächste Kopie wird schon beim Einschalten gelöscht (Feedback 07.11.) | ◐ umgesetzt (39. Build) | 0023 |
| Y5 | Synths | Arpeggiator in STRINGS arpeggiert die Akkorde der laufenden Sequenz (ohne gehaltene Taste; Tasten haben Vorrang) (Feedback 07.11.) | ◐ umgesetzt (39. Build) | – |
| U10 | Bedienung/Display | Akzente in der Lauflicht-Leiste: nur wenn die Akzent-Zeile (11. schwarze Taste) gewählt ist, zeigt das Lauflicht die Akzente; bei BD usw. die Schläge des Instruments wie vorher; STEP-Modus wie vorher (Feedback 08.11.) | ◐ 42. Build | – |
| S12 | Sequencer | STRINGS-Patterns nur 1, 2, 4, 8 … Takte (GLO + ALGORITHM, auch im STEP-Modus; Live-Aufnahmen runden auf) (Feedback 07.11.) | ◐ umgesetzt (39. Build) | 0027 |
| U11 | Bedienung/Display | Akkordwahl in STRINGS sichtbar: GLO gehalten zeigt die Tafel CHORD (9 Möglichkeiten, aktive gefüllt, ihre Töne auf einer kleinen Klaviatur); Tasten 1–9 gedimmt, die aktive voll (Feedback 07.11.) | ◐ umgesetzt (39. Build) | – |
| N16 | Neu | BASS-Live-Aufnahme wie STRINGS (N6): Anfang auf 1/16, Note so lang wie gehalten; über 2 Takte hinaus auf 1/8 (bis 4 Takte); BASS-Patterns nur 1, 2, 4, 8 Takte (Wunsch 08.11.) | ◐ umgesetzt (41. Build) | 0027 |
| S13 | Sequencer | Beat länger machen übernimmt das bisherige Pattern in den neuen Takt (wenn der noch leer ist), mit Akzenten, Rolls und Step-Einstellungen (Wunsch 08.11.) | ◐ umgesetzt (44. Build) | – |
| S14 | Sequencer | Fehler: Beat kürzer machen beim Spielen brachte ihn aus dem Takt (Playhead sprang auf den letzten Step); jetzt behält er seinen Platz im Takt, ebenso die Synth-Bereiche (Bug 08.11.) | ◐ behoben (44. Build) | – |
| S15 | Sequencer | Fill-ins (Feedback 09.11.): über den ganzen Takt generiert (vier Arten, die sich aufbauen), SEQ auch in HOME, immer im Takt nach dem aktuellen | ◐ umgesetzt (47. Build) | 0032 |
| S16 | Sequencer | Pattern-Chaining (Wunsch 09.11./12.11.): in HOME einen Platz (A–D) halten und weitere derselben Bank drücken → Kette, bis 4 Glieder, Wiederholungen erlaubt (A>B>B); jedes Glied spielt einen ganzen Durchlauf; ist der neue Platz leer, wird der gehaltene dorthin kopiert und gleich gekettet; Kette je Bereich; ein einfacher Druck löst sie auf; mit SAVE gespeichert (Version 27) | ◐ umgesetzt (49. Build) | 0034 |
| U13 | Bedienung/Display | BEAT: leere Steps 1, 5, 9, 13 leuchten gedimmt (Zählzeiten), damit die Drums leichter zu setzen sind (Wunsch 12.11.) | ◐ umgesetzt (51. Build) | 0020 |
| U14 | Bedienung/Display | OCT − / OCT + leuchten erst, wenn die Oktave verstellt ist: eine tiefer bzw. höher gedimmt, weiter voll, am Ende blinkend (Wunsch 12.11.) | ◐ umgesetzt (51. Build) | 0020 |
| S17 | Sequencer | Live-Aufnahme STRINGS/BASS (Feedback 12.11.): beginnt mit der ersten Taste (ihr Takt = Takt 1, leere Takte davor nicht aufgenommen); ein Takt bleibt ein Takt (Note kurz über den Taktstrich gehalten macht keinen zweiten) | ◐ behoben (50. Build) | 0027 |
| Y7 | Synths | Init-Sounds für RHYTHM, STRINGS, BASS (je MKS-50 und JP-6) am Ende der Sound-Liste: ein Sägezahn, Filter offen, keine Modulation; BASS mono in 4' (gut hörbar). BEAT: EDIT + PRESETS setzt das Kit auf den Werksklang (N1) | ◐ umgesetzt (50. Build) | 0027 |
| Y6 | Synths | Fehler: In RHYTHM wurde immer ein Akkord aufgenommen. Stab-Sounds mit Akkordspeicher (KEY MODE CHORD, z. B. HOUSE STAB, ELEC STAB, GATE CHORD, 4THS STAB, POWER 5+8, ORCH HIT) setzten den Akkord der Spur, und der blieb auch nach dem Wechsel auf einen Sound ohne Akkord. Jetzt endet ein Akkord, der vom Sound kam, mit dem nächsten Sound ohne Akkordspeicher (oder KEY MODE ≠ CHORD im Editor); von Hand gewählte bleiben. GLO + weiße Taste 1–9 wählt den Akkord jetzt auch in RHYTHM (Taste 1 = Einzeltöne), mit der Tafel CHORD. Bei der Schritt-Aufnahme (SH-101) bleibt GLO + Taste das Binden (Feedback 12.11.) | ◐ behoben (48. Build) | 0024 |
| U12 | Bedienung/Display | Fehler: Zeitstempel `now | 1` lag bei geradem `now` 1 ms in der Zukunft → ein kurzer SEQ-Druck galt als gehalten (Fill gleich wieder aus), die Regler-Hervorhebung und die Taktanzeige fielen für ein Bild aus (Flackern) | ◐ behoben (47. Build) | – |

## Gestrichen

| Nr | Bereich | Feature | Status | ADR |
|---|---|---|---|---|
| N25 | Neu | Pattern-Chaining: einen Platz (A–D) gedrückt halten und einen anderen dazu drücken → die Plätze laufen nacheinander (Kette, z. B. A→B, A→C→D); ist der neue Platz leer, wird der aktuelle Platz zuerst dorthin kopiert (Wunsch 09.11.); Kette je Bereich (BEAT, RHYTHM, STRINGS, BASS getrennt); ein einfacher Druck auf einen Platz löst die Kette auf | ✗ doppelt, umgesetzt als S16 | 0034 |
| S9 | Sequencer | Repeat-Punch-in nur solange gehalten | ✗ (zurückgenommen) | 0011 |
| F9 | Effekte/Master | Hall-Modulation seltener rechnen (gebaut, bringt nichts, aus) | ✗ | 0019 |
| F10 | Effekte/Master | Master in Festkomma (gebaut, langsamer, aus) | ✗ | 0019 |
| P6 | Plattform/Leistung | Auf 320 MHz umstellen: nicht nötig, das Gerät läuft schon mit 320–360 MHz | ✗ (nicht nötig) | 0019 |
| P7 | Plattform/Leistung | Überlastschutz (sparsamer rechnen bei Überlast) | ✗ (auf Wunsch) | – |
| P9 | Plattform/Leistung | STRINGS-Chorus mit halber Rate (`MKS_CHORUS_HALF`, `make chorus-ab`) | ✗ (Knackser, Nutzer 07.11.; bleibt aus) | 0019 |
