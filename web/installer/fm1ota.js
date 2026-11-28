// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// Das USB-MIDI-Update des FM-1 (M-UPGRADE) über Web MIDI.
// Übernommen aus Felucca web/fm1ota.js (github.com/hugelton/Felucca) und an
// platform/tools/fm1_install.py dieses Repositorys angeglichen (FM1-Electro-one, ADR 0054):
// Port-Suche und Paarung wie PORT_RE/pair_output, Weiterschreiben nur im eigenen Lader
// (OWN_LOADER_RE), Modellprüfung same_keyboard, Fortschritt in Prozent, Wartezeiten einstellbar.
//
// Schritt 1: die laufende Firmware liest Teile des Pakets und startet den Update-Lader.
// Schritt 2: der Lader liest das ganze Abbild und schreibt es (nur den Programmbereich).
// In beiden Schritten fragt das Gerät (SysEx-Leseanfragen) und wir antworten.

export const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
export const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
// cmd 0x12 (Erweiterung FM1-Electro-one, ADR 0054): Version und Build-Datum der laufenden App; ältere Firmwares schweigen
export const INFO_QUERY = [0xF0, 0x00, 0x32, 0x49, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
export const FINISH_CHECK = 0xE0000000, FINISH_WRITE = 0xF0000000;
export const MAXDATA = 512;
export const PORT_RE = /fm-1|smk|elite|mke-p37|starry|felucca|sloop|clipper|melodee|fomni|x0x|makkuro|maku|rainbow|snoop|vector|vox|ota|composite|sinco|usb-midi/i;
export const OWN_LOADER_RE = /^ota-(FM-1|SMK-37 Pro|SMK-37 Elite|MKE-P37)_9\d\d$/i;
const SMK_FAMILY = /^(SMK-37[ +].*|MKE-P37|STARRYKEY-37.*)$/i;

// Millisekunden, wie DELAY in fm1_install.py; der Test verkürzt sie
export const DELAY = {
  open: 300, start: 2000, reply: 10, loader: 3000, reboot: 3000, retry: 1000,
  poll: 1000, hs: 1000, info: 400, idle_check: 8000, idle_write: 180000, wait_loader: 30000, wait_reboot: 40000,
};

export class InstallError extends Error {
  constructor(code, msg, detail = "") { super(msg); this.code = code; this.detail = detail; }
}

// ------------------------------------------------------------ Leitungsformat ---

export function pack7(data) {
  const out = []; let acc = 0, nb = 0;
  for (const b of data) {
    acc |= b << nb; nb += 8;
    while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; }
  }
  if (nb) out.push(acc & 0x7F);
  return out;
}

export function unpack7(s) {
  const out = []; let acc = 0, nb = 0;
  for (const b of s) {
    acc |= (b & 0x7F) << nb; nb += 7;
    while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; }
  }
  return out;
}

export function response(addr, data, fl = 0) {
  const n = data.length;
  const body = [0x00, 0x59, 0x30, (n + 8) & 0xFF, ((n + 8) >> 8) & 0xFF, 0, fl,
    addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF,
    n & 0xFF, (n >> 8) & 0xFF, 0];
  for (let i = 0; i < n; i++) body.push(data[i]);
  let s = 0;
  for (let i = 6; i < body.length; i++) s += body[i];
  body.push(~s & 0xFF);
  return [0xF0, ...pack7(body), 0xF7];
}

export function parseRequest(pkt) {
  if (pkt.length < 2 || pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const u = unpack7(pkt.slice(1, -1));
  if (u.length !== 15 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== 0x30) return null;
  let s = 0;
  for (let i = 6; i < 14; i++) s += u[i];
  if ((~s & 0xFF) !== u[14]) return null;
  return { fl: u[6], addr: (u[7] | (u[8] << 8) | (u[9] << 16) | (u[10] << 24)) >>> 0, len: u[11] | (u[12] << 8) | (u[13] << 16) };
}

export function parseIdentity(pkt) {
  if (pkt.length < 2 || pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const d = unpack7(pkt.slice(1, -1));
  if (d.length !== 34 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== 0x11) return null;
  let txt = "";
  for (let i = 6; i < 33 && d[i]; i++) txt += String.fromCharCode(d[i]);
  const m = /^([^_]+)_(\d+)$/.exec(txt);
  return m ? { text: txt, model: m[1], version: parseInt(m[2], 10), loader: /^ota-/i.test(m[1]) } : null;
}

// Antwort auf INFO_QUERY -> { version, date } oder null
export function parseInfo(pkt) {
  if (pkt.length < 2 || pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const d = unpack7(pkt.slice(1, -1));
  if (d.length < 7 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== 0x12) return null;
  const n = d[3] | (d[4] << 8) | (d[5] << 16);
  if (d.length !== n + 7) return null;
  let s = 0;
  for (let i = 6; i < 6 + n; i++) s += d[i];
  if ((~s & 0xFF) !== d[6 + n]) return null;
  const [version = "", date = ""] = String.fromCharCode(...d.slice(6, 6 + n)).split("\0");
  if (!/^\d+\.\d+\.\d+/.test(version)) return null;
  return { version, date: /^\d{4}-\d{2}-\d{2}$/.test(date) ? date : "" };
}

export const modelOf = (text) => text.split("_")[0].replace(/^ota-/i, "");

export function sameKeyboard(a, b) {
  const ma = modelOf(a), mb = modelOf(b);
  return ma === mb || (SMK_FAMILY.test(ma) && SMK_FAMILY.test(mb));
}

export function pairOutput(name, outs) {
  if (outs.includes(name)) return name;
  const base = name.replace(/\s+\d+$/, "");
  const same = outs.filter((o) => o.replace(/\s+\d+$/, "") === base);
  if (same.length === 1) return same[0];
  return outs.length === 1 ? outs[0] : null;
}

// ----------------------------------------------------------------- Web MIDI ---

// ein MIDI-Ein-/Ausgangspaar mit SysEx-Warteschlange
class WebMidiLink {
  constructor(access, input, output) {
    this.access = access; this.input = input; this.output = output; this.q = []; this.waiter = null;
    input.onmidimessage = (ev) => {
      if (ev.data[0] !== 0xF0) return;
      this.q.push(Array.from(ev.data));
      if (this.waiter) { const w = this.waiter; this.waiter = null; w(); }
    };
  }
  get lost() {
    return this.input.state === "disconnected" || this.output.state === "disconnected" ||
      ![...this.access.inputs.values()].some((p) => p.id === this.input.id && p.state !== "disconnected");
  }
  send(bytes) { try { this.output.send(bytes); return true; } catch (_) { return false; } }
  drain() { this.q.length = 0; }
  async read(ms) {
    if (!this.q.length) {
      let tm;
      await Promise.race([new Promise((r) => { this.waiter = r; }), new Promise((r) => { tm = setTimeout(r, Math.max(ms, 0)); })]);
      clearTimeout(tm);
      this.waiter = null;
    }
    return this.q.shift() || null;
  }
  close() { this.input.onmidimessage = null; }
}

// die Schnittstelle, die Updater erwartet (wie MidoBackend in fm1_install.py)
export class WebMidiBackend {
  constructor(access) { this.access = access; }
  live(ports) { return [...ports.values()].filter((p) => p.state !== "disconnected"); }
  inputNames() { return [...new Set(this.live(this.access.inputs).map((p) => p.name || ""))]; }
  outputNames() { return [...new Set(this.live(this.access.outputs).map((p) => p.name || ""))]; }
  async open(inName, outName) {
    const input = this.live(this.access.inputs).find((p) => p.name === inName);
    const output = this.live(this.access.outputs).find((p) => p.name === outName);
    if (!input || !output) throw new Error("port gone");
    await input.open(); await output.open();
    return new WebMidiLink(this.access, input, output);
  }
}

// ------------------------------------------------------------------ Updater ---

const realSleep = (ms) => new Promise((r) => setTimeout(r, ms));

export class Updater {
  // opts: { delay, sleep, now } (der Test setzt eigene)
  constructor(backend, opts = {}) {
    this.backend = backend;
    this.delay = { ...DELAY, ...(opts.delay || {}) };
    this.sleep = opts.sleep || realSleep;
    this.now = opts.now || (() => Date.now());
  }

  async handshake(link, tries = 3) {
    link.drain();
    for (let t = 0; t < tries; t++) {
      if (!link.send(HS_QUERY)) return null;
      const end = this.now() + this.delay.hs;
      let left;
      while ((left = end - this.now()) > 0) {
        const p = await link.read(left);
        if (!p) break;
        const id = parseIdentity(p);
        if (id) return id;
      }
    }
    return null;
  }

  // Version und Build-Datum der laufenden Firmware (cmd 0x12), null bei Firmwares ohne diese Erweiterung
  async info(link) {
    link.drain();
    if (!link.send(INFO_QUERY)) return null;
    const end = this.now() + this.delay.info;
    let left;
    while ((left = end - this.now()) > 0) {
      const p = await link.read(left);
      if (!p) break;
      const i = parseInfo(p);
      if (i) return i;
    }
    return null;
  }

  // der erste passende Port, der auf die Kennungsabfrage antwortet (und want(id) erfüllt)
  async find(want = null) {
    const outs = this.backend.outputNames();
    for (const name of this.backend.inputNames().filter((n) => PORT_RE.test(n))) {
      const out = pairOutput(name, outs);
      if (!out) continue;
      let link;
      try { link = await this.backend.open(name, out); } catch (_) { continue; }
      await this.sleep(this.delay.open);      // CoreMIDI verliert sonst die erste Nachricht
      const id = await this.handshake(link, 2);
      if (id && (!want || want(id))) return { link, id, name };
      link.close();
    }
    return null;
  }

  async waitFor(want, ms) {
    const end = this.now() + ms;
    while (this.now() < end) {
      const dev = await this.find(want);
      if (dev) return dev;
      await this.sleep(this.delay.retry);
    }
    return null;
  }

  // Leseanfragen beantworten, bis das Gerät nach `finish` fragt -> { served, finished, lost }
  async serve(link, image, finish, idle, progress) {
    let served = 0, last = this.now();
    for (;;) {
      const pkt = await link.read(this.delay.poll);
      if (!pkt) {
        if (link.lost) return { served, finished: false, lost: true };
        if (this.now() - last > idle) return { served, finished: false, lost: false };
        continue;
      }
      const r = parseRequest(pkt);
      if (!r) continue;
      last = this.now();
      if (r.addr === FINISH_CHECK || r.addr === FINISH_WRITE) {
        link.send(response(r.addr, [0x73, 0x75, 0x63, 0x63, 0x65, 0x73, 0x73, 0]));   // "success\0"
        if (r.addr === finish) return { served, finished: true, lost: false };
        continue;
      }
      if (r.len > MAXDATA || r.addr + r.len > image.length) {
        throw new InstallError("badreq", `das Gerät fragte nach 0x${r.addr.toString(16)}+${r.len}, außerhalb des Pakets`);
      }
      await this.sleep(this.delay.reply);
      if (!link.send(response(r.addr, image.subarray(r.addr, r.addr + r.len), r.fl))) {
        return { served, finished: false, lost: true };
      }
      served++;
      progress(served, r.addr + r.len);
    }
  }

  async check(dev, image, step) {
    step("start", dev.id.text);
    dev.link.send(UPGRADE);
    await this.sleep(this.delay.start);
    const s = await this.serve(dev.link, image, FINISH_CHECK, this.delay.idle_check, (k) => step("check", k));
    dev.link.close();
    if (!s.finished) throw new InstallError(s.lost ? "lost" : "stopped", `nach ${s.served} Anfragen abgebrochen; nichts wurde geschrieben`);
    step("loader");
    await this.sleep(this.delay.loader);
    const ota = await this.waitFor((i) => i.loader, this.delay.wait_loader);
    if (!ota) throw new InstallError("noloader", "der Update-Lader erschien nicht");
    return ota;
  }

  async write(ota, image, step) {
    step("write", 0);
    ota.link.send(UPGRADE);
    await this.sleep(this.delay.start);
    const s = await this.serve(ota.link, image, FINISH_WRITE, this.delay.idle_write,
      (k, end) => step("write", Math.min(99, Math.floor(end * 100 / image.length))));
    ota.link.close();
    if (!s.finished) throw new InstallError(s.lost ? "lost" : "stopped", `der Lader brach nach ${s.served} Anfragen ab`);
    step("write", 100);
  }

  async verify(product, step) {
    step("reboot");
    await this.sleep(this.delay.reboot);
    const back = await this.waitFor((i) => !i.loader, this.delay.wait_reboot);
    if (!back) throw new InstallError("noreturn", "das FM-1 kam nicht zurück");
    back.link.close();
    if (back.id.text !== product) throw new InstallError("mismatch", `geschrieben, aber das FM-1 meldet ${back.id.text}`, back.id.text);
    step("done", back.id.text);
    return back.id.text;
  }

  // laufende Firmware -> Lader -> neue Firmware; dev aus find(). Steht das Gerät schon im
  // Update-Modus (eigener Lader), wird nur noch geschrieben.
  async install(dev, image, product, step = () => {}) {
    if (!sameKeyboard(dev.id.text, product)) {
      dev.link.close();
      throw new InstallError("model", `das Gerät ist ${dev.id.text}, das Paket ist für ${product}`);
    }
    if (dev.id.loader && !OWN_LOADER_RE.test(dev.id.text)) {
      dev.link.close();
      throw new InstallError("foreign", `das FM-1 steht im Update-Modus einer anderen Firmware (${dev.id.text})`);
    }
    const ota = dev.id.loader ? dev : await this.check(dev, image, step);
    await this.write(ota, image, step);
    return this.verify(product, step);
  }
}
