# 0004: SDK-Start: Standardspuren aus, Master-Pegel voll

- **Status:** angenommen
- **Stand:** 2026-10-26

## Kontext

Im Emulator zeigten sich zwei Fehler, die erst auffielen, als die Firmware gepackt werden konnte:

1. Das SDK gibt seinen acht Spuren beim Start Standard-Instrumente (u. a. zwei DX7, rund 60 KB Heap). Danach fehlte der Platz für den Stack der App-Task, der Start blieb in `cpu_assert_debug` hängen.
2. Das SDK multipliziert die Ausgabe mit seinem Master-Pegel (`fm1_output_mix`). Den setzte FM1 Opens Bildschirmcode aus dem MASTER-Regler; ohne ihn blieb er 0, das Gerät wäre stumm gewesen.

## Entscheidung

- `groove_screen_init` (in `el_sdk_app.c`) schaltet alle acht SDK-Spuren ab (`groove_track_role_set(t, GROOVE_TRACK_OFF)`). Den Zwischenspeicher, den das braucht, leiht der noch unbenutzte Speicher der Engine (`el_fm1_boot_scratch`), weil der Heap in dem Moment voll ist.
- `groove_master_update(1023)` stellt den SDK-Pegel fest auf voll. MASTER regelt FM1-Electro selbst (`EL_CMD_MASTER`).

## Alternativen

- Heap vergrößern: kostet RAM, das die Engine braucht.
- Die Standardspuren im SDK-Image patchen: verändert das SDK.
- MASTER über das SDK statt die Engine: dann zwei Wege für dieselbe Funktion, Simulator und Gerät verhielten sich anders.

## Folgen

- Im Emulator startet die Firmware, Display und Tasten laufen, Audio kommt in den DMA-Puffern an. Auf dem Gerät noch nicht geprüft.

## Verlauf

- 2026-10-26: angelegt
