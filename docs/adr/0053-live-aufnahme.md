# 0053: Live-Aufnahme nach RK-008 in jeder Firmware

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28

## Kontext

Der Nutzer will in jeder Firmware spontan kurze Sequenzen live einspielen, nach der Logik des Retrokits RK-008
(Handbuch; Code nicht öffentlich), mit eigenen Entscheidungen: REC einmal = sofort, REC doppelt = 1 Takt Einzählen; die
erste Aufnahme legt die Länge fest, gerundet auf ganze Takte; danach Overdub, REC übernimmt, STOP verwirft den letzten
Durchgang; fest auf 1/16 quantisiert, Notenlänge wie gehalten; Länge nachträglich in ganzen Takten (verlängern:
wiederholen oder leer, kürzen: Anfang oder Ende behalten); Höchstlänge nach Speicher (8–16 Takte), Anzeige groß.

## Entscheidung

- **Ein Modul für alle:** `bausteine/varianten/gemeinsam/shared/seq/syn_live.h` (nur Kopfdatei), eingebunden vom Gerüst
  (`gemeinsam/app/gm_app.c`, `gm_ui.c`: SH-101, MKS Ultra, JP-6 solo) und von den fm1-synths-Apps (JX-8P solo
  `jx8p/app/jx_app.c`, `jx_ui.c`; MKS-50 solo `mks50/app/mk_app.c`, `mk_ui.c`). Die App gibt Parts und Noten-Funktionen
  vor; die Schleife spielt wie eine gespielte Taste (in den fm1-synths-Apps mit dem Akkord des Parts).
- **Daten:** je Part bis zu 16 Takte (256 Sechzehntel), Ereignisse (Beginn, Länge in 1/16, Note, Velocity), Takt aus
  der Clock (24 Ticks je Viertel, intern oder MIDI). Ereignisse je Part: Gerüst 192, fm1-synths-Apps 247 (Blob 4,
  4 KiB). Speicher im eigenen Blob 4 mit Kennung „LIV1“ und Version: alte Stände (ohne Blob 4) laden, die Schleifen sind
  dann leer. Beim Einschalten spielt keine Schleife von selbst (wie der Sequencer).
- **Bedienung** (Raster: kurz = häufig): REC tippen = Aufnahme sofort, zweimal schnell (≤ 0,4 s) = 1 Takt Einzählen,
  REC während der Aufnahme = übernehmen; PLAY tippen während der Aufnahme = STOP (verwirft den laufenden Durchgang). Ein
  fertiger Overdub-Durchgang bleibt. Auf der SEQUENCER-Seite bleibt REC die Schritt-Aufnahme. Seite **LOOP** (Taste SEQ):
  LOOP an/aus, BARS (0 = leer), LONGER (REPEAT/EMPTY), SHORTER (START/END); groß: Takt/Länge, darüber der Zustand.
  Im Gerüst schaltet SEQ auf der LOOP-Seite die Schleife an/aus. Gestoppter Takt: REC startet ihn.
- **Speicher:** Speicher und Laufzeitdaten des Moduls können im Pool liegen (`LV_STORE_ATTR`, `LV_RT_ATTR`); das Gerüst
  tut es (SH-101 0.1.2 hatte nur 76 Byte RAM frei), JX-8P legt den Speicher in den Pool. Freigabe `lv_ready` erst nach dem
  Laden; das Gerüst setzt sie vor dem Audio zurück (`lv_boot`).
- Pakete: JX-8P solo 0.2.7, MKS-50 solo 0.2.7, JP-6 solo 0.1.1. SH-101 und MKS Ultra baut der sh101-Chat neu (Seite
  LOOP eintragen).

## Alternativen

- **Live-Aufnahme in den Step-Sequencer schreiben:** der hat eine Note je Schritt und keine Längen; Akkorde und gehaltene
  Längen gingen verloren.
- **Je App eigener Code:** dreimal dasselbe; das Modul hält es an einer Stelle.

## Folgen

- Host-Test `make -C bausteine test` (Ziel `varianten`): Länge runden, 1/16, Wiedergabe, Overdub, STOP, Einzählen,
  Länge ändern, Höchstlänge. Im Emulator geprüft: aufgenommene Note spielt die Schleife wieder (JP-6, JX-8P, MKS-50).
- FM1-Electro (`engine/`, `ui/`) bekommt es über den Bau-Chat (dort als Schalter nach ADR 0006; die STRINGS-Aufnahme N6
  geht darin auf).
- Am Gerät ungeprüft: Bedienung, Timing über MIDI-Clock.

## Verlauf

- 2026-11-28: angelegt (jx8p-Chat): Modul, Gerüst, JX-8P/MKS-50 0.2.7, JP-6 0.1.1.
