"""Exports the default parts of fm1_caps.scad as STL (same geometry and
defaults, built with manifold3d) plus a preview image. For your own
measurements use fm1_caps.scad in OpenSCAD, or change P below.
Usage: python3 make_stl.py   (needs: pip install manifold3d trimesh numpy matplotlib)"""
import math, os
import numpy as np
from manifold3d import Manifold, CrossSection, JoinType
import trimesh

P = dict(shaft_d=6.0, shaft_flat=4.5, shaft_depth=10.0, fit=0.15,
         large_d=16, large_h=15, small_d=13, small_h=13, flutes=24, flute_depth=0.6, pointer_w=0.9, pointer_depth=0.7,
         btn_w=10.4, btn_l=5.4, btn_h=2.0, btn_r=1.2, wall=0.8, top=1.2, btn_fit=0.15)
N = 96
HERE = os.path.dirname(os.path.abspath(__file__))


def cyl(d, h, d2=None, n=N):
    return Manifold.cylinder(h, d / 2, (d2 if d2 is not None else d) / 2, n)


def box(x, y, z, w, l, h):
    return Manifold.cube([w, l, h]).translate([x, y, z])


def rounded_rect(w, l, r, h):
    cs = CrossSection.square([w - 2 * r, l - 2 * r], True).offset(r, JoinType.Round, 2, 64)
    return Manifold.extrude(cs, h)


def d_shape(d, flat, h):
    """D profile: circle of d, cut at flat measured from the far side."""
    return cyl(d, h) ^ box(-d / 2, -d / 2, -1, d, flat, h + 2)


def knob(d, h, p=P):
    body = cyl(d, h - 1) + cyl(d, 1, d - 2).translate([0, 0, h - 1])
    cut = Manifold()
    for i in range(p['flutes']):
        a = 2 * math.pi * i / p['flutes']
        r = d / 2 + 0.6 - p['flute_depth']
        cut += cyl(1.2, h, n=16).translate([r * math.cos(a), r * math.sin(a), 1.5])
    w, pd = p['pointer_w'], p['pointer_depth']
    cut += box(-w / 2, 0, h - pd, w, d / 2 + 1, pd + 1)
    cut += box(-w / 2, d / 2 - pd, h * 0.45, w, 2, h)
    bd = p['shaft_d'] + 2 * p['fit']
    cut += d_shape(bd, p['shaft_flat'] + 2 * p['fit'], p['shaft_depth'] + 0.01).translate([0, 0, -0.01])
    return body - cut


def button_cap(p=P):
    iw, il = p['btn_w'] + 2 * p['btn_fit'], p['btn_l'] + 2 * p['btn_fit']
    outer = rounded_rect(iw + 2 * p['wall'], il + 2 * p['wall'], p['btn_r'] + p['wall'], p['btn_h'] + p['top'])
    inner = rounded_rect(iw, il, p['btn_r'] + p['btn_fit'], p['btn_h'] + 0.01).translate([0, 0, -0.01])
    return outer - inner


def fit_test(p=P):
    sizes = [5.8, 5.9, 6.0, 6.1, 6.2, 6.3, 6.4]
    plate = rounded_rect(len(sizes) * 10 + 4, 14, 2, 8)
    for i, s in enumerate(sizes):
        x = (i - (len(sizes) - 1) / 2) * 10
        plate -= d_shape(s, p['shaft_flat'] + s - p['shaft_d'], 8.02).translate([x, -1.5, -0.01])
        for k in range(i + 1):
            plate -= cyl(0.7, 1, n=12).translate([x - 3 + k, 5, 7.4])
    return plate


def save(m, name):
    mesh = m.to_mesh()
    t = trimesh.Trimesh(vertices=np.asarray(mesh.vert_properties)[:, :3], faces=np.asarray(mesh.tri_verts))
    path = os.path.join(HERE, 'stl', name + '.stl')
    t.export(path)
    return t


if __name__ == '__main__':
    os.makedirs(os.path.join(HERE, 'stl'), exist_ok=True)
    parts = {'knob_large': knob(P['large_d'], P['large_h']), 'knob_small': knob(P['small_d'], P['small_h']),
             'button_cap': button_cap(), 'fit_test': fit_test()}
    meshes = {}
    for name, m in parts.items():
        t = save(m, name)
        meshes[name] = t
        print(f"{name}: {t.bounds[1] - t.bounds[0]} mm, watertight {t.is_watertight}")
    # Preview
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    fig = plt.figure(figsize=(12, 3.4))
    for i, (name, t) in enumerate(meshes.items()):
        ax = fig.add_subplot(1, 4, i + 1, projection='3d')
        tri = t.vertices[t.faces]
        n = t.face_normals
        shade = 0.35 + 0.65 * np.clip(n @ np.array([0.3, -0.5, 0.8]), 0, 1)
        col = np.c_[shade * 0.95, shade * 0.95, shade * 0.97, np.ones(len(shade))]
        ax.add_collection3d(Poly3DCollection(tri, facecolors=col, edgecolor='none'))
        lo, hi = t.bounds
        c, r = (lo + hi) / 2, (hi - lo).max() / 2
        ax.set_xlim(c[0] - r, c[0] + r); ax.set_ylim(c[1] - r, c[1] + r); ax.set_zlim(c[2] - r, c[2] + r)
        ax.view_init(28, -60 if name != 'button_cap' else -60)
        ax.set_axis_off()
        ax.set_title(name.replace('_', ' '), fontsize=9)
    plt.tight_layout()
    plt.savefig(os.path.join(HERE, 'preview.png'), dpi=130)
