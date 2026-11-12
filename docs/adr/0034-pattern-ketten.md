# 0034: Pattern-Ketten in HOME (S16)

- **Status:** vorgeschlagen
- **Stand:** 2026-11-12

## Kontext

Wunsch des Nutzers (09.11., genauer 12.11.): In HOME einen Platz (A–D) halten und einen zweiten derselben Bank drücken, dann sollen beide nacheinander laufen. Ist der zweite Platz leer, soll die gehaltene Sequenz dorthin kopiert und gleich in der Kette gespielt werden. Bisher wechselte ein zweiter Druck nur den Platz. Entscheidungen des Nutzers (12.11.): die Kette wird mit SAVE gespeichert; vier Glieder reichen, Wiederholungen erlaubt.

## Entscheidung

- **Bedienung (`ui/el_ui.c`, `home_slot_key`):** Ein Platz allein gedrückt spielt wie bisher (Wechsel am Pattern- bzw. Taktende) und löst eine Kette der Bank auf. Solange ein Platz gehalten ist, hängt jeder weitere Druck in derselben Bank einen Platz an: der gehaltene zuerst, bis zu vier Glieder, Wiederholungen erlaubt (A gehalten, B, B → A>B>B). Den gehaltenen Platz selbst kann man nicht erneut drücken; eine Kette ist aber ein Kreis, also ergibt B gehalten, A, A dasselbe wie A>A>B. Ist der neue Platz leer und der gehaltene nicht, wird der gehaltene zuerst dorthin kopiert (BEAT: Pattern, Kit-Sound, Fill, Bedingungen, Locks über `DR_CMD_COPY`; Synths: Pattern und Sound über `EL_CMD_TRACK_COPY`). Hinweis im Display: „A COPIED TO B, CHAIN A>B“.
- **Ablauf (`engine/drums.c`, `engine/synth_track.c`):** Jede Bank hat ihre eigene Kette (`chain[4]`, `chain_len`, `chain_pos`). Ein Glied spielt einen ganzen Durchlauf seines Patterns (seine eigene Länge), dann folgt das nächste. Mit dem Stopp beginnt die Kette wieder beim ersten Glied. Beim Wiederholen eines Steps (Repeat-Punch-in, `dr_step_at`/`st_step_at`) rückt die Kette nicht weiter (`chain_hold`).
- **Anzeige:** In HOME stehen die Glieder statt des einzelnen Platzes neben dem Bereich (z. B. „ABB“), das spielende hell, die anderen gedimmt. Auf den weißen Tasten leuchten die Glieder gedimmt, das spielende voll.
- **Speicher (`fm1/el_store.c`):** Version 27 hängt je Bereich Länge und Plätze der Kette an. Ein Record der Version 26 ist ihr vorderer Teil und lädt ohne Ketten. Nach dem Laden beginnt jede Kette beim ersten Glied. Der Leerstart (S11) löscht die Ketten.

## Alternativen

- Kette nur im RAM (nach dem Ausschalten weg): einfacher, vom Nutzer abgelehnt.
- Umschalten am Taktende statt nach einem Durchlauf: bei Patterns verschiedener Länge (z. B. STRINGS über 2 Takte) würde ein Glied abgeschnitten.
- Wechsel der Synth-Spuren bleibt ohne Kette am Taktende (wie bisher); nur Ketten rücken am Pattern-Ende weiter.

## Folgen

- Klang unverändert: ohne Kette laufen Drums und Synths wie bisher (Demo-WAVs byte-gleich).
- Ein einfacher Druck auf einen Platz löst die Kette auf; eine neue Kette beginnt mit dem Halten.
- Host-Test `host/ui_sim.c` („chain: …“): Halten + Drücken, Kopie in den leeren Platz, A>B>B mit je einem Durchlauf, LEDs, Neubeginn beim Stopp, Speichern (27) und Laden eines 26er Records, Auflösen mit einem Druck.

## Verlauf

- 2026-11-12: angelegt und umgesetzt (49. Build); Speichern und vier Glieder mit Wiederholung vom Nutzer bestätigt
