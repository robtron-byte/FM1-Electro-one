// Test des Web-Installers ohne Gerät (ADR 0054): fm1ota.js + fm1pkg.js gegen platform/tools/fm1_install.py.
//   node web/installer/test/test.mjs
// Ruft test/vectors.py auf (Python, fm1_install.py unverändert importiert) und vergleicht:
// Paketprüfung je Paket in firmware/ samt kaputter Abwandlungen, Antwort-Bytes, und einen ganzen
// simulierten Installationslauf (jede gesendete Nachricht) inklusive Fehlerfälle.
import { execFileSync } from "node:child_process";
import { readFileSync, readdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { createHash } from "node:crypto";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as ota from "../fm1ota.js";
import { compare, sortList, newer, buttonFor, installedName, visible } from "../logik.js";
import { loadPackage, productOf, deinterleave, packageDamage, LOADER_MARK } from "../fm1pkg.js";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..", "..", "..");
const ref = JSON.parse(execFileSync("python3", [join(HERE, "vectors.py")], { maxBuffer: 1 << 28 }).toString());

let fails = 0, checks = 0;
function eq(a, b, what) {
  checks++;
  const sa = JSON.stringify(a), sb = JSON.stringify(b);
  if (sa !== sb) { fails++; console.log(`FEHLER ${what}\n  JS:     ${sa.slice(0, 300)}\n  Python: ${sb.slice(0, 300)}`); }
}
const hex = (a) => Buffer.from(a).toString("hex");
const sha = (a) => createHash("sha256").update(a).digest("hex");

// ------------------------------------------------------------ Pakete ---

function variants(raw) {
  const mid = raw.length >> 1;
  const flip = Uint8Array.from(raw); flip[mid] ^= 1;
  const head = Uint8Array.from(raw); head[0x10] ^= 1;
  return { ok: raw, flip_mid: flip, flip_head: head, cut: raw.subarray(0, raw.length - 1000), short: raw.subarray(0, 500) };
}

function verdict(raw) {
  let product;
  try { product = productOf(raw); } catch (_) { return { ok: false }; }
  let mark = false;
  try { loadPackage(raw); mark = true; } catch (e) { if (e.code !== "nomark" && e.code !== "badpkg") mark = true; }
  if (!/^[^_]+_\d+$/.test(product) || !mark) return { ok: false, product };
  const [image, n] = deinterleave(raw);
  const bad = packageDamage(image);
  if (bad) return { ok: false, product, damage: bad };
  // loadPackage muss dasselbe Abbild liefern
  const lp = loadPackage(raw);
  eq(sha(lp.image), sha(image), "loadPackage-Abbild");
  return { ok: true, product, blocks: n, image_sha: sha(image), image_len: image.length };
}

const files = readdirSync(join(ROOT, "firmware")).filter((f) => f.endsWith(".fwsc")).sort();
eq(files, Object.keys(ref.packages).sort(), "Paketliste");
for (const f of files) {
  const raw = new Uint8Array(readFileSync(join(ROOT, "firmware", f)));
  for (const [k, v] of Object.entries(variants(raw))) eq(verdict(v), ref.packages[f][k], `Paket ${f} / ${k}`);
}
console.log(`Pakete: ${files.length} Dateien × 5 Abwandlungen geprüft`);

// ------------------------------------------------------------ Antworten ---

const good = loadPackage(new Uint8Array(readFileSync(join(ROOT, "firmware", ref.sim_package))));
for (const r of ref.responses) {
  const got = r.finish !== undefined
    ? ota.response(r.finish, [0x73, 0x75, 0x63, 0x63, 0x65, 0x73, 0x73, 0])
    : ota.response(r.addr, good.image.subarray(r.addr, r.addr + r.n), r.fl);
  eq(hex(got), r.hex, `Antwort ${JSON.stringify(r).slice(0, 60)}`);
}

// ------------------------------------------------------------ Simulation ---

const HS = Buffer.from(ota.HS_QUERY).toString("hex"), UP = Buffer.from(ota.UPGRADE).toString("hex");

// gleiche Logik wie Device in vectors.py
class FakeDevice {
  constructor(ident, product, imageLen, unplugAt = null) {
    Object.assign(this, { ident, product, imageLen, unplugAt, out: [], reqs: null, lost: false });
  }
  static identity(text) {
    const d = [0, 0x59, 0x11, 0, 0, 0, ...Array.from(text.padEnd(27, "\0"), (c) => c.charCodeAt(0)), 0];
    return [0xF0, ...ota.pack7(d), 0xF7];
  }
  static request(fl, addr, n) {
    const u = [0, 0x59, 0x30, 8, 0, 0, fl, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF,
      n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF];
    let s = 0; for (let i = 6; i < 14; i++) s += u[i];
    u.push(~s & 0xFF);
    return [0xF0, ...ota.pack7(u), 0xF7];
  }
  plan(loader) {
    if (!loader) return [...[0, 512, 4096, 8192].map((a) => [0, a, 512]), [0, ota.FINISH_CHECK, 8]];
    const out = []; let a = 0, k = 0;
    while (a < this.imageLen) { const n = Math.min(512, this.imageLen - a); out.push([k & 1, a, n]); a += n; k++; }
    return [...out, [0, ota.FINISH_WRITE, 8]];
  }
  receive(pkt) {
    if (this.lost) return false;
    const h = hex(pkt);
    if (h === HS) this.out.push(FakeDevice.identity(this.ident));
    else if (h === UP) { this.reqs = this.plan(this.ident.startsWith("ota-")); this.next(); }
    else if (this.reqs !== null && pkt[0] === 0xF0) {
      if (this.unplugAt !== null && this.ident.startsWith("ota-") && this.reqs.length === this.unplugAt) {
        this.lost = true; this.out.length = 0; return true;
      }
      const done = this.reqs.length ? this.reqs.shift() : null;
      if (done && done[1] === ota.FINISH_CHECK) { this.ident = "ota-" + this.ident; this.reqs = null; }
      else if (done && done[1] === ota.FINISH_WRITE) { this.ident = this.product; this.reqs = null; }
      else this.next();
    }
    return true;
  }
  next() { if (this.reqs && this.reqs.length) this.out.push(FakeDevice.request(...this.reqs[0])); }
}

class FakeLink {
  constructor(dev, log) { this.dev = dev; this.log = log; }
  get lost() { return this.dev.lost; }
  send(pkt) { this.log.push(hex(pkt)); return this.dev.receive(Array.from(pkt)); }
  async read() { return this.dev.out.length ? this.dev.out.shift() : null; }
  drain() { this.dev.out.length = 0; }
  close() {}
}

class FakeBackend {
  constructor(dev, log) { this.dev = dev; this.log = log; }
  inputNames() { return this.dev.lost ? [] : ["FM-1"]; }
  outputNames() { return this.inputNames(); }
  async open() { this.log.push("open"); return new FakeLink(this.dev, this.log); }
}

const fast = { delay: Object.fromEntries(Object.keys(ota.DELAY).map((k) => [k, 0])), sleep: async () => {} };
Object.assign(fast.delay, { hs: 50, poll: 10, wait_loader: 1000, wait_reboot: 1000, idle_check: 500, idle_write: 500 });

async function simulate(ident, product, image, unplugAt = null) {
  const dev = new FakeDevice(ident, product, image.length, unplugAt);
  const log = [], steps = [];
  const up = new ota.Updater(new FakeBackend(dev, log), fast);
  try {
    const d = await up.find();
    if (!d) return { error: "notfound", log, steps };
    const result = await up.install(d, image, product, (k, a = null) => steps.push([k, a]));
    return { result, log, steps };
  } catch (e) {
    if (!(e instanceof ota.InstallError)) throw e;
    return { error: e.code, log, steps };
  }
}

const other = good.product !== "FM-1_965" ? "FM-1_965" : "FM-1_941";
const cases = {
  normal: [other], resume: ["ota-" + other], model: ["SMK-37 Pro_016"], foreign: ["ota-FM-1_015"], unplug: [other, 300],
};
for (const [name, [ident, unplugAt]] of Object.entries(cases)) {
  const got = await simulate(ident, good.product, good.image, unplugAt ?? null);
  const want = ref.sim[name];
  eq(got.result ?? null, want.result ?? null, `Lauf ${name}: Ergebnis`);
  eq(got.error ?? null, want.error ?? null, `Lauf ${name}: Fehler`);
  eq(got.log.length, want.log.length, `Lauf ${name}: Anzahl Nachrichten`);
  eq(sha(got.log.join(",")), sha(want.log.join(",")), `Lauf ${name}: Byte-Folge aller gesendeten Nachrichten`);
  eq(got.steps, want.steps, `Lauf ${name}: Schritte/Fortschritt`);
  console.log(`Lauf ${name}: ${got.result || got.error}, ${got.log.length} Nachrichten`);
}

// ---------------------------------------------- Web-MIDI-Schicht (fake MIDIAccess) ---
// Fährt WebMidiBackend/WebMidiLink (die Teile, die im Browser das Gerät ansprechen) mit einem nachgebildeten
// MIDIAccess: eigene Port-Objekte, der Lader meldet sich unter neuem Portnamen (ota-…), Abziehen trennt die Ports.
class FakeAccess {
  constructor(dev, log) {
    Object.assign(this, { dev, log, inputs: new Map(), outputs: new Map(), n: 0, cur: null });
    this.plug();
  }
  portName() { return this.dev.ident.startsWith("ota-") ? "ota-FM-1" : "FM-1 USB MIDI"; }
  plug() {
    const id = `p${++this.n}`, name = this.portName(), acc = this;
    const input = { id, name, state: "connected", onmidimessage: null, async open() {} };
    const output = { id, name, state: "connected", async open() {},
      send(bytes) {
        if (this.state === "disconnected") throw new Error("disconnected");
        acc.log.push(hex(bytes));
        acc.dev.receive(Array.from(bytes));
        acc.after();
      } };
    this.inputs.set(id, input); this.outputs.set(id, output); this.cur = { input, output, name };
  }
  unplug() {
    for (const p of [this.cur.input, this.cur.output]) p.state = "disconnected";
    this.inputs.delete(this.cur.input.id); this.outputs.delete(this.cur.output.id);
  }
  after() {
    if (this.dev.lost) { this.unplug(); return; }
    const msgs = this.dev.out.splice(0), input = this.cur.input;
    for (const m of msgs) setTimeout(() => input.onmidimessage && input.onmidimessage({ data: Uint8Array.from(m) }), 0);
    if (this.portName() !== this.cur.name) { this.unplug(); this.plug(); }   // Neustart: neuer Port
  }
}

async function simulateWebMidi(ident, product, image, unplugAt = null) {
  const dev = new FakeDevice(ident, product, image.length, unplugAt);
  const log = [], steps = [];
  const be = new ota.WebMidiBackend(new FakeAccess(dev, log));
  const open = be.open.bind(be);
  be.open = async (i, o) => { log.push("open"); return open(i, o); };
  const up = new ota.Updater(be, fast);
  try {
    const d = await up.find();
    if (!d) return { error: "notfound", log, steps };
    return { result: await up.install(d, image, product, (k, a = null) => steps.push([k, a])), log, steps };
  } catch (e) {
    if (!(e instanceof ota.InstallError)) throw e;
    return { error: e.code, log, steps };
  }
}

for (const [name, [ident, unplugAt]] of Object.entries(cases)) {
  const got = await simulateWebMidi(ident, good.product, good.image, unplugAt ?? null);
  const want = ref.sim[name];
  eq(got.result ?? got.error, want.result ?? want.error, `Web MIDI ${name}: Ergebnis`);
  eq(sha(got.log.join(",")), sha(want.log.join(",")), `Web MIDI ${name}: Byte-Folge aller gesendeten Nachrichten`);
  eq(got.steps, want.steps, `Web MIDI ${name}: Schritte/Fortschritt`);
  console.log(`Web MIDI ${name}: ${got.result || got.error}, ${got.log.length} Nachrichten`);
}

// ---------------------------------------------- Info-Abfrage (cmd 0x12, ADR 0054) ---
// ota.c der gemeinsamen Plattform wird mit dem Host-C-Compiler übersetzt und beantwortet die Abfrage selbst;
// fm1ota.js muss seine Antwort lesen. Ohne APP_DATE (ältere Firmware) kommt keine Antwort.
{
  const qInfo = hex(ota.INFO_QUERY), qHs = hex(ota.HS_QUERY);
  const src = join(HERE, "ota_info_host.c"), inc = join(ROOT, "bausteine", "varianten", "gemeinsam", "platform", "firmware", "src");
  const bin = join(tmpdir(), `ota_info_${process.pid}`);
  let cc = true;
  try {
    execFileSync("cc", ["-std=c11", "-w", "-I", inc, `-DAPP_VER="0.1.4-alpha"`, `-DAPP_DATE="2026-11-28"`, src, "-o", bin]);
    execFileSync("cc", ["-std=c11", "-w", "-I", inc, `-DAPP_VER="0.1.10-alpha"`, `-DAPP_DATE="2026-11-28"`, `-DAPP_PKG="DK808"`, src, "-o", bin + "_name"]);
    execFileSync("cc", ["-std=c11", "-w", "-I", inc, src, "-o", bin + "_alt"]);
  } catch (_) { cc = false; console.log("Info-Abfrage: kein C-Compiler, Prüfung gegen ota.c übersprungen"); }
  if (cc) {
    const out = execFileSync(bin, [qHs, qInfo]).toString().trim().split("--").map((x) => x.trim());
    const id = ota.parseIdentity(Array.from(Buffer.from(out[0], "hex")));
    eq(id && id.text, "FM-1_941", "ota.c: Kennung unverändert");
    eq(ota.parseInfo(Array.from(Buffer.from(out[1], "hex"))), { version: "0.1.4-alpha", date: "2026-11-28", name: "" }, "ota.c: Info-Antwort gelesen");
    const withName = execFileSync(bin + "_name", [qInfo]).toString().trim().split("--")[0].trim();
    eq(ota.parseInfo(Array.from(Buffer.from(withName, "hex"))), { version: "0.1.10-alpha", date: "2026-11-28", name: "DK808" }, "ota.c: Info mit Paketname");
    const alt = execFileSync(bin + "_alt", [qInfo]).toString().trim();
    eq(alt, "--", "ota.c ohne APP_DATE: keine Info-Antwort (ältere Firmware)");
    eq(ota.parseIdentity(Array.from(Buffer.from(out[1], "hex"))), null, "Info-Antwort ist keine Kennung (alte Installer unberührt)");
    // Updater.info über eine Leitung, die wie ota.c antwortet bzw. schweigt
    for (const [name, reply, want] of [["mit", out[1], { version: "0.1.4-alpha", date: "2026-11-28", name: "" }], ["ohne", null, null]]) {
      const link = { q: [], send(p) { if (hex(p) === qInfo && reply) this.q.push(Array.from(Buffer.from(reply, "hex"))); return true; },
        async read() { return this.q.shift() || null; }, drain() { this.q.length = 0; } };
      eq(await new ota.Updater(null, { ...fast, delay: { ...fast.delay, info: 50 } }).info(link), want, `Updater.info ${name} Erweiterung`);
    }
  }
}

// ---------------------------------------------- Vergleich und Reihenfolge (logik.js) ---
{
  const e = { name: "JXSH", version: "0.1.1-alpha", kennung: "FM-1_991" };
  eq(compare(e, null), { state: "unknown", button: null }, "Version unbekannt: kein Update-Vorschlag");
  eq(compare(e, { version: "", date: "" }), { state: "unknown", button: null }, "leere Version: kein Update-Vorschlag");
  eq(compare(e, { version: "0.1.1-alpha", date: "2026-11-28" }), { state: "current", button: "installed" }, "gleiche Version: Installiert");
  eq(compare(e, { version: "0.1.0-alpha", date: "" }), { state: "update", button: "update" }, "ältere Version: Update");
  eq(compare(e, { version: "0.1.10-alpha", date: "" }), { state: "ahead", button: null }, "neuere Version als die Liste");
  eq(newer("0.1.10-alpha", "0.1.9-alpha"), true, "Versionen numerisch verglichen");
  const meta = JSON.parse(readFileSync(join(HERE, "..", "beschreibungen.json"), "utf8"));
  const names = ["MKSULTRA", "SH101", "JP6", "JXSH", "MKS50", "FM1_ELECTRO", "JX8P", "NEU"];
  const l = names.map((n, i) => ({ name: n, kennung: `FM-1_${900 + i}` }));
  eq(sortList(l, meta).map((x) => x.name), ["FM1_ELECTRO", "JXSH", "SH101", "JX8P", "MKS50", "JP6", "MKSULTRA", "NEU"], "feste Reihenfolge der Liste");
  eq(visible(l, meta).map((x) => x.name).includes("MKS50"), false, "MKS50 versteckt (beschreibungen.json)");
  eq(visible(l, meta).length, l.length - 1, "nur MKS50 versteckt");
  eq(sortList(l, meta, "JXSH")[0].name, "JXSH", "installierte Firmware steht oben");
  eq(sortList(l, meta, "JXSH").slice(1).map((x) => x.name)[0], "FM1_ELECTRO", "danach die feste Reihenfolge");
  const dev = (info, text = "FM-1_991", loader = false) => ({ text, loader, info });
  const L = [e, { name: "DK808", version: "0.1.0-alpha", kennung: "FM-1_911" }, { name: "DK101", version: "0.1.0-alpha", kennung: "FM-1_911" }];
  const bf = (entry, d) => buttonFor(entry, d, installedName(L, d));
  eq(installedName(L, dev(null)), "JXSH", "eindeutige Kennung ohne Name: erkannt");
  eq(installedName(L, dev(null, "FM-1_911")), null, "geteilte Kennung ohne Name: mehrdeutig, keiner installiert");
  eq(installedName(L, dev({ version: "0.1.0-alpha", name: "DK101" }, "FM-1_911")), "DK101", "geteilte Kennung mit Name: DK101");
  eq(installedName(L, dev({ version: "0.1.0-alpha", name: "SH101" }, "FM-1_991")), "JXSH", "unbekannter Name: Rückfall auf Kennung");
  eq(installedName(L, dev(null, "ota-FM-1_911", true)), null, "Update-Modus: keiner installiert");
  // ältere Pakete (vor APP_PKG) senden den Anzeigenamen der App ("JX+SH", "808"): Rückfall auf die Kennung
  eq(installedName(L, dev({ version: "0.1.3-alpha", name: "JX+SH" })), "JXSH", "Anzeigename statt Paketname: Rückfall auf Kennung");
  eq(installedName(L, dev({ version: "0.1.0-alpha", name: "808" }, "FM-1_911")), null, "Anzeigename bei geteilter Kennung: mehrdeutig");
  eq(bf(L[1], dev({ version: "0.1.0-alpha", name: "DK101" }, "FM-1_911")), "install", "DK808 bei laufendem DK101: Installieren");
  eq(bf(L[2], dev({ version: "0.1.0-alpha", name: "DK101" }, "FM-1_911")), "installed", "DK101 bei laufendem DK101: Installiert");
  eq(bf(L[1], dev(null, "FM-1_911")), "install", "geteilte Kennung ohne Name: Installieren, keine Behauptung");
  eq(buttonFor(e, null, null), "install", "Knopf: nicht verbunden -> Installieren");
  eq(bf(e, dev(null, "FM-1_941")), "install", "Knopf: andere Firmware drauf -> Installieren");
  eq(bf(e, dev(null)), "install", "Knopf: drauf, Version unbekannt -> Installieren");
  eq(bf(e, dev({ version: "0.1.0-alpha" })), "update", "Knopf: drauf, älter -> Update");
  eq(bf(e, dev({ version: "0.1.1-alpha" })), "installed", "Knopf: drauf, gleich -> Installiert");
  eq(bf(e, dev({ version: "0.1.2-alpha" })), "install", "Knopf: drauf, neuer als die Liste -> Installieren");
  eq(bf(e, dev(null, "ota-FM-1_991", true)), "install", "Knopf: Update-Modus -> Installieren (schreibt fertig)");
}

// eigene Fälle (nur JS): ohne Lader-Kennung abgelehnt, falsche Datei, Fortschritt monoton bis 100
const noMark = Uint8Array.from(readFileSync(join(ROOT, "firmware", ref.sim_package)));
const at = Buffer.from(noMark).indexOf(LOADER_MARK);
noMark[at] ^= 0x20;
try { loadPackage(noMark); eq("angenommen", "abgelehnt", "Paket ohne Lader-Kennung"); } catch (e) { eq(e.code, "nomark", "Paket ohne Lader-Kennung"); }
try { loadPackage(new TextEncoder().encode("hallo")); eq(1, 0, "Textdatei"); } catch (e) { eq(e.code, "badpkg", "Textdatei"); }
const run = await simulate(other, good.product, good.image);
const w = run.steps.filter((s) => s[0] === "write").map((s) => s[1]);
eq(w.every((v, i) => i === 0 || v >= w[i - 1]) && w.at(-1) === 100, true, "Fortschritt steigt bis 100");

console.log(fails ? `\n${fails} von ${checks} Prüfungen FEHLGESCHLAGEN` : `\nalle ${checks} Prüfungen gleich mit fm1_install.py`);
process.exit(fails ? 1 : 0);
