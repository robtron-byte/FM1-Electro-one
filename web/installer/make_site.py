#!/usr/bin/env python3
"""Baut die Seite des Web-Installers (GitHub Pages, ADR 0054).

  python3 web/installer/make_site.py AUSGABE_ORDNER

Liest jede .fwsc in firmware/, prüft sie wie platform/tools/fm1_install.py (Kennung, Felucca-Lader,
Prüfsummen) und schreibt:
  index.html, app.js, fm1ota.js, fm1pkg.js   die Seite
  liste.json                                  Name, Version, Kennung, Beschreibung, Stimmen, Datei
  firmware/<Datei>.fwsc                       die Pakete
liste.json und die Pakete sind nur der Rückfall: die Seite liest firmware/ und beschreibungen.json live aus dem
Repository (Zweig main), damit immer die neuesten Pakete erscheinen.
Pakete ohne Felucca-Lader (z. B. FM1_ELECTRO.fwsc aus dem alten SDK-Build) oder mit Fehler fehlen in der Liste.
"""
import hashlib
import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / "platform" / "tools"))
import fm1_install  # noqa: E402

NAME_RE = re.compile(r"^(?P<name>.+?)-(?P<ver>\d+\.\d+\.\d+[^/]*)\.fwsc$")


def vkey(v):
    return [int(x) for x in re.findall(r"\d+", v)]


def liste(fw_dir):
    meta = json.loads((HERE / "beschreibungen.json").read_text(encoding="utf-8"))
    order = [k for k in meta if not k.startswith("_")]
    out = []
    for f in sorted(fw_dir.glob("*.fwsc")):
        m = NAME_RE.match(f.name)
        if not m:
            print(f"  übersprungen (kein NAME-VERSION.fwsc): {f.name}")
            continue
        try:
            product, image = fm1_install.load_package(str(f), False)
        except fm1_install.InstallError as e:
            print(f"  übersprungen: {e}")
            continue
        d = meta.get(m["name"], {})
        raw = f.read_bytes()
        out.append({
            "name": m["name"], "titel": d.get("titel", m["name"]), "version": m["ver"], "kennung": product,
            "de": d.get("de", ""), "en": d.get("en", ""),
            "stimmen_de": d.get("stimmen_de", ""), "stimmen_en": d.get("stimmen_en", ""),
            "datei": "firmware/" + f.name, "bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest(),
        })
    rank = {n: i for i, n in enumerate(order)}
    out.sort(key=lambda e: (rank.get(e["name"], len(rank)), e["name"], [-x for x in vkey(e["version"])]))
    return out


def main(dest):
    dest = Path(dest)
    (dest / "firmware").mkdir(parents=True, exist_ok=True)
    for f in ("index.html", "app.js", "fm1ota.js", "fm1pkg.js", "beschreibungen.json"):
        shutil.copy(HERE / f, dest / f)
    # Browser und GitHub Pages halten Dateien bis zu 10 Minuten im Cache: jede Fassung der Seite bekommt eine eigene
    # Kennung in den Aufrufen (?v=…), damit nach einem Deploy sofort die neuen Skripte und Bilder laden
    v = hashlib.sha256(b"".join((HERE / f).read_bytes() for f in ("index.html", "app.js", "fm1ota.js", "fm1pkg.js"))
                       + b"".join(f.read_bytes() for f in sorted((HERE / "bilder").glob("*.svg")))
                       + b"".join(f.read_bytes() for f in sorted((HERE / "bildschirme").glob("*.png")))).hexdigest()[:10]
    html = (dest / "index.html").read_text(encoding="utf-8").replace('src="app.js"', f'src="app.js?v={v}"')
    (dest / "index.html").write_text(html, encoding="utf-8")
    js = (dest / "app.js").read_text(encoding="utf-8")
    for mod in ("./fm1ota.js", "./fm1pkg.js"):
        js = js.replace(f'from "{mod}"', f'from "{mod}?v={v}"')
    js = js.replace("const BILD_V = \"\";", f"const BILD_V = \"?v={v}\";")
    (dest / "app.js").write_text(js, encoding="utf-8")
    (dest / "bilder").mkdir(exist_ok=True)
    for f in (HERE / "bilder").glob("*.svg"):
        shutil.copy(f, dest / "bilder" / f.name)
    (dest / "bildschirme").mkdir(exist_ok=True)
    for f in (HERE / "bildschirme").glob("*.png"):
        shutil.copy(f, dest / "bildschirme" / f.name)
    items = liste(ROOT / "firmware")
    for e in items:
        shutil.copy(ROOT / e["datei"], dest / e["datei"])
    (dest / "liste.json").write_text(json.dumps(items, ensure_ascii=False, indent=1), encoding="utf-8")
    (dest / ".nojekyll").write_text("")
    for e in items:
        print(f"  {e['titel']:12s} {e['version']:14s} {e['kennung']}  {e['datei']}")
    print(f"Seite: {dest} ({len(items)} Pakete)")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
