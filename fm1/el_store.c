/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "el_store.h"
#include "fm1.h"
#include <stddef.h>
#include <string.h>

/* All 24 slots (generated style slots and user slots) and the settings are
 * stored, so the last grooves come back after a restart. Version 14 appends
 * the arpeggiator settings. Older records still load: version 12 is the same
 * without them, version 13 also carried note loops (the looper now lives on
 * the branch "looper"), of which only the arpeggiator settings are taken.
 * Version 15 has the layout of 14; the toms' knobs were reordered (see
 * old_tom_params). */
enum { MAGIC = 0x43444c45u /* "ELDC" */, VERSION = 15, VERSION_14 = 14, VERSION_13 = 13, VERSION_12 = 12, PARAMS = 10, KIT_PARAMS = 4 };

typedef struct {
    st_pattern_t pattern[ST_PATTERNS];
    uint8_t v[SYN_VALUES];
    uint8_t engine, preset, level, muted, chord;
} track_payload_t;

/* Fields of version 12; the later versions append to them. Each version is
 * its own struct, so the sizes include the compiler's padding exactly as
 * that version wrote them. */
#define FIELDS_V12                         \
    dr_pattern_t pattern[DR_PATTERNS];     \
    uint8_t params[DR_PARTS][PARAMS];      \
    uint8_t kit_params[KIT_PARAMS];        \
    uint8_t kit, level, current, muted;    \
    uint16_t bpm_x10, reserved;            \
    track_payload_t track[EL_TRACKS];      \
    uint8_t fx[FX_PARAMS];

typedef struct { FIELDS_V12 } payload_v12_t;
/* Version 13 per area: arpeggiator, loop bars, muted, count, 192 events. */
typedef struct { uint8_t arp[ARP_PARAMS], bars, muted; uint16_t count; uint8_t loop[192][3]; } area_v13_t;
typedef struct { FIELDS_V12 area_v13_t area[EL_TRACKS]; } payload_v13_t;
typedef struct { FIELDS_V12 uint8_t arp[EL_TRACKS][ARP_PARAMS]; } payload_t;   /* versions 14, 15 */
/* Before version 15 the toms stored Level, Tune, Decay, Sound, Pan, Rev,
 * Dly; now Pitch, Bend, Decay, Noise, Level, Sound, Pan, Rev, Dly. Index
 * of each old knob in the new order: */
static const uint8_t old_tom_params[7] = {4, 0, 2, 5, 6, 7, 8};
#define V12_BYTES sizeof(payload_v12_t)
#define V13_BYTES sizeof(payload_v13_t)

typedef struct {
    uint32_t magic, sequence;
    uint16_t version, bytes;
    uint32_t crc;
    payload_t data;
} record_t;
/* Each copy spans SECTORS sectors; two copies alternate. FM1 Open's own data
 * ends at 536576 bytes (GROOVE_FLASH_BYTES); without room after it, the
 * copies use the last FM1 Open project slots (sectors 15 - 2 * SECTORS .. 14). */
enum { SECTORS = (sizeof(record_t) + FM1_FLASH_SECTOR_BYTES - 1) / FM1_FLASH_SECTOR_BYTES,
       FM1_OPEN_BYTES = 536576u, FALLBACK_SECTOR = 15 - 2 * SECTORS };
_Static_assert(SECTORS == 3, "three sectors per copy, as in every version so far");

/* Static: too large for the control task stack. Room for the larger
 * version 13 record when an old one is read. */
/* User sound bank: one sector per copy. */
enum { BANK_MAGIC = 0x42534c45u /* "ELSB" */, BANK_VERSION = 1 };
typedef struct { char name[EL_SOUND_NAME]; uint8_t engine, used; uint8_t v[SYN_VALUES]; } sound_slot_t;
typedef struct {
    uint32_t magic, sequence;
    uint16_t version, count;
    uint32_t crc;
    sound_slot_t slot[EL_USER_SOUNDS];
} bank_t;
_Static_assert(sizeof(bank_t) <= FM1_FLASH_SECTOR_BYTES, "sound bank fits one sector");

static union {
    record_t r;
    uint8_t v13[offsetof(record_t, data) + V13_BYTES];
    bank_t bank;
} buffer;
_Static_assert(sizeof buffer <= 3 * FM1_FLASH_SECTOR_BYTES, "version 13 record fits three sectors");
#define record buffer.r

static uint32_t crc32(const void *data, size_t bytes)
{
    const uint8_t *p = data;
    uint32_t crc = 0xffffffffu;
    while (bytes--) {
        crc ^= *p++;
        for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

uint32_t el_store_address(unsigned which)
{
    if (fm1_flash_size() >= FM1_OPEN_BYTES + 2u * SECTORS * FM1_FLASH_SECTOR_BYTES)
        return FM1_OPEN_BYTES + which * SECTORS * FM1_FLASH_SECTOR_BYTES;
    return (FALLBACK_SECTOR + which * SECTORS) * FM1_FLASH_SECTOR_BYTES;
}

/* Reads one sector into record; returns its sequence, or 0 when invalid. */
static uint32_t read_valid(unsigned which)
{
    if (fm1_flash_read(el_store_address(which), &buffer, sizeof buffer) != FM1_FLASH_OK) return 0;
    if (record.magic != MAGIC) return 0;
    if (!((record.version == VERSION || record.version == VERSION_14) && record.bytes == sizeof(payload_t)) &&
        !(record.version == VERSION_13 && record.bytes == V13_BYTES) &&
        !(record.version == VERSION_12 && record.bytes == V12_BYTES)) return 0;
    if (crc32(&record.data, record.bytes) != record.crc) return 0;
    return record.sequence;
}

/* Arpeggiator settings (version 13 and later). */
static void load_areas(el_engine_t *e, const payload_t *d, unsigned version)
{
    for (unsigned i = 0; i < EL_TRACKS; ++i) {
        const uint8_t *arp = version == VERSION_13 ? ((const payload_v13_t *)(const void *)d)->area[i].arp : d->arp[i];
        for (unsigned p = 0; p < ARP_PARAMS; ++p)
            e->track[i].arp.v[p] = (uint8_t)el_clampi(arp[p], p == ARP_GATE ? 5 : 0, arp_param_max(p));
    }
}

int el_store_load(el_engine_t *e)
{
    if (!fm1_flash_size()) return FM1_FLASH_UNAVAILABLE;
    uint32_t a = read_valid(0), b = read_valid(1);
    if (!a && !b) return 1;
    if ((int32_t)(a - b) > 0 && !read_valid(0)) return 1; /* reload the newer one */
    dr_machine_t *m = &e->drums;
    const payload_t *d = &record.data;
    for (unsigned i = 0; i < DR_PATTERNS; ++i) {
        dr_pattern_t p = d->pattern[i];
        p.name[DR_NAME] = 0;
        p.length = (uint8_t)el_clampi(p.length, 1, DR_STEPS);
        p.swing = (uint8_t)el_clampi(p.swing, 50, 75);
        p.kit = (uint8_t)(p.kit % DR_KITS);
        p.style = (uint8_t)(p.style % DR_STYLES);
        m->pattern[i] = p;
    }
    /* Audio is not running yet: the kit can be set directly. DR_CMD_KIT also
     * stores the kit in the current pattern, so select that one first. */
    m->current = m->next = (uint8_t)(d->current % DR_PATTERNS);
    dr_post(m, DR_CMD_KIT, (uint8_t)(d->kit % DR_KITS), 0, 0, 0);
    dr_process_commands(m);
    for (unsigned part = 0; part < DR_PARTS; ++part) {
        int tom = (part == D8_LT || part == D8_HT) && record.version < VERSION;
        for (int p = 0; p < (tom ? 7 : tr808_nparams((int)part)) && p < PARAMS; ++p)
            tr808_set(&m->kit, (int)part, tom ? old_tom_params[p] : p, d->params[part][p]);
    }
    for (int p = 0; p < KIT_PARAMS; ++p) tr808_set(&m->kit, D8_KIT, p, d->kit_params[p]);
    m->level_value = (uint8_t)el_clampi(d->level, 0, 127);
    m->level = m->level_value / 127.0f;
    m->current = m->next = (uint8_t)(d->current % DR_PATTERNS);
    e->transport.swing = m->pattern[m->current].swing;
    m->muted = d->muted ? 1 : 0;
    el_transport_set_bpm(&e->transport, el_clampi(d->bpm_x10, 300, 3000) * 0.1f);
    for (unsigned i = 0; i < EL_TRACKS; ++i) {
        st_track_t *t = &e->track[i];
        const track_payload_t *s = &d->track[i];
        for (unsigned p = 0; p < ST_PATTERNS; ++p) {
            t->pattern[p] = s->pattern[p];
            t->pattern[p].length = (uint8_t)el_clampi(t->pattern[p].length, 1, ST_STEPS);
            t->pattern[p].gate = (uint8_t)el_clampi(t->pattern[p].gate, 10, 100);
            t->pattern[p].rate = (uint8_t)el_clampi(t->pattern[p].rate, 0, ST_RATES - 1);
            for (unsigned k = 0; k < ST_STEPS; ++k)
            {
                unsigned chord = t->pattern[p].step[k].chord, type = ST_CHORD_TYPE(chord), inv = ST_CHORD_INV(chord);
                t->pattern[p].step[k].chord = (uint8_t)((type > MKS_CHORDS ? 0u : type) | (inv > 2u ? 0u : inv) << 4);
            }
        }
        uint8_t v[SYN_VALUES];
        unsigned engine = s->engine < SYN_ENGINES ? s->engine : SYN_MKS;
        for (unsigned p = 0; p < syn_value_count(engine); ++p) v[p] = (uint8_t)el_clampi(s->v[p], 0, syn_param_max(engine, p));
        syn_load(&t->part, engine, v);
        int user = s->preset >= EL_USER_SOUND && s->preset < EL_USER_SOUND + EL_USER_SOUNDS;
        t->preset = (uint8_t)(s->preset < syn_sound_count() || user ? s->preset : 0);
        t->level_value = (uint8_t)el_clampi(s->level, 0, 127);
        t->muted = s->muted ? 1 : 0;
        t->chord = (uint8_t)el_clampi(s->chord, 0, MKS_CHORDS);
    }
    for (unsigned p = 0; p < FX_PARAMS; ++p) e->fx.v[p] = (uint8_t)el_clampi(d->fx[p], 0, fx_param_max(p));
    if (record.version >= VERSION_13) load_areas(e, d, record.version);
    return 0;
}

int el_store_save(el_engine_t *e)
{
    uint32_t a = read_valid(0), b = read_valid(1);
    uint32_t sequence = ((int32_t)(a - b) > 0 ? a : b) + 1u;
    unsigned target = (int32_t)(a - b) > 0 ? 1u : 0u; /* overwrite the older one */
    if (!a && !b) target = 0;
    const dr_machine_t *m = &e->drums;
    memset(&record, 0, sizeof record);
    payload_t *d = &record.data;
    memcpy(d->pattern, m->pattern, sizeof d->pattern);
    for (unsigned part = 0; part < DR_PARTS; ++part)
        for (int p = 0; p < tr808_nparams((int)part) && p < PARAMS; ++p)
            d->params[part][p] = (uint8_t)tr808_get(&m->kit, (int)part, p);
    for (int p = 0; p < KIT_PARAMS; ++p) d->kit_params[p] = (uint8_t)tr808_get(&m->kit, D8_KIT, p);
    d->kit = m->kit_index;
    d->level = m->level_value;
    d->current = m->current;
    d->muted = m->muted;
    d->bpm_x10 = (uint16_t)(e->transport.bpm * 10.0f + 0.5f);
    for (unsigned i = 0; i < EL_TRACKS; ++i) {
        const st_track_t *t = &e->track[i];
        track_payload_t *s = &d->track[i];
        memcpy(s->pattern, t->pattern, sizeof s->pattern);
        memcpy(s->v, syn_values(&t->part), syn_value_count(t->part.engine));
        s->engine = t->part.engine;
        s->preset = t->preset;
        s->level = t->level_value;
        s->muted = t->muted;
        s->chord = t->chord;
    }
    memcpy(d->fx, e->fx.v, sizeof d->fx);
    for (unsigned i = 0; i < EL_TRACKS; ++i) memcpy(d->arp[i], e->track[i].arp.v, ARP_PARAMS);
    record.magic = MAGIC;
    record.sequence = sequence ? sequence : 1u;
    record.version = VERSION;
    record.bytes = sizeof(payload_t);
    record.crc = crc32(d, sizeof *d);
    uint32_t address = el_store_address(target);
    int r = 0;
    for (unsigned s = 0; s < SECTORS && !r; ++s) r = fm1_flash_erase(address + s * FM1_FLASH_SECTOR_BYTES);
    if (!r) r = fm1_flash_write(address, &record, sizeof record);
    return r;
}

/* ---- user sounds ----------------------------------------------------------- */

/* After the two groove copies when the partition has room there, otherwise
 * the FM1 Open project slots just below the groove fallback (7 and 8). */
uint32_t el_sound_address(unsigned which)
{
    if (fm1_flash_size() >= FM1_OPEN_BYTES + (2u * SECTORS + 2u) * FM1_FLASH_SECTOR_BYTES)
        return FM1_OPEN_BYTES + (2u * SECTORS + which) * FM1_FLASH_SECTOR_BYTES;
    return (FALLBACK_SECTOR - 2u + which) * FM1_FLASH_SECTOR_BYTES;
}

static uint32_t bank_valid(unsigned which)
{
    bank_t *k = &buffer.bank;
    if (fm1_flash_read(el_sound_address(which), k, sizeof *k) != FM1_FLASH_OK) return 0;
    if (k->magic != BANK_MAGIC || k->version != BANK_VERSION || k->count != EL_USER_SOUNDS) return 0;
    if (crc32(k->slot, sizeof k->slot) != k->crc) return 0;
    return k->sequence;
}

/* Newest valid copy into the buffer; returns its index, or -1 for none. */
static int bank_read(void)
{
    uint32_t a = bank_valid(0), b = bank_valid(1);
    if (!a && !b) return -1;
    unsigned newest = (int32_t)(a - b) > 0 || !b ? 0u : 1u;
    if (newest == 1u) bank_valid(1);   /* the buffer still holds copy 1 when b is valid */
    else bank_valid(0);
    return (int)newest;
}

int el_sound_get(unsigned slot, el_sound_t *out)
{
    if (slot >= EL_USER_SOUNDS || !out) return -1;
    memset(out, 0, sizeof *out);
    if (!fm1_flash_size()) return FM1_FLASH_UNAVAILABLE;
    if (bank_read() < 0) return 0;
    const sound_slot_t *s = &buffer.bank.slot[slot];
    if (!s->used || s->engine >= SYN_ENGINES) return 0;
    memcpy(out->name, s->name, EL_SOUND_NAME);
    out->name[EL_SOUND_NAME] = 0;
    out->engine = s->engine;
    out->used = 1;
    for (unsigned p = 0; p < syn_value_count(s->engine); ++p)
        out->v[p] = (uint8_t)el_clampi(s->v[p], 0, syn_param_max(s->engine, p));
    return 0;
}

int el_sound_put(unsigned slot, const el_sound_t *in)
{
    if (slot >= EL_USER_SOUNDS || !in || in->engine >= SYN_ENGINES) return -1;
    if (!fm1_flash_size()) return FM1_FLASH_UNAVAILABLE;
    bank_t *k = &buffer.bank;
    int current = bank_read();
    uint32_t sequence = current >= 0 ? k->sequence + 1u : 1u;
    if (current < 0) memset(k, 0, sizeof *k);
    sound_slot_t *s = &k->slot[slot];
    memset(s, 0, sizeof *s);
    memcpy(s->name, in->name, EL_SOUND_NAME);
    s->engine = in->engine;
    s->used = 1;
    memcpy(s->v, in->v, syn_value_count(in->engine));
    k->magic = BANK_MAGIC;
    k->version = BANK_VERSION;
    k->count = EL_USER_SOUNDS;
    k->sequence = sequence ? sequence : 1u;
    k->crc = crc32(k->slot, sizeof k->slot);
    uint32_t address = el_sound_address(current == 0 ? 1u : 0u);   /* overwrite the older copy */
    int r = fm1_flash_erase(address);
    if (!r) r = fm1_flash_write(address, k, sizeof *k);
    return r;
}

