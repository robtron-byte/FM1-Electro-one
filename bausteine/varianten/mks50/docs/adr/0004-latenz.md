# 0004: Kleine Latenz beim Spielen über MIDI

- **Status:** vorgeschlagen
- **Stand:** 2026-11-10

## Kontext

Der Nutzer nennt eine kleine Latenz über MIDI „sehr sehr wichtig“. In FM1-Electro vergehen vom Eintreffen einer Note bis zum Ton etwa 6–12 ms, schwankend um bis zu 5,8 ms:

- Der Audio-Interrupt rechnet 256 Samples (5,8 ms).
- MIDI wird in der Hauptschleife gelesen, zwischen Tasten und Display.
- Der I2S-Puffer hat zwei Hälften: Was gerechnet ist, erklingt eine Hälfte später.

## Entscheidung

- **Block von 64 Samples** (1,45 ms): `MKS_BLOCK` in `engine/mk_config.h` (seit 14.11.2026 `SYN_BLOCK` in `../shared/syn_config.h`), gleich der I2S-Hälfte (`HALF_FRAMES` der Plattform, jetzt einstellbar). `MK_BLOCK=128` baut zum Vergleich mit 128.
- **MIDI wird im Audio-Interrupt gelesen,** am Anfang jedes Blocks (`midi_to_synth` in `app/mk_app.c`). Die Hauptschleife und das Display halten keine Note mehr auf.
- **Kein Versatz im Signalweg:** Stimmen und Chorus eines Blocks werden im selben Block gerechnet. FM1-Electro rechnet Effekte und Master einen Block später.
- **Tasten des Geräts und Bedienung** gehen über eine Warteschlange an den Audio-Interrupt (`ev_put`).

Damit bleiben: bis zu 1,45 ms Warten auf den Blockanfang, 1,45 ms I2S-Hälfte, dazu der Weg durch USB oder die TRS-Buchse (die Plattform fragt beide mit 2 kHz ab). Ziel sind unter 5 ms mit höchstens 1,5 ms Schwankung.

`make pool` misst den Anteil der Engine: Eine Note klingt 3 Samples (0,07 ms) nach dem Blockanfang.

## Alternativen

- **32 Samples:** halbiert die Wartezeit noch einmal, verdoppelt aber die Übergaben an Kern 1 und lässt einer Lastspitze nur 0,7 ms.
- **USB im Audio-Interrupt abfragen:** spart bis zu 0,5 ms. Die Plattform stellt die USB-Abfrage während eines Renderns bewusst zurück; das wird erst nach einem Test am Gerät angefasst.

## Folgen

- Vier Mal so viele Blöcke pro Sekunde: Der feste Aufwand pro Block (Interrupt, Übergabe an Kern 1) zählt vierfach. Die Messfirmware zeigt, ob 64 mit sechs Stimmen stabil läuft; sonst 128.
- Die Zeit zwischen Tastendruck und Ton am Gerät ist noch nicht gemessen.

## Verlauf

- 2026-11-10: angelegt
- 2026-11-14: Dateien nach `../shared/` verschoben (Repo-ADR 0001), Entscheidung unverändert, Klang bitgleich
