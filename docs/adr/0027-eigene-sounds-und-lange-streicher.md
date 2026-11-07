# 0027: Eigene schlichte Sounds, Bässe neben der Bassdrum, längere Streicher

- **Status:** vorgeschlagen
- **Stand:** 2026-11-07

## Kontext

Rückmeldung vom Nutzer:

- Die vorhandenen Sounds bleiben, aber der Bass soll der 808-Bassdrum nicht in die Quere kommen.
- Es fehlen schlichte Sounds, die von sich aus wenig tun und erst in der Sequenz interessant werden, und in STRINGS brauche er Pads.
- STRINGS: Die Pattern-Länge bleibt in ganzen Takten, soll aber länger werden können. Eine Noten-Aufnahme soll auch in Achteln gespielt werden, nicht nur in ganzen Takten.

Messung (`make bass-check`, Bässe bei C2 = MIDI 36): Die Bässe der Apps spielen eine Oktave tiefer (16'), der Grundton liegt bei 32–34 Hz, also unter der Bassdrum. Sechs Werks-Bässe liegen mit dem Grundton bei 64–66 Hz auf dem Körper der 808-Bassdrum (45–60 Hz): ACID LINE, SYNC GROWL, BPF BASS, SUB BASS, WOBBLE, AUTO BASS. Der Mix drückt den Bass schon bei jedem Kick-Anschlag weg (Ducking, `engine/mixbus.c`) und trennt ihn bei 32 Hz.

## Entscheidung

- **Werks-Sounds bleiben unverändert** (sie sind aus den Apps erzeugt). Neu ist `engine/extra_presets.c`, erzeugt von `tools/extra_presets.py`: 23 eigene Sounds hinter denen beider Engines (`syn_extras`, `engine/synth.c`), damit die Nummern gespeicherter Sounds gültig bleiben. Jeder ist ein Werks-Sound mit wenigen geänderten Parametern.
  - **Schlicht:** RHYTHM PLAIN SAW, PLAIN PLS, SHORT BLIP, SOFT KEY, PLAIN LEAD, PLAIN PLK, BLIP; STRINGS PLAIN STR, PLAIN STR6.
  - **Pads (STRINGS):** SOFT PAD, SLOW PAD, AIR PAD, SOFT PAD 6, SLOW PAD 6, AIR PAD 6. Das Einschwingen ist langsam genug für einen Akkord pro Takt, aber nicht so langsam, dass er nicht ankommt.
  - **Bass:** PLAIN BASS, SQR BASS, SAW BASS, PULSE BASS bei 33 Hz unter der Bassdrum; MID BASS, MID PLUCK, MID BASS 6, MID PLUCK6 eine Oktave höher bei 130 Hz, ohne Energie unter 90 Hz.
- **Messung** (`host/preset_probe.c`, `tools/preset_check.py`, `make bass-check`): Anteil der Energie unter 40 Hz, bei 40–90 Hz (Bassdrum), 90–300 Hz und darüber, dazu stärkster Teilton und Pegel. „Auf der Bassdrum“ heißt: mehr als 40 % bei 40–90 Hz.
- **STRINGS länger:** Schrittlängen 4 TAKTE und 8 TAKTE (`ST_RATE_4BARS`, `ST_RATE_8BARS`, 64 und 128 Sechzehntel). Mit 32 Steps ergibt das bis zu 256 Takte. Das Speicherformat bleibt gleich (die Länge wird nur anders begrenzt), ältere Stände laden unverändert. Neuer Generator-Stil LONG (ein Akkord pro vier Takte, 16 Takte).
- **Live-Aufnahme in Achteln:** Wird in STRINGS im LIVE-Modus (nicht SH-101-Step) auf ein leeres Pattern aufgenommen, dessen Schritte gröber als Achtel sind, stellt REC das Pattern auf 1/8 mit 32 Steps (4 Takte) um (`st_live_grid_needed` in `engine/synth_track.h`, `EL_CMD_RECORD_AREA`). Vorher landete jede Note auf dem Taktanfang. Ein Pattern mit Noten behält seine Schrittlänge; die Step-Aufnahme auch.

## Alternativen

- **Werks-Sounds ändern (Hochpass in die Bässe):** verändert die Klänge der Apps; der Nutzer wollte sie behalten.
- **Sidechain oder Hochpass im Mix:** gibt es bereits (Ducking der Bässe bei der Bassdrum). Mehr davon würde alle Bässe verändern.
- **64 Steps pro Pattern:** hätte Achtel-Aufnahmen über 8 Takte erlaubt, kostet aber SRAM (rund 1,2 KB, der Heap ist knapp, ADR 0019/0021) und ändert das Speicherformat. Offen, falls 4 Takte in Achteln nicht reichen; längere Aufnahmen gehen schon jetzt in Vierteln (8 Takte) oder gröber.
- **Aufnahme automatisch fein stellen, auch bei vorhandenen Noten:** würde die vorhandenen Schritte umrechnen müssen und passt oft nicht in 32 Steps.

## Folgen

- Klang der neuen Sounds nur nach Messung entworfen, nicht gehört: Der Nutzer soll sie prüfen und mit dem Gerät nachstellen (Parameter in `tools/extra_presets.py`).
- Die eigenen Sounds stehen am Ende der Werks-Liste jedes Bereichs. Die eigenen Sounds des Nutzers (U01…) stehen davor (ADR 0026).
- Die MKS-50- und die JP-6-Engine kennen keine Sound-Namen über 10 Zeichen.

## Verlauf

- 2026-11-07: angelegt; `make test` (Test „extras“ und „strings“) und `make web` laufen.
- 2026-11-07: Gerätetest S8: Die Live-Aufnahme soll nur den Anfang auf 1/8 rastern, nicht die Länge. Umgesetzt mit N6: Eine Live-Aufnahme in STRINGS auf ein leeres Pattern läuft in Achteln über 4 Takte; jede Note beginnt auf der nächsten Achtel und behält ihre Haltedauer (in 32steln, im Step gespeichert, Speicherversion 26); sie klingt so lange neben den anderen Steps. Beim Ende von REC wird das Pattern auf die genutzten ganzen Takte gekürzt (1–4)
- 2026-11-07: N7 (Entscheidung Nutzer: Viertel-Raster statt mehr Speicher): Läuft eine solche Aufnahme über 4 Takte hinaus, rücken ihre Noten auf Viertel (32 Steps = 8 Takte) und sie geht in Takt 5 weiter; beim Ende von REC auf die genutzten ganzen Takte gekürzt (1–8)
- 2026-11-07: Feedback: STRINGS-Patterns nie ungerade lang. GLO + ALGORITHM verdoppelt/halbiert in STRINGS (1, 2, 4, 8 … Takte, auch im STEP-Modus); eine Live-Aufnahme rundet beim Ende auf die nächste dieser Längen auf
