# 0045: MKS Ultra solo: volles Filter, Stimmen auf Kern 1, Master auf Kern 0 im Fließband

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28 (0.1.0-alpha gebaut, `check.sh` ok; am Gerät nicht gemessen; A/B-Frage an den Nutzer offen)

## Kontext

Der Baustein `bausteine/mksultra` (Alpha Juno / MKS-50 „Ultra“, Rust-Nachbau) rechnet je Stimme einen DCO bei 2× Fs
mit BLEP und ein VCF bei 4× Fs mit zwei nichtlinearen SVFs, dazu eine Master-Kette mit Chorus, Delay (2 × 4 s), Hall
(1 s Vorverzögerung), EQ. Wünsche: Ron-Presets vorn (1–6), mindestens 6 Stimmen, am Filter nicht sparen („erst an den
Stimmen“), Ziel < 70 % je Kern im Emulator. Entscheidung des Koordinators (28.11.): A = volles Filter, so viele Stimmen
wie sicher passen; B (VCF 2×) als Schalter zum Anhören; A gegen B fragt man den Nutzer.

## Messungen (fm1-emu, Block 64 = 1,45 ms, Zeitgeber wie Seite SYSTEM)

Bitgleiche Optimierungen im Baustein (Golden- und Ron-Prüfsummen unverändert): Inline-`fabsf`/`fminf`/`fmaxf`/`floorf`,
`f2l` ohne 64-Bit-Wandlung, Hall-Indizes in 32 Bit, Hüllkurve zählt zwischen ihren Steuerraten-Schritten ganzzahlig,
Halbband-Ketten ausgerollt, Stimmen und Master in zwei Durchgängen (`mksultra_voices`, `mksultra_master`). Dazu gleich
klingend `MKSULTRA_FM1` (LFO-Phase Festkomma statt double):

| | vorher | nachher |
|---|---|---|
| Master-Kette ohne Note (Stringer: Chorus, Hall) | 295 % | 59 % |
| je Stimme (Stringer) | 75 % | 36–43 % |
| je Stimme mit Unisono (Micro Ticks) | – | ~98 % |

Firmware (Kern 1 Stimmen von Block N, Kern 0 Master von Block N−1):

| Fassung | Stringer, 2 Tasten | Micro Ticks (Unisono), 2 Tasten | Lead 041 |
|---|---|---|---|
| **A**: 1 Stimme, VCF 4× | Kern 1 42 %, Kern 0 59 % | Kern 1 Spitze 98 % | Kern 1 47 %, Kern 0 19 % |
| **B**: 2 Stimmen, VCF 2× (`MKSULTRA_VCF2X`) | Kern 1 62 %, Kern 0 59 % | Kern 1 Spitze 79 % | – |
| A mit 2 Stimmen (verworfen) | Kern 1 80 % | – | – |

6 Stimmen bräuchten mit A ~300 %, mit B ~220 % bei 2 × 70 % = 140 %. **Keine Fassung schafft 6 Stimmen.**

Ron-Presets gegen den Baustein (Akkord 48 55 60 64, `make` Ziel wie `test_fm1`, mit den Schaltern der Firmware):

| Preset | A (1 Stimme) | B (2 Stimmen, VCF 2×) |
|---|---|---|
| 041 Lead, Short Hit, Micro Ticks (einstimmig) | Pegel ±0,1 %, Differenz −76 dB (Micro Ticks −8 dB: Delay) | Pegel ±0,6 %, Differenz −24 … −28 dB (Micro Ticks −5 dB) |
| Bass Hit 01 | Pegel −8,1 dB (fehlende Akkordtöne) | −4,5 dB |
| R2 Random | −6,0 dB | −3,0 dB |
| Stringer | −5,7 dB | −2,8 dB |

Die Pegelprüfung der Ron-Presets (RMS 1 %) besteht mit 6 Stimmen (`make -C bausteine mksultra`, `test_fm1`), mit 1 oder 2
Stimmen nicht: die Akkorde verlieren Töne.

## Regel des Nutzers: fest 4 Stimmen, Delay 0,6 s (28.11.)

Gebaut mit `MKSULTRA_DELAY_EXTERN` (Delay 0,6 s, 16 Bit: linker Kanal im Pool, rechter im freien RAM) und 4 Stimmen
(`tools/build.py`; `MKU_VCF2X=1` für VCF 2×). Gemessen (Stringer, 4 Tasten):

| Fassung | Kern 1 (Stimmen) | Kern 0 (Master) | verspätete Blöcke in 4 s |
|---|---|---|---|
| VCF 4×, 4 Stimmen | 153 % | 60 % | 1399 (Aussetzer) |
| VCF 2×, 4 Stimmen | 114 % | 60 % | 1368 (Aussetzer) |

Auch mit den Stimmen auf beide Kerne verteilt bliebe es bei ~106 % (4×) bzw. ~87 % (2×) je Kern. **4 Stimmen passen
nicht unter 70 %**; deshalb kein neues Paket in `firmware/` (dort bleibt 0.1.0-alpha mit 1 Stimme). Frage an den Nutzer.

## Begrenzter Versuch für 4 Stimmen (Auftrag 28.11.: 2 + 2 Stimmen, Gerüst-Effekte, VCF 2×)

Einzelmessungen (Stringer, eine Note, Emulator, Anteil eines Blocks) und daraus die Last je Kern bei 2 + 2 Stimmen
(Kern 1 zwei Stimmen, Kern 0 zwei Stimmen plus Master-Kette und Steuerung ~2 %). Die Aufteilung der Stimmen selbst ist
nicht gebaut: Sie ändert die Summe nicht, und schon die Summe passt nicht.

| Baustein der Last | gemessen |
|---|---|
| Stimme VCF 4× | 44 % |
| Stimme VCF 2× | 28,5 % (4 Stimmen 114 %) |
| Original-Master mit Hall und Chorus | 59–60 % |
| Original-Master ohne Hall (Chorus, Filter, DC) | 13,4 % |
| Original-Master ohne Hall und Chorus | 5,9 % |
| Gerüst-Delay + Hall (`syn_fx`, gemessen in SH-101 solo) | 6,6 % (JX-8P/MKS-50: 10–17 %) |

| Fassung, 4 Stimmen 2 + 2 | Kern 1 | Kern 0 |
|---|---|---|
| VCF 4×, Original-Effekte | 88 % | 150 % |
| VCF 4×, Gerüst-Effekte, Original-Chorus | 88 % | ~110 % |
| VCF 2×, Gerüst-Effekte, Original-Chorus | 57 % | ~79 % |
| VCF 2×, Gerüst-Effekte, ohne Chorus | 57 % | ~72 % |
| VCF 2×, Gerüst-Effekte, ohne Chorus, DCO 1× (geschätzt, DCO 2× ≈ 11 % je Stimme) | ~46 % | ~61 % |

Keine gemessene Kombination bleibt sicher unter 70 % je Kern; nur mit DCO 1× (nicht gebaut, Aliasing des Oszillators)
käme es geschätzt darunter. **0.1.0 bleibt**, Frage an den Nutzer: 1 Stimme volles Klangbild, oder 4 Stimmen mit VCF 2×,
DCO 1×, ohne Original-Chorus und mit den Gerüst-Effekten statt Original-Delay/Hall.

## Entscheidung (vorgeschlagen)

- **Standard A:** volles Filter, **1 Stimme** (`MKSULTRA_VOICES=1`, mono wie bei einem Preset mit einer Stimme),
  `firmware/MKSULTRA-0.1.1-alpha.fwsc`, Kennung FM-1_93. Unisono-Presets liegen auch einstimmig an der Grenze (98 %).
- **B als Schalter:** `MKU_B=1 python3 tools/build.py` baut `build/MKSULTRA-ab-0.1.1-alpha.fwsc` (2 Stimmen, VCF 2×,
  `check.sh` ok), nicht in `firmware/`.
- **Zwei Kerne im Fließband:** Kern 1 rechnet die Stimmen von Block N, Kern 0 die Master-Kette von Block N−1 (bitgleich,
  1,45 ms später). Delay und Hall der Vorlage; das Gerüst-Delay ist aus (`GM_FX 0`, neu im Gerüst).
- **Speicher (Pool 344 kB):** Delay- und Hall-Leitungen 16 Bit (`MKSULTRA_FX_INT16`), Delay 0,5 s statt 4 s
  (`MKSULTRA_DELAY_TENTHS=5`), Vorverzögerung 0,2 s, Arp-Liste 256, Hall genau 69 000 Plätze.
- **Presets:** Ron (1–6), Bank A, Bank B, MKS, emptyvessel = 213 (das Gerüst zählt Werkssounds in einem Byte). Jeder Sound
  trägt sein Preset (verborgene Werte BASE): das Preset lädt genau, nur geänderte Werte kommen aus den Bytes (0..200).
- Stimmenklau des Bausteins (gleiche Note, ausgeklungene, älteste losgelassene, dann älteste gehaltene) bleibt.

## Offen

- **Frage an den Nutzer:** A (1 Stimme, volles Filter) oder B (2 Stimmen, VCF 2×)? Mehr Stimmen nur mit weiteren
  Klangabstrichen (z. B. Unisono begrenzen, Hall abschaltbar, DCO 1×).
- **Delay:** Micro Ticks braucht 0,58 s (links) und 1,17 s (rechts); mit 0,5 s Puffer enden die Echos bei 0,5 s.
  1,2 s in 16 Bit (212 kB) passen nicht neben den Hall. Möglich: Delay-Puffer in den freien RAM (64 kB) auslagern.
- Beim Preset-Wechsel löscht der Baustein Delay und Hall im Audio-Interrupt: ein später Block (Knacks) je Wechsel.
- 307 Presets der übrigen Bänke fehlen (Gerüst: Index in einem Byte).
- Am Gerät: Last, Pegel (`GM_OUT_GAIN`), Bedienung.

## Alternativen

- Alle Stimmen auf beiden Kernen verteilt und Master seriell danach: Master (59 %) und eine Stimme passen nicht
  zusammen auf einen Kern.
- Effekte des Gerüsts statt der Vorlage: Presets klängen anders (Delay/Hall sind Teil der Ron-Presets).

## Verlauf

- 2026-11-28: angelegt (sh101-Chat), Messungen im Emulator.
- 2026-11-28: begrenzter Versuch 2 + 2 mit Gerüst-Effekten und VCF 2×: Kern 0 ~72–79 %, passt nicht; Tabelle oben.
- 2026-11-28: Regel 4 Stimmen + Delay 0,6 s gemessen: passt nicht (VCF 4× 153 %, VCF 2× 114 % auf Kern 1).
- 2026-11-28: 0.1.1-alpha: Seite LOOP (Live-Aufnahme des Gerüsts, ADR 0053), aktueller Gerüststand; Klang unverändert
  (eigene Effekte, nicht vom `fx_in`-Fehler betroffen).
