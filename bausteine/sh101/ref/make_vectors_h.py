#!/usr/bin/env python3
"""Erzeugt ref/vectors.h aus ref/vectors.json (Testvektoren der Vorlage). Aufruf: python3 ref/make_vectors_h.py"""
import json, os
here = os.path.dirname(os.path.abspath(__file__))
d = json.load(open(os.path.join(here, "vectors.json")))
o = ["/* Erzeugt von make_vectors_h.py aus vectors.json (MonoBass RenderCheck --vectors). Nicht von Hand ändern. */",
     "typedef struct { const char *key; float value; } vec_param_t;",
     "typedef struct { double time; int note; double length; } vec_note_t;",
     "typedef struct { const char *name; int rate; double seconds; float velocity; int np; vec_param_t params[8];",
     "  int nn; vec_note_t notes[4]; float rms[20]; float samples[64]; } vec_t;",
     "static const vec_t vectors[] = {"]
for v in d:
    ps = ", ".join('{"%s", %rf}' % (k, float(x)) for k, x in sorted(v["params"].items()))
    ns = ", ".join("{%r, %d, %r}" % (float(a), b, float(c)) for a, b, c in v["notes_time_note_length"])
    o.append('  {"%s", %d, %r, %rf, %d, {%s}, %d, {%s},' % (v["name"], v["sampleRate"], float(v["seconds"]),
             float(v["velocity"]), len(v["params"]), ps, len(v["notes_time_note_length"]), ns))
    o.append("   {%s}," % ", ".join("%.6ff" % x for x in v["rms_per_100ms"]))
    o.append("   {%s}}," % ", ".join("%.9gf" % x for x in v["samples_2400_to_2463"]))
o.append("};")
open(os.path.join(here, "vectors.h"), "w").write("\n".join(o) + "\n")
