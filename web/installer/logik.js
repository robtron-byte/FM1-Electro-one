// Reine Logik der Seite, ohne DOM (ADR 0054): Versionsvergleich und Reihenfolge der Liste. Geprüft in test/test.mjs.

const vkey = (v) => (v.match(/\d+/g) || []).map(Number);

// ist Version a neuer als b?
export function newer(a, b) {
  const x = vkey(a), y = vkey(b);
  for (let i = 0; i < Math.max(x.length, y.length); i++) if ((x[i] || 0) !== (y[i] || 0)) return (x[i] || 0) > (y[i] || 0);
  return false;
}

// Vergleich installiert (info aus cmd 0x12 oder null) gegen den neuesten Eintrag der Liste.
// -> state: "unknown" (Version nicht abfragbar: nie ein Update vorschlagen), "current", "update", "ahead"
//    button: null, "installed" (ausgegraut) oder "update"
export function compare(entry, info) {
  if (!entry || !info || !info.version) return { state: "unknown", button: null };
  if (newer(entry.version, info.version)) return { state: "update", button: "update" };
  if (newer(info.version, entry.version)) return { state: "ahead", button: null };
  return { state: "current", button: "installed" };
}

// welcher Eintrag läuft auf dem Gerät? device = { text, loader, info } oder null -> Paketname oder null.
// Zuerst der Paketname aus cmd 0x12 (Firmwares können sich eine Kennung teilen, z. B. DK808 und DK101 FM-1_911),
// sonst die Kennung, aber nur wenn genau ein Eintrag sie hat (sonst mehrdeutig: keiner gilt als installiert)
export function installedName(list, device) {
  if (!device || device.loader) return null;
  const n = device.info && device.info.name;
  if (n && list.some((e) => e.name === n)) return n;
  const same = list.filter((e) => e.kennung === device.text);
  return same.length === 1 ? same[0].name : null;
}

// feste Reihenfolge aus beschreibungen.json (Feld "reihe"), unbekannte Namen danach alphabetisch;
// nach dem Verbinden steht die installierte Firmware (Paketname aus installedName) oben
export function sortList(list, meta, installed = null) {
  const rank = (e) => (meta[e.name] && Number.isFinite(meta[e.name].reihe) ? meta[e.name].reihe : 1e6);
  return [...list].sort((a, b) =>
    (b.name === installed) - (a.name === installed) || rank(a) - rank(b) || a.name.localeCompare(b.name));
}

// Knopf je Eintrag der Liste (Wunsch des Nutzers 28.11.): device = { text, loader, info } nach „Verbinden“ oder null
// -> "install" (nicht drauf, nicht verbunden, oder Version unbekannt: ohne Update-Behauptung),
//    "update" (drauf, neuere Version hier), "installed" (drauf, gleiche Version: ausgegraut)
export function buttonFor(entry, device, installed) {
  if (!device || device.loader || installed !== entry.name) return "install";
  const c = compare(entry, device.info);
  return c.state === "update" ? "update" : c.state === "current" ? "installed" : "install";
}
