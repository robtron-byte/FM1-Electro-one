#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2023 Andrey Grigoryev (kagaimiq), 2026 M-VAVE: FM1 Open's
# sdk/Package.cs (JieLi formats and ciphers from jl-misctools 0a5b12d),
# ported to Python for FM1-Electro. See FM1 Open's THIRD_PARTY.md.
"""Packages an FM1-Electro firmware (sdk.elf from FM1 Open's Build.ps1) as an
.fwsc update with a smaller user partition than FM1 Open's 524 KB.

FM1 Open keeps 524 KB of Flash for its sample bank and projects. FM1-Electro
uses neither (its grooves and sounds take 32 KB), and its code does not fit
next to that partition. This packager gives the partition USER_KB instead and
writes the same size into the firmware (fm1_layout), which the SDK compares
with the partition when it starts.

Usage:
  python tools/package_fm1.py <path to fm1_open> [--user-kb 384] [--elf sdk.elf] [--out file.fwsc]

Build first in FM1 Open with  .\\sdk\\Build.ps1 -Toolchain 'C:\\JL\\pi32'
(without -FirmwareOnly: that would package with FM1 Open's layout and stop).
Only the Python standard library is needed."""
import argparse
import os
import re
import struct
import sys

AREA, ENTRY, APPDATA = 0x4000, 0x02000120, 0x120
FLASHLIMIT, BTSIZE, MINVM = 0xff000, 4096, 32768
FM1_OPEN_USER = 536576          # FM1 Open's user partition, as linked by Build.ps1
UPDATE_FAMILY = 7
PUBLIC = 0x980f
SECTIONS = ('.text', '.data', '.ram0_data', '.cache_ram_data')   # the order of Build.ps1
DEFAULT_USER_KB = 384           # grooves and sounds sit in its last 32 KB


def mk_table():
    t = []
    for i in range(256):
        v = i << 8
        for _ in range(8):
            v = ((v << 1) ^ (0x1021 if v & 0x8000 else 0)) & 0xffffffff
        t.append(v & 0xffff)
    return t


TABLE = mk_table()


def crc(b, begin, length):
    v = 0
    for i in range(begin, begin + length):
        v = ((v << 8) ^ TABLE[(v >> 8) ^ b[i]]) & 0xffff
    return v


def cipher(b, begin, length, state):
    for i in range(begin, begin + length):
        b[i] ^= state & 0xff
        state = ((state << 1) ^ (0x1021 if state & 0x8000 else 0)) & 0xffff


def roundup(size, a):
    return (size + a - 1) & -a


class Image:
    def __init__(self, size, fill):
        self.b = bytearray([fill]) * size

    def number(self, off, value, width):
        for n in range(width):
            self.b[off + n] = (value >> (8 * n)) & 0xff

    def copy(self, off, data):
        self.b[off:off + len(data)] = data

    def text(self, off, s):
        self.copy(off, s.encode('ascii'))

    def checksum(self, field, off, size):
        self.number(field, crc(self.b, off, size), 2)

    def record(self, off, name, c, address, size, attr, last, extra):
        self.b[off:off + 32] = bytes(32)
        self.number(off + 2, c, 2)
        self.number(off + 4, address, 4)
        self.number(off + 8, size & 0xffffffff, 4)
        self.number(off + 12, attr, 2)
        self.number(off + 14, last, 2)
        for n in range(16, 32):
            self.b[off + n] = 255
        self.text(off + 16, name)
        self.b[off + 16 + len(name)] = 0
        if extra is not None:
            self.number(off + 28, extra, 4)
        self.checksum(off, off + 2, 30)


def read_res(d, name, size=None):
    b = open(os.path.join(d, name), 'rb').read()
    if size is not None and len(b) != size:
        sys.exit('Unexpected FM1 resource: ' + name)
    return b


def encode_boot(plain):
    r = bytearray(plain)
    banks = struct.unpack_from('<H', plain, 0)[0]
    if banks < 1 or banks > len(plain) // 16:
        sys.exit('Invalid boot directory.')
    prev = banks * 16
    for i in range(banks):
        h = i * 16
        length = struct.unpack_from('<H', plain, h + 2)[0]
        start = struct.unpack_from('<i', plain, h + 8)[0]
        if (length == 0 or start < prev or start > len(plain) - length or
                crc(plain, h, 14) != struct.unpack_from('<H', plain, h + 14)[0] or
                crc(plain, start, length) != struct.unpack_from('<H', plain, h + 12)[0]):
            sys.exit('Invalid boot bank.')
        cipher(r, h, 16, 0xffff)
        cipher(r, start, length, 0xffff)
        prev = start + length
    return bytes(r)


def build_flash(app, d, end, vm_end, user):
    boot = read_res(d, 'uboot.boot', 14488)
    config = read_res(d, 'boot-config.bin', 721)
    tool = read_res(d, 'cfg_tool.bin', 383)
    image = Image(end, 255)
    settings = Image(34 + len(config), 0)
    for bit in range(16):
        settings.b[bit] = 16
        settings.b[16 + bit] = 16 ^ (255 if ((PUBLIC >> bit) & 1) == 0 else 0)
    settings.checksum(32, 0, 32)
    settings.copy(34, config)
    image.copy(0, bytes([0, 0, 0x20, 2, 0x2f, 0x10, 0x4c, 0xc9, 0, 0xf0, 0x0f, 0, 0x10, 0x10, 0xa4, 0xff,
                         0x4e, 0x5d, 0x0b, 0x41, 0xe0, 0xcd, 0x78, 0x1d, 0xc8, 0x56, 0x41, 0x7f, 0xb3, 0x67, 0xcf, 0x9f]))
    image.checksum(0, 2, 30)
    config_at = 160 + len(boot)
    image.record(32, 'uboot.boot', crc(boot, 0, len(boot)), 160, len(boot), 0, 0, None)
    image.record(64, 'isd_config.ini', crc(settings.b, 0, len(settings.b)), config_at, len(settings.b), 0x8002, 0, None)
    image.record(96, 'app_dir_head', 65535, AREA, 0xffffffff, 0xff81, 0, None)
    image.record(128, 'key_mac', 65535, FLASHLIMIT, 4096, 0x0112, 65535, None)
    for p in range(0, 160, 32):
        cipher(image.b, p, 32, 0xffff)
    image.copy(160, encode_boot(boot))
    image.copy(config_at, settings.b)
    payload = APPDATA + len(app) + len(tool)
    a = AREA + APPDATA
    image.copy(a, app)
    image.copy(a + len(app), tool)
    image.record(AREA + 32, 'app.bin', crc(app, 0, len(app)), APPDATA, len(app), 0xff82, 0, None)
    image.record(AREA + 64, 'cfg_tool.bin', crc(tool, 0, len(tool)), APPDATA + len(app), len(tool), 0xff82, 0, None)
    image.record(AREA + 96, 'VM', 65535, end, vm_end - end, 0x8112, 0, MINVM)
    image.record(AREA + 128, 'PRCT', 65535, 0, end, 0x8292, 0, 0x45444f43)
    image.record(AREA + 160, 'BTIF', 65535, vm_end, BTSIZE, 0x8192, 1, 0x4f545541)
    image.record(AREA, 'app_area_head', crc(image.b, AREA + 32, payload - 32), ENTRY, payload, 0xff83, 0, None)
    reserved = AREA + payload
    image.record(reserved + 32, 'USER', 65535, FLASHLIMIT - user, user, 0x8192, 1, 0x4f545541)
    image.record(reserved, 'EXT_RESERVED', crc(image.b, reserved + 32, 32), 32, 64, 0xff93, 1, None)
    p = 0
    while p < payload + 64:
        cipher(image.b, AREA + p, min(32, payload + 64 - p), PUBLIC ^ (p >> 2))
        p += 32
    return image


def wrap(flash, loader, product):
    if not 1 <= len(loader) < 32768:
        sys.exit('Invalid FM1 update loader.')
    loader_at = 1024 + roundup(len(flash.b), 32)
    total = loader_at + roundup(len(loader), 32)
    ufw = Image(total, 255)
    ufw.b[0:224] = bytes(224)
    for i, (f, name) in enumerate(((bytes(flash.b), 'flash.bin'), (loader, 'ota.bin'))):
        r = 64 + i * 80
        ufw.number(r, 0 if i == 0 else 100, 2)
        ufw.number(r + 2, i, 2)
        ufw.number(r + 4, crc(f, 0, len(f)), 2)
        ufw.number(r + 8, 1024 if i == 0 else loader_at, 4)
        ufw.number(r + 12, len(f), 4)
        ufw.number(r + 16, roundup(len(f), 32), 4)
        ufw.text(r + 64, name)
        cipher(ufw.b, r, 80, 0xffff)
    ufw.number(4, total, 4)
    ufw.number(8, 2, 2)
    ufw.number(10, 4, 2)
    ufw.number(12, 512, 4)
    ufw.text(16, 'AC791N')
    ufw.checksum(2, 64, 160)
    ufw.checksum(0, 2, 62)
    cipher(ufw.b, 0, 64, 0xffff)
    ufw.copy(1024, flash.b)
    ufw.copy(loader_at, loader)
    out = bytearray()
    for src in range(total):
        out.append(ufw.b[src])
        if src < 940 and (src + 1) % 47 == 0:
            idx = src // 47
            out.append((ord(product[idx]) + idx + 1) & 0xff if idx < len(product) else 125)
    return bytes(out)


def read_elf(path):
    """The four packaged sections in Build.ps1's order (what objcopy -O binary
    -j writes for each) and the symbol table."""
    e = open(path, 'rb').read()
    if e[:6] != b'\x7fELF\x01\x01':
        sys.exit('Expected an ELF32 little-endian file: ' + path)
    entry, = struct.unpack_from('<I', e, 24)
    shoff, = struct.unpack_from('<I', e, 32)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', e, 46)
    sh = [struct.unpack_from('<IIIIIIIIII', e, shoff + i * shentsize) for i in range(shnum)]
    strtab = sh[shstrndx]

    def name(table, off):
        start = table[4] + off
        return e[start:e.index(b'\0', start)].decode('ascii', 'replace')
    sections = {name(strtab, s[0]): s for s in sh}
    parts = []
    for n in SECTIONS:
        s = sections.get(n)
        if s and s[5] and s[1] != 8:          # present, not empty, not NOBITS
            parts.append((n, s[3], bytearray(e[s[4]:s[4] + s[5]])))
    symbols = {}
    for s in sh:
        if s[1] == 2:                         # SHT_SYMTAB
            names = sh[s[6]]
            for i in range(s[5] // 16):
                st_name, value, size, info, other, shndx = struct.unpack_from('<IIIBBH', e, s[4] + i * 16)
                if st_name:
                    symbols[name(names, st_name)] = (value, size)
    return entry, parts, symbols


def patch_layout(parts, symbols, user):
    """Writes the new partition size into fm1_layout (FM1 Open's Build.ps1
    links it with 536576); the SDK only accepts a partition of that size."""
    if 'fm1_layout' not in symbols:
        sys.exit('fm1_layout not found in the firmware.')
    address, size = symbols['fm1_layout']
    for _, base, data in parts:
        if base <= address and address + 8 <= base + len(data):
            off = address - base
            old_user, family = struct.unpack_from('<II', data, off)
            if family != UPDATE_FAMILY or old_user not in (FM1_OPEN_USER, user):
                sys.exit(f'Unexpected fm1_layout ({old_user}, {family}).')
            struct.pack_into('<I', data, off, user)
            return
    sys.exit('fm1_layout is not in a packaged section.')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('fm1_open', help='FM1 Open checkout prepared with tools/prepare_fm1.py and built with Build.ps1')
    ap.add_argument('--user-kb', type=int, default=DEFAULT_USER_KB,
                    help=f'user partition in KB (multiple of 4, at least 32; default {DEFAULT_USER_KB}). '
                         'Keep it the same from update to update.')
    ap.add_argument('--elf', help='firmware ELF (default: <fm1_open>/build/dx7/sdk.elf)')
    ap.add_argument('--out', help='output (default: <fm1_open>/build/dx7/generated/FM1_ELECTRO.fwsc)')
    a = ap.parse_args()
    root = a.fm1_open
    user = a.user_kb * 1024
    if a.user_kb % 4 or user < 32768 or user >= FM1_OPEN_USER:
        sys.exit('--user-kb: a multiple of 4 from 32 up to 520.')
    elf = a.elf or os.path.join(root, 'build', 'dx7', 'sdk.elf')
    out = a.out or os.path.join(root, 'build', 'dx7', 'generated', 'FM1_ELECTRO.fwsc')
    entry, parts, symbols = read_elf(elf)
    if entry != ENTRY:
        sys.exit('Firmware entry does not match FM1 Open.')
    if 'el_fm1_render' not in symbols:
        sys.exit('This is not an FM1-Electro firmware (run tools/prepare_fm1.py, then Build.ps1).')
    patch_layout(parts, symbols, user)
    app = b''.join(bytes(data) for _, _, data in parts)
    version = open(os.path.join(root, 'app', 'version.h'), encoding='utf-8').read()
    m = re.search(r'^#define FM1_UPDATE_PRODUCT "(FM-1_8[0-9]{6})"\s*$', version, re.M)
    if not m:
        sys.exit('FM1_UPDATE_PRODUCT not found in app/version.h.')
    product = m.group(1)
    res = os.path.join(root, 'sdk', 'tools', 'pack')
    limit = FLASHLIMIT - user - BTSIZE - MINVM - AREA - APPDATA - 383 - 64
    if len(app) > limit:
        sys.exit(f'Firmware {len(app)} bytes, room for {limit} next to {a.user_kb} KB user partition.')
    size = roundup(AREA + APPDATA + len(app) + 383 + 64, 4096)
    vm_end = FLASHLIMIT - user - BTSIZE
    if size > vm_end - MINVM:
        sys.exit('Insufficient FM1 settings space.')
    flash = build_flash(app, res, size, vm_end, user)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    open(out, 'wb').write(wrap(flash, read_res(res, 'ota.bin'), product))
    print(f'{out}: firmware {len(app)} bytes (room for {limit}), settings (VM) {vm_end - size} bytes, '
          f'user partition {user} bytes')


if __name__ == '__main__':
    main()
