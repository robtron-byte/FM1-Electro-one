// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (fm1_install.py)
// JS-Port der Paketprüfung aus platform/tools/fm1_install.py (load_package, package_damage, ...), FM1-Electro-one.
//
// Ein .fwsc-Paket wird vor dem Schreiben geprüft: Kennung (FM-1_9xx), Felucca-Lader im Paket
// (FELUCCA-LOADER-1), Kopf-, Dateilisten- und Datei-Prüfsummen. Fremde oder kaputte Dateien werden abgelehnt.

export const BLOCKS = 20, BLK = 0x30, KEEP = 0x2F, MAX_BLOCKS = 64;
export const LOADER_MARK = "FELUCCA-LOADER-1";

export class PackageError extends Error {
  constructor(code, msg) { super(msg); this.code = code; }
}

function idChar(m, i) { const c = (m - i - 1) & 0xFF; return c >= 0x20 && c < 0x7F; }

function marker(raw, k) { return (k + 1) * BLK <= raw.length ? raw[k * BLK + KEEP] : null; }

export function identityBlocks(raw) {
  let n = BLOCKS, m;
  while (n < MAX_BLOCKS && (m = marker(raw, n)) !== null && m !== 0x7D && idChar(m, n)) n++;
  return n > BLOCKS && marker(raw, n) === 0x7D ? n : BLOCKS;
}

export function blockCount(raw) {
  let n = identityBlocks(raw);
  while (n < MAX_BLOCKS && (n + 1) * BLK <= raw.length && raw[n * BLK + KEEP] === 0x7D) n++;
  return n;
}

export function logicalImage(raw, blocks = BLOCKS) {
  const tail = raw.length - blocks * BLK;
  const out = new Uint8Array(blocks * KEEP + Math.max(0, tail));
  for (let i = 0; i < blocks; i++) out.set(raw.subarray(i * BLK, i * BLK + KEEP), i * KEEP);
  if (tail > 0) out.set(raw.subarray(blocks * BLK), blocks * KEEP);
  return out;
}

export function deinterleave(raw) {
  const n = blockCount(raw);
  const image = logicalImage(raw, n);
  if (n !== BLOCKS && packageDamage(image)) {
    const alt = logicalImage(raw, BLOCKS);
    if (!packageDamage(alt)) return [alt, BLOCKS];
  }
  return [image, n];
}

export function productOf(raw) {
  if (raw.length < BLOCKS * BLK) throw new PackageError("badpkg", "zu kurz für ein FM-1-Paket");
  const ib = identityBlocks(raw);
  const n = ib === BLOCKS ? BLOCKS : Math.min(ib, deinterleave(raw)[1]);
  let s = "";
  for (let i = 0; i < n; i++) {
    const m = raw[i * BLK + KEEP];
    if (m !== 0x7D) s += String.fromCharCode((m - i - 1) & 0xFF);
  }
  return s;
}

// CRC-16/XMODEM
export function crc16(d, crc = 0) {
  for (let k = 0; k < d.length; k++) {
    crc ^= d[k] << 8;
    for (let j = 0; j < 8; j++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xFFFF : (crc << 1) & 0xFFFF;
  }
  return crc;
}

// JieLi-ENC-Stromchiffre (ihr eigenes Gegenstück)
export function dec(b) {
  const out = Uint8Array.from(b);
  let key = 0xFFFF;
  for (let i = 0; i < out.length; i++) {
    out[i] ^= key & 0xFF;
    key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
  return out;
}

const u16 = (b, o) => b[o] | (b[o + 1] << 8);
const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;

// was am Update-Abbild kaputt ist (Kopf, Dateiliste, CRC jeder Datei), oder null
export function packageDamage(image) {
  if (image.length < 0x40) return "too short";
  const hdr = dec(image.subarray(0, 0x40));
  if (crc16(hdr.subarray(2, 0x40)) !== u16(hdr, 0)) return "header CRC";
  const count = u16(hdr, 8);
  if (!(count >= 1 && count <= 8) || image.length < 0x40 + 0x50 * count) return "file list";
  const lst = image.subarray(0x40, 0x40 + 0x50 * count);
  if (crc16(lst) !== u16(hdr, 2)) return "file list CRC";
  for (let i = 0; i < count; i++) {
    const e = dec(lst.subarray(i * 0x50, (i + 1) * 0x50));
    const dcrc = u16(e, 4), off = u32(e, 8), size = u32(e, 12);
    let name = "";
    for (let k = 0x40; k < 0x50 && e[k]; k++) name += e[k] < 0x80 ? String.fromCharCode(e[k]) : "�";
    if (off + size > image.length) return `${name}: cut short`;
    const typ = u16(e, 0);
    if ((typ === 0 || typ === 100) && crc16(image.subarray(off, off + size)) !== dcrc) return `${name}: CRC`;
  }
  return null;
}

function hasMark(raw) {
  const m = Array.from(LOADER_MARK, (c) => c.charCodeAt(0));
  outer: for (let i = 0; i + m.length <= raw.length; i++) {
    if (raw[i] !== m[0]) continue;
    for (let k = 1; k < m.length; k++) if (raw[i + k] !== m[k]) continue outer;
    return true;
  }
  return false;
}

// -> { product, image }; wirft PackageError("badpkg", ...) bei fremden oder kaputten Dateien
export function loadPackage(raw) {
  raw = raw instanceof Uint8Array ? raw : new Uint8Array(raw);
  const product = productOf(raw);
  if (!/^[^_]+_\d+$/.test(product)) throw new PackageError("badpkg", `kein FM-1-Paket (Kennung ${JSON.stringify(product)})`);
  if (!hasMark(raw)) throw new PackageError("nomark", "kein Felucca-Update-Lader im Paket");
  const [image] = deinterleave(raw);
  const bad = packageDamage(image);
  if (bad) throw new PackageError("damaged", `beschädigtes Paket (${bad})`);
  return { product, image };
}
