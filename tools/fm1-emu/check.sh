#!/bin/bash
# Prüft ein FM1-Electro-Paket im Emulator fm1-emu (github.com/joakimeriksson/fm1-emu, Stand
# 0bebe54) auf Fehler, die nur das echte Gerät mit einer CPU-Ausnahme und einem Neustart
# beantwortet (docs/adr/0018). Vorher: fm1-emu klonen, hardware-checks.patch anwenden,
# `cargo build --release -p fm1-emu`.
#   tools/fm1-emu/check.sh <fm1-emu-Verzeichnis> <paket.fwsc> <sdk.elf>
set -e
EMU=${1:?fm1-emu-Verzeichnis}; FW=${2:?Paket}; ELF=${3:?sdk.elf}
NM=${NM:-nm}
sym() { $NM "$ELF" | awk -v s="$1" '$NF==s {print $1; exit}'; }
TCB=$(sym CurrentTCB); WDT=$(sym wdt_clear)
cd "$EMU"
OUT=$(FM1_FPU_WATCH=1 FM1_STK_TCB=0x$TCB FM1_WDT_PC=$WDT FM1_FLASH_UNDER=random FM1_RAM_RANDOM=7 \
  ./target/release/fm1-emu --fwsc "$FW" --ms 4000 \
  --keys '1000:PLAY,1300:n7,1500:k12,1800:enc0:+3,2000:enc2:-5,2300:n5,2600:k14,3000:enc4:+7,3500:n9' 2>&1)
echo "$OUT" | grep -E "^(halt|TRAP|GUARD |DIV0|FPU kind [0-3]|STK isr|WDT|misaligned|GUARDREGS|cpu0)"
fail=0
echo "$OUT" | grep -q "^halt" && { echo "FEHLER: Emulator hält an"; fail=1; }
echo "$OUT" | grep -q "^TRAP:" && { echo "FEHLER: Division durch null bei aktivem Trap"; fail=1; }
echo "$OUT" | grep -q "^GUARD stack" && { echo "FEHLER: Stack außerhalb seines Fensters"; fail=1; }
echo "$OUT" | grep -q "^misaligned accesses: 0 pcs" || { echo "FEHLER: unausgerichtete Zugriffe"; fail=1; }
echo "$OUT" | grep -q "^FPU kind [0-3]" && { echo "FEHLER: NaN/Inf/Division durch null/Wandlung außerhalb"; fail=1; }
echo "$OUT" | grep -qE "^GUARDREGS \[[0-9a-f]*[4-7c-f], " && { echo "FEHLER: Divisions-Trap (EMU_CON Bit 2) noch an"; fail=1; }
[ $fail = 0 ] && echo "hardware checks ok"
exit $fail
