#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Gerätebuild einer Solo- oder Kombi-Firmware aus bausteine/varianten/ (ADR 0044).

Nach fm1-synths mks50/tools/build.py (github.com/robtron-byte/fm1-synths, Commit 2a7b4fa): nutzt
gemeinsam/platform/tools/build.py, wie es ist (Update-Lader, Prüfungen, Paket), und ersetzt nur das Übersetzen der App:
die App-Datei der Variante (eine Einheit mit platform.c, -Os) und die Klangdateien (-O2, FPU an; ihre Objekte heißen
dsp_*.o und liegen laut app.ld im SRAM). Eine Variante ruft build(cfg) aus ihrem tools/build.py auf.

  JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 <variante>/tools/build.py

Auf dem Mac (Docker) über tools/mac_docker.py, das tut build() selbst. Der Build bricht ab, wenn der Link scheitert,
das Bild nicht neu ist oder das Protokoll etwas Undefiniertes nennt (FM1-Electro, docs/UEBERGABE.md, 24. Build)."""
import importlib.util
import io
import os
import platform as host
import subprocess
import sys
import time
from contextlib import redirect_stdout
from pathlib import Path

GEMEINSAM = Path(__file__).resolve().parents[1]
PLATFORM = GEMEINSAM / "platform"
SHARED = GEMEINSAM / "shared"
BAUSTEINE = GEMEINSAM.parents[1]
REPO = BAUSTEINE.parent
FPU = ["-mcpu=r3", "-mfprev1", "-ffp-contract=off"]


def build(cfg):
    """cfg: name (Dateiname, z. B. 'SH101'), root (Ordner der Variante), app (App-Datei), dsp (Klangdateien, SRAM),
    flash (übrige Dateien), inc (Include-Ordner), defs, product ('FM-1_9x0'), prefix ('FM-1_9x'), screen,
    colour (RGB565), family (≤7 Zeichen A-Z0-9), store_magic (uint32), single_const (Dateien mit
    -cl-single-precision-constant; Standard: alle außer den Bausteinen, die double brauchen)."""
    if host.system() == "Darwin" and os.environ.get("GM_IN_DOCKER_SHIM") != "1":
        os.environ["GM_IN_DOCKER_SHIM"] = "1"
        shim = GEMEINSAM / "tools" / "mac_docker.py"
        sys.exit(subprocess.call([sys.executable, str(shim), str(REPO), sys.argv[0], *sys.argv[1:]]))
    root = Path(cfg["root"])
    spec = importlib.util.spec_from_file_location("dinghy_build", PLATFORM / "tools" / "build.py")
    b = importlib.util.module_from_spec(spec)
    sys.modules["dinghy_build"] = b
    spec.loader.exec_module(b)
    b.PRODUCT = cfg["product"]
    b.BOARD_PREFIX["fm1"] = cfg["prefix"]
    b.SCREEN_NAME = cfg["screen"]
    b.SCREEN_COLOUR = cfg["colour"]
    b.DATA_FAMILY = cfg["family"]
    block = int(os.environ.get("GM_BLOCK", "64"))
    full_ver = (root / "VERSION").read_text().strip()
    # Build-Datum: beantwortet die Firmware auf die Info-Abfrage (cmd 0x12) zusammen mit der Version (ADR 0054)
    APP_DATE = os.environ.get("GM_APP_DATE") or time.strftime("%Y-%m-%d")
    if "--release" not in sys.argv:
        sys.argv += ["--release", full_ver]
    else:
        full_ver = sys.argv[sys.argv.index("--release") + 1]
    inc = ["-I" + str(d) for d in [*cfg.get("inc", []), GEMEINSAM / "app", SHARED, SHARED / "fx", SHARED / "ui",
                                   SHARED / "third_party" / "fm1_open", BAUSTEINE]]
    defs = ["-DEL_PLATFORM=1", f"-DSYN_BLOCK={block}", *cfg.get("defs", []),
            *os.environ.get("GM_EXTRA_CFLAGS", "").split()]
    double_ok = set(map(str, cfg.get("double", [])))   # Klangdateien, die double wie ihre Vorlage rechnen
    start = time.time()

    def build_app():
        out, fw = b.OUT, b.FW
        tc = b.toolchain()
        sysinc = ["-isystem", str(tc / "pi32v2" / "include")]
        flags = [*b.CFLAGS, *b.BOARD_DEFS[b.BOARD], "-Ifirmware/hal", "-Ifirmware/src", "-Ifirmware/gen",
                 f'-DFELUCCA_ID="{b.PRODUCT}"', f'-DAPP_VER="{full_ver}"', f'-DAPP_DATE="{APP_DATE}"', f"-DSTORE_MAGIC={cfg['store_magic']:#x}u",
                 *inc, *defs, "-DLCD_BAUD=1"]
        cmds = [("cc", "-c", fw / "crt0.S", "-o", out / "crt0.o"),
                ("cc", "-c", fw / "hal" / "fm1_vec.S", "-o", out / "fm1_vec.o"),
                ("cc", "-c", fw / "hal" / "fm1_isr.S", "-o", out / "fm1_isr.o"),
                ("cc", "-c", fw / "hal" / "fm1_cpu1.S", "-o", out / "fm1_cpu1.o"),
                ("cc", *flags, *FPU, "-cl-single-precision-constant", *sysinc, "-c", str(cfg["app"]),
                 "-o", out / "gm_app.o")]
        objs = []
        for prefix, group in (("dsp_", cfg.get("dsp", [])), ("r_", cfg.get("flash", []))):
            for src in map(Path, group):
                o = out / (prefix + src.stem + ".o")
                single = [] if str(src) in double_ok else ["-cl-single-precision-constant"]
                cmds.append(("cc", "-O2", "-ffunction-sections", "-fdata-sections", "-fno-common", "-fno-builtin",
                             *FPU, *single, *inc, *defs, *sysinc, "-c", str(src), "-o", o))
                objs.append(o)
        b.tc_all(*cmds)
        elf = out / "dinghy.elf"
        elf.unlink(missing_ok=True)
        lib = tc / "pi32v2" / "lib" / "r3"
        b.tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", fw / "app.ld", out / "crt0.o", out / "fm1_vec.o",
             out / "fm1_isr.o", out / "fm1_cpu1.o", out / "gm_app.o", *objs, "--start-group", str(lib / "libm.a"),
             str(lib / "libcompiler_rt.a"), "--end-group", "-o", elf, "-Map", out / "app.map")
        if not elf.exists() or elf.stat().st_mtime < start:
            raise SystemExit("build: der Link brachte keine neue dinghy.elf")
        return finish(b, elf)

    b.build_app = build_app
    os.chdir(PLATFORM)
    log = io.StringIO()

    class Tee:
        def write(self, s):
            sys.__stdout__.write(s)
            log.write(s)

        def flush(self):
            sys.__stdout__.flush()
    with redirect_stdout(Tee()):
        rc = b.main()
    bad = [w for w in ("undefined", "failed", "truncated") if w in log.getvalue().lower()]
    if rc or bad:
        raise SystemExit(f"build: fehlgeschlagen (rc {rc}, Protokoll: {', '.join(bad) or 'ok'})")
    pkg = sorted(b.OUT.glob("dinghy*.fwsc"), key=lambda p: p.stat().st_mtime)[-1]
    if pkg.stat().st_mtime < start:
        raise SystemExit("build: kein neues Paket")
    dest = root / "build"
    dest.mkdir(exist_ok=True)
    target = dest / f"{cfg['name']}-{full_ver}.fwsc"
    target.write_bytes(pkg.read_bytes())
    (dest / f"{cfg['name'].lower()}.fwsc").write_bytes(pkg.read_bytes())
    for n in ("dinghy.elf", "app.map", "dinghy.dis"):
        (dest / n).write_bytes((b.OUT / n).read_bytes())
    print(f"{cfg['name']} {full_ver}: {target} (Block {block})")
    return target


def finish(b, elf):
    """platform/tools/build.py build_app nach dem Link, mit .c1_text und .dsp_text (wie fm1-synths mks50)."""
    import re
    out = b.OUT
    for sect in ("text.bin", "data.bin", "ramtext.bin", "c1text.bin", "dsptext.bin"):
        (out / sect).unlink(missing_ok=True)
    *_, syms, dis, rt = b.tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, out / "text.bin"),
                                 ("common/bin/objcopy", "-O", "binary", "-j", ".data", elf, out / "data.bin"),
                                 ("common/bin/objcopy", "-O", "binary", "-j", ".ram_text", elf, out / "ramtext.bin"),
                                 ("common/bin/objcopy", "-O", "binary", "-j", ".c1_text", elf, out / "c1text.bin"),
                                 ("common/bin/objcopy", "-O", "binary", "-j", ".dsp_text", elf, out / "dsptext.bin"),
                                 ("common/bin/objdump", "-t", elf),
                                 ("common/bin/objdump", "-d", elf),
                                 ("common/bin/objdump", "-d", "-j", ".ram_text", elf))
    (out / "dinghy.dis").write_text(dis)

    def symv(name):
        return int(re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M).group(1), 16)
    img = bytearray((out / "text.bin").read_bytes())
    for sect, lname in (("ramtext.bin", "_rt_load"), ("c1text.bin", "_c1_load"), ("data.bin", "_data_load"),
                        ("dsptext.bin", "_dsp_load")):
        load = symv(lname)
        blob = (out / sect).read_bytes() if (out / sect).exists() else b""
        if blob:
            if load - b.APP_XIP < len(img):
                raise SystemExit(f"{lname} überlappt das Bild")
            img += b"\xff" * (load - b.APP_XIP - len(img))
            img += blob
    img += b"\xff" * (-len(img) % 4)
    (out / "dinghy.bin").write_bytes(img)
    return bytes(img), syms, dis, rt
