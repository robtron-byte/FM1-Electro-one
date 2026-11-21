# 0037: Neue Aufnahme, Transpose für alle, BASS nur Einzeltöne, INIT+-Sounds

- **Status:** vorgeschlagen
- **Stand:** 2026-11-20

## Kontext

Wünsche vom 20.11. (FEATURES N26–N29): Der Sequencer in STRINGS ist schwer zu bedienen. Man weiß nicht, wie man ein Pattern löscht oder eine neue Aufnahme beginnt, die das Vorhandene ersetzt. Bisher gab es nur EDIT + REC (löschen) und danach REC. BASS soll einstimmig bleiben und bei der Live-Aufnahme keine Akkorde aufnehmen. Bisher setzte der Akkordspeicher eines Sounds den Akkord der Spur, auch in BASS. TRANSPOSE (GLO + SEQ) wirkte nur auf einen Bereich und fiel beim Ausschalten auf 0 zurück. Außerdem sollen am Anfang der Sound-Liste Init-Sounds stehen, die zum Bereich schon leicht passen.

## Entscheidung

- **Neue Aufnahme (N28):** REC eine Sekunde halten löscht das Pattern des Bereichs, beendet eine laufende Aufnahme und startet eine neue auf dem leeren Pattern. Live wächst sie wie bei N6/N16, im SH-101-Modus beginnt eine neue Sequenz. Ein kurzer Druck auf REC bleibt Overdub, EDIT + REC löscht nur. Im Display steht „NEW TAKE: PATTERN CLEARED, REC ON“. Das gilt in allen Bereichen, auch in BEAT. Code: `ui/el_ui.c` (`rec_at`, `rec_renewed`).
- **BASS nur Einzeltöne (N26):** Die Spur-Akkorde in BASS bleiben 0: `st_follow_chord`, `EL_CMD_TRACK_CHORD` und das Laden setzen sie nicht. `record_step` schreibt in BASS keinen Akkord, und beim Abspielen klingen alte Akkord-Steps als Grundton. Es bleibt bei einer Stimme (Antwort des Nutzers).
- **Transpose (N27):** Im TRANSPOSE-Modus verschiebt eine Taste den Bereich (C4 = 0), EDIT + Taste verschiebt RHYTHM, STRINGS und BASS zusammen. Die Drums bleiben unberührt. Nur ein Kit mit FOLLOW BASS (ADR 0016) folgt wie bisher der BASS-Transposition. GLO + SEQ schaltet nur die Tasten zurück aufs Spielen, der Wert bleibt. Er steht oben rechts (z. B. „KEYS O+0 T+3“) und wird gespeichert: Speicherversion 28 hängt `transpose[3]` an, ein Stand der Version 27 lädt mit 0. Live- und Step-Aufnahmen speichern die Note abzüglich der Transposition, damit sie so wiederkommt, wie sie gespielt wurde.
- **INIT+-Sounds (N29):** In `tools/extra_presets.py` gibt es drei neue Sounds, hinten angehängt, damit die Nummern gespeicherter Sounds bleiben: INIT RHY+, INIT STR+ und INIT BASS+ (alle MKS-50). In PRESETS stehen sie direkt nach den eigenen Sounds (`next_sound`: Rang nach Name „INIT…+“). Die schlichten INIT-Sounds aus Y7 bleiben unverändert. Weil das neue Sounds sind und kein vorhandener Klang geändert wird, gibt es keinen A/B-Schalter.

## Alternativen

- REC ersetzt immer, Overdub mit EDIT + REC: Das hätte die gewohnte Bedienung umgedreht. Der Nutzer hat REC halten gewählt.
- BASS mit drei Stimmen für Akkorde: mehr Rechenlast. Der Nutzer will BASS einstimmig.
- Die vorhandenen INIT-Sounds ändern oder an den Anfang der Werksliste verschieben: Das ändert einen Klang (ADR 0006) und die Nummern gespeicherter Sounds.

## Folgen

- Speicherversion 28. Ältere Stände laden weiter (Test: `el_store_test_as(27)`).
- Die Sounds, mit denen eine Spur beim Generieren oder beim leeren Start beginnt, bleiben gleich, also auch die Demo-WAVs.
- Transpose wirkt bei Overdub auf die Aufnahme: Eine gespielte Note wird mit −Transposition gespeichert.

## Verlauf

- 2026-11-20: angelegt (N26–N29), Wahl des Nutzers: REC halten, BASS einstimmig ohne Akkorde, Transpose bleibt und wird gespeichert
- 2026-11-21: Mit dem JX-8P (ADR 0039, Schalter `EL_SYNTH_JX`) entfallen die INIT+-Sounds (MKS-50-Basis); ohne Schalter bleiben sie
