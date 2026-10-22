# Third-party components

| Component | Files | Source / license |
| --- | --- | --- |
| DSP math helpers | `third_party/daisy808/dsp_math.h`, `engine.h` (the 808 voices themselves are now FM1-Electro's own `engine/tr808.c`) | Taken unchanged from [m-vave/fm1_open](https://github.com/m-vave/fm1_open) `third_party/synth`. MIT. Copyright (c) 2020 Electrosmith, Corp, Emilie Gillet; 2026 M-VAVE. Based on [DaisySP](https://github.com/daisyaudio/DaisySP) and Mutable Instruments Plaits drum models, with the SVF from stmlib (MIT, Emilie Gillet). |
| Parameter descriptor | `third_party/daisy808/fm1_param.h` | From m-vave/fm1_open `app/synth`, GPL-3.0-only with the FM1 SDK linking exception. |
| Audio block format | `sdk_include/fm1_block.h` | From m-vave/fm1_open `sdk/include`, GPL-3.0-only with the FM1 SDK linking exception. |
| FM1 binary SDK | not included in this repository | The device build links `libfm1_engine.a` from m-vave/fm1_open under M-VAVE's separate binary SDK terms (see its `THIRD_PARTY.md`). |

## MIT license (808 drum model)

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
