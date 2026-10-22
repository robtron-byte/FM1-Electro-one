#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Generate engine/jp6_presets.c from the JP-6 iOS app's FactoryPresets.swift
(defaults from Param.swift next to it).
Usage: convert_jp6_presets.py <jp6-ios/Shared/Model/FactoryPresets.swift> <output.c>"""
import os
import re
import sys

# engine/jp6.h order (enum jp6_param), as the app's Param names.
ORDER = [
    "vco1Tri", "vco1Saw", "vco1Pulse", "vco1Square", "vco1Range", "vco1CrossMod", "vcoSync",
    "vco2Tri", "vco2Saw", "vco2Pulse", "vco2Noise", "vco2Range", "vco2Tune", "vco2Fine",
    "pwmDepth", "pwmSource", "vcoLfoDepth", "vcoEnvDepth", "vcoModDest", "sourceMix", "hpfCutoff",
    "vcfMode", "vcfCutoff", "vcfResonance", "vcfEnvDepth", "vcfEnvSource", "vcfLfoDepth",
    "vcfKeyFollow", "vcfVelocity", "vcaLevel", "vcaLfoDepth", "vcaVelocity",
    "lfoRate", "lfoDelay", "lfoWave",
    "env1Attack", "env1Decay", "env1Sustain", "env1Release", "env1KeyFollow", "env1Polarity",
    "env2Attack", "env2Decay", "env2Sustain", "env2Release", "env2KeyFollow", "vco1CrossModEnv",
    "keyMode", "portaTime", "portaSwitch", "volume", "keyShift", "voiceDrift", "unisonDetune",
    "lfoSync", "lfoDivision", "lfoKeySync", "envRetrigger", "chorus", "chorusRate", "chorusMode",
    "distMode", "distDrive", "distTone", "distLevel", "distMix",
    "delayMix", "reverbMix", "chordIndex",
]

# App settings that are not part of the FM1 sound engine: performance
# controllers (no aftertouch / mod wheel), the app's own delay / reverb
# details (the firmware has its own), arpeggiator and sequencer.
IGNORED = re.compile(r"^(vcoAfterDepth|vcfAfterDepth|vcaAfterDepth|modSensitivity|benderRange|lfo2\w*|"
                     r"delay(?!Mix)\w*|reverb(?!Mix)\w*|arp\w*|seq\w*|tempo|midiChannel|stutter\w*)$")

# Sound areas of FM1-Electro: 1 rhythmic sounds, 2 strings/pads, 3 bass.
# Leads and the init tone are left out, as for the MKS-50 sounds.
AREA = {"zap": 1, "stab": 1, "sequence": 1, "fxPerc": 1, "keys": 1,
        "stringsBrass": 2, "pad": 2, "bass": 3}
BASS_NAMES = set()


def defaults(param_swift):
    text = open(param_swift, encoding="utf-8").read()
    body = text[text.index("var defaultValue: Int {"):]
    body = body[:body.index("var identifier")]
    out = {}
    for names, value in re.findall(r"case ([.\w, ]+):\s*return (\d+)", body):
        for name in re.findall(r"\.(\w+)", names):
            out[name] = int(value)
    return out


def calls(text):
    """Yields the argument text of every make(...) in the source table."""
    body = text[text.index("private static let source"):text.index("private static func make")]
    pos = 0
    while True:
        start = body.find("make(", pos)
        if start < 0:
            return
        depth, i = 0, start + 4
        while True:
            c = body[i]
            depth += c in "(["
            depth -= c in ")]"
            i += 1
            if depth == 0:
                break
        yield body[start + 5:i - 1]
        pos = i


def parse(text, base):
    for args in calls(text):
        m = re.match(r"\s*\.(\w+),\s*\"([^\"]+)\",\s*\[([^\]]*)\](.*)$", args, re.S)
        if not m:
            raise SystemExit("cannot parse: " + args[:60])
        category, name, tone, rest = m.groups()
        values = dict(base)
        # make() overrides these patch values (its argument defaults).
        values.update(keyMode=0, portaSwitch=0, portaTime=30, chordIndex=0, volume=100)
        settings = re.findall(r"\.(\w+):\s*(-?\d+)", tone)
        head = rest.split("extra:")[0]
        for key, arg in (("keyMode", "keyMode"), ("portaSwitch", "porta"), ("portaTime", "portaTime"),
                         ("chordIndex", "chord"), ("volume", "volume")):
            hit = re.search(rf"\b{arg}:\s*(\d+)", head)
            if hit:
                values[key] = int(hit.group(1))
        if "extra:" in rest:
            extra = rest.split("extra:")[1]
            extra = extra[:extra.index("]")]
            settings += re.findall(r"\.(\w+):\s*(-?\d+)", extra)
        for key, value in settings:
            if key in values and key in ORDER:
                values[key] = int(value)
            elif not IGNORED.match(key):
                raise SystemExit(f"{name}: unknown parameter .{key}")
        area = 3 if name in BASS_NAMES else AREA.get(category, 0)
        yield area, name.upper()[:10], category, [values[k] for k in ORDER]


def enum_count(header):
    text = open(header, encoding="utf-8").read()
    body = text[text.index("enum jp6_param {"):text.index("JP_COUNT")]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    return len(re.findall(r"\bJP_\w+", body))


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    source = sys.argv[1]
    base = {k: 0 for k in ORDER}
    base.update({k: v for k, v in defaults(os.path.join(os.path.dirname(source), "Param.swift")).items()
                 if k in base})
    header = os.path.join(os.path.dirname(os.path.abspath(sys.argv[2])), "jp6.h")
    if os.path.exists(header) and enum_count(header) != len(ORDER):
        raise SystemExit(f"jp6.h has {enum_count(header)} parameters, the converter {len(ORDER)}")
    presets = [p for p in parse(open(source, encoding="utf-8").read(), base) if p[0]]
    presets.sort(key=lambda p: p[0])
    out = ["/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception",
           " * Generated by tools/convert_jp6_presets.py from the JP-6 iOS app's factory",
           " * sounds. Do not edit by hand. Control task only (Flash). */",
           '#include "jp6.h"', "",
           "const jp6_preset_t jp6_presets[] = {"]
    for area, name, category, values in presets:
        out.append(f'    {{"{name}", {area}, {{{", ".join(str(v) for v in values)}}}}}, /* {category} */')
    out += ["};", "const unsigned jp6_preset_count = sizeof jp6_presets / sizeof jp6_presets[0];", ""]
    open(sys.argv[2], "w", encoding="utf-8").write("\n".join(out))
    counts = {a: sum(1 for p in presets if p[0] == a) for a in (1, 2, 3)}
    print(f"{len(presets)} presets: rhythm {counts[1]}, strings {counts[2]}, bass {counts[3]}")


if __name__ == "__main__":
    main()
