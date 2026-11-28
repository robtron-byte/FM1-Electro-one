#!/bin/sh
# Erzeugt gemeinsam/fx/gm_dist_tab.c (Kurven der Distortion als Flash-Konstanten, GM_DIST_ROM=1).
set -e
D=$(cd "$(dirname "$0")/.." && pwd); R="$D/../../.."
cc -O2 -std=c99 -ffp-contract=off -DFM1_HOST_TEST -I"$R/sdk_include" -I"$D/fx" "$D/tools/gen_dist_tab.c" -lm -o /tmp/gen_dist_tab
/tmp/gen_dist_tab > "$D/fx/gm_dist_tab.c"
