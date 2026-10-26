"""Knobs, caps and label strips for the M-VAVE FM-1 running FM1-Electro.

All sizes are measured from the CAD model of the FM-1 (Plasticity, STEP,
mm; see README.md, "Maße"). The parts are built in device coordinates
(x to the right, y to the back, z up; the housing top is z = 21.5), placed
on print plates face down and written as STL (bodies and inlays apart) and
3MF (one object per body and inlay colour, named after the part).

Usage: python3 make_parts.py   (needs: pip install manifold3d trimesh numpy matplotlib)
"""
import math, os
import numpy as np
from manifold3d import Manifold, CrossSection, JoinType, FillRule
import trimesh
from matplotlib.textpath import TextPath
from matplotlib.font_manager import FontProperties

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "stl")
FONT = FontProperties(fname="/usr/share/fonts/opentype/inter/Inter-Bold.otf")

# ---- measured (CAD model) ---------------------------------------------------
TOP = 21.5                       # housing top
# Keys: stadiums (ends a half circle), flanks 30 degrees off vertical,
# 1.5 mm high, 9 mm apart; an LED bar 0.86 x 7.86 in the middle of each.
KEY_W0, KEY_STRAIGHT, KEY_TOP, KEY_DRAFT = 7.90, 7.00, 23.0, math.tan(math.radians(30))
WHITE_X = [13.4 + 9 * i for i in range(16)]
WHITE_Y = 16.5
BLACK_X = [17.9, 26.9, 35.9, 53.9, 62.9, 80.9, 89.9, 98.9, 116.9, 125.9, 143.9]
BLACK_Y = 31.5
LED_W, LED_L = 1.2, 8.2          # light guide (visible flat 0.86 x 7.86)
# Buttons (two rows of six, 10 mm apart): rounded squares 8.4 at the
# housing, 6.4 flat on top, 1.26 high, flanks 25 degrees.
BTN_S0, BTN_TOP, BTN_DRAFT = 8.43, 22.76, math.tan(math.radians(25))
BTN_X = [93.15 + 10 * i for i in range(6)]
BTN_ROWS = {"back": 63.65, "front": 53.65}
# OCT buttons: rounded rectangles 10 x 5.6 at the housing, 8.4 x 4.0 on top, 1.5 high.
OCT_W0, OCT_L0, OCT_TOP = 10.05, 5.65, 23.0
OCT_X, OCT_Y = [16.4, 28.8], 51.3
# Encoders: knurled shafts, 6.0 mm, 20 teeth, smooth up to z 23, knurled
# up to their top at z 28.2. MASTER is the volume pot and keeps its knob.
SHAFT_D, SHAFT_TOP, KNURL_FROM = 6.0, 28.2, 23.0
ENCODERS = {"SELECT": (31, 81), "PRESETS": (14, 64), "ALGORITHM": (31, 64),
            "KNOB 1": (91, 81), "KNOB 2": (109, 81), "KNOB 3": (127, 81), "KNOB 4": (145, 81)}

# ---- fit (change after the fit test) ----------------------------------------
P = dict(
    clr=0.10,        # gap between cap and key/button, per side
    wall=0.70,       # cap wall at its lower edge
    skirt=0.50,      # how far a cap reaches down the key: the rest is travel
    lid=1.00,        # cap top
    inlay=0.40,      # depth of the text (two layers at 0.2)
    bore=5.85,       # knob bore on the knurl: press fit (fit test: 5.6 .. 6.1)
    knob_gap=0.70,   # knob above the housing
)

# ---- colours (FM1-Electro: areas as on the display and in the manual) ------
COL = {"beat": "#ff5a1f", "rhythm": "#e9ad00", "strings": "#8b6cf6", "bass": "#12b0a2",
       "white": "#f2f2ee", "black": "#1d1d1f", "grey": "#77787b", "anthracite": "#3a3b3d",
       "red": "#e0322b", "green": "#27a644", "clear": "#e8ecef"}
AREAS = ["beat", "rhythm", "strings", "bass"]

N = 64


# ---- geometry helpers ---------------------------------------------------------
def rrect(w, l, r):
    r = max(0.05, min(r, w / 2 - 0.01, l / 2 - 0.01))
    return CrossSection.square([w - 2 * r, l - 2 * r], True).offset(r, JoinType.Round, 2, N)


def slab(cs, z, h=0.01):
    return Manifold.extrude(cs, h).translate([0, 0, z])


def loft(a, za, b, zb):
    """Convex solid between section a at za and section b at zb."""
    return Manifold.hull(slab(a, za) + slab(b, zb - 0.01))


def key_section(z, grow=0.0):
    w = KEY_W0 - 2 * KEY_DRAFT * (z - TOP) + 2 * grow
    return rrect(w, w + KEY_STRAIGHT, w / 2)


def btn_section(z, grow=0.0):
    s = BTN_S0 - 2 * BTN_DRAFT * (z - TOP)
    r = max(0.4, 1.2 - BTN_DRAFT * (z - TOP))
    return rrect(s + 2 * grow, s + 2 * grow, r + grow)


def oct_section(z, grow=0.0):
    d = 2 * BTN_DRAFT * (z - TOP)
    r = max(0.4, 1.2 - BTN_DRAFT * (z - TOP))
    return rrect(OCT_W0 - d + 2 * grow, OCT_L0 - d + 2 * grow, r + grow)


def text_cs(s, height, x=0.0, y=0.0, align="center"):
    """Text as a cross-section, cap height = height, centred on (x, y)."""
    if not s:
        return CrossSection()
    path = TextPath((0, 0), s, size=1.0, prop=FONT)
    polys = [p for p in path.to_polygons(closed_only=True) if len(p) > 2]
    cap = TextPath((0, 0), "H", size=1.0, prop=FONT).get_extents().height
    k = height / cap
    ext = path.get_extents()
    dx = -(ext.x0 + ext.width / 2) if align == "center" else -ext.x0
    cs = CrossSection([[(float((px + dx) * k), float(py * k)) for px, py in p[:-1]] for p in polys], FillRule.EvenOdd)
    return cs.translate([x, y - height / 2])


def text_width(s, height):
    path = TextPath((0, 0), s, size=1.0, prop=FONT)
    cap = TextPath((0, 0), "H", size=1.0, prop=FONT).get_extents().height
    return path.get_extents().width * height / cap


def inlay(body, cs, ztop, depth):
    """Cut the text into the top of body; returns (body, text part)."""
    if cs.is_empty():
        return body, Manifold()
    t = Manifold.extrude(cs, depth).translate([0, 0, ztop - depth])
    return body - t, t ^ body


# ---- parts ----------------------------------------------------------------------
def cap(section, top, labels, slot=None):
    """A cap over a key or button: skirt P['skirt'] down the flank, lid on top,
    labels inlaid in the lid. slot: (w, l) window for the LED, through."""
    zb = top - P["skirt"]
    zt = top + P["lid"]
    outer = loft(section(zb, P["clr"] + P["wall"]), zb, section(zb, P["clr"] + P["wall"] - 0.45), zt)
    hole = loft(section(zb - 0.01, P["clr"]), zb - 0.01, section(top, P["clr"]), top)
    body = outer - hole
    if slot:
        body -= Manifold.extrude(rrect(slot[0], slot[1], slot[0] / 2), P["lid"] + 1).translate([0, 0, top - 0.5])
    texts = []
    for cs in labels:
        body, t = inlay(body, cs, zt, P["inlay"])
        texts.append(t)
    return body, texts, zt


def key_cap(row_white, n):
    """Key n (0-based) of the white (front) or black (back) row, centred at 0."""
    if row_white:
        num = str(n + 1)
        letter = "ABCD"[n % 4]
        labels = [text_cs(num, 1.7, 0, 5.15), text_cs(letter, 1.7, 0, -5.15)]
    else:
        fx = ["STU", "HAL", "TAP", "REP", "DLY", "BIT", "PHA"]
        top_label = "M" if n < 4 else fx[n - 4]
        labels = [text_cs(top_label, 1.5, 0, 5.15)]
        if n < 4:
            labels.append(text_cs(str(n + 1), 1.5, 0, -5.15))
    return cap(key_section, KEY_TOP, labels, slot=(LED_W + 0.2, LED_L + 0.2))


BUTTONS = {  # name, row, column, body colour, text colour
    "FX": ("BEAT", "back", 0, "clear", "beat"), "SEL": ("RHY", "back", 1, "clear", "rhythm"),
    "ENV": ("STR", "back", 2, "clear", "strings"), "LFO": ("BASS", "back", 3, "clear", "bass"),
    "EDIT": ("EDIT", "back", 4, "clear", "black"), "GLO": ("GLO", "back", 5, "clear", "black"),
    "HOME": ("HOME", "front", 0, "clear", "black"), "SAVE": ("SAVE", "front", 1, "clear", "black"),
    "ARP": ("ARP", "front", 2, "clear", "black"), "SEQ": ("SEQ", "front", 3, "clear", "black"),
    "PLAY": ("PLAY", "front", 4, "clear", "green"), "REC": ("REC", "front", 5, "clear", "red"),
}


def button_cap(label):
    h = 1.9 if text_width(label, 1.9) < 6.6 else 6.6 / text_width(label, 1.0)
    return cap(btn_section, BTN_TOP, [text_cs(label, h)])


def oct_cap(label):
    return cap(oct_section, OCT_TOP, [text_cs(label, 2.0)])


def knob(d, label, number=None):
    """Knob on a knurled encoder shaft, its bottom P['knob_gap'] above the
    housing, resting on the shaft top. Label (or number) and a ring inlaid
    on top."""
    zb = TOP + P["knob_gap"]
    depth = SHAFT_TOP - zb
    h = depth + 5.0
    body = Manifold.cylinder(h - 0.79, d / 2, d / 2, 96).translate([0, 0, zb])
    body += Manifold.cylinder(0.8, d / 2, d / 2 - 0.8, 96).translate([0, 0, zb + h - 0.8])
    grip = Manifold()
    for i in range(30):
        a = 2 * math.pi * i / 30
        grip += Manifold.cylinder(h - 2.6, 0.55, 0.55, 12).translate(
            [(d / 2 + 0.25) * math.cos(a), (d / 2 + 0.25) * math.sin(a), zb + 1.0])
    body -= grip
    # bore: clearance over the smooth part, press fit on the knurl, chamfered entry
    smooth = KNURL_FROM - zb
    body -= Manifold.cylinder(smooth + 0.01, SHAFT_D / 2 + 0.2, SHAFT_D / 2 + 0.2, 64).translate([0, 0, zb - 0.01])
    body -= Manifold.cylinder(0.6, SHAFT_D / 2 + 0.5, SHAFT_D / 2 + 0.2, 64).translate([0, 0, zb - 0.01])
    body -= Manifold.cylinder(depth - smooth + 0.02, P["bore"] / 2, P["bore"] / 2, 64).translate([0, 0, KNURL_FROM - 0.01])
    zt = zb + h
    ring = CrossSection.circle(d / 2 - 1.3, 96) - CrossSection.circle(d / 2 - 2.1, 96)
    labels = [ring]
    if number:
        labels.append(text_cs(number, 3.6))
    else:
        labels.append(text_cs(label, 2.0 if len(label) <= 3 else 1.6))
    texts = []
    for cs in labels:
        body, t = inlay(body, cs, zt, P["inlay"])
        texts.append(t)
    return body, texts, zt


def strip_front():
    """Strip on the housing in front of the keys: the four areas over their
    slot keys (white 1-4 BEAT A-D, 5-8 RHYTHM, 9-12 STRINGS, 13-16 BASS)."""
    y0, y1, t = 2.6, 6.3, 0.8      # between the housing edge (2) and the key pocket (6.7)
    body = Manifold.extrude(rrect(144.0, y1 - y0, 1.0), t).translate([81.0, (y0 + y1) / 2, TOP])
    texts = {}
    for a, name in enumerate(["BEAT", "RHYTHM", "STRINGS", "BASS"]):
        x0, x1 = WHITE_X[4 * a] - 3.2, WHITE_X[4 * a + 3] + 3.2
        bar = rrect(x1 - x0, 0.6, 0.3).translate([(x0 + x1) / 2, y1 - 0.65])
        cs = text_cs(name, 1.8, (x0 + x1) / 2, y0 + 1.45) + bar
        body, tx = inlay(body, cs, TOP + t, P["inlay"])
        texts[AREAS[a]] = tx
    return body, texts


def strip_back():
    """Strip behind the black keys: MUTE over black 1-4, LIVE FX over 5-11."""
    y0, y1, t = 42.2, 46.8, 0.8    # between the key pocket (41.8) and the button pockets (47.3)
    body = Manifold.extrude(rrect(144.0, y1 - y0, 1.0), t).translate([81.0, (y0 + y1) / 2, TOP])
    cs = CrossSection()
    groups = [("MUTE", BLACK_X[0], BLACK_X[3]), ("LIVE FX \u00b7 HOLD + WHITE KEY = VALUE", BLACK_X[4], BLACK_X[10])]
    for name, xa, xb in groups:
        xc = (xa + xb) / 2
        cs += text_cs(name, 1.9, xc, (y0 + y1) / 2 + 0.45)
        cs += rrect(xb - xa + 6.0, 0.6, 0.3).translate([xc, y0 + 0.75])
    body, tx = inlay(body, cs, TOP + t, P["inlay"])
    return body, {"white": tx}


def fit_test():
    """Seven bores 5.6 .. 6.2 on the knurl (dots count up), plus one of each cap."""
    sizes = [5.6, 5.7, 5.8, 5.85, 5.9, 6.0, 6.1]
    plate = Manifold.extrude(rrect(len(sizes) * 10 + 4, 16, 2), 8.5)
    for i, s in enumerate(sizes):
        x = -len(sizes) * 5 + 5 + i * 10
        plate -= Manifold.cylinder(1.5, SHAFT_D / 2 + 0.2, SHAFT_D / 2 + 0.2, 64).translate([x, 2, -0.01])
        plate -= Manifold.cylinder(7.0, s / 2, s / 2, 64).translate([x, 2, 1.49])
        for k in range(i + 1):
            plate -= Manifold.cylinder(0.6, 0.45, 0.45, 16).translate([x - 3 + k * 1.0, -6, 8.0])
    return plate


# ---- plates and files -------------------------------------------------------
def face_down(m, ztop):
    """Turn a part built in device coordinates top down onto the bed."""
    return m.rotate([180, 0, 0]).translate([0, 0, ztop])


def to_mesh(m):
    g = m.to_mesh()
    return trimesh.Trimesh(vertices=np.asarray(g.vert_properties)[:, :3], faces=np.asarray(g.tri_verts), process=False)


class Plate:
    def __init__(self, name):
        self.name, self.parts = name, []   # (object name, colour key, manifold on the bed)

    def add(self, name, colour, m):
        if not m.is_empty():
            assert name.endswith("text") or len(m.decompose()) == 1, name + " falls apart"
            self.parts.append((name, colour, m))

    def write(self):
        os.makedirs(OUT, exist_ok=True)
        scene = trimesh.Scene()
        groups = {}
        for name, colour, m in self.parts:
            mesh = to_mesh(m)
            rgb = trimesh.visual.color.hex_to_rgba(COL[colour])
            mesh.visual.face_colors = np.tile(rgb, (len(mesh.faces), 1))
            scene.add_geometry(mesh, node_name=f"{name} [{colour}]", geom_name=f"{name} [{colour}]")
            groups.setdefault("body" if name.endswith("body") else "text", []).append(mesh)
        scene.export(os.path.join(OUT, self.name + ".3mf"))
        for kind, meshes in groups.items():
            trimesh.util.concatenate(meshes).export(os.path.join(OUT, f"{self.name}_{kind}.stl"))


def grid(i, cols, dx, dy):
    return (i % cols) * dx, -(i // cols) * dy


def build():
    placed = []        # (name, colour, manifold in device coordinates) for the preview
    plates = []

    # Knobs
    pl = Plate("knobs")
    for i, (name, (x, y)) in enumerate(ENCODERS.items()):
        if name.startswith("KNOB"):
            n = int(name[-1])
            body, texts, zt = knob(15.0, name, str(n))
            body_c, text_c = "white", AREAS[n - 1]
        else:
            body, texts, zt = knob(13.0, {"SELECT": "SEL", "PRESETS": "PRE", "ALGORITHM": "ALG"}[name])
            body_c, text_c = "anthracite", "white"
        gx, gy = grid(i, 4, 20, 20)
        pl.add(f"{name} body", body_c, face_down(body, zt).translate([gx, gy, 0]))
        for t in texts:
            pl.add(f"{name} text", text_c, face_down(t, zt).translate([gx, gy, 0]))
        placed.append((name, body_c, body.translate([x, y, 0])))
        for t in texts:
            placed.append((name, text_c, t.translate([x, y, 0])))
    plates.append(pl)

    # Buttons and OCT
    pl = Plate("buttons")
    for i, (btn, (label, row, col, body_c, text_c)) in enumerate(BUTTONS.items()):
        body, texts, zt = button_cap(label)
        gx, gy = grid(i, 6, 12, 12)
        pl.add(f"{btn} body", body_c, face_down(body, zt).translate([gx, gy, 0]))
        for t in texts:
            pl.add(f"{btn} text", text_c, face_down(t, zt).translate([gx, gy, 0]))
        x, y = BTN_X[col], BTN_ROWS[row]
        placed.append((btn, body_c, body.translate([x, y, 0])))
        for t in texts:
            placed.append((btn, text_c, t.translate([x, y, 0])))
    for i, label in enumerate(["OCT-", "OCT+"]):
        body, texts, zt = oct_cap(label.replace("-", "−"))
        pl.add(f"{label} body", "clear", face_down(body, zt).translate([i * 14, -26, 0]))
        for t in texts:
            pl.add(f"{label} text", "black", face_down(t, zt).translate([i * 14, -26, 0]))
        placed.append((label, "clear", body.translate([OCT_X[i], OCT_Y, 0])))
        for t in texts:
            placed.append((label, "black", t.translate([OCT_X[i], OCT_Y, 0])))
    plates.append(pl)

    # Keys: white row in the colours of the areas (slots A-D), black row:
    # MUTE 1-4 in the area colours, the live effects black.
    pl = Plate("keys")
    for i, x in enumerate(WHITE_X):
        body, texts, zt = key_cap(True, i)
        body_c, text_c = AREAS[i // 4], "white" if i // 4 != 1 else "black"
        gx, gy = grid(i, 8, 10, 17)
        pl.add(f"W{i + 1} body", body_c, face_down(body, zt).translate([gx, gy, 0]))
        for t in texts:
            pl.add(f"W{i + 1} text", text_c, face_down(t, zt).translate([gx, gy, 0]))
        placed.append((f"W{i + 1}", body_c, body.translate([x, WHITE_Y, 0])))
        for t in texts:
            placed.append((f"W{i + 1}", text_c, t.translate([x, WHITE_Y, 0])))
    for i, x in enumerate(BLACK_X):
        body, texts, zt = key_cap(False, i)
        body_c = AREAS[i] if i < 4 else "black"
        text_c = "black" if i == 1 else "white"
        gx, gy = grid(i, 8, 10, 17)
        pl.add(f"B{i + 1} body", body_c, face_down(body, zt).translate([gx, gy - 36, 0]))
        for t in texts:
            pl.add(f"B{i + 1} text", text_c, face_down(t, zt).translate([gx, gy - 36, 0]))
        placed.append((f"B{i + 1}", body_c, body.translate([x, BLACK_Y, 0])))
        for t in texts:
            placed.append((f"B{i + 1}", text_c, t.translate([x, BLACK_Y, 0])))
    plates.append(pl)

    # Label strips
    pl = Plate("strips")
    for k, (fn, body_c) in enumerate([(strip_front, "anthracite"), (strip_back, "anthracite")]):
        body, texts = fn()
        zt = TOP + 0.8
        cy = 4.45 if k == 0 else 44.5
        pl.add(f"strip {k + 1} body", body_c, face_down(body, zt).translate([-81, cy - 8 * k, 0]))
        for c, t in texts.items():
            pl.add(f"strip {k + 1} text", c, face_down(t, zt).translate([-81, cy - 8 * k, 0]))
        placed.append((f"strip {k + 1}", body_c, body))
        for c, t in texts.items():
            placed.append((f"strip {k + 1}", c, t))
    plates.append(pl)

    for p in plates:
        p.write()
    trimesh.util.concatenate([to_mesh(fit_test())]).export(os.path.join(OUT, "fit_test.stl"))
    return placed


if __name__ == "__main__":
    placed = build()
    import pickle, sys
    with open(sys.argv[1] if len(sys.argv) > 1 else os.devnull, "wb") as f:
        pickle.dump([(n, c, to_mesh(m).vertices, to_mesh(m).faces) for n, c, m in placed], f)
    print("written to", OUT)
