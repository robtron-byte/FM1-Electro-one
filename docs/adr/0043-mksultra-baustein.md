# 0043: mksultra – Alpha-Juno-Klang des Rust-Nachbaus „MKS Ultra“ als bitgenauer C-Baustein, Sparen zuerst an den Stimmen

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28

## Kontext

Der Nutzer hat einen Rust-Nachbau des Alpha Juno / MKS-50 („MKS Ultra“, Dropbox
`Programmieren/Musik/MKS50 App/MKS-Ultra-Vorlage/app/`). Er soll als Baustein nach ADR 0040 ins Repository und später eine
Solo-Firmware bekommen. Die Vorlage liefert eine Anleitung zum Portieren und 7 Golden-Renders mit FNV-1a-Prüfsummen.

Die Stimme ist teuer: DCO bei 2× Fs, VCF bei 4× Fs mit zwei nichtlinearen SVFs, dazu Chorus, Delay und Reverb. Dass das
FM-1 12 solche Stimmen schafft, ist nicht zu erwarten (noch nicht gemessen). Der Nutzer will zuerst an den Stimmen
sparen, nicht am Filter (Wunsch 28.11.).

## Entscheidung

- `bausteine/mksultra/` ist ein reiner C99-Port der Rust-Engine (Route B der Anleitung). Er behält Typen, Klammerung,
  Zufallsreihenfolge und Rechenwege bei und wird mit `-ffp-contract=off` gebaut.
- Er trifft alle 7 Golden-Renders bitgenau (`make -C bausteine mksultra`). Dazu wurden alle 520 Presets mit einem
  kurzen Ablauf gegen den Rust-Kern geprüft (`make sweep`): alle bitgleich.
- Die 520 Presets werden zu einer C-Tabelle (`tools/mks2c.py`). Die Zahlen werden als Literal übernommen, damit sie gleich
  gerundet werden.
- Die Sparfassung ist der Bauschalter `MKSULTRA_VOICES` (Standard 12). Die Zufallswerte der weggelassenen Stimmen
  werden trotzdem gezogen. Dadurch bleibt die Sparfassung bitgleich, solange nicht mehr Noten klingen, als sie Stimmen
  hat. Gemessen:
  - Bei 6 und bei 4 Stimmen sind alle 7 Golden-Fälle Sample für Sample gleich.
  - Bei 8 überlappenden Noten werden Stimmen gestohlen (bei 4 Stimmen auch gehaltene Noten). Ab da ist die Differenz
    etwa so laut wie das Signal (−2,9 … +0,8 dB), der Gesamtpegel ändert sich kaum (±0,4 dB).
- Das Filter bleibt in jeder Fassung unverändert.
- Effekt-Puffer liegen im Zustand (kein malloc). Ihre Größe bestimmt `MKSULTRA_MAX_SR` (Standard 48000, rund 2 MB).

## Alternativen

- **Rust-Engine direkt einbinden (Route A):** Sie wäre bitgenau ohne Port. Dafür braucht sie eine Rust-Toolchain für den
  JieLi-Prozessor, `Vec` auf dem Heap und `std`-Mathematik. Für das FM-1 ist das nicht machbar.
- **Gleich eine float-/Festkomma-Sparfassung:** Dann gäbe es keine Referenz mehr. ADR 0040 verlangt zuerst die volle
  Genauigkeit.
- **Am Filter sparen** (nur 2× statt 4×, ein SVF): Das widerspricht dem Wunsch des Nutzers und verändert den Klang am
  stärksten.

## Folgen

- Die Solo-Firmware (ADR 0045) hat eine geprüfte Referenz. Im Emulator muss sie einmessen:
  - wie viele Stimmen das FM-1 schafft,
  - wie groß die Puffer sein dürfen (2 MB Delay bei 48 kHz passen eventuell nicht ins SRAM).
- Bitgleichheit gilt für macOS arm64 (Apple-libm). Auf dem Gerät kann ein anderes libm die letzten Bits der
  Koeffizienten ändern; dann gilt die Toleranzprüfung der Anleitung.
- Rechenzeit auf dem PC (Apple Silicon, -O2):
  - normale Stimme ≈ 100 ns je Sample (0,5 % Echtzeit bei 48 kHz),
  - Unisono-Stimme ≈ 330 ns,
  - Grundlast ohne Effekte 0,08 %.

## Verlauf

- 2026-11-28: angelegt (mksultra-Chat): Port fertig, 7/7 Golden bitgenau, Sparfassung gemessen
