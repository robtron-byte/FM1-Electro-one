#!/bin/bash
# Display-Bilder der Firmware-Liste (bildschirme/<NAME>.png, 240 × 240) aus dem Emulator fm1-emu
# (github.com/joakimeriksson/fm1-emu, wie tools/fm1-emu/check.sh). Je Paket ein Lauf, Bild nach dem Start
# (Startseite mit Name, Klang, Reglern und Version). Dauert je Paket gut eine Minute.
#   web/installer/bildschirme/make_bildschirme.sh ~/fm1-emu firmware/SH101-0.1.3-alpha.fwsc [MS]
# MS: Zeitpunkt des Bildes in ms (Standard 900). Ergebnis: bildschirme/SH101.png (Name vor der Version).
set -e
EMU=${1:?fm1-emu-Verzeichnis}; FW=$(cd "$(dirname "$2")" && pwd)/$(basename "$2"); MS=${3:-900}
HERE=$(cd "$(dirname "$0")" && pwd)
NAME=$(basename "$FW" .fwsc | sed -E 's/-[0-9]+\.[0-9]+\.[0-9]+.*$//')
TMP=$(mktemp -d); mkdir "$TMP/work"
(cd "$TMP" && "$EMU/target/release/fm1-emu" --fwsc "$FW" --ms $((MS + 100)) --quiet --snap "$MS" > /dev/null)
sips -s format png "$TMP/work/lcd_$MS.bmp" --out "$HERE/$NAME.png" > /dev/null
rm -rf "$TMP"
echo "$HERE/$NAME.png"
