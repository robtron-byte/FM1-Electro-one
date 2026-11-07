#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Generate engine/extra_presets.c: FM1-Electro's own sounds (docs/adr/0026).

They come after the factory sounds of both engines, so the numbers of saved
sounds stay as they were. Each sound is a factory sound of the same engine
(BASE) with a few parameters changed; names of parameters as in engine/mks.h
and engine/jp6.h without the MP_ / JP_ prefix.

Usage: extra_presets.py            (writes engine/extra_presets.c)"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def enum_names(header, start, prefix):
    text = (ROOT / "engine" / header).read_text()
    a = text.index(start)
    body = re.sub(r"/\*.*?\*/", "", text[a:text.index("};", a)], flags=re.S).split("{", 1)[1]
    return [n[len(prefix):] for n in re.findall(r"\b(%s[A-Z0-9_]+)\b" % prefix, body) if not n.endswith("COUNT")]


def table(path):
    out = {}
    for n, a, v in re.findall(r'\{"([^"]+)", (\d), \{([^}]*)\}\}', (ROOT / "engine" / path).read_text()):
        out[n] = [int(x) for x in v.split(",")]
    return out


MKS_NAMES, JP_NAMES = enum_names("mks.h", "enum mks_param", "MP_"), enum_names("jp6.h", "enum jp6_param", "JP_")
MKS, JP = table("mks_presets.c"), table("jp6_presets.c")
RHYTHM, STRINGS, BASS = 1, 2, 3

# (engine, area, name, base sound, changes). Plain sounds do little by
# themselves (no LFO, no sweeps, no arpeggio): the sequence makes them
# interesting. The bass sounds keep the low end out of the 808 kick's way.
SOUNDS = [
    # ---- BASS -----------------------------------------------------------------
    # PLAIN/SQR/SAW/PULSE keep the 33 Hz of the 16' basses, under the kick;
    # the MID basses play an octave higher, clear of it (tools/preset_check.py).
    ("mks", BASS, "PLAIN BASS", "FAT BASS", dict(SUB=0, SUB_LEVEL=0, SAW=3, HPF=1, CUTOFF=34, RESO=10, VCF_ENV=34, T2=60, L2=70, T3=60, L3=70, T4=20, PORTA=0)),
    ("mks", BASS, "SQR BASS", "FAT BASS", dict(SUB=0, SUB_LEVEL=0, SAW=0, PULSE=3, PW=0, HPF=1, CUTOFF=36, RESO=8, VCF_ENV=40, T2=56, L2=60, T3=56, L3=60, T4=18)),
    ("mks", BASS, "MID BASS", "FAT BASS", dict(VOLUME=127, RANGE=0, SUB=0, SUB_LEVEL=0, SAW=3, HPF=3, CUTOFF=50, RESO=18, VCF_ENV=50, VCF_KEY=60, T2=60, L2=70, T3=60, L3=70, T4=20, PORTA=0)),
    ("mks", BASS, "MID PLUCK", "FAT BASS", dict(VOLUME=127, RANGE=0, SUB=0, SUB_LEVEL=0, SAW=3, HPF=3, CUTOFF=40, RESO=20, VCF_ENV=70, VCF_KEY=60, T2=50, L2=60, T3=56, L3=30, T4=16, PORTA=0)),
    ("jp6", BASS, "SAW BASS", "JUPI BASS", dict(VCO1_SQUARE=0, MIX=0, CUTOFF=40, RESO=16, VCF_ENV=40, ENV1_D=46, ENV1_S=20, DRIFT=0)),
    ("jp6", BASS, "PULSE BASS", "JUPI BASS", dict(VCO1_SAW=0, VCO1_SQUARE=1, MIX=0, CUTOFF=38, RESO=10, VCF_ENV=34, ENV1_D=44, ENV1_S=30, DRIFT=0)),
    ("jp6", BASS, "MID BASS 6", "JUPI BASS", dict(VOLUME=127, VCO1_RANGE=3, VCO2_RANGE=4, HPF=40, CUTOFF=52, RESO=22, VCF_ENV=56, DRIFT=0)),
    ("jp6", BASS, "MID PLUCK6", "JUPI BASS", dict(VOLUME=127, VCO1_RANGE=3, VCO2_RANGE=4, MIX=0, HPF=40, CUTOFF=50, RESO=22, VCF_ENV=70, ENV1_D=46, ENV1_S=20, ENV2_D=56, ENV2_S=40, ENV2_R=22, DRIFT=0)),
    # ---- RHYTHM: plain voices -----------------------------------------------
    ("mks", RHYTHM, "PLAIN SAW", "PLUCK KEYS", dict(PULSE=0, SAW=3, CHORUS=0, HPF=1, CUTOFF=60, RESO=8, VCF_ENV=30, VCF_KEY=70, T2=48, L2=60, T3=50, L3=40, T4=24)),
    ("mks", RHYTHM, "PLAIN PLS", "PLUCK KEYS", dict(PULSE=3, SAW=0, PW=40, CHORUS=0, HPF=1, CUTOFF=58, RESO=8, VCF_ENV=34, VCF_KEY=70, T2=48, L2=60, T3=50, L3=40, T4=24)),
    ("mks", RHYTHM, "SHORT BLIP", "PLUCK KEYS", dict(VOLUME=127, PULSE=2, SAW=0, PW=20, CHORUS=0, HPF=1, CUTOFF=70, RESO=12, VCF_ENV=40, VCF_KEY=60, T2=30, L2=0, T3=30, L3=0, T4=14)),
    ("mks", RHYTHM, "SOFT KEY", "PLUCK KEYS", dict(VOLUME=127, PULSE=1, SAW=0, SUB=2, CHORUS=1, HPF=0, CUTOFF=56, RESO=0, VCF_ENV=20, T2=60, L2=70, T3=60, L3=50, T4=40)),
    ("jp6", RHYTHM, "PLAIN LEAD", "SYNPOP KEY", dict(VCO1_PULSE=0, VCO1_SAW=1, VCO2_PULSE=0, MIX=0, PWM_DEPTH=0, CUTOFF=70, VCF_ENV=26, ENV1_D=56, ENV1_S=60, ENV2_D=80, ENV2_S=90, ENV2_R=30, CHORUS=0)),
    ("jp6", RHYTHM, "PLAIN PLK", "SYNPOP KEY", dict(VOLUME=127, VCO2_PULSE=0, MIX=0, PWM_DEPTH=0, CUTOFF=58, VCF_ENV=50, ENV1_D=44, ENV1_S=0, ENV2_D=52, ENV2_S=0, ENV2_R=26, CHORUS=0)),
    ("jp6", RHYTHM, "BLIP", "SYNPOP KEY", dict(VOLUME=127, VCO2_PULSE=0, MIX=0, PWM_DEPTH=0, CUTOFF=74, VCF_ENV=34, ENV1_D=30, ENV1_S=0, ENV2_D=34, ENV2_S=0, ENV2_R=18, CHORUS=0)),
    # ---- STRINGS: pads --------------------------------------------------------
    ("mks", STRINGS, "SOFT PAD", "WARM PAD", dict(PULSE=2, SAW=2, VCF_LFO=0, DCO_LFO=0, CUTOFF=54, RESO=4, VCF_ENV=20, T1=78, L1=127, T2=80, L2=112, T3=76, L3=104, T4=90)),
    ("mks", STRINGS, "SLOW PAD", "WARM PAD", dict(PULSE=1, SAW=3, VCF_LFO=0, DCO_LFO=0, CUTOFF=50, RESO=6, VCF_ENV=30, T1=90, L1=127, T2=90, L2=114, T3=84, L3=106, T4=100)),
    ("mks", STRINGS, "AIR PAD", "WARM PAD", dict(PULSE=0, SAW=3, HPF=3, VCF_LFO=0, DCO_LFO=0, CUTOFF=78, RESO=0, VCF_ENV=12, T1=88, L1=127, T2=86, L2=114, T3=80, L3=106, T4=100)),
    ("mks", STRINGS, "PLAIN STR", "STRINGS", dict(DCO_LFO=0, DCO_AFTER=0, VCF_AFTER=0, VCA_AFTER=0, LFO_DELAY=0, T1=64, T4=66)),
    ("jp6", STRINGS, "SOFT PAD 6", "JUPI PAD", dict(PWM_DEPTH=0, VCO1_PULSE=0, CUTOFF=60, RESO=8, VCF_ENV=16, ENV1_A=84, ENV1_S=70, ENV2_A=84, ENV2_R=100)),
    ("jp6", STRINGS, "SLOW PAD 6", "JUPI PAD", dict(PWM_DEPTH=0, VCO1_PULSE=0, CUTOFF=56, RESO=6, VCF_ENV=24, ENV1_A=108, ENV1_S=80, ENV1_R=110, ENV2_A=104, ENV2_R=112)),
    ("jp6", STRINGS, "AIR PAD 6", "JUPI PAD", dict(PWM_DEPTH=0, VCO1_PULSE=0, HPF=44, CUTOFF=88, RESO=0, VCF_ENV=10, ENV1_A=90, ENV2_A=90)),
    ("jp6", STRINGS, "PLAIN STR6", "JP STRINGS", dict(VCO_LFO=0, DRIFT=8, CHORUS=1)),
]


def build():
    out = []
    for engine, area, name, base, changes in SOUNDS:
        names, source = (MKS_NAMES, MKS) if engine == "mks" else (JP_NAMES, JP)
        v = list(source[base])
        for key, value in changes.items():
            v[names.index(key)] = value
        assert len(name) <= 10, name
        out.append((engine, area, name, v))
    return out


def main():
    sounds = build()
    lines = ["/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception",
             " * Generated by tools/extra_presets.py (docs/adr/0026): FM1-Electro's own sounds,",
             " * after the factory sounds of both engines. Do not edit by hand. */",
             '#include "synth.h"', "",
             "const syn_extra_t syn_extras[] = {"]
    for engine, area, name, v in sounds:
        pad = ", ".join(str(x) for x in v)
        lines.append(f'    {{"{name}", {area}, {"SYN_MKS" if engine == "mks" else "SYN_JP6"}, {{{pad}}}}},')
    lines += ["};", "const unsigned syn_extra_count = sizeof syn_extras / sizeof syn_extras[0];", ""]
    (ROOT / "engine" / "extra_presets.c").write_text("\n".join(lines))
    print(len(sounds), "sounds:", ", ".join(f"{s[2]}" for s in sounds))


if __name__ == "__main__":
    main()
