#!/usr/bin/env python3
"""Erzeugt die kleinen Bilder der Firmware-Liste (bilder/<NAME>.svg, 120 × 80). Eigene, schlichte Zeichnungen:
keine Logos oder Schriftzüge der Hersteller. Neuer Paketname ohne Bild: die Seite zeigt allgemein.svg.

  python3 web/installer/bilder/make_bilder.py
"""
from pathlib import Path

HERE = Path(__file__).resolve().parent


def svg(body, bg="#e9e6df"):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 120 80" width="120" height="80">'
            f'<rect width="120" height="80" rx="10" fill="{bg}"/>{body}</svg>\n')


def keys(x, y, w, h, n, white="#fafafa", black="#222", line="#999"):
    """kleine Klaviatur: n weiße Tasten"""
    kw = w / n
    out = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{white}" stroke="{line}" stroke-width="0.6"/>'
    for i in range(1, n):
        out += f'<line x1="{x + i * kw:.1f}" y1="{y}" x2="{x + i * kw:.1f}" y2="{y + h}" stroke="{line}" stroke-width="0.6"/>'
    for i in range(n - 1):
        if i % 7 in (2, 6):
            continue
        out += f'<rect x="{x + (i + 0.65) * kw:.1f}" y="{y}" width="{kw * 0.7:.1f}" height="{h * 0.6:.1f}" fill="{black}"/>'
    return out


def sliders(x, y, n, gap, h, cap="#eee", track="#111", vals=None):
    out = ""
    for i in range(n):
        v = (vals or [0.3, 0.7, 0.5, 0.9, 0.2, 0.6, 0.4, 0.8])[i % 8]
        cx = x + i * gap
        out += f'<line x1="{cx}" y1="{y}" x2="{cx}" y2="{y + h}" stroke="{track}" stroke-width="1.6"/>'
        out += f'<rect x="{cx - 2.5}" y="{y + h * (1 - v) - 2:.1f}" width="5" height="4" rx="1" fill="{cap}"/>'
    return out


def rack(x, y, w, h, body="#2b2b2b", screen="#9fe870", wave=""):
    out = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" fill="{body}"/>'
    out += f'<circle cx="{x + 5}" cy="{y + 5}" r="1.6" fill="#777"/><circle cx="{x + w - 5}" cy="{y + 5}" r="1.6" fill="#777"/>'
    out += f'<circle cx="{x + 5}" cy="{y + h - 5}" r="1.6" fill="#777"/><circle cx="{x + w - 5}" cy="{y + h - 5}" r="1.6" fill="#777"/>'
    out += f'<rect x="{x + 12}" y="{y + 7}" width="{w * 0.42:.0f}" height="{h - 14}" rx="2" fill="#10210c"/>'
    out += wave or f'<path d="M{x + 16} {y + h / 2 + 3} h6 v-6 h6 v6 h6 v-6 h6" stroke="{screen}" stroke-width="1.5" fill="none"/>'
    kx = x + 12 + w * 0.42 + 10
    for i in range(3):
        out += f'<circle cx="{kx + i * 11:.0f}" cy="{y + h / 2}" r="4" fill="#ddd"/><line x1="{kx + i * 11:.0f}" y1="{y + h / 2}" x2="{kx + i * 11 + 2:.0f}" y2="{y + h / 2 - 3:.0f}" stroke="#333" stroke-width="1.2"/>'
    return out


B = {}

# FM1-Electro: Drum-Lauflicht (16 Steps in vier Farben) über einer kleinen Klaviatur
steps = ""
cols = ["#d9442b", "#ef8a2e", "#f2c230", "#f1ecdc"]
for i in range(16):
    on = i in (0, 4, 6, 8, 12, 14)
    steps += (f'<rect x="{9 + i * 6.4:.1f}" y="14" width="5" height="9" rx="1" fill="{cols[i // 4]}" '
              f'stroke="#222" stroke-width="{1.2 if on else 0.3}" opacity="{1 if on else 0.55}"/>')
B["FM1_ELECTRO"] = svg('<rect x="4" y="6" width="112" height="68" rx="6" fill="#2a2a2a"/>' + steps
                       + '<circle cx="18" cy="33" r="3.5" fill="#ccc"/><circle cx="30" cy="33" r="3.5" fill="#ccc"/>'
                       + '<circle cx="42" cy="33" r="3.5" fill="#ccc"/><rect x="60" y="29" width="48" height="9" rx="2" fill="#0c1a2a"/>'
                       + '<path d="M63 36 l5 -5 v5 l5 -5 v5 l5 -5 v5 l5 -5 v5 l5 -5 v5" stroke="#6cf" fill="none" stroke-width="1"/>'
                       + keys(9, 44, 102, 25, 14))

# SH-101: grauer Mono-Synth, Schieberegler, ein Sägezahn, Klaviatur
B["SH101"] = svg('<rect x="4" y="6" width="112" height="68" rx="6" fill="#9a9c9e"/>'
                 + '<rect x="4" y="6" width="112" height="34" rx="6" fill="#55585c"/>'
                 + sliders(12, 12, 7, 8, 22, cap="#f2f2f2")
                 + '<rect x="72" y="12" width="38" height="22" rx="2" fill="#2a2c2e"/>'
                 + '<path d="M75 30 l10 -14 v14 l10 -14 v14 l10 -14 v14" stroke="#ff6a3d" stroke-width="1.6" fill="none"/>'
                 + keys(9, 44, 102, 25, 14, black="#1a1a1a"))

# JX-8P: dunkles Bedienfeld, viele kleine Regler, Chorus-Wellen
B["JX8P"] = svg('<rect x="4" y="6" width="112" height="68" rx="6" fill="#1d1f24"/>'
                + sliders(12, 12, 10, 6.4, 20, cap="#4aa3ff", track="#555")
                + '<rect x="80" y="12" width="30" height="20" rx="2" fill="#0a1220"/>'
                + '<path d="M82 20 q4 -6 8 0 t8 0 t8 0" stroke="#4aa3ff" fill="none" stroke-width="1.3"/>'
                + '<path d="M82 26 q4 -6 8 0 t8 0 t8 0" stroke="#9fd0ff" fill="none" stroke-width="1" opacity=".7"/>'
                + keys(9, 44, 102, 25, 14))

# MKS-50: Rack-Modul mit Display
B["MKS50"] = svg('<rect x="0" y="0" width="0" height="0"/>' + rack(6, 22, 108, 36))

# JP-6: breites Bedienfeld mit bunten Tastern, Klaviatur
btn = ""
bc = ["#e94b3c", "#f39c34", "#f4d03f", "#58b368", "#3f8fd8", "#e94b3c", "#f39c34", "#f4d03f", "#58b368", "#3f8fd8"]
for i, c in enumerate(bc):
    btn += f'<rect x="{11 + i * 10}" y="28" width="7" height="6" rx="1" fill="{c}"/>'
B["JP6"] = svg('<rect x="4" y="6" width="112" height="68" rx="6" fill="#2b2b2b"/>'
               + sliders(14, 10, 12, 8, 14, cap="#ddd", track="#666") + btn + keys(9, 44, 102, 25, 14))

# MKS Ultra: Rack-Modul, Display mit resonanter Filterkurve
B["MKSULTRA"] = svg(rack(6, 22, 108, 36, body="#3a2a4a", screen="#d59cff",
                         wave='<path d="M20 50 h16 q6 0 8 -16 q2 16 6 16 h0" stroke="#d59cff" stroke-width="1.5" fill="none"/>'))

# JX-8P + SH-101: zwei Hälften
B["JXSH"] = svg('<rect x="4" y="6" width="56" height="68" rx="6" fill="#1d1f24"/>'
                + '<rect x="60" y="6" width="56" height="68" rx="6" fill="#9a9c9e"/>'
                + '<rect x="60" y="6" width="56" height="34" rx="6" fill="#55585c"/>'
                + sliders(12, 12, 7, 6.4, 22, cap="#4aa3ff", track="#555")
                + sliders(68, 12, 6, 8, 22, cap="#f2f2f2")
                + keys(9, 44, 102, 25, 14))

# allgemein: Klaviatur mit Fragezeichen-freiem Chip
B["allgemein"] = svg('<rect x="4" y="6" width="112" height="68" rx="6" fill="#3a3d42"/>'
                     + '<rect x="40" y="13" width="40" height="22" rx="3" fill="#20252b" stroke="#8aa" stroke-width="1"/>'
                     + ''.join(f'<line x1="{44 + i * 6}" y1="10" x2="{44 + i * 6}" y2="13" stroke="#8aa"/>'
                               f'<line x1="{44 + i * 6}" y1="35" x2="{44 + i * 6}" y2="38" stroke="#8aa"/>' for i in range(6))
                     + keys(9, 44, 102, 25, 14))

for name, s in B.items():
    (HERE / f"{name}.svg").write_text(s, encoding="utf-8")
print(", ".join(sorted(B)))
