#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Holt die eigenen Presets des Nutzers aus den iOS-Apps MKS-50, JX-8P und JP-6 vom iPhone in den Dropbox-Ordner
# "Eigene Presets iPhone/<app>/" (docs/adr/0051). Danach erzeugt es die Preset-Tabellen der Bausteine neu, die dafür ein
# Werkzeug haben (bausteine/<app>/tools/eigene2c.py), damit der Build ohne Dropbox geht. Die Tabellen dann einchecken.
#
#   tools/iphone_presets_pull.sh                 alle drei Apps
#   tools/iphone_presets_pull.sh jx8p            nur eine
#   IPHONE=<UDID oder Name> tools/...            anderes Gerät (Standard: das iPhone "Phonie")
#   ZIEL=<Ordner> tools/...                      anderer Zielordner
#   NUR_HOLEN=1 tools/...                        Tabellen nicht neu erzeugen
#
# Voraussetzung: Xcode (xcrun devicectl), iPhone per Kabel oder WLAN gekoppelt und entsperrt. Vorhandene Dateien im
# Zielordner werden überschrieben, nichts wird gelöscht (auf dem iPhone gelöschte Presets bleiben im Ordner).
set -euo pipefail

IPHONE="${IPHONE:-00008110-001E15DA0130401E}"
ZIEL="${ZIEL:-$HOME/ediundsepp Dropbox/Ron Zöllner/Privat/PP/Programmieren/Musik/MKS50 App/Eigene Presets iPhone}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
APPS=("$@")
[ ${#APPS[@]} -gt 0 ] || APPS=(mks50 jx8p jp6)

bundle() {
    case "$1" in
    mks50) echo de.robtron.mks50 ;;
    jx8p) echo de.robtron.jx8p ;;
    jp6) echo de.robtron.jp6 ;;
    *) echo "unbekannte App: $1 (mks50, jx8p, jp6)" >&2; return 1 ;;
    esac
}

command -v xcrun >/dev/null || { echo "xcrun fehlt (Xcode installieren)" >&2; exit 1; }
[ -d "$ZIEL" ] || { echo "Zielordner fehlt: $ZIEL" >&2; exit 1; }

fehler=0
for app in "${APPS[@]}"; do
    id="$(bundle "$app")"
    tmp="$(mktemp -d)"
    echo "== $app ($id) von $IPHONE"
    if xcrun devicectl device copy from --device "$IPHONE" --domain-type appDataContainer --domain-identifier "$id" \
        --source "Library/Application Support/UserPresets" --destination "$tmp" >/dev/null; then
        mkdir -p "$ZIEL/$app"
        n=0
        while IFS= read -r -d '' f; do cp "$f" "$ZIEL/$app/"; n=$((n + 1)); done < <(find "$tmp" -name '*.json' -print0)
        echo "   $n Presets nach $ZIEL/$app"
    else
        echo "   nicht geholt (App nicht auf dem iPhone, iPhone gesperrt oder nicht verbunden?)" >&2
        fehler=1
    fi
    rm -rf "$tmp"
    gen="$REPO/bausteine/$app/tools/eigene2c.py"
    if [ -z "${NUR_HOLEN:-}" ] && [ -f "$gen" ]; then
        python3 "$gen" "$ZIEL/$app"
    fi
done
[ -n "${NUR_HOLEN:-}" ] || echo "Neue Tabellen prüfen (make -C bausteine test) und einchecken."
exit $fehler
