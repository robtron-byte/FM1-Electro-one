# 0007: Bedienung

- **Status:** abgelöst durch 0008 (Bedienung und Farben); der Nutzer fand sie zu unübersichtlich
- **Stand:** 2026-11-10 (gebaut, im Emulator geprüft, am Gerät noch nicht)

## Kontext

Der Auftrag (Schritt 4) verlangt Part-Wahl, Sound-Wahl (alle Werks-Sounds plus eigene), einen Editor für alle MKS-50-Parameter, einen MIDI-Kanal pro Part und Speichern im Flash. Der Nutzer wünscht:

- **einfach und konsistent:** links = weniger/zurück, rechts = mehr/weiter; Regler starten am aktuellen Wert; kurze Klicks für Häufiges, Kombinationen nur für Seltenes; das Display zeigt, was passiert;
- **Beschriftung des FM-1 nutzen, wo sie passt,** und die Bezeichnungen und Farben am Alpha Juno / MKS-50 ausrichten;
- **einfaches Speichern** und **Kategorien** für die Presets (ADR 0006);
- **kleine Latenz** (ADR 0004): Die Bedienung darf nie eine Note aufhalten. Sie läuft in der Hauptschleife und reicht ihre Änderungen über die Warteschlange an den Audio-Interrupt weiter (wie die Messfirmware).

Das FM-1 hat: Encoder SELECT, PRESETS, ALGORITHM, KNOB 1–4 (je mit Rastung), den MASTER-Regler, OCT−/OCT+, die Tasten FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY/STOP, REC und 27 Notentasten. Auf den Obertasten stehen OP1–OP6, PIT, GLO, MONO, POLY; das sind Notentasten und bleiben es.

## Entscheidung

### Die Regler haben immer dieselbe Bedeutung

| Bedienelement | Bedeutung | Passt zur Beschriftung |
|---|---|---|
| **SELECT** | Part A–D wählen (links A, rechts D) | „Select“ |
| **PRESETS** | Sound innerhalb der Kategorie (links vorheriger, rechts nächster) | „Presets“ |
| **ALGORITHM** | Gruppe wechseln: in HOME die Kategorie, im Editor den Abschnitt | |
| **KNOB 1–4** | die vier Werte der Seite, von links nach rechts wie auf dem Display | „Knob“ |
| **MASTER** | Gesamtlautstärke | |
| **OCT− / OCT+** | Oktave der Notentasten | |
| **Notentasten** | spielen den gewählten Part | |

Jeder Regler startet am aktuellen Wert (kein Sprung). Beim Drehen eines KNOB wird sein Wert groß eingeblendet. Bei Werten mit Namen (PULSE, RANGE, CHORUS …) schaltet jede Raste einen Schritt, bei 0–127 beschleunigt eine schnelle Drehung.

### Die Tasten

| Taste | Kurz | Lang (1 s) |
|---|---|---|
| **HOME** | Spielansicht (alle Parts) | letzten ungespeicherten Stand des Sounds zurückholen |
| **EDIT** | Editor öffnen; weiter zum nächsten Abschnitt (DCO → VCF → VCA → ENV → LFO → CHORUS → PATCH → MIDI → DCO) | |
| **ENV** | Abschnitt ENV; noch einmal gedrückt: die nächste Seite | |
| **LFO** | Abschnitt LFO; noch einmal: die nächste Seite | |
| **FX** | Abschnitt CHORUS (der Chorus gehört zum MKS-50-Klang) | |
| **GLO** | Abschnitt PATCH, bei jedem Druck die nächste Seite, danach die Seite MIDI, danach wieder PATCH | |
| **SEL** | im Editor: nächste Seite des Abschnitts | |
| **PLAY** | die Lastanzeige (Mittel und Spitze je Kern, Stimmen, Chorus, späte Blöcke) ein und aus | |
| **SAVE** | speichern | speichern unter (Platz, Name, Kategorie) |
| **OCT− + OCT+** | alle Töne aus (Panik) | |
| ARP, SEQ, REC | ohne Funktion in dieser Firmware; Platz für spätere | |

Die Taste des aktuellen Abschnitts leuchtet. Eine Taste, die das Gewünschte nicht tut, gibt es nicht: Jede Funktion hat genau einen Weg, Abschnitte zusätzlich den Weg über ALGORITHM (links zurück, rechts weiter).

### Seiten des Editors

Vier Werte je Seite (KNOB 1–4); die Namen sind die der MKS-50-Tabelle, wie das Display des Originals sie kürzt. Parameter, die die Engine noch nicht auswertet (Aftertouch, Bender), erscheinen erst, wenn sie klingen (`docs/KLANG.md`).

| Abschnitt | Seite 1 | Seite 2 | Seite 3 |
|---|---|---|---|
| **DCO** | PULSE, SAW, SUB, RANGE | SUB LEVEL, NOISE, PW, PWM RATE | DCO LFO, DCO ENV, DCO ENV MODE |
| **VCF** | CUTOFF, RESONANCE, KEY FOLLOW, ENV | LFO, ENV MODE, HPF | |
| **VCA** | LEVEL, MODE, VOLUME | | |
| **ENV** | T1, L1, T2, L2 | T3, L3 (Sustain), T4, KEY FOLLOW | |
| **LFO** | RATE, DELAY, WAVE, SYNC | DIVISION, KEY SYNC | |
| **CHORUS** | MODE (OFF, FREE, I, II, I+II), RATE | | |
| **PATCH** | KEY MODE, GLIDE, TIME, TRANSPOSE | RETRIGGER | ZUSATZ: DETUNE, SYNC, SYNC ENV, DRIFT; UNISON DETUNE |

Auf der ENV-Seite zeichnet das Display die Hüllkurve (T1–T4, L1–L3) wie das Diagramm auf dem Original.

Die Seite **MIDI** (GLO ein zweites Mal): KNOB 1–4 setzen den Kanal von Part A–D (OMNI, 1–16). Part A hat Kanal 1, B 2, C 3, D 4.

### Spielansicht (HOME)

Vier Zeilen A–D: Part-Buchstabe, Kategorie-Farbe, Soundname (ein `*` hinter dem Namen, wenn geändert), MIDI-Kanal, klingende Stimmen. Der gewählte Part ist hervorgehoben. KNOB 1–4 regeln die Lautstärke von Part A–D. Darunter die Stimmenzahl im Pool (6).

### Sound-Wahl, Kategorien

- Kategorien der 56 Werks-Sounds (`engine/mks_cats.c`, erzeugt aus den Kategorien der iOS-App): BASS, SEQUENCE, ZAPS, STRINGS, STAB, KEYS, PAD, FX+PERC; dazu **ROLAND** (die Werks-Tones des MKS-50, sobald importiert, ADR 0006; leer, wird übersprungen) und **EIGENE**. LEAD und RISERS der iOS-App haben hier noch keine Sounds.
- ALGORITHM springt in die nächste Kategorie (an ihren ersten Sound), PRESETS geht durch die Sounds darin und am Ende in die nächste Kategorie.
- Das Display zeigt Kategorie, Nummer und Name des Sounds; die Zeile in der Farbe der Kategorie.

### Speichern

- **SAVE kurz:** speichert den Sound auf seinen Platz. Ein Werks-Sound ist unveränderlich: Er wird als Kopie auf den nächsten freien Platz in EIGENE gelegt, das Display meldet „GESPEICHERT: EIGENE 03“.
- **SAVE lang:** Speichern unter. SELECT bewegt den Cursor im Namen, PRESETS ändert den Buchstaben, ALGORITHM die Kategorie, SAVE bestätigt, HOME bricht ab. Der Name beginnt mit dem des Ausgangs-Sounds, die Kategorie ist die des Ausgangs-Sounds.
- Ein Sound wird je Part mit dem Part gespeichert: Beim Einschalten sind alle vier Parts wie beim Ausschalten (Sound, Kanal, Lautstärke).
- Gespeichert wird nur im Stillstand oder mit vorab gelöschtem Sektor (ADR 0006); Spielen wird nicht unterbrochen.

### Farben und Beschriftung

Auf Wunsch des Nutzers (10.11.2026): **reduziert und monoton, im Retro-Stil, mit einzelnen Highlights.**

- **Eine Farbe:** Bernstein auf Schwarz wie die alten LCDs und LEDs. Drei Stufen davon: hell (Text, Werte, Balken), mittel (Bezeichnungen), dunkel (Rahmen, Hinweise, alles Unwichtige).
- **Das Gewählte steht invertiert** (schwarz auf Bernstein): die Titelzeile, der gewählte Part, die gerade gedrehte Bezeichnung, der aktive Abschnitt in der Leiste, die Cursor-Stelle im Namen.
- **Rot nur für das, was Aufmerksamkeit braucht:** das Quadrat „Sound geändert, nicht gespeichert“, Meldungen wie „SPEICHER VOLL“, Last über 88 %.
- Kein Abschnitt hat eine eigene Farbe; die Abschnitte erkennt man an der Leiste DCO VCF VCA ENV LFO CHO PAT MID und am Titel.
- Beschriftung in Großbuchstaben mit den Namen der MKS-50-Tabelle.
- Alle Farben stehen in `app/mk_theme.h`. Die Farben der Frontplatte des Originals ließen sich im Netz nicht belegen; ein Foto davon ändert sie in einem Schritt.

## Alternativen

- **Alle Parameter auf eine lange Liste mit einem Regler:** wenige Tasten, aber Suchen statt Greifen. Die Abschnitte entsprechen den Feldern der MKS-50 und dem PG-300-Programmer.
- **Obertasten OP1–OP6, PIT, GLO, MONO, POLY als Funktionstasten:** Ihre Beschriftung passt teils (MONO/POLY = Key-Modus), aber sie sind Notentasten; wer spielt, würde sie unabsichtlich auslösen.
- **Part-Wahl über Tasten statt SELECT:** SELECT ist als Regler beschriftet und links/rechts folgerichtig.

## Folgen

- Die Spielansicht, der Editor mit sieben Abschnitten und das Speichern brauchen je ein Flash-Format (Sounds, Parts, Einstellungen) und die Presets ein Kategorie-Feld.
- Die Parameter-Seiten sind Daten (Tabelle), kein Code je Seite: Eine Änderung ist eine Zeile.
- Die Bedienung läuft in der Hauptschleife; nichts in ihr wartet auf den Audio-Interrupt.

## Verlauf

- 2026-11-10: angelegt nach dem Wunsch des Nutzers
- 2026-11-10: auf Wunsch des Nutzers gebaut und im Emulator geprüft (Spielansicht, Editor mit allen Seiten, Speichern kurz und unter, MIDI-Kanäle, Lastanzeige, Neustart mit gespeicherten Sounds); Farben monoton in Bernstein mit roten Highlights statt der Abschnittsfarben
- 2026-11-12: abgelöst durch ADR 0008
