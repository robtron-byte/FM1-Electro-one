// Bedienung des Web-Installers (ADR 0054). Logik: fm1ota.js (Update), fm1pkg.js (Paketprüfung).
import { Updater, WebMidiBackend, InstallError } from "./fm1ota.js";
import { loadPackage, productOf } from "./fm1pkg.js";
import { newer, compare, sortList } from "./logik.js";

// Die Liste kommt live aus dem Repository (Zweig main): immer die neuesten Pakete, ohne neues Deploy (ADR 0054)
const REPO = "robtron-byte/FM1-Electro-one";
const RAW = `https://raw.githubusercontent.com/${REPO}/main/`;
const BILD_V = "";   // make_site.py setzt hier die Fassung ein (Cache)
const ANLEITUNGEN = [];   // make_site.py setzt die Namen mit Anleitung ein (anleitung/<NAME>.html)
const NAME_RE = /^(.+?)-(\d+\.\d+\.\d+[^/]*)\.fwsc$/;

const $ = (id) => document.getElementById(id);

// Kennungen der Firmwares (bausteine/ARBEITSTAFEL.md). Die Kennung ändert sich nicht mit jeder Version: das Gerät
// meldet also nur, welche Firmware läuft; die Seite nennt dazu die neueste verfügbare Version.
const FAMILIES = {
  "96": "FM1-Electro", "94": "SH-101", "95": "MKS-50", "97": "JX-8P", "93": "MKS Ultra", "92": "JP-6",
  "91": "808 + SH-101", "90": "808 + SH-101 + JX-8P", "99": "JX-8P + SH-101", "98": "MKS-50 + JX-8P",
};

const EN = {
  title: "FM-1 Installer",
  lead: "Install firmware on the M-VAVE FM-1. Only the program is replaced; your memory (sounds, grooves) is not erased. Everything runs here in the browser, nothing is sent to a server.",
  nomidi: "This browser has no Web MIDI. Please open this page in Google Chrome or Microsoft Edge on a computer (not Safari, not Firefox).",
  s1: "Connect the FM-1",
  s1text: "Switch the FM-1 on and connect it to the computer with a USB data cable. Then click “Connect” and allow MIDI access.",
  connect: "Connect",
  s2: "Choose a firmware",
  loading: "Loading the list …",
  drop: "Or drop your own .fwsc file anywhere on this page (or click here).",
  s3: "Install",
  unplug: "<b>Do not unplug</b> or switch off the FM-1 while writing. It takes about two minutes.",
  install: "Install",
  done: "Done",
  helpT: "If something goes wrong",
  help1: "If the connection was cut, the FM-1 stays in update mode. Just reconnect, click “Connect” and “Install” again: the write is finished.",
  help2: "<b>Rescue:</b> if the FM-1 does not start at all, switch it off, hold <b>OCT −</b> and switch it on. It then starts without sound, in update mode only. Connect here and install a working firmware.",
  moreT: "More",
  more1: "The installer uses the same USB-MIDI update (M-UPGRADE) as the platform's fm1_install.py: the running firmware starts the update loader, the loader writes the program area, then the FM-1 restarts and reports its identity. Every file is checked first (identity, update loader in the package, checksums); foreign or damaged files are refused.",
  more2: "Whether another firmware can read the data saved by the previous one depends on the firmware. Installing does not erase it.",
  more3: "Back to M-VAVE's original firmware: use the FM-1 Firmware Hub (thefirmwarehub.com). Install at your own risk.",
  foot: "Update protocol after Felucca by Leo Kuroshita / Hügelton Instruments (GPL-3.0).",
};

const MSG = {
  de: {
    searching: "Suche das FM-1 …", notfound: "Kein FM-1 gefunden. USB-Datenkabel? Ist eine andere App (z. B. M-Flasher oder ein Musikprogramm) offen? Dann schließen und noch einmal.",
    denied: "MIDI-Zugriff nicht erlaubt. Oben in der Adresszeile auf das Schloss klicken und MIDI erlauben, dann noch einmal.",
    running: (n) => `Verbunden: ${n}`, loader: (n) => `Das FM-1 steht im Update-Modus (${n}). Firmware wählen und „Installieren“: das Schreiben wird beendet.`,
    foreign: (n) => `Das FM-1 steht im Update-Modus einer anderen Firmware (${n}). Dieses Update bitte mit dessen eigenem Installer beenden.`,
    other: (k) => `andere Firmware (Kennung ${k})`, kennung: "Kennung",
    onDevice: "Auf dem FM-1", available: "Neueste hier", unknownVer: "Version nicht abfragbar",
    from: "vom", current: "aktuell", newer: "Update möglich", ahead: "neuer als die Liste hier",
    updateTo: (v) => `Update auf ${v}`, installedBtn: "Installiert", installed: "auf dem FM-1",
    today: "heute", yesterday: "gestern", daysAgo: (n) => `vor ${n} Tagen`,
    voices: "Stimmen", chosen: (n) => `Gewählt: ${n}`, fetching: "Paket wird geladen …", fetchfail: "Paket konnte nicht geladen werden.",
    badpkg: "Diese Datei ist kein FM-1-Paket. Nichts wurde geschrieben.",
    nomark: "Diese Datei enthält keinen Felucca-Update-Lader und wird hier nicht installiert (z. B. Original-Firmware: dafür den FM-1 Firmware Hub nutzen).",
    damaged: "Die Datei ist beschädigt (Prüfsumme). Bitte neu herunterladen. Nichts wurde geschrieben.",
    sha: "Die heruntergeladene Datei stimmt nicht mit der Liste überein. Seite neu laden.",
    confirm: (p) => `${p} installieren?\n\nWährend des Schreibens das FM-1 nicht abziehen und nicht ausschalten.`,
    start: (n) => `Update startet auf ${n} …`, check: (k) => `Das FM-1 prüft das Paket (${k}) …`,
    loaderStep: "Wechsel in den Update-Modus …", write: (p) => `Schreiben ${p} % – nicht abziehen!`,
    reboot: "Das FM-1 startet neu …", done: (n) => `Fertig. Das FM-1 läuft jetzt mit ${n}.`,
    err: {
      notfound: "Kein FM-1 gefunden.", model: "Das Paket passt nicht zu diesem Gerät. Nichts wurde geschrieben.",
      foreign: "Das FM-1 steht im Update-Modus einer anderen Firmware.",
      lost: "Verbindung unterbrochen. Wieder anschließen, „Verbinden“ und noch einmal „Installieren“.",
      stopped: "Das FM-1 antwortet nicht mehr. Wieder anschließen, „Verbinden“ und noch einmal „Installieren“.",
      noloader: "Der Update-Modus erschien nicht. USB-Kabel ab- und wieder anstecken, „Verbinden“ und noch einmal „Installieren“: das FM-1 bleibt im Update-Modus, bis es fertig ist.",
      noreturn: "Geschrieben, aber das FM-1 meldet sich nicht zurück. Aus- und wieder einschalten.",
      mismatch: "Geschrieben, aber das FM-1 meldet eine andere Firmware",
      badreq: "Das FM-1 fragte nach Daten außerhalb des Pakets. Abgebrochen.",
    },
  },
  en: {
    searching: "Looking for the FM-1 …", notfound: "No FM-1 found. USB data cable? Is another app (M-Flasher or a music program) open? Close it and try again.",
    denied: "MIDI access was not allowed. Click the lock in the address bar, allow MIDI and try again.",
    running: (n) => `Connected: ${n}`, loader: (n) => `The FM-1 is in update mode (${n}). Choose a firmware and “Install”: the write is finished.`,
    foreign: (n) => `The FM-1 is in the update mode of another firmware (${n}). Finish that update with its own installer.`,
    other: (k) => `other firmware (identity ${k})`, kennung: "identity",
    onDevice: "On the FM-1", available: "Newest here", unknownVer: "version cannot be read",
    from: "from", current: "up to date", newer: "update available", ahead: "newer than the list here",
    updateTo: (v) => `Update to ${v}`, installedBtn: "Installed", installed: "on the FM-1",
    today: "today", yesterday: "yesterday", daysAgo: (n) => `${n} days ago`,
    voices: "voices", chosen: (n) => `Chosen: ${n}`, fetching: "Loading the package …", fetchfail: "The package could not be loaded.",
    badpkg: "This file is not an FM-1 package. Nothing was written.",
    nomark: "This file has no Felucca update loader and is not installed here (e.g. original firmware: use the FM-1 Firmware Hub).",
    damaged: "The file is damaged (checksum). Please download it again. Nothing was written.",
    sha: "The downloaded file does not match the list. Reload the page.",
    confirm: (p) => `Install ${p}?\n\nDo not unplug or switch off the FM-1 while writing.`,
    start: (n) => `Starting the update on ${n} …`, check: (k) => `The FM-1 checks the package (${k}) …`,
    loaderStep: "Switching to update mode …", write: (p) => `Writing ${p} % – do not unplug!`,
    reboot: "The FM-1 restarts …", done: (n) => `Done. The FM-1 now runs ${n}.`,
    err: {
      notfound: "No FM-1 found.", model: "The package does not fit this device. Nothing was written.",
      foreign: "The FM-1 is in the update mode of another firmware.",
      lost: "Connection lost. Reconnect, click “Connect” and “Install” again.",
      stopped: "The FM-1 stopped answering. Reconnect, click “Connect” and “Install” again.",
      noloader: "Update mode did not appear. Unplug and replug the USB cable, “Connect” and “Install” again: the FM-1 stays in update mode until it is done.",
      noreturn: "Written, but the FM-1 did not come back. Switch it off and on.",
      mismatch: "Written, but the FM-1 reports another firmware",
      badreq: "The FM-1 asked for data outside the package. Stopped.",
    },
  },
};

let lang = (() => { try { return localStorage.getItem("fm1lang") || (navigator.language.startsWith("de") ? "de" : "en"); } catch (_) { return "de"; } })();
const DE = {};
document.querySelectorAll("[data-t]").forEach((el) => { DE[el.dataset.t] = el.innerHTML; });
const m = () => MSG[lang];

let meta = {}, list = [], access = null, device = null, pkg = null, busy = false;

function applyLang() {
  document.documentElement.lang = lang;
  document.querySelectorAll("[data-t]").forEach((el) => { el.innerHTML = (lang === "en" ? EN : DE)[el.dataset.t]; });
  $("lang").textContent = lang === "de" ? "English" : "Deutsch";
  renderList();
  renderDevice();
  if (pkg) setStatus("pkgstatus", m().chosen(pkg.label), "ok");
}

function setStatus(id, text, cls = "") { const el = $(id); el.textContent = text; el.className = "status " + cls; }

function describe(text) {
  const e = list.find((x) => x.kennung === text);
  if (e) return `${e.titel} (${m().kennung} ${text})`;
  const f = /^FM-1_(9\d)\d$/.exec(text);
  if (f && FAMILIES[f[1]]) return `${FAMILIES[f[1]]} (${m().kennung} ${text})`;
  return m().other(text);
}

// "2026-11-28" -> "28.11.2026 (heute)"
function fmtDate(iso) {
  if (!iso) return "";
  const [y, mo, d] = iso.split("-").map(Number);
  const then = Date.UTC(y, mo - 1, d), now = new Date();
  const days = Math.round((Date.UTC(now.getFullYear(), now.getMonth(), now.getDate()) - then) / 86400000);
  const age = days <= 0 ? m().today : days === 1 ? m().yesterday : m().daysAgo(days);
  const shown = lang === "en" ? iso : `${String(d).padStart(2, "0")}.${String(mo).padStart(2, "0")}.${y}`;
  return `${shown} (${age})`;
}

function verDate(v, date) { return date ? `${v} ${m().from} ${fmtDate(date)}` : v; }

function renderDevice() {
  if (!device) return;
  const t = device.text;
  if (!device.loader) {
    const e = list.find((x) => x.kennung === t);
    const title = e ? e.titel : describe(t);
    const c = compare(e, device.info);
    // Version unbekannt: neutral, nie ein Update vorschlagen (ADR 0054)
    let line = `${m().onDevice}: ${title} ` + (device.info ? verDate(device.info.version, device.info.date) : `(${m().unknownVer})`);
    if (e && c.state !== "unknown") {
      line += ` · ${m().available}: ${verDate(e.version, e.date)} → ` + (c.state === "update" ? m().newer : c.state === "ahead" ? m().ahead : m().current);
    }
    setStatus("devstatus", line, "ok");
  }
  else if (/^ota-(FM-1|SMK-37 Pro|SMK-37 Elite|MKE-P37)_9\d\d$/i.test(t)) setStatus("devstatus", m().loader(t), "ok");
  else setStatus("devstatus", m().foreign(t), "bad");
}

function renderList() {
  const box = $("list");
  if (!list.length) return;
  box.innerHTML = "";
  const inst = device && !device.loader ? device.text : null;
  for (const e of sortList(list, meta, inst)) {
    const lab = document.createElement("label");
    lab.className = "fw";
    lab.dataset.kennung = e.kennung;
    const desc = lang === "en" ? e.en : e.de, voices = lang === "en" ? e.stimmen_en : e.stimmen_de;
    lab.innerHTML = `<input type="radio" name="fw"><span class="media"><img class="pic" alt="" width="96" height="64"><img class="screen" alt="" width="96" height="96"></span><span class="txt"><b></b><br><span class="d"></span><br><span class="meta"></span></span>`;
    const pic = lab.querySelector(".pic");
    pic.onerror = () => { pic.onerror = null; pic.src = `bilder/allgemein.svg${BILD_V}`; };
    pic.src = `bilder/${encodeURIComponent(e.name)}.svg${BILD_V}`;
    if (ANLEITUNGEN.includes(e.name)) {
      const a = document.createElement("a");
      a.className = "howto"; a.target = "_blank"; a.rel = "noopener";
      a.href = `anleitung/${encodeURIComponent(e.name)}.html${BILD_V ? BILD_V + "&" : "?"}lang=${lang}`;
      a.innerHTML = '<svg viewBox="0 0 24 24" width="18" height="18" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 5.5A2.5 2.5 0 0 1 6.5 3H20v15H6.5A2.5 2.5 0 0 0 4 20.5z"/><path d="M4 20.5A2.5 2.5 0 0 0 6.5 23H20v-5"/><path d="M8 7h8M8 11h6"/></svg><span></span>';
      a.querySelector("span").textContent = lang === "en" ? "Manual" : "Anleitung";
      lab.querySelector(".txt").append(document.createElement("br"), a);
    }
    const scr = lab.querySelector(".screen");   // Display des FM-1 mit dieser Firmware (Emulator); fehlt es, entfällt es
    scr.onerror = () => scr.remove();
    scr.title = lang === "en" ? "The FM-1 screen with this firmware" : "So sieht das Display des FM-1 mit dieser Firmware aus";
    scr.src = `bildschirme/${encodeURIComponent(e.name)}.png${BILD_V}`;
    lab.querySelector("b").textContent = `${e.titel} ${e.version}`;
    lab.querySelector(".d").textContent = desc;
    lab.querySelector(".meta").textContent = [voices, e.date ? fmtDate(e.date) : "", `${m().kennung} ${e.kennung}`].filter(Boolean).join(" · ");
    const badge = document.createElement("span");
    badge.className = "badge"; badge.textContent = m().installed;
    lab.querySelector("b").after(badge);
    // installierte Firmware: markiert, kleiner Knopf „Update auf X“ bzw. „Installiert“ (nur bei bekannter Version)
    if (inst === e.kennung) {
      lab.classList.add("installed");
      const c = compare(e, device.info);
      if (c.button) {
        const b = document.createElement("button");
        b.type = "button"; b.className = "upd";
        b.textContent = c.button === "update" ? m().updateTo(e.version) : m().installedBtn;
        b.disabled = c.button !== "update" || busy;
        b.addEventListener("click", (ev) => { ev.preventDefault(); updateTo(e); });
        lab.querySelector(".txt").append(document.createElement("br"), b);
      }
    }
    const input = lab.querySelector("input");
    input.checked = pkg && pkg.entry === e;
    input.addEventListener("change", () => chooseListed(e));
    box.appendChild(lab);
  }
}

async function sha256(bytes) {
  const h = await crypto.subtle.digest("SHA-256", bytes);
  return Array.from(new Uint8Array(h), (b) => b.toString(16).padStart(2, "0")).join("");
}

function pkgError(e) {
  setStatus("pkgstatus", m()[e.code] || e.message, "bad");
  pkg = null; updateButtons();
}

// Git-Blob-SHA-1 (wie die GitHub-API sie meldet) einer Datei
async function gitBlobSha(bytes) {
  const head = new TextEncoder().encode(`blob ${bytes.length}\0`);
  const all = new Uint8Array(head.length + bytes.length);
  all.set(head); all.set(bytes, head.length);
  const h = await crypto.subtle.digest("SHA-1", all);
  return Array.from(new Uint8Array(h), (b) => b.toString(16).padStart(2, "0")).join("");
}

async function chooseListed(e) {
  setStatus("pkgstatus", m().fetching);
  try {
    const r = await fetch(e.url, { cache: "no-cache" });
    if (!r.ok) throw new Error(r.status);
    const raw = new Uint8Array(await r.arrayBuffer());
    const okSum = e.sha256 ? await sha256(raw) === e.sha256 : e.gitsha ? await gitBlobSha(raw) === e.gitsha : true;
    if (!okSum) { pkg = null; setStatus("pkgstatus", m().sha, "bad"); updateButtons(); return; }
    const p = loadPackage(raw);
    pkg = { ...p, entry: e, label: `${e.titel} ${e.version}` };
    setStatus("pkgstatus", m().chosen(pkg.label), "ok");
  } catch (err) {
    if (err.code) pkgError(err); else { pkg = null; setStatus("pkgstatus", m().fetchfail, "bad"); }
  }
  updateButtons();
}


async function json(url) { const r = await fetch(url, { cache: "no-cache" }); if (!r.ok) throw new Error(r.status); return r.json(); }

// live: firmware/ und beschreibungen.json aus main; je Name nur die neueste Version; Kennung aus den ersten 1 KB
async function liveList() {
  const files = await json(`https://api.github.com/repos/${REPO}/contents/firmware?ref=main`);
  // Beschreibungen live aus main, Felder der mitgelieferten Datei als Rückfall (raw.githubusercontent.com cacht 5 Minuten)
  let local = {}, live = {};
  try { local = await json("beschreibungen.json"); } catch (_) { /* ohne */ }
  try { live = await json(RAW + "web/installer/beschreibungen.json"); } catch (_) { /* ohne */ }
  meta = {};
  for (const k of new Set([...Object.keys(local), ...Object.keys(live)])) meta[k] = k.startsWith("_") ? live[k] || local[k] : { ...local[k], ...live[k] };
  let dates = {};
  try { dates = await json("daten.json"); } catch (_) { /* ohne */ }
  const best = {};
  for (const f of files) {
    const mm = NAME_RE.exec(f.name);
    if (f.type !== "file" || !mm) continue;
    if (!best[mm[1]] || newer(mm[2], best[mm[1]].version)) best[mm[1]] = { file: f, version: mm[2] };
  }
  const names = Object.keys(best);
  const out = await Promise.all(names.map(async (name) => {
    const { file, version } = best[name], d = meta[name] || {};
    const url = RAW + "firmware/" + encodeURIComponent(file.name);
    let kennung = "";
    try {
      const r = await fetch(url, { headers: { Range: "bytes=0-1023" }, cache: "no-cache" });
      kennung = productOf(new Uint8Array(await r.arrayBuffer()));
    } catch (_) { /* ohne Kennung */ }
    if (!/^[^_]+_9\d\d$/.test(kennung)) return null;   // nur Pakete der eigenen Plattform (FM-1_9xx)
    let date = dates[file.name] || "";
    if (!date) {
      try { date = (await json(`https://api.github.com/repos/${REPO}/commits?path=firmware/${encodeURIComponent(file.name)}&per_page=1`))[0].commit.committer.date.slice(0, 10); } catch (_) { /* ohne Datum */ }
    }
    return { name, titel: d.titel || name, version, kennung, date, de: d.de || "", en: d.en || "",
      stimmen_de: d.stimmen_de || "", stimmen_en: d.stimmen_en || "", url, gitsha: file.sha };
  }));
  return out.filter(Boolean);
}

async function loadList() {
  try {
    list = await liveList();
    if (!list.length) throw new Error("leer");
  } catch (_) {
    try { list = (await json("liste.json")).map((e) => ({ ...e, url: e.datei })); } catch (_) { list = []; }
    try { meta = await json("beschreibungen.json"); } catch (_) { meta = {}; }
  }
  if (!list.length) $("list").textContent = "";
  renderList(); renderDevice();
}

async function chooseFile(file) {
  document.querySelectorAll('input[name="fw"]').forEach((i) => { i.checked = false; });
  try {
    const p = loadPackage(new Uint8Array(await file.arrayBuffer()));
    pkg = { ...p, entry: null, label: `${file.name} (${describe(p.product)})` };
    setStatus("pkgstatus", m().chosen(pkg.label), "ok");
  } catch (err) { pkgError(err); }
  updateButtons();
}

function updateButtons() {
  $("install").disabled = busy || !access || !device || !pkg;
  $("connect").disabled = busy || !access && !navigator.requestMIDIAccess;
}

async function getAccess() {
  if (access) return access;
  try { access = await navigator.requestMIDIAccess({ sysex: true }); }
  catch (_) { setStatus("devstatus", m().denied, "bad"); return null; }
  return access;
}

async function connect() {
  if (!(await getAccess())) return;
  busy = true; updateButtons(); device = null;
  setStatus("devstatus", m().searching);
  try {
    const up = new Updater(new WebMidiBackend(access));
    const dev = await up.find();
    if (!dev) { setStatus("devstatus", m().notfound, "bad"); return; }
    const info = dev.id.loader ? null : await up.info(dev.link);
    dev.link.close();
    device = { ...dev.id, info };
    renderDevice(); renderList();
  } finally { busy = false; updateButtons(); }
}

// Knopf „Update auf X“: Eintrag wählen, laden, installieren
async function updateTo(e) {
  if (busy) return;
  const input = [...document.querySelectorAll(".fw")].find((l) => l.dataset.kennung === e.kennung)?.querySelector("input");
  if (input) input.checked = true;
  await chooseListed(e);
  if (pkg && pkg.entry === e) install();
}

async function install() {
  if (!pkg || !device || busy) return;
  if (!confirm(m().confirm(pkg.label))) return;
  busy = true; updateButtons();
  $("done").hidden = true; $("bar").hidden = false;
  const bar = $("bar").firstElementChild;
  bar.style.width = "0%";
  const up = new Updater(new WebMidiBackend(access));
  const step = (k, a) => {
    if (k === "start") { setStatus("progress", m().start(describe(a))); bar.style.width = "2%"; }
    else if (k === "check") setStatus("progress", m().check(a));
    else if (k === "loader") { setStatus("progress", m().loaderStep); bar.style.width = "5%"; }
    else if (k === "write") { setStatus("progress", m().write(a)); bar.style.width = `${5 + a * 0.9}%`; }
    else if (k === "reboot") { setStatus("progress", m().reboot); bar.style.width = "97%"; }
    else if (k === "done") { setStatus("progress", m().done(describe(a)), "ok"); bar.style.width = "100%"; $("done").hidden = false; }
  };
  try {
    const dev = await up.find();
    if (!dev) throw new InstallError("notfound", "notfound");
    const now = await up.install(dev, pkg.image, pkg.product, step);
    device = { text: now, loader: false, info: null };
    renderDevice(); renderList();
    busy = false;
    setTimeout(connect, 1500);   // neu abfragen: zeigt die jetzt installierte Version
  } catch (e) {
    const text = (m().err[e.code] || e.message) + (e.code === "mismatch" ? ` (${e.detail})` : "");
    setStatus("progress", text, "bad");
    if (e.code !== "model" && e.code !== "foreign" && e.code !== "notfound") device = null;
  } finally {
    busy = false; updateButtons();
  }
}

window.addEventListener("beforeunload", (ev) => { if (busy) { ev.preventDefault(); ev.returnValue = ""; } });

$("lang").addEventListener("click", () => {
  lang = lang === "de" ? "en" : "de";
  try { localStorage.setItem("fm1lang", lang); } catch (_) { /* egal */ }
  applyLang();
});
$("connect").addEventListener("click", connect);
$("install").addEventListener("click", install);
$("file").addEventListener("change", (ev) => { if (ev.target.files[0]) chooseFile(ev.target.files[0]); });
// eine .fwsc kann überall auf der Seite abgelegt werden
const drop = $("drop");
let dragDepth = 0;
document.addEventListener("dragenter", (ev) => { ev.preventDefault(); dragDepth++; drop.classList.add("over"); document.body.classList.add("dragging"); });
document.addEventListener("dragover", (ev) => { ev.preventDefault(); });
document.addEventListener("dragleave", () => { if (--dragDepth <= 0) { dragDepth = 0; drop.classList.remove("over"); document.body.classList.remove("dragging"); } });
document.addEventListener("drop", (ev) => {
  ev.preventDefault(); dragDepth = 0; drop.classList.remove("over"); document.body.classList.remove("dragging");
  if (busy) return;
  if (ev.dataTransfer.files[0]) { chooseFile(ev.dataTransfer.files[0]); drop.scrollIntoView({ behavior: "smooth", block: "center" }); }
});

if (!navigator.requestMIDIAccess) { $("nomidi").hidden = false; $("connect").disabled = true; }

loadList();

applyLang();
updateButtons();
