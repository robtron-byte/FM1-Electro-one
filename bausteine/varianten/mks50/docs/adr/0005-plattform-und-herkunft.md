# 0005: Plattform, Kennung, Herkunft und Lizenz

- **Status:** angenommen
- **Stand:** 2026-11-10

## Kontext

Der Auftrag nennt die Quellen: die Engine und die Plattform aus FM1-Electro, eine eigene Kennung, GPL-3.0-only.

## Entscheidung

- **Plattform** in `platform/` (Wurzel des Repositorys, für alle Synths von fm1-synths): Dinghy `f77bf03` mit den Änderungen von FM1-Electro `e7b3b14`. Herkunft und eigene Änderungen stehen in `platform/ORIGIN.md`.
- **Engine** in `mks50/engine/`: aus FM1-Electro `e7b3b14`. Die unveränderte Fassung liegt als Referenz in `mks50/host/ref/`.
- **Kennung** `FM-1_950`, Name im Update-Lader „MKS-50“, eigener Speicher-Tag (`STORE_MAGIC` „MK50“). FM1-Electro hat `FM-1_960`, Dinghy `FM-1_990`.
- **Lizenz:** GPL-3.0-only. Dateien aus FM1-Electro standen dort unter GPL-3.0-only mit einer zusätzlichen Linking-Erlaubnis; sie werden hier ohne diese Erlaubnis weitergegeben. `dsp_math.h` bleibt MIT. Alles steht in `mks50/LICENSES.md`.
- **Build:** `mks50/tools/build.py` ersetzt nur das Übersetzen der App und nutzt sonst `platform/tools/build.py`. Es bricht ab, wenn der Link scheitert, kein neues Abbild entsteht oder das Log „undefined“ nennt.
- **SDK-Dateien:** `uboot.boot`, `cfg_tool.bin` und `cfg/eq_cfg_hw.bin` liegen nicht im Repository. Ohne Zugang zu gitee.com lassen sie sich aus einem vorhandenen Paket von FM1-Electro gewinnen; die Prüfsummen prüft `platform/tools/build.py`.

## Alternativen

- **Plattform als Unterordner von `mks50/`:** Jeder weitere Synth müsste sie kopieren.

## Folgen

- Fehlerbehebungen an Dinghy oder SLOOP werden bewusst nachgezogen.

## Verlauf

- 2026-11-10: angelegt
