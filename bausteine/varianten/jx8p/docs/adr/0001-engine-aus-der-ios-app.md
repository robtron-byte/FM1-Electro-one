# 0001: Engine aus der iOS-App, Gerüst aus der MKS-50-Firmware

- **Status:** angenommen
- **Stand:** 2026-11-14

## Kontext

Der Nutzer will eine FM-1-Firmware „auf Basis des MKS-50, aber mit der Synth-Engine des JX-8P“. Es gibt zwei
JX-8P-Engines von ihm: Kestrel-8 (macOS, C++, `main` des Repos JX8P) und die JX-8P-iOS-App (Swift, Branch
`cursor/ios-jx8p-grundlage-76ee`). Kestrel-8 rechnet `pow`, `tan` und `exp` pro Sample und Stimme. Die iOS-App ist die
MKS-50-iOS-App mit einer anderen Stimme: Hüllkurve, Ladder, Chorus, LFO, Effekte, Arpeggiator sind gleich, und sie
rechnet die Modulation schon mit Kontrollrate (alle 16 Samples).

## Entscheidung

- **Klanggrundlage ist die iOS-App** (vom Nutzer gewählt, 14.11.2026): `DCO.swift` (JXOscillators), `HardwareCurves.swift`,
  `SynthEngine.swift` (adsr, envelopeSettings, envAmount, renderControlBlock, renderVoice).
- **Gerüst ist die MKS-50-Firmware:** `engine/jx.c` übernimmt Pool, Parts, Note-Logik und die Block-Phasen für zwei
  Kerne aus `../mks50/engine/mks.c` Zeile für Zeile, mit derselben Schnittstelle (`jx_plan` … `jx_mix`). So kann die
  App der MKS-50-Firmware (Bedienung, MIDI, Arp, Chord, Sync, Speichern) mit wenig Änderung übernommen werden.
- **Bausteine aus `../shared/`:** Hüllkurve, Ladder, Biquad, Chorus, Chop-Oszillator, Mathe-Hilfen, Delay und Hall.
- **Parameter:** die Tone-Parameter des JX-8P unter ihrer Nummer #11–#58 (wie `ToneData.tone` der App und die
  SysEx-Nachrichten APR/IPR), dahinter die Patch-Ebene der App.
- **Sounds:** die 64 Werks-Tones und PG8X INIT aus der App, nach Kategorien sortiert wie dort (Repo-ADR 0002).

## Alternativen

- **Kestrel-8 als Grundlage:** eine andere Engine als die iOS-App, ohne Kontrollrate. Verworfen (Nutzer).
- **Eine gemeinsame Engine für MKS und JX mit umschaltbarer Stimme:** später möglich, wenn Pool und App nach
  `shared/` wandern (Repo-ADR 0001, zweite Stufe).

## Folgen

- Pool und Block-Phasen sind jetzt zweimal da (`mks.c`, `jx.c`). Sie wandern in der zweiten Stufe nach `shared/`.
- Die Lasteinheiten in `jx_plan` sind die der MKS-Stimme, bis die Messung (ADR 0002) andere zeigt.

## Verlauf

- 2026-11-14: angelegt; Engine gebaut, `make test` besteht
