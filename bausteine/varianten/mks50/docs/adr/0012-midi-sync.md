# 0012: MIDI-Sync: Clock, Transport, Song Position, Clock senden

- **Status:** vorgeschlagen
- **Stand:** 2026-11-14 (gebaut, im Emulator geprüft; am Gerät noch nicht)

## Kontext

ADR 0010 folgte schon einer eingehenden Clock, aber unvollständig: STOP und CONTINUE wurden ignoriert, Song Position kam gar nicht an (die Plattform verwarf F2), das Tempo wurde ungeglättet übernommen, der LFO folgte nur dem Tempo, nicht dem Takt, und das FM-1 konnte selbst keine Clock senden. Beim Bau fiel außerdem ein Fehler auf: In einem Block mit einem Clock-Byte galt die Clock als ausgefallen (Zeitvergleich mit dem Blockanfang), sodass das Clock-Tempo nie übernommen wurde (TEST_03, TEST_04). Der Nutzer wünschte alles (Empfangen und Senden).

## Entscheidung

- **Quelle** (Seite TEMPO, KNOB 2): AUTO (einer einlaufenden Clock folgen, sonst eigenes Tempo), INT (nur eigenes Tempo), MIDI (nur der Clock folgen; ohne Clock steht alles).
- **Transport:** START setzt Arps und synchronisierte LFOs auf Schlag 1; STOP hält die Arps an und beendet ihre Töne (auch bei HOLD); CONTINUE läuft an der Position weiter; **Song Position** (F2) setzt die Position, z. B. vor CONTINUE. Vor dem ersten START läuft alles frei wie bisher.
- **Glättung** wie in der iOS-App: Das Tempo wird je 24 Ticks gemessen und nur bei mehr als 0,3 % Änderung übernommen, auf 0,1 BPM gerundet. So leiern die Delay-Echos bei einer wackligen Clock nicht.
- **LFO-Phase:** Ein Sound mit LFO SYNC (ohne KEY SYNC) folgt der Position: Der LFO-Zyklus beginnt auf dem Takt.
- **LFO-Sync bedienbar** (Seite LFO, KNOB 4 SYNC): Mit SYNC an stellt KNOB 1 RATE den Notenwert (1/32 … 4 BARS) statt der Frequenz, wie TIME beim Delay. Das Bild zeigt die echte LFO-Phase, Notenwert, Tempo und MIDI, wenn eine Clock führt. Gleich in der JX-8P-Firmware.
- **Tempo bleibt:** Endet das Folgen (Quelle auf INT gestellt, oder bei AUTO bleibt die Clock 250 ms aus), läuft die eigene Clock mit dem letzten Clock-Tempo weiter; es wird zum TEMPO-Wert (das Display meldet TEMPO KEPT). Zehntel bleiben im Lauf erhalten, gespeichert wird ganzzahlig.
- **Eigene Clock:** Bei Quelle INT oder AUTO ohne Clock zählt die Firmware selbst 24 Ticks je Viertel im Audio-Interrupt. Arps laufen nach START auf diesem Raster. **PLAY halten** (0,6 s) startet und stoppt; PLAY tippen bleibt Arp-HOLD. Die PLAY-Taste blinkt im Takt, solange der Transport läuft.
- **Clock senden** (KNOB 3 SEND): AUS oder USB. Die TRS-Buchse des FM-1 ist nur ein Eingang. Gesendet werden die eigene Clock und START/STOP; folgt das Gerät einer Clock von der TRS-Buchse, reicht es diese samt Transport und Song Position an USB weiter (eine Clock von USB nie zurück).
- **Plattform:** `usb.c` und `midi_uart.c` lassen Song Position durch (`platform/ORIGIN.md`).
- **Seite TEMPO** zeigt Tempo, Quelle (MIDI CLOCK, INTERNAL, WAITING FOR CLOCK), Transport mit Takt.Schlag und vier Taktlichter. Die Seite MIDI steht direkt dahinter (GLO zweimal).
- Quelle und SEND stehen im Ausschalt-Journal (ADR 0011); Sicherungen aus TEST_04 werden übernommen.

## Prüfung (Emulator, Clock über die TRS-Buchse eingespeist)

| Fall | Ergebnis |
|---|---|
| 100 BPM | 100, MIDI CLOCK |
| schwankend 23/27 ms (Mittel 100 BPM) | ruhig 100 |
| Wechsel 100 → 140 BPM | 140 |
| START, nach 2,2 s STOP | STOP, Position 1.4 |
| STOP, Song Position Takt 2, CONTINUE | weiter ab 2.1 |
| Arp läuft, STOP | still |
| eigene Clock 120 BPM, SEND USB | 48 F8 je Sekunde, FA/FC bei PLAY halten |
| Clock 137 BPM, dann aus (AUTO) / Quelle MIDI → INT | TEMPO 137, INTERNAL, TEMPO KEPT: 137 |
| Clock von TRS, SEND USB | an USB weitergereicht (40 F8/s bei 100 BPM), eigene schweigt |

## Folgen

- Gesendete und empfangene Ticks haben Block-Auflösung (1,45 ms).
- Mit Quelle MIDI und ohne Clock spielen Arps nicht.

## Verlauf

- 2026-11-14: angelegt auf Wunsch des Nutzers; Fehler beim Übernehmen des Clock-Tempos behoben
- 2026-11-14: Wunsch des Nutzers: das letzte Clock-Tempo bleibt beim Wechsel auf intern (TEST_06)
- 2026-11-14: LFO-Sync auf der Seite LFO bedienbar (Wunsch des Nutzers, auch JX-8P); im Emulator mit Clock 100 BPM: Phase steht auf der Songposition (TEST_07)
