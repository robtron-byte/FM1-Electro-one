#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Ein Plattform-Build-Skript (fm1-synths-Art: platform/tools/build.py mit Quellen außerhalb von platform/) auf dem Mac
laufen lassen. Der Build ruft die JieLi-Werkzeuge in Docker auf und hängt nur platform/ als /work ein; Quellen
außerhalb (shared/, engine/, bausteine/) sieht der Container dann nicht. Dieses Skript hängt stattdessen ein
gemeinsames Oberverzeichnis unter demselben Pfad ein, so stimmen auch absolute Pfade.

  python3 mac_docker.py <Oberverzeichnis> <build.py> [Argumente …]"""
import runpy
import subprocess
import sys

root, script, *rest = sys.argv[1:]
_run = subprocess.run


def run(cmd, *a, **k):
    if isinstance(cmd, list) and cmd[:2] == ["docker", "run"] and "/work" in " ".join(cmd):
        out = []
        i = 0
        while i < len(cmd):
            if cmd[i] == "-v" and cmd[i + 1].endswith(":/work"):
                src = cmd[i + 1][: -len(":/work")]
                out += ["-v", f"{root}:{root}"]
                i += 2
                continue
            if cmd[i] == "-w" and cmd[i + 1] == "/work":
                out += ["-w", k.get("cwd") and str(k["cwd"]) or src]
                i += 2
                continue
            if cmd[i] == "-v" and cmd[i + 1].endswith(":/opt/jieli:ro"):   # die Werkzeuge auch unter ihrem Pfad
                tcp = cmd[i + 1][: -len(":/opt/jieli:ro")]
                out += ["-v", cmd[i + 1], "-v", f"{tcp}:{tcp}:ro"]
                i += 2
                continue
            out.append(cmd[i])
            i += 1
        cmd = out
    return _run(cmd, *a, **k)


subprocess.run = run
sys.argv = [script, *rest]
runpy.run_path(script, run_name="__main__")
