# 0030: Die Verzerrung kommt phasengleich zum Original zurück

- **Status:** vorgeschlagen
- **Stand:** 2026-11-07

## Kontext

Rückmeldung vom Nutzer: Mit aufgedrehter Verzerrung gibt es Phasenauslöschungen, wenn sie kaum verzerrt, und bei TUBE klingt es genauso.

Die Verzerrung ist ein Send (Parallel-Verzerrung): das Original bleibt im Mix, die verzerrte Kopie wird dazu gemischt. `dist_process` verzögert aber das Signal um **24 Samples** (0,54 ms; die Oversampling-Filter, `DIST_LATENCY`), auch wenn es umgangen ist. Bei wenig Drive und bei TUBE (weiche Kennlinie) ist die verzerrte Kopie fast nur eine verzögerte, gefilterte Kopie des Originals. Beides zusammen ist ein Kammfilter: Auslöschungen bei 920 Hz, 2,8 kHz, 4,6 kHz … (bei 24 Samples liegt die erste Kerbe bei 1 / (2 × 0,54 ms)).

Gemessen (`host/dist_comb_check.c`, `tools/dist_comb.py`; STRINGS-Akkord, Master aus, Send 60 und 127): vorher Kerben von −6,0 dB (TUBE, Drive 64), −4,7 dB (TUBE, Drive 0), −4,2 dB (RAT), −3,4 dB (OD) bei 845 Hz; ein gehaltenes A#5 (932 Hz) verlor mit dem Send auf 127 bei TUBE ohne Drive 5,8 dB, bei OD 4,5 dB.

Bei Typ OFF kommt nichts zurück (`fx_distortion_process`); das ist bitgleich zum Stand ohne Send (alle vier Sends geprüft) und war nicht die Ursache.

## Entscheidung

Die trockenen Busse laufen genauso spät wie die Verzerrung (`bus_align_dry` in `engine/electro.c`, vor dem Mischen der Verzerrung in `bus_post`): Kick, Drums L und R, Bass (mono) und Synths L und R werden um `DIST_LATENCY` Samples verzögert (`delay_dry`, einfache Schleifen ohne libc, 96 Byte Stapel). Die Verzögerung läuft **immer**, auch bei Send 0 und Typ OFF: eine Latenz, die mit dem Send sprang, würde beim Aufdrehen knacken. Das Original und die Kopie sind danach bis auf die Phase der Filter der Verzerrung (Hochpass, Höhenanhebung, Klang-Tiefpass: bis ±50° oberhalb 800 Hz) deckungsgleich. Mit dem Original zusammen gibt es keine Kerben mehr: ≤ 1,4 dB statt bis 6 dB, und das A#5 behält seinen Pegel.

`DIST_LATENCY` steht jetzt in `engine/dist.h` (vorher privat in `dist.c`). Der Speicher dafür: `el_engine_t.dry_tail`, 6 × 24 Float = 576 Byte.

## Alternativen

- **Die Verzögerung nur bei aktiver Verzerrung:** spart die Schleifen, springt aber beim Einschalten um 24 Samples (Knacken, kurzer Kammfilter beim Übergang).
- **Das Oversampling ohne Verzögerung (IIR statt linearphasiger FIR-Filter):** würde die Verzerrung selbst und ihren Klang ändern (Aliasing, Phase), und gilt für die JP-6-Sounds mit, die dieselben Filter nutzen.
- **Nur das Nichtlineare der Verzerrung addieren (verzerrte minus lineare Kopie):** bräuchte die ausgerichtete lineare Kopie ohnehin, und die Filter der Verzerrung stimmen sonst nicht mit dem Original überein.

## Folgen

- Alles im Mix kommt 0,54 ms später (Kick, Drums, Bass, Synths gemeinsam; zueinander ändert sich nichts). Die Effekt-Rückläufe (Hall, Delay) werden vor der Ausrichtung addiert und laufen mit.
- Rechenzeit: ein Laden und ein Speichern je Sample und Bus (6 Busse). Nicht auf dem Gerät gemessen; der Emulator-Spitzenwert war zuletzt 63 %.
- Die Rückführung der Verzerrung ist weit lauter als das Original (bei 100 % Drive ein Mehrfaches): Der Master-Kompressor und Limiter drücken dann den ganzen Mix. Das ist ein Pegelthema, keine Phase, und ist nicht geändert.
- Die Verzerrung innerhalb der JP-6-Sounds (Dry/Wet intern, `mix` < 127) ist nicht betroffen: sie gleicht ihre Trockenverzögerung schon selbst aus.

## Verlauf

- 2026-11-07: angelegt; `make test` (Test „dist comb“, schlägt mit dem alten Stand fehl: −5,8 dB) und `make mb-fixed-check` laufen.
