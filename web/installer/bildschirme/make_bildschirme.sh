#!/bin/bash
# Display-Bilder der Firmware-Liste (bildschirme/<NAME>.png, 240 × 240) aus dem Emulator fm1-emu
# (github.com/joakimeriksson/fm1-emu, wie tools/fm1-emu/check.sh). Ein Lauf je Paket, gut eine Minute.
#   web/installer/bildschirme/make_bildschirme.sh ~/fm1-emu firmware/SH101-0.1.4-alpha.fwsc [MS]
# Ergebnis: bildschirme/SH101.png (Name vor der Version).
# Eigene Bedienung vor dem Bild (z. B. Seite STEPS mit einem Pattern): Datei
#   bausteine/varianten/<name klein>/BILDSCHIRM   mit   KEYS='…' (fm1-emu --keys) und MS=… (Zeitpunkt des Bildes)
# Ohne diese Datei: Startbildschirm nach 900 ms. Ein drittes Argument MS überschreibt.
set -e
EMU=${1:?fm1-emu-Verzeichnis}; FW=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd)
NAME=$(basename "$FW" .fwsc | sed -E 's/-[0-9]+\.[0-9]+\.[0-9]+.*$//')
LOWER=$(echo "$NAME" | tr 'A-Z' 'a-z')
KEYS=""; MS=900
[ -f "$ROOT/bausteine/varianten/$LOWER/BILDSCHIRM" ] && . "$ROOT/bausteine/varianten/$LOWER/BILDSCHIRM"
MS=${3:-$MS}
TMP=$(mktemp -d); mkdir "$TMP/work"
(cd "$TMP" && "$EMU/target/release/fm1-emu" --fwsc "$FW" --ms $((MS + 100)) --quiet ${KEYS:+--keys "$KEYS"} --snap "$MS" > /dev/null)
sips -s format png "$TMP/work/lcd_$MS.bmp" --out "$HERE/$NAME.png" > /dev/null
rm -rf "$TMP"
echo "$HERE/$NAME.png"
