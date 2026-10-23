#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Build the browser panel simulator: embed build/web/fm1sim.wasm into
host/web/index.html. Usage: build_web.py <wasm> <template> <output.html>"""
import base64
import sys

wasm, template, out = sys.argv[1:4]
page = open(template, encoding="utf-8").read()
data = base64.b64encode(open(wasm, "rb").read()).decode("ascii")
assert "%%WASM%%" in page
open(out, "w", encoding="utf-8").write(page.replace("%%WASM%%", data))
print(f"{out}: {len(page) + len(data)} bytes")
