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

// feste Reihenfolge aus beschreibungen.json (Feld "reihe"), unbekannte Namen danach alphabetisch;
// nach dem Verbinden steht die installierte Firmware (Kennung) oben
export function sortList(list, meta, installedKennung = null) {
  const rank = (e) => (meta[e.name] && Number.isFinite(meta[e.name].reihe) ? meta[e.name].reihe : 1e6);
  return [...list].sort((a, b) =>
    (b.kennung === installedKennung) - (a.kennung === installedKennung) || rank(a) - rank(b) || a.name.localeCompare(b.name));
}
