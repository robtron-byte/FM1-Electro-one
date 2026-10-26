#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Copy FM1-Electro into an FM1 Open checkout, insert its two hooks and move
the parts of FM1 Open's app layer that FM1-Electro replaces out of the
build (fm1/el_sdk_app.c stands in for them; they would keep the SDK's own
synth engine, effects and buffers linked, about 100 KB of SRAM).
Usage: prepare_fm1.py <path to fm1_open>   (then run build.bat there)"""
import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCES = ["engine", "ui", "third_party/daisy808", "fm1"]
INCLUDE = '#include "electro/el_fm1.h"\n'
PATCHES = {
    "app/board/audio.c": [
        ('#include "audio_output.h"\n', '#include "audio_output.h"\n' + INCLUDE),
        ("fm1_engine_render(rendered, FM1_BLOCK_FRAMES)", "el_fm1_render(rendered, FM1_BLOCK_FRAMES)"),
    ],
    "app/midi_router.c": [
        ('#include <string.h>\n', '#include <string.h>\n' + INCLUDE),
        ("void fm1_app_midi_received(unsigned source, const uint8_t *m, size_t n)\n{\n",
         "void fm1_app_midi_received(unsigned source, const uint8_t *m, size_t n)\n{\n"
         "    if (el_fm1_midi(source, m, n)) return;\n"),
    ],
}


# FM1 Open's own screen, presets, projects, samples, FM library and project
# transfer: moved to <fm1_open>/fm1_electro_unused (outside app/, so the
# build skips them). Board drivers, audio output and the MIDI router stay.
UNUSED = ["screen.c", "presets.c", "project.c", "samples.c", "fm_factory.c", "fm_library.c", "transfer.c"]


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    target = pathlib.Path(sys.argv[1]).resolve()
    if not (target / "sdk" / "Build.ps1").is_file():
        sys.exit(f"{target} is not an FM1 Open checkout")
    for name, edits in PATCHES.items():
        path = target / name
        text = path.read_text(encoding="utf-8")
        if "el_fm1" in text:
            continue  # already prepared
        for old, new in edits:
            if text.count(old) != 1:
                sys.exit(f"{name}: hook point not found; this FM1 Open version is not supported")
            text = text.replace(old, new)
        path.write_text(text, encoding="utf-8", newline="")
    parked = target / "fm1_electro_unused"
    parked.mkdir(exist_ok=True)
    for name in UNUSED:
        src = target / "app" / name
        if src.exists():
            shutil.move(str(src), str(parked / name))
        elif not (parked / name).exists():
            sys.exit(f"app/{name} not found; this FM1 Open version is not supported")
    out = target / "app" / "electro"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir()
    for folder in SOURCES:
        for f in sorted((ROOT / folder).iterdir()):
            if f.suffix in (".c", ".h", ".inc"):
                shutil.copy2(f, out / f.name)
    print(f"FM1-Electro copied to {out}; hooks inserted; FM1 Open's own app parts moved to {parked}. "
          f"Build with build.bat in {target}.")


if __name__ == "__main__":
    main()
