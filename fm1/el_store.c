/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "el_store.h"
#include "fm1.h"
#include <stddef.h>
#include <string.h>

/* The four slots (A-D) of every area and the settings are stored, so the
 * grooves come back after a restart. Records of earlier versions (24 slots
 * shared by all areas) still load: slot A takes the groove that played
 * last, B-D the user slots 17-19.
 * Version history: 12 base layout; 13 note loops (the looper now lives on
 * the branch "looper"; only its arpeggiator settings are taken); 14 the
 * arpeggiator settings; 15 toms' knobs reordered (old_tom_params); 16 eight
 * chord types instead of 13 (st_chord_v15); 17 MIDI channel per area; 18
 * OPTIONS; 19 four slots per area, each area with its own current slot; 20
 * the master compressor settings in OPTIONS (the same layout as 19, which
 * left those bytes zero). */
enum { MAGIC = 0x43444c45u /* "ELDC" */, VERSION = 20, VERSION_19 = 19, VERSION_18 = 18, VERSION_17 = 17, VERSION_16 = 16, VERSION_15 = 15,
       VERSION_14 = 14, VERSION_13 = 13, VERSION_12 = 12, PARAMS = 10, KIT_PARAMS = 4, OLD_SLOTS = 24, OLD_USER = 16 };

typedef struct {
    st_pattern_t pattern[ST_PATTERNS];
    uint8_t v[SYN_VALUES];
    uint8_t engine, preset, level, muted, chord, current;
} track_payload_t;

typedef struct {
    dr_pattern_t pattern[DR_PATTERNS];
    uint8_t params[DR_PARTS][PARAMS];
    uint8_t kit_params[KIT_PARAMS];
    uint8_t kit, level, current, muted;
    uint16_t bpm_x10, reserved;
    track_payload_t track[EL_TRACKS];
    uint8_t fx[FX_PARAMS];
    uint8_t arp[EL_TRACKS][ARP_PARAMS];
    uint8_t midi_channel[EL_AREAS];
    uint8_t option[EL_OPTIONS];
} payload_t;

/* Versions 12-18: 24 slots. Fields of version 12; the later versions append
 * to them. Each version is its own struct, so the sizes include the
 * compiler's padding exactly as that version wrote them. */
typedef struct {
    st_pattern_t pattern[OLD_SLOTS];
    uint8_t v[SYN_VALUES];
    uint8_t engine, preset, level, muted, chord;
} old_track_t;
#define FIELDS_V12                         \
    dr_pattern_t pattern[OLD_SLOTS];       \
    uint8_t params[DR_PARTS][PARAMS];      \
    uint8_t kit_params[KIT_PARAMS];        \
    uint8_t kit, level, current, muted;    \
    uint16_t bpm_x10, reserved;            \
    old_track_t track[EL_TRACKS];          \
    uint8_t fx[FX_PARAMS];

typedef struct { FIELDS_V12 } payload_v12_t;
/* Version 13 per area: arpeggiator, loop bars, muted, count, 192 events. */
typedef struct { uint8_t arp[ARP_PARAMS], bars, muted; uint16_t count; uint8_t loop[192][3]; } area_v13_t;
typedef struct { FIELDS_V12 area_v13_t area[EL_TRACKS]; } payload_v13_t;
typedef struct { FIELDS_V12 uint8_t arp[EL_TRACKS][ARP_PARAMS]; } payload_v16_t;   /* versions 14-16 */
#define FIELDS_V17 FIELDS_V12 uint8_t arp[EL_TRACKS][ARP_PARAMS]; uint8_t midi_channel[EL_AREAS];
typedef struct { FIELDS_V17 } payload_v17_t;
typedef struct { FIELDS_V17 uint8_t option[EL_OPTIONS]; } payload_v18_t;
/* Before version 15 the toms stored Level, Tune, Decay, Sound, Pan, Rev,
 * Dly; now Pitch, Bend, Decay, Noise, Level, Sound, Pan, Rev, Dly. Index
 * of each old knob in the new order: */
static const uint8_t old_tom_params[7] = {4, 0, 2, 5, 6, 7, 8};
#define V12_BYTES sizeof(payload_v12_t)
#define V13_BYTES sizeof(payload_v13_t)
#define V16_BYTES sizeof(payload_v16_t)
#define V17_BYTES sizeof(payload_v17_t)
#define V18_BYTES sizeof(payload_v18_t)

typedef struct {
    uint32_t magic, sequence;
    uint16_t version, bytes;
    uint32_t crc;
    payload_t data;
} record_t;
/* Each copy spans SECTORS sectors; two copies alternate. Where they lie
 * depends on the user partition:
 * - larger than FM1 Open's 536576 bytes (GROOVE_FLASH_BYTES; the simulators):
 *   right after FM1 Open's data;
 * - exactly FM1 Open's: its last project slots (sectors 15 - 2 * SECTORS .. 14);
 * - smaller (FM1-Electro's own layout, tools/package_fm1.py): the last
 *   OWN_SECTORS sectors of the partition. The partition always ends at the
 *   same Flash address, so the grooves stay put if its size changes.
 * Three sectors, as in every version so far: the places stay where they were. */
enum { SECTORS = 3, FM1_OPEN_BYTES = 536576u, FALLBACK_SECTOR = 15 - 2 * SECTORS, OWN_SECTORS = 2 * SECTORS + 2 };
_Static_assert(sizeof(record_t) <= SECTORS * FM1_FLASH_SECTOR_BYTES, "record fits its sectors");

/* User sound bank: one sector per copy. */
enum { BANK_MAGIC = 0x42534c45u /* "ELSB" */, BANK_VERSION = 2, BANK_VERSION_1 = 1 };   /* 1: 13 chords */
typedef struct { char name[EL_SOUND_NAME]; uint8_t engine, used; uint8_t v[SYN_VALUES]; } sound_slot_t;
typedef struct {
    uint32_t magic, sequence;
    uint16_t version, count;
    uint32_t crc;
    sound_slot_t slot[EL_USER_SOUNDS];
} bank_t;
_Static_assert(sizeof(bank_t) <= FM1_FLASH_SECTOR_BYTES, "sound bank fits one sector");

/* Records of versions before 19 (24 shared slots, up to 12 KB) are read
 * and upgraded where they can exist: in the simulator's browser storage
 * and the host tests. The device firmware started at version 19, so there
 * the buffer only needs today's record (2 KB) and saves about 11 KB SRAM. */
#ifndef EL_STORE_UPGRADE
#ifdef FM1_HOST_TEST
#define EL_STORE_UPGRADE 1
#else
#define EL_STORE_UPGRADE 0
#endif
#endif

/* Static: too large for the control task stack. Room for the larger
 * records of earlier versions when one is read. */
static union {
    record_t r;
#if EL_STORE_UPGRADE
    uint8_t v13[offsetof(record_t, data) + V13_BYTES];
    uint8_t v18[offsetof(record_t, data) + V18_BYTES];
#endif
    bank_t bank;
} buffer;
_Static_assert(sizeof buffer <= SECTORS * FM1_FLASH_SECTOR_BYTES, "every record fits three sectors");
#define record buffer.r
#if EL_STORE_UPGRADE
static payload_t upgraded;   /* an older record in today's layout */
#endif

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

/* First byte of FM1-Electro's own area in a partition smaller than FM1
 * Open's: sound bank copies, then the two groove copies. */
static uint32_t own_area(void) { return fm1_flash_size() - OWN_SECTORS * FM1_FLASH_SECTOR_BYTES; }

uint32_t el_store_address(unsigned which)
{
    uint32_t size = fm1_flash_size();
    if (size >= FM1_OPEN_BYTES + 2u * SECTORS * FM1_FLASH_SECTOR_BYTES)
        return FM1_OPEN_BYTES + which * SECTORS * FM1_FLASH_SECTOR_BYTES;
    if (size < FM1_OPEN_BYTES) return own_area() + (2u + which * SECTORS) * FM1_FLASH_SECTOR_BYTES;
    return (FALLBACK_SECTOR + which * SECTORS) * FM1_FLASH_SECTOR_BYTES;
}

/* A sound's chord memory (0-based) from the 13 chords of version 15 and
 * user sound bank 1 to today's eight. */
static void old_chord_memory(unsigned engine, uint8_t *v)
{
    unsigned p = engine == SYN_JP6 ? JP_CHORD : MP_CHORD;
    v[p] = v[p] < 13u ? (uint8_t)(st_chord_v15[v[p] + 1u] - 1u) : 0u;
}

/* Reads one sector into record; returns its sequence, or 0 when invalid. */
static uint32_t read_valid(unsigned which)
{
    if (fm1_flash_read(el_store_address(which), &buffer, sizeof buffer) != FM1_FLASH_OK) return 0;
    if (record.magic != MAGIC) return 0;
    int current = record.version >= VERSION_19 && record.version <= VERSION && record.bytes == sizeof(payload_t);
    if (!EL_STORE_UPGRADE && !current) return 0;
    if (!current &&
        !(record.version == VERSION_18 && record.bytes == V18_BYTES) &&
        !(record.version == VERSION_17 && record.bytes == V17_BYTES) &&
        !(record.version >= VERSION_14 && record.version <= VERSION_16 && record.bytes == V16_BYTES) &&
        !(record.version == VERSION_13 && record.bytes == V13_BYTES) &&
        !(record.version == VERSION_12 && record.bytes == V12_BYTES)) return 0;
    if (crc32(&record.data, record.bytes) != record.crc) return 0;
    return record.sequence;
}

/* An older record (24 slots shared by all areas) in today's layout: slot A
 * from the groove that played last, B-D from the user slots 17-19. */
static const payload_t *upgrade(void)
{
    unsigned version = record.version;
    if (version >= VERSION_19) return &record.data;
#if !EL_STORE_UPGRADE
    return &record.data;   /* never reached: read_valid takes only today's records */
#else
    const payload_v12_t *o = (const payload_v12_t *)(const void *)&record.data;
    payload_t *d = &upgraded;
    memset(d, 0, sizeof *d);
    unsigned map[DR_PATTERNS], n = 0;
    map[n++] = o->current % OLD_SLOTS;
    for (unsigned k = OLD_USER; k < OLD_SLOTS && n < DR_PATTERNS; ++k) if (k != map[0]) map[n++] = k;
    for (unsigned i = 0; i < DR_PATTERNS; ++i) {
        d->pattern[i] = o->pattern[map[i]];
        for (unsigned t = 0; t < EL_TRACKS; ++t) d->track[t].pattern[i] = o->track[t].pattern[map[i]];
    }
    memcpy(d->params, o->params, sizeof d->params);
    memcpy(d->kit_params, o->kit_params, sizeof d->kit_params);
    d->kit = o->kit;
    d->level = o->level;
    d->muted = o->muted;
    d->bpm_x10 = o->bpm_x10;
    for (unsigned t = 0; t < EL_TRACKS; ++t) {
        memcpy(d->track[t].v, o->track[t].v, sizeof d->track[t].v);
        d->track[t].engine = o->track[t].engine;
        d->track[t].preset = o->track[t].preset;
        d->track[t].level = o->track[t].level;
        d->track[t].muted = o->track[t].muted;
        d->track[t].chord = o->track[t].chord;
    }
    memcpy(d->fx, o->fx, sizeof d->fx);
    for (unsigned t = 0; t < EL_TRACKS; ++t) {
        if (version == VERSION_13) memcpy(d->arp[t], ((const payload_v13_t *)(const void *)o)->area[t].arp, ARP_PARAMS);
        else if (version >= VERSION_14) memcpy(d->arp[t], ((const payload_v16_t *)(const void *)o)->arp[t], ARP_PARAMS);
    }
    if (version >= VERSION_17) memcpy(d->midi_channel, ((const payload_v17_t *)(const void *)o)->midi_channel, EL_AREAS);
    if (version >= VERSION_18) memcpy(d->option, ((const payload_v18_t *)(const void *)o)->option, EL_OPTIONS);
    return d;
#endif
}

int el_store_load(el_engine_t *e)
{
    if (!fm1_flash_size()) return FM1_FLASH_UNAVAILABLE;
    uint32_t a = read_valid(0), b = read_valid(1);
    if (!a && !b) return 1;
    if ((int32_t)(a - b) > 0 && !read_valid(0)) return 1; /* reload the newer one */
    unsigned version = record.version;
    const payload_t *d = upgrade();
    dr_machine_t *m = &e->drums;
    int old_chords = version < VERSION_16;
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
        int tom = (part == D8_LT || part == D8_HT) && version < VERSION_15;
        for (int p = 0; p < (tom ? 7 : tr808_nparams((int)part)) && p < PARAMS; ++p)
            tr808_set(&m->kit, (int)part, tom ? old_tom_params[p] : p, d->params[part][p]);
    }
    for (int p = 0; p < KIT_PARAMS; ++p) tr808_set(&m->kit, D8_KIT, p, d->kit_params[p]);
    m->level_value = (uint8_t)el_clampi(d->level, 0, 127);
    m->level = m->level_value / 127.0f;
    if (version >= VERSION_18) {   /* older records keep el_init's options */
        /* Before version 20 only the first options existed; the compressor
         * keeps el_init's settings. */
        unsigned count = version >= VERSION ? EL_OPTIONS : EL_OPT_COMP_THRESHOLD;
        for (unsigned i = 0; i < count; ++i) e->option[i] = (uint8_t)el_clampi(d->option[i], 0, el_option_max(i));
        el_apply_comp(e);
        e->mix.enabled = e->option[EL_OPT_MASTER] ? 1 : 0;
        e->master = e->option[EL_OPT_OUT_LEVEL] / 127.0f;
    }
    /* OPTIONS POWER-ON: every area on slot A instead of where it was. */
    int slot_a = e->option[EL_OPT_START] == EL_START_SLOT_A;
    m->current = m->next = (uint8_t)(slot_a ? 0u : d->current % DR_PATTERNS);
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
                if (old_chords) type = type < 14u ? st_chord_v15[type] : 0u;
                t->pattern[p].step[k].chord = (uint8_t)((type > MKS_CHORDS ? 0u : type) | (inv > 2u ? 0u : inv) << 4);
            }
        }
        uint8_t v[SYN_VALUES];
        unsigned engine = s->engine < SYN_ENGINES ? s->engine : SYN_MKS;
        memcpy(v, s->v, sizeof v);
        if (old_chords) old_chord_memory(engine, v);
        for (unsigned p = 0; p < syn_value_count(engine); ++p) v[p] = (uint8_t)el_clampi(v[p], 0, syn_param_max(engine, p));
        syn_load(&t->part, engine, v);
        int user = s->preset >= EL_USER_SOUND && s->preset < EL_USER_SOUND + EL_USER_SOUNDS;
        t->preset = (uint8_t)(s->preset < syn_sound_count() || user ? s->preset : 0);
        t->level_value = (uint8_t)el_clampi(s->level, 0, 127);
        t->muted = s->muted ? 1 : 0;
        t->chord = (uint8_t)el_clampi(old_chords && s->chord < 14u ? st_chord_v15[s->chord] : s->chord, 0, MKS_CHORDS);
        t->current = t->next = (uint8_t)(slot_a ? 0u : s->current % ST_PATTERNS);
    }
    for (unsigned p = 0; p < FX_PARAMS; ++p) e->fx.v[p] = (uint8_t)el_clampi(d->fx[p], 0, fx_param_max(p));
    if (version >= VERSION_13)
        for (unsigned i = 0; i < EL_TRACKS; ++i)
            for (unsigned p = 0; p < ARP_PARAMS; ++p)
                e->track[i].arp.v[p] = (uint8_t)el_clampi(d->arp[i][p], p == ARP_GATE ? 5 : 0, arp_param_max(p));
    if (version >= VERSION_17)   /* older records keep el_init's channels */
        for (unsigned a = 0; a < EL_AREAS; ++a) e->midi_channel[a] = (uint8_t)el_clampi(d->midi_channel[a], 0, EL_MIDI_OFF);
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
    d->current = m->next;
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
        s->current = t->next;
    }
    memcpy(d->fx, e->fx.v, sizeof d->fx);
    for (unsigned i = 0; i < EL_TRACKS; ++i) memcpy(d->arp[i], e->track[i].arp.v, ARP_PARAMS);
    for (unsigned a = 0; a < EL_AREAS; ++a) d->midi_channel[a] = e->midi_channel[a];
    for (unsigned i = 0; i < EL_OPTIONS; ++i) d->option[i] = e->option[i];
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

/* After the two groove copies when the partition has room there; in
 * FM1-Electro's own layout at the start of its area; otherwise the FM1 Open
 * project slots just below the groove fallback (7 and 8). */
uint32_t el_sound_address(unsigned which)
{
    uint32_t size = fm1_flash_size();
    if (size >= FM1_OPEN_BYTES + (2u * SECTORS + 2u) * FM1_FLASH_SECTOR_BYTES)
        return FM1_OPEN_BYTES + (2u * SECTORS + which) * FM1_FLASH_SECTOR_BYTES;
    if (size < FM1_OPEN_BYTES) return own_area() + which * FM1_FLASH_SECTOR_BYTES;
    return (FALLBACK_SECTOR - 2u + which) * FM1_FLASH_SECTOR_BYTES;
}

static uint32_t bank_valid(unsigned which)
{
    bank_t *k = &buffer.bank;
    if (fm1_flash_read(el_sound_address(which), k, sizeof *k) != FM1_FLASH_OK) return 0;
    if (k->magic != BANK_MAGIC || (k->version != BANK_VERSION && k->version != BANK_VERSION_1) || k->count != EL_USER_SOUNDS) return 0;
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
    bank_t *k = &buffer.bank;
    if (k->version == BANK_VERSION_1) {   /* in the buffer only; the next put writes version 2 */
        for (unsigned i = 0; i < EL_USER_SOUNDS; ++i)
            if (k->slot[i].used && k->slot[i].engine < SYN_ENGINES) old_chord_memory(k->slot[i].engine, k->slot[i].v);
        k->version = BANK_VERSION;
    }
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

