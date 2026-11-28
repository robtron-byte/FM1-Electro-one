# 0040: Bausteine – Klangerzeuger als austauschbare Teile (`bausteine/`)

- **Status:** angenommen (Wunsch des Nutzers 28.11.)
- **Stand:** 2026-11-28

## Kontext

Der Nutzer hat in seiner Dropbox Nachbauten, die nach seinem Gehör näher am Original sind als unsere Engines:
DrumKern (808, Swift), MonoBass (SH-101, Swift) und MKS Ultra (Alpha Juno, Rust). Er will
herausfinden, welche Kombination auf dem FM-1 am besten passt. Mögliche Kombinationen sind 808 + 101,
808 + 101 + JX-8P oder der Alpha Juno allein. Jeder Klangerzeuger soll auch allein auf einem FM-1 laufen können, mit vollem Klang.

Wünsche vom 28.11., wörtlich zusammengefasst:
- Ordner „Bausteine“.
- Zuerst die Nachbildung des Klangs, so gebaut, dass die Basis auch auf stärkeren Geräten taugt.
- Alle Teile aufnehmen, auch die, die nicht in seine Firmware kommen (z. B. Toms, Congas). Sie sollen sich aber entfernen lassen.
- Beim MKS Ultra zuerst an den Stimmen sparen, nicht am Filter.
- Mehrere Chats sollen parallel arbeiten können. Dafür muss dokumentiert sein, wer woran arbeitet.

## Entscheidung

- **Ordner `bausteine/`**, ein Unterordner je Klangerzeuger (`dk808/`, `sh101/`, `mksultra/`), Aufbau wie in fm1-synths:
  ein Ordner pro Synth. Jeder Baustein besteht aus:
  - `README.md`;
  - dem Kern `<name>.h/.c` in reinem C ohne Bezug auf `engine/`, `ui/` oder die Plattform;
  - `test/` mit Host-Test und WAVs zum Anhören;
  - `ref/` mit den Referenzdaten der Vorlage.
- **Volle Genauigkeit zuerst:** Der Kern rechnet wie die Vorlage (DrumKern in `double`). Er ist die Referenz für spätere
  Sparfassungen (float, Festkomma, weniger Stimmen). Die kommen als Schalter dazu und werden gegen die Referenz gemessen.
- **Abschaltbare Teile** über Präprozessor-Masken, z. B. `DK808_INSTRUMENTS`. Standard ist alles. Weggelassene Teile werden
  nicht übersetzt.
- **Getrennt vom Gerätebuild:**
  - Eigenes `bausteine/Makefile` (`make -C bausteine test`).
  - `make test`, `app.ld` und `tools/build_platform.py` bleiben unberührt.
  - Ins Gerät kommt ein Baustein erst über einen Adapter, als Schalter nach ADR 0006, mit eigenem ADR (reserviert: 0041 dk808, 0042 sh101, 0043 mksultra).
- **Parallele Arbeit:**
  - `bausteine/ARBEITSTAFEL.md` zeigt je Baustein, wer daran arbeitet, den Stand und den nächsten Schritt.
  - Dort sind auch die ADR- und FEATURES-Nummern reserviert.
  - Ein Chat trägt sich ein und pusht, bevor er anfängt.
- **Varianten:** `bausteine/varianten/` beschreibt die geplanten Firmwares als Liste von Bausteinen und Schaltern.

## Alternativen

- Die Nachbauten direkt in `engine/` einbauen: Das hätte den Gerätebuild bei jedem Schritt berührt und paralleles Arbeiten
  erschwert. Ein Kern in voller Genauigkeit passt außerdem nicht ins Audio-SRAM.
- Ein eigenes Repository je Baustein: Das wäre mehr Verwaltung. Die Adapter brauchen FM1-Electro ohnehin.
- Sofort in float oder Festkomma portieren: Dann gäbe es keine Referenz, gegen die man die Sparfassung prüfen kann.

## Folgen

- Jeder Baustein lässt sich auf dem PC gegen die Vorlage prüfen, ohne das Gerät.
- Für das FM-1 braucht jeder Baustein noch eine Sparfassung und einen Adapter. Die Last entscheidet das Gerät, nicht der
  Emulator.
- Die vorhandenen Engines (JX-8P, MKS-50, JP-6, bisherige 808) bleiben vorerst in `engine/`. Herauslösen nur bitgleich und
  in Absprache mit dem Bau-Chat.

## Verlauf

- 2026-11-28: angelegt, Gerüst und Arbeitstafel; dk808 als erster Baustein in Arbeit
