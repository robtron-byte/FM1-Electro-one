#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build FM1-Electro on its own platform (platform/, Dinghy / Felucca's, docs/adr/0021).

  JIELI_TOOLCHAIN=... AC79_SDK=... python3 tools/build_platform.py [--release X.Y]

Uses platform/tools/build.py as it is (the update loader, the checks, the package) and replaces only the app's
compile: fm1/el_app.c (one unit with platform.c, built as the platform's apps are, -Os) and FM1-Electro's engine, UI
and store (-O2, the FPU on). Output: platform/build/fm1-electro.fwsc, identity FM-1_96N."""
import importlib.util
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("dinghy_build", ROOT / "platform" / "tools" / "build.py")
b = importlib.util.module_from_spec(spec)
sys.modules["dinghy_build"] = b
spec.loader.exec_module(b)

b.PRODUCT = "FM-1_960"
b.BOARD_PREFIX["fm1"] = "FM-1_96"
b.SCREEN_NAME = "FM1-ELECTRO"
b.SCREEN_COLOUR = 0xFD20          # orange, as the BEAT tab
b.DATA_FAMILY = "ELECTRO"

FPU = ["-mcpu=r3", "-mfprev1", "-ffp-contract=off", "-cl-single-precision-constant"]
ENGINE = sorted((ROOT / "engine").glob("*.c")) + sorted((ROOT / "ui").glob("*.c")) + [ROOT / "fm1" / "el_store.c",
                                                                                     ROOT / "fm1" / "el_glue.c"]
INC = ["-I" + str(ROOT / d) for d in ("engine", "ui", "fm1", "third_party/daisy808", "sdk_include")]


def build_app():
    out, fw = b.OUT, b.FW
    tc = b.toolchain()
    flags = [*b.CFLAGS, *b.BOARD_DEFS[b.BOARD], "-Ifirmware/hal", "-Ifirmware/src", "-Ifirmware/gen",
             f'-DFELUCCA_ID="{b.PRODUCT}"', f'-DAPP_VER="{b.VERSION or "0.0"}"', "-DSTORE_MAGIC=0x43454C45u",
             *INC, "-DEL_PLATFORM=1"]
    eng = ["-O2", "-ffunction-sections", "-fdata-sections", "-fno-common", "-fno-builtin", *FPU, *INC,
           "-DEL_PLATFORM=1", "-isystem", str(tc / "pi32v2" / "include")]
    cmds = [("cc", "-c", fw / "crt0.S", "-o", out / "crt0.o"),
            ("cc", "-c", fw / "hal" / "fm1_vec.S", "-o", out / "fm1_vec.o"),
            ("cc", "-c", fw / "hal" / "fm1_isr.S", "-o", out / "fm1_isr.o"),
            ("cc", "-c", fw / "hal" / "fm1_cpu1.S", "-o", out / "fm1_cpu1.o"),
            ("cc", *flags, *FPU, "-isystem", str(tc / "pi32v2" / "include"), "-c", str(ROOT / "fm1" / "el_app.c"),
             "-o", out / "el_app.o")]
    objs = []
    for src in ENGINE:
        o = out / ("e_" + src.stem + ".o")
        cmds.append(("cc", *eng, "-c", str(src), "-o", o))
        objs.append(o)
    b.tc_all(*cmds)
    elf = out / "dinghy.elf"
    lib = tc / "pi32v2" / "lib" / "r3"
    b.tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", fw / "app.ld", out / "crt0.o", out / "fm1_vec.o",
         out / "fm1_isr.o", out / "fm1_cpu1.o", out / "el_app.o", *objs, "--start-group", str(lib / "libm.a"), str(lib / "libcompiler_rt.a"),
         "--end-group", "-o", elf, "-Map", out / "fm1-electro.map")
    return b.link_outputs(elf) if hasattr(b, "link_outputs") else finish(elf)


def finish(elf):
    """platform/tools/build.py's build_app after its link, unchanged."""
    out = b.OUT
    for sect in ("text.bin", "data.bin", "ramtext.bin", "c1text.bin"):
        (out / sect).unlink(missing_ok=True)
    *_, syms, dis, rt = b.tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, out / "text.bin"),
                                 ("common/bin/objcopy", "-O", "binary", "-j", ".data", elf, out / "data.bin"),
                                 ("common/bin/objcopy", "-O", "binary", "-j", ".ram_text", elf, out / "ramtext.bin"),
                                 ("common/bin/objcopy", "-O", "binary", "-j", ".c1_text", elf, out / "c1text.bin"),
                                 ("common/bin/objdump", "-t", elf),
                                 ("common/bin/objdump", "-d", elf),
                                 ("common/bin/objdump", "-d", "-j", ".ram_text", elf))
    (out / "dinghy.dis").write_text(dis)
    import re

    def symv(name):
        return int(re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M).group(1), 16)
    img = bytearray((out / "text.bin").read_bytes())
    for sect, lname in (("ramtext.bin", "_rt_load"), ("c1text.bin", "_c1_load"), ("data.bin", "_data_load")):
        load = symv(lname)
        blob = (out / sect).read_bytes() if (out / sect).exists() else b""
        if blob:
            if load - b.APP_XIP < len(img):
                raise SystemExit(f"{lname} overlaps the image")
            img += b"\xff" * (load - b.APP_XIP - len(img))
            img += blob
    img += b"\xff" * (-len(img) % 4)
    (out / "dinghy.bin").write_bytes(img)
    return bytes(img), syms, dis, rt


b.build_app = build_app
if __name__ == "__main__":
    os.chdir(ROOT / "platform")
    rc = b.main()
    pkg = sorted((b.OUT).glob("dinghy*.fwsc"), key=lambda p: p.stat().st_mtime)[-1]
    target = b.OUT / pkg.name.replace("dinghy", "fm1-electro")
    pkg.replace(target)
    print(f"FM1-Electro: {target}")
    sys.exit(rc)
