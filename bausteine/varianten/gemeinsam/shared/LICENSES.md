# Lizenzen und Herkunft: `shared/`

Alles hier steht unter der GNU General Public License, Version 3 (`GPL-3.0-only`, Text in `../LICENSE`), außer wo
unten anders angegeben.

| Was | Dateien | Herkunft | Lizenz |
|---|---|---|---|
| Konfiguration | `syn_config.h` | aus `mks50/engine/mk_config.h` (14.11.2026), dort nach FM1-Electro `engine/el_config.h` (github.com/robtron-byte/FM1-Electro-one, Commit `e7b3b14`) | GPL-3.0-only |
| DSP-Bausteine | `dsp/syn_dsp.h`, `dsp/syn_dsp_inline.h`, `dsp/syn_chop_inline.h` (ChopShape aus `DCO.swift`) | unverändert aus `mks50/engine/mks.c` (14.11.2026), dort aus FM1-Electro (`e7b3b14`, `engine/mks.c`), einem Port der DSP der MKS-50-iOS-App des Nutzers (github.com/robtron-byte/Mks50-iOS-: `DSPMath.swift`, `Envelope.swift`, `LadderFilter.swift`, `Chorus.swift`, `HardwareCurves.swift`) | dort GPL-3.0-only mit FM1 Opens Linking-Erlaubnis, hier GPL-3.0-only |
| Delay und Hall | `fx/syn_fx.c`, `fx/syn_fx.h` | bis 14.11.2026 `mks50/engine/mks_fx.*`: Port der Effekte der MKS-50-iOS-App des Nutzers (Commit `080db22`, `Shared/DSP/Effects.swift`, Kurven aus `SynthEngine.swift`) | GPL-3.0-only |
| Pixelschriften | `ui/syn_font.h` (`tools/make_font.py`) | 5×7: die klassische Zeichen-LCD-Schrift (gemeinfreie Tabelle); 3×5: eigene | GPL-3.0-only |
| Mathe-Hilfen | `third_party/fm1_open/dsp_math.h` | m-vave/fm1_open `third_party/synth` über FM1-Electro; Copyright (c) 2014 Emilie Gillet (SVF), 2026 M-VAVE | MIT (Text unten) |

## MIT-Lizenz (`dsp_math.h`)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
