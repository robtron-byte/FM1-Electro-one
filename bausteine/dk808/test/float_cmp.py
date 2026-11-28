#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""dk808: float gegen double (docs/adr/0050): float_cmp.py <double.raw> <float.raw> <Grenze dB>"""
import array, math, sys
NM = ["BD", "LG", "SD", "RS", "CP", "LC", "MC", "HC", "LT", "MT", "HT", "CH", "OH", "CY", "CL", "MA", "CB"]
N = 44100 * 2
a, b = array.array('f'), array.array('f')
a.frombytes(open(sys.argv[1], 'rb').read()); b.frombytes(open(sys.argv[2], 'rb').read())
limit = float(sys.argv[3]); worst = -999; fails = 0
print("dk808 float gegen double (Fehlerenergie unter dem Signal; größte Abweichung je Sample / Spitze):")
for i, n in enumerate(NM):
    row = f"  {n:2}"
    for v in range(2):
        o = (i * 2 + v) * N
        e = s = mx = pk = 0.0
        for k in range(o, o + N):
            d = a[k] - b[k]; e += d * d; s += a[k] * a[k]
            mx = max(mx, abs(d)); pk = max(pk, abs(a[k]))
        db = 10 * math.log10(e / s) if s > 0 and e > 0 else (-300.0 if s > 0 else 0.0)
        if s > 0: worst = max(worst, db); fails += db > limit
        row += f"   v{(64, 127)[v]:<3} {db:7.1f} dB  {mx / pk if pk else 0:.1e}"
    print(row)
print(f"dk808 float: schlechteste {worst:.1f} dB (Grenze {limit:.0f} dB) {'FEHLER' if fails else 'ok'}")
sys.exit(1 if fails else 0)
