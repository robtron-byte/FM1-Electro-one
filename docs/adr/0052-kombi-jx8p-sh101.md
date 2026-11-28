# 0052: Kombi JX-8P + SH-101 auf dem Gerüst mit zwei Klangerzeugern

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28 (0.1.2-alpha gebaut, `check.sh` ok, Emulator-Last gemessen; am Gerät ungeprüft)

## Kontext

Wunsch: eine Firmware, deren Parts den JX-8P (polyphon, Pool 4 Stimmen) oder den SH-101 (mono) spielen, bedient wie
die Kombi MKS-50 + JX-8P aus fm1-synths (Seite PARTS), mit Sequencer, Live-Aufnahme LOOP, Delay ≤ 0,6 s, Hall und
Distortion-Send (TUBE zuerst); der SH-101 behält seinen Drive; eigene Presets vorn; Last < 70 % je Kern, RAM-Reserve
≥ 4 kB. Das Gerüst `varianten/gemeinsam` kannte bisher je Firmware einen Klangerzeuger mit einer Parametertabelle.

## Entscheidung

- **Gerüst (abwärtskompatibel, 67e2c37):** `GM_ENGINES` Klangerzeuger; der Erzeuger eines Sounds steht in einem seiner
  Werte (`GM_ENG_OF`), Tabellen, INIT, Seiten und Ebenen je Erzeuger (`GM_PARAMS_E`, `GM_INIT_E`, `GM_PAGES_E`,
  `GM_LAYERS_E`), Kategorien je Erzeuger (`GM_CAT_ENG`), Regel `GM_ENG_OK`. Neue Felder `K_ENGINE` (Seite PARTS; abgewiesen: Meldung „… IN USE“),
  `K_CHANP` (MIDI-Kanal je Part), `K_EXTRA` (Werte der Variante im Set, `GM_SET_EXTRA`; `GM_EXTRA_IDX` bildet ein Feld auf
  den gewählten Part ab). Ohne diese Schalter bauen alle
  bisherigen Varianten wie vorher (SH-101, MKS Ultra, JP-6 geprüft).
- **Gerüst, Fehler behoben:** Kern 1 las `fx_in`, während Kern 0 es schon für den nächsten Block füllte. Jetzt liest
  Kern 1 den Abzug `fx_src` (vor dem Start von Kern 1 kopiert); `fx_in` behält Namen und Rolle für die Varianten.
- **Variante `varianten/jxsh/`:** 4 Parts. Kern 0 rechnet den SH-101-Part (Baustein sh101, Schalter wie SH-101 solo),
  Kern 1 im Haken `GM_FX_EXTRA` den JX-8P-Pool (`jx.c`, `JX_POOL=4`), Delay, Hall und die Distortion (`gm_dist`, Kurven
  im Flash). Puffer zwischen den Kernen nach Blocknummer abwechselnd; Sends ein Block später.
- **Nur ein SH-101** (`GM_ENG_OK`): der Baustein ist eine Stimme; ein zweiter SH-Part wird auf PARTS abgewiesen.
- **Distortion global** (Modus, Drive, Tone, Level im Set), **Send je Part** (Seite DIST SEND); der Drive des SH-101
  bleibt im Sound.
- **Sounds:** EIGENE (die JX-8P-Presets des Nutzers) zuerst, dann die 11 JX-Werksgruppen, JX INIT, SH BASS/LEAD/SEQ/FX.
- **Speicher:** Hall-Diffusoren im RAMTEXT (`SYN_FX_DIFF_ATTR`), Bildpuffer im NOINIT (`CV_SECTION`): RAM 13,1 kB frei,
  Pool 2,9 kB frei.
- Kennung FM-1_99 (99 war nur die Dinghy-Vorlage), `STORE_MAGIC` „1XSH“, Journal „TXSH“, Paket
  `firmware/JXSH-0.1.4-alpha.fwsc`.
- **Pegel (Gain-Staging, 0.1.2):** gleicher MIX = gleich laut. Gemessen JX 44,9 dB, SH 63,4 dB (Median der klingenden
  Werkssounds, je eine Note); Trim SH −6 dB, JX +12,5 dB (mit Sends). Danach 55,8 gegen 57,4 dB.
- **Sequencer der iOS-Apps (`GM_SEQ_IOS`, Gerüst, Standard aus):** Akzent (127/88, GATE + 15 %), Tie, GATE-Mehrschritt-
  Skala, G.VAR, Zufallsfolgen KRAFT/ROTHER/ROBOT/ACID/MIAMI (NEW, VARY), Bank mit 8 Plätzen (Blob 5), Umschalten am Ende
  der Folge; Seiten SEQUENCER, PATTERN, STEP, BANK. Modul `shared/seq/syn_pat.h` mit Host-Test. Ohne Touch-Raster: Schritte
  über die Seite STEP. RATE bleibt das Raster des Gerüsts.

## Messungen (fm1-emu, Block 64 = 1,45 ms; Messbau `-DJXSH_LAST`)

| Fall | Kern 0 | Kern 1 | verspätet |
|---|---|---|---|
| SH-101 SQUELCH + 4 JX-Stimmen (Chorus voll) + Delay + Hall + Distortion RAT | 60 % (Spitze 61 %) | 36 % | 0 |
| dasselbe ohne Distortion | 60 % | 28 % | 0 |
| nur 4 JX-Stimmen + Effekte | 23 % | 36 % | 0 |
| nur SH-101 + Effekte | 60 % | 8 % | 0 |

## Alternativen

- JX-Pool auf beide Kerne verteilen (wie JX-8P solo): unnötig, Kern 1 hat mit 4 Stimmen genug Luft; der SH-101 braucht
  Kern 0 allein.
- Zwei SH-101-Parts: zweite Instanz kostet ~37 % auf Kern 0 und 3 kB Pool; nicht gewünscht (SH-101 bleibt mono).
- Eigener Kombi-Rahmen wie fm1-synths `combo/`: doppelter Code; das Gerüst braucht mehrere Erzeuger ohnehin für die
  808-Kombis.

## Folgen

- Die 808-Kombis (FM-1_91, FM-1_90) nutzen dieselbe Naht (`GM_ENGINES`).
- Pool fast voll (2,9 kB): weitere Puffer gehören ins RAM.

## Verlauf

- 2026-11-28: angelegt, 0.1.0-alpha gebaut und im Emulator gemessen
- 2026-11-28: 0.1.1-alpha: FX-Ebene des SH-Parts (DIST) gilt dem gewählten Part, Meldung beim abgewiesenen SH-101,
  Lauf mit Tasten durch PARTS/DIST SEND/MIDI, Distortion-Kosten gemessen
- 2026-11-28: 0.1.2-alpha: Gain-Staging JX/SH, Sequencer der iOS-Apps (Abnahme 0.1.1 durch den Koordinator: beides
  gefordert); im Emulator NEW, SEQ, STORE, Platzwechsel am Folgenende geprüft
