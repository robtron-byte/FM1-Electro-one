#!/usr/bin/env python3
"""Referenz für den Node-Test des Web-Installers: rechnet mit platform/tools/fm1_install.py (unverändert
importiert) die Werte aus, die web/installer/fm1ota.js und fm1pkg.js genauso liefern müssen.

  python3 web/installer/test/vectors.py > vectors.json

- je Paket in firmware/ (und je kaputter Abwandlung): Kennung, Prüfergebnis, SHA-256 des logischen Abbilds
- Antwort-Bytes für feste Leseanfragen, beide Abschluss-Adressen
- ein ganzer simulierter Installationslauf (Gerät: fake_device.Device, gleiche Logik wie in test.mjs):
  jede Nachricht, die der Installer schickt, als Hex
"""
import hashlib
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent.parent
sys.path.insert(0, str(ROOT / "platform" / "tools"))
import fm1_install as fi  # noqa: E402

for k in fi.DELAY:
    fi.DELAY[k] = 0.0
fi.DELAY.update({"hs": 0.05, "poll": 0.01, "wait_loader": 1.0, "wait_reboot": 1.0, "idle_check": 0.5, "idle_write": 0.5})


def variants(raw):
    """dieselben Abwandlungen wie in test.mjs"""
    mid = len(raw) // 2
    flip = bytearray(raw)
    flip[mid] ^= 0x01
    head = bytearray(raw)
    head[0x10] ^= 0x01
    return {"ok": raw, "flip_mid": bytes(flip), "flip_head": bytes(head), "cut": raw[:len(raw) - 1000],
            "short": raw[:500]}


def verdict(raw):
    try:
        product = fi.product_of(raw)
    except fi.InstallError:
        return {"ok": False}
    import re
    if not re.fullmatch(r"[^_]+_\d+", product) or fi.LOADER_MARK not in raw:
        return {"ok": False, "product": product}
    image, n = fi.deinterleave(raw)
    bad = fi.package_damage(image)
    if bad:
        return {"ok": False, "product": product, "damage": bad}
    return {"ok": True, "product": product, "blocks": n, "image_sha": hashlib.sha256(image).hexdigest(),
            "image_len": len(image)}


# ------------------------------------------------------------- Simulation ---

class Device:
    """ein FM-1 im Test: läuft mit `ident`; UPGRADE startet die Leseanfragen. Gleiche Logik wie FakeDevice in test.mjs."""

    def __init__(self, ident, product, image_len, unplug_at=None):
        self.ident, self.product, self.image_len, self.unplug_at = ident, product, image_len, unplug_at
        self.out = []          # was das Gerät schickt (an den Installer)
        self.reqs, self.mode = [], None
        self.lost = False

    @staticmethod
    def identity(text):
        d = bytearray([0x00, 0x59, 0x11, 0, 0, 0]) + text.encode().ljust(27, b"\0") + b"\0"
        return b"\xF0" + fi.pack7(bytes(d)) + b"\xF7"

    @staticmethod
    def request(fl, addr, n):
        u = bytearray([0x00, 0x59, 0x30, 8, 0, 0, fl]) + addr.to_bytes(4, "little") + n.to_bytes(3, "little")
        u.append(~sum(u[6:14]) & 0xFF)
        return b"\xF0" + fi.pack7(bytes(u)) + b"\xF7"

    def plan(self, loader):
        if not loader:
            return [(0, a, 512) for a in (0, 512, 4096, 8192)] + [(0, fi.FINISH_CHECK, 8)]
        out, a, k = [], 0, 0
        while a < self.image_len:
            n = min(512, self.image_len - a)
            out.append((k & 1, a, n))
            a += n
            k += 1
        return out + [(0, fi.FINISH_WRITE, 8)]

    def receive(self, pkt):
        if self.lost:
            return False
        if pkt == fi.HS_QUERY:
            self.out.append(self.identity(self.ident))
        elif pkt == fi.UPGRADE:
            self.reqs = self.plan(self.ident.startswith("ota-"))
            self.next()
        elif self.reqs is not None and pkt[:1] == b"\xF0":
            if self.unplug_at is not None and self.ident.startswith("ota-") and len(self.reqs) == self.unplug_at:
                self.lost = True
                self.out.clear()
                return True
            done = self.reqs.pop(0) if self.reqs else None
            if done and done[1] == fi.FINISH_CHECK:
                self.ident, self.reqs = "ota-" + self.ident, None
            elif done and done[1] == fi.FINISH_WRITE:
                self.ident, self.reqs = self.product, None
            else:
                self.next()
        return True

    def next(self):
        if self.reqs:
            self.out.append(self.request(*self.reqs[0]))


class Link:
    def __init__(self, dev, log):
        self.dev, self.log = dev, log

    @property
    def lost(self):
        return self.dev.lost

    def send(self, pkt):
        pkt = bytes(pkt)
        self.log.append(pkt.hex())
        return self.dev.receive(pkt)

    def read(self, timeout):
        return self.dev.out.pop(0) if self.dev.out else None

    def drain(self):
        self.dev.out.clear()

    def close(self):
        pass


class Backend:
    def __init__(self, dev, log):
        self.dev, self.log = dev, log

    def input_names(self):
        return [] if self.dev.lost else ["FM-1"]

    def output_names(self):
        return self.input_names()

    def open(self, i, o):
        self.log.append("open")
        return Link(self.dev, self.log)


def simulate(ident, product, image, unplug_at=None):
    dev = Device(ident, product, len(image), unplug_at)
    log = []
    be = Backend(dev, log)
    up = fi.Updater(be)
    steps = []
    try:
        d = up.find()
        if not d:
            return {"error": "notfound", "log": log, "steps": steps}
        res = up.install(d, image, product, lambda k, a=None: steps.append([k, a]))
        return {"result": res, "log": log, "steps": steps}
    except fi.InstallError as e:
        return {"error": e.code, "log": log, "steps": steps}


def main():
    out = {"packages": {}, "responses": [], "sim": {}}
    pkgs = sorted((ROOT / "firmware").glob("*.fwsc"))
    for f in pkgs:
        raw = f.read_bytes()
        out["packages"][f.name] = {k: verdict(v) for k, v in variants(raw).items()}
    good = next(f for f in pkgs if out["packages"][f.name]["ok"]["ok"])
    product, image = fi.load_package(str(good), False)
    for fl, addr, n in [(0, 0, 512), (1, 512, 512), (0, len(image) - 100, 100), (1, 0x40, 0x50), (0, 7, 1)]:
        out["responses"].append({"fl": fl, "addr": addr, "n": n, "hex": fi.response(addr, image[addr:addr + n], fl).hex()})
    for addr in (fi.FINISH_CHECK, fi.FINISH_WRITE):
        out["responses"].append({"finish": addr, "hex": fi.response(addr, b"success\0").hex()})
    out["sim_package"] = good.name
    other = "FM-1_965" if product != "FM-1_965" else "FM-1_941"
    out["sim"]["normal"] = simulate(other, product, image)
    out["sim"]["resume"] = simulate("ota-" + other, product, image)
    out["sim"]["model"] = simulate("SMK-37 Pro_016", product, image)
    out["sim"]["foreign"] = simulate("ota-FM-1_015", product, image)
    out["sim"]["unplug"] = simulate(other, product, image, unplug_at=300)
    json.dump(out, sys.stdout, indent=1)


if __name__ == "__main__":
    main()
