#!/usr/bin/env python3
"""Anleitungen der Firmwares als Webseiten (ADR 0054). Wird von make_site.py aufgerufen.

Quelle je Paketname NAME (Dateiname vor der Version):
  bausteine/varianten/<name klein>/ANLEITUNG.md (+ ANLEITUNG.en.md)   Solo- und Kombi-Firmwares
  web/installer/anleitungen/<NAME>.md (+ <NAME>.en.md)                FM1-Electro und alles ohne Variante
Ergebnis: anleitung/<NAME>.html, Deutsch mit umschaltbarem Englisch, druckbar, am Handy lesbar.
Erlaubtes Markdown: # ## ### Überschriften, Absätze, - und 1. Listen, Tabellen mit |, **fett**, `Code`.
"""
import html
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent


def sources(name):
    """-> (de, en) Pfade oder None"""
    for de in (ROOT / "bausteine" / "varianten" / name.lower() / "ANLEITUNG.md", HERE / "anleitungen" / f"{name}.md"):
        if de.exists():
            en = de.with_name(de.stem + ".en.md")
            return de, en if en.exists() else None
    return None


def inline(t):
    t = html.escape(t, quote=False)
    t = re.sub(r"`([^`]+)`", r"<code>\1</code>", t)
    t = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", t)
    return t


def md(text):
    out, para, lst, table = [], [], None, []

    def flush():
        nonlocal para, lst, table
        if para:
            out.append("<p>" + inline(" ".join(para)) + "</p>")
            para = []
        if lst:
            tag, items = lst
            out.append(f"<{tag}>" + "".join(f"<li>{inline(i)}</li>" for i in items) + f"</{tag}>")
            lst = None
        if table:
            rows = [[c.strip() for c in r.strip().strip("|").split("|")] for r in table]
            body = [r for r in rows[1:] if not all(re.fullmatch(r":?-{2,}:?", c) for c in r)]
            h = "".join(f"<th>{inline(c)}</th>" for c in rows[0])
            b = "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>" for r in body)
            out.append(f'<div class="tw"><table><thead><tr>{h}</tr></thead><tbody>{b}</tbody></table></div>')
            table = []

    for line in text.splitlines():
        s = line.rstrip()
        m = re.match(r"^(#{1,3})\s+(.*)$", s)
        if m:
            flush()
            n = len(m[1])
            out.append(f"<h{n}>{inline(m[2])}</h{n}>")
        elif s.lstrip().startswith("|"):
            if para or lst:
                flush()
            table.append(s)
        elif re.match(r"^\s*[-*]\s+", s):
            if para or table:
                flush()
            if not lst or lst[0] != "ul":
                flush()
                lst = ("ul", [])
            lst[1].append(re.sub(r"^\s*[-*]\s+", "", s))
        elif re.match(r"^\s*\d+\.\s+", s):
            if para or table:
                flush()
            if not lst or lst[0] != "ol":
                flush()
                lst = ("ol", [])
            lst[1].append(re.sub(r"^\s*\d+\.\s+", "", s))
        elif not s.strip():
            flush()
        elif lst and line.startswith("  "):
            lst[1][-1] += " " + s.strip()      # Fortsetzung eines Listenpunkts
        else:
            if lst or table:
                flush()
            para.append(s.strip())
    flush()
    return "\n".join(out)


PAGE = """<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Silkscreen&display=swap" rel="stylesheet">
<style>
  /* wie der Installer: Farben aus gm_theme.h, Pixelschrift nur für Überschriften */
  :root {{ --bg:#f3f2ee; --ink:#151517; --mute:#5c5c63; --line:#d9d8d2; --acc:#c2410c; --th:#ecebe6; --px:"Silkscreen", ui-monospace, monospace; }}
  @media (prefers-color-scheme: dark) {{ :root {{ --bg:#000; --ink:#ededeb; --mute:#a2a2a9; --line:#303034; --acc:#ff5e21; --th:#1b1b1e; }} }}
  * {{ box-sizing:border-box; }}
  body {{ margin:0; background:var(--bg); color:var(--ink); font:17px/1.55 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif; }}
  main {{ max-width:780px; margin:0 auto; padding:16px 16px 48px; }}
  nav {{ display:flex; justify-content:space-between; align-items:center; gap:8px; margin-bottom:8px; }}
  nav a {{ color:var(--acc); }}
  button {{ font:inherit; font-size:15px; min-height:36px; padding:6px 12px; border-radius:6px; border:1px solid var(--line); background:transparent; color:var(--ink); cursor:pointer; }}
  h1 {{ font:400 28px/1.2 var(--px); margin:16px 0 8px; }} h2 {{ font:400 19px/1.3 var(--px); margin:32px 0 8px; border-bottom:2px solid var(--acc); padding-bottom:8px; }}
  h3 {{ font-size:18px; margin:24px 0 8px; }}
  .tw {{ overflow-x:auto; }} table {{ border-collapse:collapse; width:100%; font-size:15px; margin:8px 0; }}
  th, td {{ border:1px solid var(--line); padding:5px 8px; text-align:left; vertical-align:top; }} th {{ background:var(--th); }}
  code {{ font-size:.92em; background:var(--th); padding:0 4px; border-radius:4px; }}
  .pic {{ float:right; width:150px; height:150px; image-rendering:pixelated; border:3px solid #111; border-radius:6px; margin:0 0 8px 12px; }}
  .meta {{ color:var(--mute); font-size:14px; }}
  [hidden] {{ display:none !important; }}
  @media (max-width:520px) {{ .pic {{ width:110px; height:110px; }} }}
  @media print {{ nav {{ display:none; }} body {{ font-size:11pt; background:#fff; color:#000; }} main {{ max-width:none; padding:0; }}
    h2 {{ break-after:avoid; }} table, tr {{ break-inside:avoid; }} .pic {{ border-color:#000; }} }}
</style>
</head>
<body>
<main>
  <nav><a href="../" id="back">← Installer</a><span><button type="button" id="print">Drucken</button> <button type="button" id="lang">{other}</button></span></nav>
  {pic}
  <article lang="de" id="de">{de}</article>
  <article lang="en" id="en" hidden>{en}</article>
  <p class="meta" id="src">{src}</p>
</main>
<script>
  const hasEn = {has_en};
  let lang = new URLSearchParams(location.search).get("lang") || (function () {{ try {{ return localStorage.getItem("fm1lang"); }} catch (_) {{ return null; }} }})() || (navigator.language.startsWith("de") ? "de" : "en");
  function show() {{
    if (!hasEn) lang = "de";
    document.getElementById("de").hidden = lang !== "de";
    document.getElementById("en").hidden = lang !== "en";
    document.documentElement.lang = lang;
    document.getElementById("lang").textContent = lang === "de" ? "English" : "Deutsch";
    document.getElementById("lang").hidden = !hasEn;
    document.getElementById("print").textContent = lang === "de" ? "Drucken" : "Print";
    document.getElementById("back").textContent = lang === "de" ? "← Zum Installer" : "← Back to the installer";
  }}
  document.getElementById("lang").onclick = () => {{ lang = lang === "de" ? "en" : "de"; try {{ localStorage.setItem("fm1lang", lang); }} catch (_) {{}} show(); }};
  document.getElementById("print").onclick = () => print();
  show();
</script>
</body>
</html>
"""


def build(name, title, dest, screen=None):
    """schreibt dest/anleitung/<name>.html; -> True, wenn es eine Quelle gibt"""
    src = sources(name)
    if not src:
        return False
    de, en = src
    de_html = md(de.read_text(encoding="utf-8"))
    en_html = md(en.read_text(encoding="utf-8")) if en else ""
    pic = f'<img class="pic" src="../bildschirme/{name}.png" alt="" onerror="this.remove()">' if screen else ""
    rel = de.relative_to(ROOT).as_posix()
    (dest / "anleitung").mkdir(parents=True, exist_ok=True)
    (dest / "anleitung" / f"{name}.html").write_text(PAGE.format(
        title=html.escape(f"{title}: Anleitung"), other="English", pic=pic, de=de_html, en=en_html,
        has_en="true" if en else "false", src=html.escape(f"Quelle: {rel}")), encoding="utf-8")
    return True
