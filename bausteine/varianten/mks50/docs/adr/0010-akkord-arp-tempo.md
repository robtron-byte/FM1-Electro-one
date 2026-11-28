# 0010: Chord-Modus, Arpeggiator, Tempo und MIDI-Clock

- **Status:** vorgeschlagen
- **Stand:** 2026-11-12 (gebaut, im Emulator geprüft; am Gerät noch nicht)

## Kontext

Der Nutzer braucht einen Chord-Modus und einen Arpeggiator. Vorgeschlagen und angenommen („passt, mach weiter“): Akkorde wie in der iOS-App, ein Arp je Part, Tempo aus der MIDI-Clock oder einer eigenen Einstellung.

## Entscheidung

- **Ein Weg für jede Note** (`play_on`/`play_off` in `app/mk_app.c`, im Audio-Interrupt): Taste oder MIDI → Arp des Parts (wenn an) → Akkord des Parts (Key-Modus CHORD) → Engine. Keine Note wartet auf Bildschirm oder Hauptschleife (ADR 0004).
- **Chord-Modus:** Key-Modus CHORD (`MP_KEY_MODE` 1) mit einem der acht Akkordtypen der iOS-App (`MP_CHORD`: m7, SUS2, MIN, 5+8, SUS4, 7TH, MAJ, m7b5), Teil des Sounds. Eine Taste spielt den Akkord; ihr Loslassen beendet genau die Töne, die sie begonnen hat, auch wenn der Typ inzwischen gewechselt wurde. Bedienung: ein Regler CHORD (OFF oder ein Typ) setzt Key-Modus und Typ zugleich.
- **Arpeggiator je Part** (wie `Arpeggiator.swift` der App): UP, DOWN, UP/DN, RANDOM, ORDER; 1–4 Oktaven; Notenwerte 1/32 bis 1/1 wie beim Delay; GATE 5–100 %; HOLD hält, was zuletzt gegriffen wurde (ein neuer Griff ersetzt es). Im Chord-Modus spielt er die Akkordtöne nacheinander. Der erste Schritt erklingt sofort beim Anschlag. Die Einstellungen gehören zum Part und stehen im Flash.
- **Tempo:** Einstellung TEMPO (40–240 BPM, Seite TEMPO unter GLO). Kommt MIDI-Clock (in den letzten 250 ms), folgt das Tempo ihr; der Arp läuft dann auf den Clock-Ticks (Notenwerte als ganze Tick-Zahlen), START setzt ihn auf den Taktanfang. Tempo-Sync des LFO und des Delays folgen demselben Tempo.

## Bedienung (ADR 0008)

- **ARP tippen:** Seiten ARP (an, Modus, Rate, Oktaven), ARP 2 (Gate, Hold, Tempo), CHORD (Akkord, Key-Modus, mit Tastaturbild).
- **ARP halten:** Live-Ebene ARP, MODE, RATE, CHORD.
- **PLAY tippen:** HOLD des Arps für den gewählten Part ein/aus — beide Hände frei. **PLAY halten:** Transport (ADR 0012).

## Alternativen

- **Akkord lernen wie am MKS-50** (einen Akkord greifen und merken): später möglich; die acht Typen decken die Electro-Sounds ab.
- **Ein Arp für alle Parts:** einfacher, aber zwei Parts könnten nicht unterschiedlich laufen.

## Folgen

- Der Arp hat Block-Auflösung (1,45 ms); bei MIDI-Clock folgt er den Ticks.

## Verlauf

- 2026-11-12: angelegt; gebaut auf Wunsch des Nutzers
- 2026-11-14: MIDI-Sync vervollständigt (ADR 0012); das Clock-Tempo wurde bis dahin wegen eines Zeitvergleichs nie übernommen
- 2026-11-14: RAND spielte immer die tiefste Note: der Zufallsgenerator (Xorshift) begann bei 0 und blieb dort; er startet jetzt beim ersten RAND-Schritt von der Uhr (alle drei Firmwares), 0.2.2-alpha
