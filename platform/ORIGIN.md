# Herkunft der Plattform

`platform/` ist **Dinghy** von Leo Kuroshita (@kurogedelic), Hügelton Instruments, mit der Plattform von
[Felucca](https://github.com/hugelton/Felucca), wie [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) und OberVave sie
tragen.

- Quelle: https://github.com/zvenson/dinghy, Commit `f77bf03` (2026-11-05)
- Lizenz: GPL-3.0-only (siehe `LICENSE` und `LICENSING.md` in diesem Ordner; die Dateiköpfe nennen Urheber und Lizenz)
- Übernommen für FM1-Electro nach `docs/adr/0021`. Eigene Änderungen an Dateien der Plattform stehen in deren Kopf und in
  der Git-Historie dieses Repositorys.

Für den Bau braucht die Plattform drei Dateien des JieLi-AC79-SDK (`AC79_SDK`). Ohne Zugang zu dessen Repository nimmt
FM1-Electro: `uboot.boot` aus FM1 Open (`sdk/tools/pack`, im SDK-Format kodiert; derselbe Bootlader wie M-VAVEs V16),
`cfg_tool.bin` aus FM1 Open (identisch mit der SDK-Version) und `cfg/eq_cfg_hw.bin` aus M-VAVEs Firmware.
