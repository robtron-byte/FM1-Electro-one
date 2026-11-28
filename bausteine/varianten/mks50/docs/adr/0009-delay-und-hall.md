# 0009: Delay und Hall aus der iOS-App

- **Status:** vorgeschlagen
- **Stand:** 2026-11-12 (gebaut, am Rechner und im Emulator geprüft; am Gerät noch nicht gehört)

## Kontext

Der Nutzer wünscht Delay und Hall aus seiner MKS-50-iOS-App (`Shared/DSP/Effects.swift`: `StereoDelay`, `Reverb`). Die App hält dafür etwa 3 MB Puffer; das FM-1 hat neben Stimmen, Chorus und Bildschirm rund 330 KB.

## Entscheidung

- **Port Zeile für Zeile** (`engine/mks_fx.c`, `mks_fx.h`; seit 14.11.2026 `../shared/fx/syn_fx.*`, Repo-ADR 0001): das Delay mit Hermite-Lesen, Sättigung und Ton im Feedback, Wow, Flutter und Drift, rechter Kanal 1,2 % länger, Ping-Pong, Feedback bis 115 %; der Hall mit Pre-Delay, frühen Reflexionen (ROOM), vier Diffusoren, acht modulierten Linien mit Householder-Matrix, Typen SPACE, ROOM, PLATE. Die Kurven (Zeit 0,5 ms · 4000^(t/127), Feedback, RT60 0,15 s · 400^(d/127), Pegel (x/127)^1,5) sind die der App.
- **Ein Delay und ein Hall für das ganze Gerät**, gespeist von jedem Part über seine Sends (`MP_DELAY_SEND`, `MP_REVERB_SEND`, schon in den Sounds). Die Effekt-Einstellungen sind global und stehen mit den Parts im Flash.
- **Abweichungen wegen des Speichers:**
  - Delay höchstens 0,70 s (App: 2 s); seine Linien als 16-Bit-Werte (2 × 64 KB).
  - Hall-Linien 4096 Samples (App: 32768): SPACE bei sehr großem SIZE kappt die längsten Linien bei 4032 Samples. Sein Eingang (Pre-Delay, Reflexionen) als 16-Bit-Werte im RAM.
  - Der Bildschirm zeichnet in Streifen von 30 Zeilen statt 124 (`CV_MAX`), das gibt 45 KB frei.
- **Ablauf auf zwei Kernen:** Die Effekte bearbeiten die Sends des vorigen Blocks, und zwar auf Kern 1, während Kern 0 seine Stimmen rechnet. Das Effektsignal kommt dadurch 1,45 ms nach dem trockenen; das trockene Signal und die Latenz einer Note ändern sich nicht. Ist ein Effekt auf Pegel 0 oder 5,8 s lang still, rechnet er nichts.
- Der Verteiler (`mks_plan`, `extra1`) zählt die Effekte wie drei Stimmen: Kern 1 bekommt dann zwei Stimmen weniger.

## Messung (Emulator, strenges Modell; das Gerät liegt etwa 15–20 % höher)

| JX STRINGS, 6 Stimmen, Delay und Hall an | Gesamt | Kern 0 | Kern 1 | Delay + Hall |
|---|---|---|---|---|
| Effekte am Ende von Kern 1 | 73 % | 53 % | 45 % | 23 % |
| Effekte parallel zu den Stimmen (gebaut) | 56 % (Spitze 57 %) | 53 % | 45 % | 23 % |

Ohne Effekte lag ein ähnlicher Klang bei 38 %. Am Gerät zeigt die Seite SYSTEM (GLO) dieselben Zahlen.

## Prüfung

- `make fx`: alle drei Hall-Typen, Ping-Pong, Feedback über 100 %, mit Gleitkomma-Prüfung: keine NaN/Inf, Ausklang vorhanden, bei Pegel 0 Stille. Hörprobe: `build/host/fx_demo.wav`.
- `make identity`: der trockene Klang bleibt bitgleich zu FM1-Electro.
- Emulator: mit Sends klingt 1–1,5 s nach dem Ton noch etwas, ohne Sends nicht.

## Alternativen

- **FM1-Electros Effekte** (Dattorro-Hall mit halber Rate): billiger, aber nicht die gewünschten.
- **Hall mit halber Rate:** wäre eine Klangänderung; käme nur als Schalter mit A/B, falls das Gerät es braucht.

## Folgen

- Kommen am Gerät späte Blöcke vor, ist der nächste Schritt eine bitgleiche Verbilligung des Halls; danach erst ein A/B-Schalter.

## Verlauf

- 2026-11-12: angelegt; gebaut auf Wunsch des Nutzers
- 2026-11-14: Dateien nach `../shared/` verschoben (Repo-ADR 0001), Entscheidung unverändert, Klang bitgleich
