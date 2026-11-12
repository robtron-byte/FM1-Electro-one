# 0024: Jeder Pattern-Platz behält seinen Sound

- **Status:** vorgeschlagen
- **Stand:** 2026-11-12

## Kontext

BEAT, RHYTHM, STRINGS und BASS haben je vier Plätze A–D. Bisher hatte jeder Bereich nur einen Sound für alle vier Plätze:

- **BEAT:** Ein Platz merkte sich nur die Nummer seines Kits (`dr_pattern_t.kit`). Startete ein Platz mit einem anderen Kit, lud `apply_kit` das Werks-Kit neu; alle Reglereinstellungen der Drums waren dann weg. Rückmeldung vom Gerät: „die Drum-Einstellungen setzen sich zurück, wenn ich von A zu B gehe“.
- **Synth-Bereiche:** ein Sound pro Bereich (`st_track_t.part`). Wer auf B einen anderen Sound wählte, hatte ihn auch auf A.

Wunsch: „wenn ich die Pattern wechsle, soll in jedem Pattern der Sound gespeichert werden, so dass er gleich ist, wenn ich wieder zurückspringe“.

## Entscheidung

Jeder Platz hat seinen eigenen Sound. Er wird gesetzt, wenn der Platz zu spielen beginnt (am Takt oder sofort). Was man am Sound einstellt, gilt für den Platz, der gerade spielt: man hört, was man ändert.

- **BEAT** (`engine/drums.c`, `dr_machine_t.sound[DR_PATTERNS]`): Kit und alle Regler der Instrumente plus KIT-Pegel, Akzent, Choke und Engine (808/PO-12) als `tr808_sound_t` (`engine/tr808.h`, `tr808_sound_apply`: Werte kopieren, Panorama nur neu rechnen, wo es sich ändert; kein `tr808_init` mehr beim Kitwechsel). Drum-Tonart und FOLLOW (KIT-Parameter 4, 5, ADR 0016) gehören weiter dem ganzen Bereich. **Geändert 2026-11-12 (Wunsch des Nutzers):** BEAT hat ein Kit für alle Plätze. Jede Regleränderung (`DR_CMD_PARAM`) und jeder Kitwechsel (`DR_CMD_KIT`) gilt für A–D zugleich (`dr_share_kit`); ein neuer Groove für den spielenden Platz in einem anderen Kit stellt alle Plätze auf dieses Kit um, einer für einen anderen Platz spielt im vorhandenen Kit. Gespeichert wird weiter je Platz (gleiche Werte, Speicherversion unverändert); ältere Grooves mit verschiedenen Kits übernehmen beim Laden das Kit des spielenden Platzes. Die Synth-Bereiche behalten einen Sound pro Platz.
  - Kit wählen (PRESETS) gibt dem spielenden Platz das Werks-Kit.
  - Ein Generator-Groove mit einem anderen Kit gibt dem Platz dessen Werks-Kit; mit demselben Kit bleiben die Regler. Das gilt jetzt auch sofort für den spielenden Platz (vorher erst nach einem Platzwechsel).
  - Platz kopieren nimmt den Sound mit.
- **RHYTHM, STRINGS, BASS** (`engine/synth_track.c`, `st_track_t.sound[ST_PATTERNS]`: Engine, Sound-Nummer, Werte): `st_enter` lädt den Sound des Platzes, wenn ein anderer geladen ist (Werte verglichen), mit Akkordspeicher wie beim Laden eines Sounds: `st_follow_chord` setzt den Akkord eines Sounds mit Akkordspeicher; ein Akkord, der vom vorigen Sound kam, endet mit einem Sound ohne Akkordspeicher (auch wenn im Editor KEY MODE oder CHORD geändert wird); ein von Hand gewählter Akkord (GLO + Taste 1–9 in STRINGS und RHYTHM) bleibt. `EL_CMD_TRACK_PARAM` und `EL_CMD_TRACK_PRESET` schreiben in den spielenden Platz. Neue Patterns vom Generator behalten den Sound ihres Platzes.
- **Speicher** (`fm1/el_store.c`, Version 21): die Sounds aller Plätze hinten angehängt (Drums 4 × 124 Byte, Synths 3 × 4 × (Werte + 2)). Records der Versionen 19 und 20 sind der vordere Teil davon und werden weiter gelesen: Jeder Platz bekommt den einen gespeicherten Sound; Drum-Plätze mit einem anderen Kit dessen Werks-Kit, so wie sie bisher klangen.

## Alternativen

- **Ein Sound pro Bereich, der beim Platzwechsel nicht mehr zurückgesetzt wird:** behebt das Zurücksetzen der Drums, erfüllt aber nicht „in jedem Pattern der Sound gespeichert“.
- **Sound in `dr_pattern_t`/`st_pattern_t`:** Diese Strukturen gehen auch durch den Generator und die Staging-Kopien (`dr_post_pattern`, `el_load_pattern`); dort würde jeder neue Groove den Sound überschreiben. Deshalb eigene Felder neben den Patterns.

## Folgen

- Ein Platzwechsel kostet im Audio-Interrupt ein paar hundert Byte Kopie (Drums) bzw. einmal `syn_load` (Synth, nur wenn sich der Sound unterscheidet). Last im Emulator (FM1_STRICT=1) unverändert: PEAK 84 %, MAX 89 %.
- RAM: Drums 4 × 114 Byte, Synths 3 × 4 × (Werte + 2) Byte.
- Ein eigener Sound (USER) lädt in den spielenden Platz; andere Plätze behalten ihren.
- Host-Test `host/ui_sim.c` („slots: a sound per slot …“): A → B → A, Neustart, Record der Version 20.

## Verlauf

- 2026-11-06: angelegt nach den Rückmeldungen „in jedem Pattern den Sound speichern“ und „die Drum-Einstellungen setzen sich zurück, wenn ich von A zu B gehe“
- 2026-11-12: BEAT: ein Kit für alle Plätze statt einem pro Platz (B18, Wunsch „die Beat-Kits immer über alle Patterns bearbeiten und speichern, nicht für jedes einzeln“)
- 2026-11-12: Akkordspeicher folgt dem Sound (Y6, Feedback „in RHYTHM wird immer ein Akkord aufgenommen“): vorher blieb der Akkord eines Stab-Sounds an der Spur hängen; Akkordwahl mit GLO + Taste auch in RHYTHM
