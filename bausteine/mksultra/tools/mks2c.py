#!/usr/bin/env python3
"""Wandelt die .mks-Presets der Vorlage in eine C-Tabelle (mksultra_presets.c).

    python3 tools/mks2c.py "<…/TAL Pha Nachbau/app/presets>" mksultra_presets.c

Liest die Dateien wie engine/src/preset.rs (parse_preset): Attribute des ersten <program>, Stimmungstabelle,
<Chord>, <voicetuning>. Zahlen werden als Text übernommen und mit Suffix f als C-Literal geschrieben, damit der
C-Compiler sie genau so auf float rundet wie Rusts `parse::<f32>()` (kein Umweg über double).
"""
import os
import re
import sys

PARAM_IDS = """Volume DcoEnvMode VcfEnvMode VcaEnvMode DcoWavePulse DcoWaveSaw DcoWaveSub DcoRange DcoSubLevel Noise
HpfCutoff DcoChorusSwitch DcoLfoMod DcoEnvMod DcoAtMod DcoPwmDepth DcoPwmRate VcfCutoff VcfResonance VcfLfoMod
VcfEnvMod VcfKeyFollow VcfAftertouch VcaLevel VcaAftertouch LfoRate LfoDelay EnvAttackTime EnvAttackLevel
EnvBreakTime EnvBreakLevel EnvDecayTime EnvSustainLevel EnvReleaseTime EnvKeyFollow ChorusRate BenderRange
MasterFineTune ModWheelVcfRange ModWheelDcoLfo ModWheelVcfLfo NumberOfVoices PortamentoMode PortamentoTime
LfoWaveform LfoSync LfoTrigger LfoInvert mpsEnabled mpeEnabled sysexEnabled resonanceBoost resonanceBoostFrequency
specialSawVolume specialPulseVolume specialPulseSync specialPulseTune specialPulseFineTune specialPulseDcoEnv
specialPulseDcoLfo UnisonoVoices UnisonoDetune UnisonoSpread RoundRobin Octave NoDcOffset arprate arprange arpmode
arpsyncmode reverbDryWet reverbSize reverbDelay reverbTone delayDryWet delayTime delaySync delaySpread delayTone
delayFeedback eqBellFreq eqBellQ eqBellGain paramPortamentoTimeMode portamentoChordOriginalMode""".split()
assert len(PARAM_IDS) == 85

# Rusts f32::from_str: Dezimalzahl, optional Exponent, oder inf/infinity/nan (ohne Groß-/Kleinschreibung)
RE_FLOAT = re.compile(r'^[+-]?(\d+\.?\d*([eE][+-]?\d+)?|\.\d+([eE][+-]?\d+)?)$')
RE_SPECIAL = re.compile(r'^[+-]?(inf|infinity|nan)$', re.I)
RE_INT = re.compile(r'^[+-]?\d+$')


def unescape(s):
    return (s.replace('&quot;', '"').replace('&apos;', "'").replace('&lt;', '<').replace('&gt;', '>')
            .replace('&#10;', '\n').replace('&amp;', '&'))


def element_attrs(xml, tag, start):
    """wie preset.rs element_attrs: (attrs, index nach '>') oder None"""
    pat = '<' + tag
    search = start
    while True:
        i = xml.find(pat, search)
        if i < 0:
            return None
        j = i + len(pat)
        if j >= len(xml):
            return None
        nxt = xml[j]
        if nxt.isspace() or nxt in '>/':
            break
        search = j
    i = j
    attrs = []
    n = len(xml)
    while True:
        while i < n and xml[i].isspace():
            i += 1
        if i >= n:
            return None
        if xml[i] == '>':
            return attrs, i + 1
        if xml[i] == '/':
            i += 1
            continue
        ns = i
        while i < n and xml[i] != '=' and not xml[i].isspace():
            i += 1
        name = xml[ns:i]
        while i < n and xml[i] not in '"\'':
            i += 1
        if i >= n:
            return None
        q = xml[i]
        i += 1
        vs = i
        while i < n and xml[i] != q:
            i += 1
        attrs.append((name, unescape(xml[vs:i])))
        i += 1


def root_attrs(xml):
    """wie preset.rs root_attrs: erstes Element, das nicht mit <? oder <! beginnt"""
    i = 0
    while True:
        s = xml.find('<', i)
        if s < 0 or s + 1 >= len(xml):
            return None
        if xml[s + 1] in '?!':
            i = s + 1
            continue
        m = re.match(r'[\w-]*', xml[s + 1:])
        return element_attrs(xml, m.group(0), s)


def f32_literal(text):
    """C-Literal für Rusts `text.parse::<f32>()`, None wenn Rust ablehnt."""
    if RE_SPECIAL.match(text):
        t = text.lower().lstrip('+')
        neg = t.startswith('-')
        if 'nan' in t:
            return '(-NAN)' if neg else 'NAN'
        return '(-INFINITY)' if neg else 'INFINITY'
    if not RE_FLOAT.match(text):
        return None
    t = text
    if not any(c in t for c in '.eE'):
        t += '.0'
    if t.endswith('.') or re.search(r'\.[eE]', t):
        t = t.replace('.', '.0', 1)
    if t.startswith('.') or t.startswith('-.') or t.startswith('+.'):
        t = t.replace('.', '0.', 1)
    return t + 'f'


def parse(xml):
    p = {'name': '', 'version': 0, 'params': [], 'tuning': [], 'chord': [], 'vt': []}
    root = root_attrs(xml)
    if root:
        for k, v in root[0]:
            if k == 'version':
                v2 = v.strip()
                p['version'] = int(v2) if RE_INT.match(v2) else 0
            elif k == 'presetName':
                p['name'] = v
    prog = element_attrs(xml, 'program', 0)
    if prog is None:
        return None
    attrs, body = prog
    for k, v in attrs:
        if k == 'programname':
            if not p['name']:
                p['name'] = v
            continue
        if k == 'path':
            continue
        lit = f32_literal(v.strip())
        if lit is not None:
            p['params'].append((k, lit))
    pos = body
    tt = xml.find('<tuningtable', body)
    if tt >= 0:
        pos = tt
        end = xml.find('</tuningtable>', pos)
        if end < 0:
            end = len(xml)
        q = pos
        while True:
            r = element_attrs(xml, 'entry', q)
            if r is None or r[1] > end:
                break
            for k, v in r[0]:
                if k == 'tuning':
                    p['tuning'].append(f32_literal(v) or '0.0f')
                    break
            q = r[1]
    ch = element_attrs(xml, 'Chord', pos)
    if ch:
        notes = [-1] * 6
        for k, v in ch[0]:
            if k.startswith('midiNote'):
                idx = k[len('midiNote'):]
                if re.match(r'^\d+$', idx) and int(idx) < 6:
                    notes[int(idx)] = int(v) if RE_INT.match(v) and -2**31 <= int(v) < 2**31 else -1
        p['chord'] = notes
    q = pos
    while True:
        r = element_attrs(xml, 'voicetuning', q)
        if r is None:
            break
        vt = dict(cutoff='0.5f', detune='0.5f', attack='0.5f', decay='0.5f', release='0.5f', pan='0.5f')
        for k, v in r[0]:
            if k in vt:
                vt[k] = f32_literal(v) or '0.5f'
        p['vt'].append(vt)
        q = r[1]
    return p


def cstr(s):
    out = []
    for b in s.encode('utf-8'):
        c = chr(b)
        if c in '\\"':
            out.append('\\' + c)
        elif 32 <= b < 127:
            out.append(c)
        else:
            out.append('\\%03o' % b)
    return '"' + ''.join(out) + '"'


def main():
    src, dst = sys.argv[1], sys.argv[2]
    files = []
    for root, _, names in os.walk(src):
        for n in names:
            if n.endswith('.mks'):
                files.append(os.path.relpath(os.path.join(root, n), src))
    files.sort()
    lines = ['/* SPDX-License-Identifier: GPL-3.0-only',
             ' * Erzeugt von tools/mks2c.py aus den %d Presets der Vorlage (presets/, alle .mks). Nicht von Hand ändern. */' % len(files),
             '#include "mksultra.h"', '#include <math.h>', '']
    entries = []
    for i, rel in enumerate(files):
        with open(os.path.join(src, rel), encoding='utf-8', errors='replace') as f:
            p = parse(f.read())
        if p is None:
            print('übersprungen (kein <program>):', rel, file=sys.stderr)
            continue
        have = [0, 0, 0]
        vals = ['0.0f'] * 85
        seen = set()
        for k, lit in p['params']:
            if k in PARAM_IDS and k not in seen:  # Preset::get nimmt das erste Vorkommen
                seen.add(k)
                idx = PARAM_IDS.index(k)
                have[idx // 32] |= 1 << (idx % 32)
                vals[idx] = lit
        tun = 'NULL'
        if p['tuning'] and p['tuning'] != ['%d.0f' % k for k in range(len(p['tuning']))]:
            lines.append('static const float tun%d[%d] = {%s};' % (i, len(p['tuning']), ', '.join(p['tuning'])))
            tun = 'tun%d' % i
        ntun = len(p['tuning']) if tun != 'NULL' else 0
        chord = p['chord'] or [-1] * 6
        vts = ', '.join('{%s, %s, %s, %s, %s, %s}' % (v['cutoff'], v['detune'], v['attack'], v['decay'], v['release'], v['pan'])
                        for v in p['vt'][:6]) or '{0}'
        entries.append('    {%s, %s, %d, {0x%xu, 0x%xu, 0x%xu},\n     {%s},\n     %d, %s, %d, {%s}, %d, {%s}},' % (
            cstr(rel.replace(os.sep, '/')), cstr(p['name']), p['version'], have[0], have[1], have[2],
            ', '.join(vals), ntun, tun, len(p['chord']), ', '.join(str(c) for c in chord), min(len(p['vt']), 6), vts))
    lines.append('')
    lines.append('const mksultra_preset_t mksultra_presets[] = {')
    lines += entries
    lines.append('};')
    lines.append('const int mksultra_preset_count = %d;' % len(entries))
    with open(dst, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print('%d Presets -> %s' % (len(entries), dst))


if __name__ == '__main__':
    main()
