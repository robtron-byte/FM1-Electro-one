/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "el_store.h"
#include "fm1.h"
#include <stddef.h>
#include <stdlib.h>
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
 * left those bytes zero); 21 a sound per slot (docs/adr/0024), appended:
 * records 19 and 20 are its first part, every slot gets their one sound; 22
 * the drums' step conditions (docs/adr/0025), appended again (the synths'
 * are in their step flags; records before 22 hold 0 there: always); 23 the
 * drums' step probabilities, appended (the synths' in their step bytes); 24
 * the drums' pitch and level locks (docs/adr/0028), appended; 25 the options
 * after the first twelve (the compressor's parallel share, docs/adr/0031);
 * 26 the drums without toms and congas (B12: ten parts, older records are
 * mapped part by part), every option in one array, no separate copy of the
 * playing kit (each slot has its sound), each kit as last set (N1), a length
 * per synth step (N6), a fill per drum slot (N12); 27 the pattern chain of
 * every area (S16), appended (a record of 26 is its first part: no chains). */
enum { MAGIC = 0x43444c45u /* "ELDC" */, VERSION = 27, VERSION_26 = 26, VERSION_25 = 25, VERSION_24 = 24, VERSION_23 = 23, VERSION_22 = 22, VERSION_21 = 21, VERSION_20 = 20, VERSION_19 = 19, VERSION_18 = 18, VERSION_17 = 17, VERSION_16 = 16, VERSION_15 = 15,
       VERSION_14 = 14, VERSION_13 = 13, VERSION_12 = 12, PARAMS = 10, KIT_PARAMS = 4, OLD_SLOTS = 24, OLD_USER = 16 };

typedef struct {
    st_pattern_t pattern[ST_PATTERNS];
    uint8_t v[SYN_VALUES];
    uint8_t engine, preset, level, muted, chord, current;
} track_payload_t;

/* Up to version 25 the drums had twelve parts: BD SD LT HT RS CP CB CY OH
 * CH ZP PU. Their patterns and tables in that layout, and where each old
 * part went (-1: the toms, gone). */
enum { OLD_PARTS = 12 };
/* Up to version 25 a synth step had no length (N6: version 26). */
typedef struct { uint8_t note, flags, chord; } st_step_v25_t;
typedef struct { uint8_t length, gate, rate; st_step_v25_t step[ST_STEPS]; } st_pattern_v25_t;
typedef struct {
    st_pattern_v25_t pattern[ST_PATTERNS];
    uint8_t v[SYN_VALUES];
    uint8_t engine, preset, level, muted, chord, current;
} track_payload_v25_t;
static void pattern_from_v25(const st_pattern_v25_t *o, st_pattern_t *n)
{
    memset(n, 0, sizeof *n);
    n->length = o->length; n->gate = o->gate; n->rate = o->rate;
    for (unsigned k = 0; k < ST_STEPS; ++k) { n->step[k].note = o->step[k].note; n->step[k].flags = o->step[k].flags; n->step[k].chord = o->step[k].chord; }
}
typedef struct {
    char name[DR_NAME + 1];
    uint8_t length, swing, kit, style;
    uint32_t hit[OLD_PARTS], roll[OLD_PARTS], accent;
} dr_pattern_v25_t;
static int new_part(unsigned old) { return old < 2u ? (int)old : old < 4u ? -1 : (int)old - 2; }

/* Versions 19 and 20; params, kit_params and track[].v hold the sound
 * that plays (version 21: of the playing slot). */
#define FIELDS_V19                         \
    dr_pattern_v25_t pattern[DR_PATTERNS]; \
    uint8_t params[OLD_PARTS][PARAMS];     \
    uint8_t kit_params[KIT_PARAMS];        \
    uint8_t kit, level, current, muted;    \
    uint16_t bpm_x10, reserved;            \
    track_payload_v25_t track[EL_TRACKS];  \
    uint8_t fx[FX_PARAMS];                 \
    uint8_t arp[EL_TRACKS][ARP_PARAMS];    \
    uint8_t midi_channel[EL_AREAS];        \
    uint8_t option[EL_OPTIONS_STORED];
typedef struct { FIELDS_V19 } payload_v19_t;

/* Version 21: each slot's sound (26: ten parts). */
typedef struct {
    uint8_t params[OLD_PARTS][PARAMS];
    uint8_t kit_params[KIT_PARAMS];
} drum_sound_v25_t;
typedef struct {
    uint8_t params[DR_PARTS][PARAMS];
    uint8_t kit_params[KIT_PARAMS];
} drum_sound_payload_t;
typedef struct {
    uint8_t v[SYN_VALUES];
    uint8_t engine, preset;
} synth_sound_payload_t;

#define FIELDS_V21                                              \
    FIELDS_V19                                                  \
    drum_sound_v25_t drum_sound[DR_PATTERNS];                   \
    synth_sound_payload_t synth_sound[EL_TRACKS][ST_PATTERNS];
typedef struct { FIELDS_V21 } payload_v21_t;
#define FIELDS_V22 FIELDS_V21 uint32_t drum_cond[DR_PATTERNS][3][OLD_PARTS];   /* dr_machine_t.cond */
typedef struct { FIELDS_V22 } payload_v22_t;
#define FIELDS_V23 FIELDS_V22 uint32_t drum_prob[DR_PATTERNS][4][OLD_PARTS];   /* dr_machine_t.prob */
typedef struct { FIELDS_V23 } payload_v23_t;
#define FIELDS_V24 FIELDS_V23                                           \
    uint32_t drum_pitch[DR_PATTERNS][5][DR_PITCH_PARTS];   /* dr_machine_t.pitch */ \
    uint32_t drum_vol[DR_PATTERNS][3][OLD_PARTS];          /* dr_machine_t.vol */
typedef struct { FIELDS_V24 } payload_v24_t;
typedef struct {
    FIELDS_V24
    uint8_t option_ext[EL_OPTIONS - EL_OPTIONS_STORED];   /* the options after the first EL_OPTIONS_STORED */
} payload_v25_t;

/* Version 26 (B12, N1). */
typedef struct {
    dr_pattern_t pattern[DR_PATTERNS];
    uint8_t kit, level, current, muted;
    uint16_t bpm_x10, reserved;            /* reserved: drum key | follow << 8 */
    track_payload_t track[EL_TRACKS];
    uint8_t fx[FX_PARAMS];
    uint8_t arp[EL_TRACKS][ARP_PARAMS];
    uint8_t midi_channel[EL_AREAS];
    uint8_t option[EL_OPTIONS];
    drum_sound_payload_t drum_sound[DR_PATTERNS];
    synth_sound_payload_t synth_sound[EL_TRACKS][ST_PATTERNS];
    uint32_t drum_cond[DR_PATTERNS][3][DR_PARTS];
    uint32_t drum_prob[DR_PATTERNS][4][DR_PARTS];
    uint32_t drum_pitch[DR_PATTERNS][5][DR_PITCH_PARTS];
    uint32_t drum_vol[DR_PATTERNS][3][DR_PARTS];
    drum_sound_payload_t kit_mem[DR_KITS];  /* dr_machine_t.kit_mem (N1) */
    uint8_t kit_mem_used;
    uint8_t fill_auto;                      /* every how many bars the fill plays by itself (N12) */
    dr_fill_t fill[DR_PATTERNS];            /* each slot's fill (N12) */
    /* version 27: each area's pattern chain (S16): links (0 none), then the slots */
    uint8_t chain[EL_AREAS][1 + DR_CHAIN];
} payload_t;
#define V26_BYTES offsetof(payload_t, chain)
_Static_assert(DR_CHAIN == ST_CHAIN, "one chain layout for every area");
#define V25_BYTES sizeof(payload_v25_t)
#define V24_BYTES sizeof(payload_v24_t)
#define V23_BYTES sizeof(payload_v23_t)
#define V22_BYTES sizeof(payload_v22_t)
#define V21_BYTES sizeof(payload_v21_t)
#define V19_BYTES sizeof(payload_v19_t)

/* Versions 12-18: 24 slots. Fields of version 12; the later versions append
 * to them. Each version is its own struct, so the sizes include the
 * compiler's padding exactly as that version wrote them. */
typedef struct {
    st_pattern_v25_t pattern[OLD_SLOTS];
    uint8_t v[SYN_VALUES];
    uint8_t engine, preset, level, muted, chord;
} old_track_t;
#define FIELDS_V12                         \
    dr_pattern_v25_t pattern[OLD_SLOTS];   \
    uint8_t params[OLD_PARTS][PARAMS];     \
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
typedef struct { FIELDS_V17 uint8_t option[EL_OPTIONS_STORED]; } payload_v18_t;
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

/* Too large for the control task stack. Room for the larger records of
 * earlier versions when one is read. Taken from the heap only while a
 * record or the sound bank is read or written: as a static it cost the
 * SDK's start 2.6 KB of heap it is short of (docs/adr/0019). */
typedef union {
    record_t r;
    /* Records 19-25: read into the front, converted into version 26 behind
     * them (the device has this one buffer only, fm1/el_app.c). */
    struct { uint8_t v25[offsetof(record_t, data) + V25_BYTES]; payload_t conv; } up;
#if EL_STORE_UPGRADE
    uint8_t v13[offsetof(record_t, data) + V13_BYTES];
    uint8_t v18[offsetof(record_t, data) + V18_BYTES];
#endif
    bank_t bank;
} store_buffer_t;
_Static_assert(sizeof(store_buffer_t) <= SECTORS * FM1_FLASH_SECTOR_BYTES, "every record fits three sectors");
static store_buffer_t *held;
#define buffer (*held)
static int take(void) { return (held = malloc(sizeof *held)) != NULL; }
static void give(void) { free(held); held = NULL; }
#define record buffer.r
#if EL_STORE_UPGRADE
static payload_v25_t upgraded;   /* a record before version 19 in version 25's layout */
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
    int current = (record.version == VERSION && record.bytes == sizeof(payload_t)) ||
                  (record.version == VERSION_26 && record.bytes == V26_BYTES) ||
                  (record.version == VERSION_25 && record.bytes == V25_BYTES) ||
                  (record.version == VERSION_24 && record.bytes == V24_BYTES) ||
                  (record.version == VERSION_23 && record.bytes == V23_BYTES) ||
                  (record.version == VERSION_22 && record.bytes == V22_BYTES) ||
                  (record.version == VERSION_21 && record.bytes == V21_BYTES) ||
                  (record.version >= VERSION_19 && record.version <= VERSION_20 && record.bytes == V19_BYTES);
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
static const payload_v25_t *upgrade(void)
{
    unsigned version = record.version;
    if (version >= VERSION_19) return (const payload_v25_t *)(const void *)&record.data;
#if !EL_STORE_UPGRADE
    return (const payload_v25_t *)(const void *)&record.data;   /* never reached: read_valid takes only versions from 19 */
#else
    const payload_v12_t *o = (const payload_v12_t *)(const void *)&record.data;
    payload_v25_t *d = &upgraded;
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
    if (version >= VERSION_18) memcpy(d->option, ((const payload_v18_t *)(const void *)o)->option, EL_OPTIONS_STORED);
    return d;
#endif
}

/* A slot's sound in the stored form (ten parts). */
static void sound_to_payload(const tr808_sound_t *snd, drum_sound_payload_t *ds)
{
    for (unsigned part = 0; part < DR_PARTS; ++part)
        for (int p = 0; p < tr808_nparams((int)part) && p < PARAMS; ++p)
            ds->params[part][p] = (uint8_t)tr808_sound_get(snd, (int)part, p);
    for (int p = 0; p < KIT_PARAMS; ++p) ds->kit_params[p] = (uint8_t)tr808_sound_get(snd, D8_KIT, p);
}

/* Twelve old parts' knobs onto the ten (the toms' dropped). */
static void params_from_v25(const uint8_t from[OLD_PARTS][PARAMS], const uint8_t kit[KIT_PARAMS], drum_sound_payload_t *ds)
{
    for (unsigned old = 0; old < OLD_PARTS; ++old)
        if (new_part(old) >= 0) memcpy(ds->params[new_part(old)], from[old], PARAMS);
    memcpy(ds->kit_params, kit, KIT_PARAMS);
}

/* A record of versions 12-25 (in version 25's layout) in version 26's: the
 * drum parts mapped, the toms dropped. Before version 21 one sound: the
 * slots in its kit take it, the others their kit's factory sound (as they
 * started). Fields a version did not have stay 0 (load decides by version). */
static void from_v25(const payload_v25_t *o, unsigned version, payload_t *d)
{
    memset(d, 0, sizeof *d);
    for (unsigned i = 0; i < DR_PATTERNS; ++i) {
        const dr_pattern_v25_t *op = &o->pattern[i];
        dr_pattern_t *np = &d->pattern[i];
        memcpy(np->name, op->name, sizeof np->name);
        np->length = op->length;
        np->swing = op->swing;
        np->kit = op->kit;
        np->style = op->style;
        np->accent = op->accent;
        for (unsigned old = 0; old < OLD_PARTS; ++old)
            if (new_part(old) >= 0) { np->hit[new_part(old)] = op->hit[old]; np->roll[new_part(old)] = op->roll[old]; }
    }
    d->kit = o->kit; d->level = o->level; d->current = o->current; d->muted = o->muted;
    d->bpm_x10 = o->bpm_x10; d->reserved = o->reserved;
    for (unsigned t = 0; t < EL_TRACKS; ++t) {
        const track_payload_v25_t *ot = &o->track[t];
        track_payload_t *nt = &d->track[t];
        for (unsigned i = 0; i < ST_PATTERNS; ++i) pattern_from_v25(&ot->pattern[i], &nt->pattern[i]);
        memcpy(nt->v, ot->v, sizeof nt->v);
        nt->engine = ot->engine; nt->preset = ot->preset; nt->level = ot->level; nt->muted = ot->muted; nt->chord = ot->chord; nt->current = ot->current;
    }
    memcpy(d->fx, o->fx, sizeof d->fx);
    memcpy(d->arp, o->arp, sizeof d->arp);
    memcpy(d->midi_channel, o->midi_channel, sizeof d->midi_channel);
    memcpy(d->option, o->option, EL_OPTIONS_STORED);
    if (version >= VERSION_25) memcpy(d->option + EL_OPTIONS_STORED, o->option_ext, EL_OPTIONS - EL_OPTIONS_STORED);
    for (unsigned i = 0; i < DR_PATTERNS; ++i) {
        if (version >= VERSION_21) params_from_v25(o->drum_sound[i].params, o->drum_sound[i].kit_params, &d->drum_sound[i]);
        else if (o->pattern[i].kit % DR_KITS == o->kit % DR_KITS) params_from_v25(o->params, o->kit_params, &d->drum_sound[i]);
        else {
            tr808_sound_t f;
            dr_kit_sound(&f, o->pattern[i].kit % DR_KITS);
            sound_to_payload(&f, &d->drum_sound[i]);
        }
    }
    if (version >= VERSION_21) memcpy(d->synth_sound, o->synth_sound, sizeof d->synth_sound);
    for (unsigned i = 0; i < DR_PATTERNS; ++i)
        for (unsigned old = 0; old < OLD_PARTS; ++old) {
            int n = new_part(old);
            if (n < 0) continue;
            if (version >= VERSION_22) for (unsigned k = 0; k < 3; ++k) d->drum_cond[i][k][n] = o->drum_cond[i][k][old];
            if (version >= VERSION_23) for (unsigned k = 0; k < 4; ++k) d->drum_prob[i][k][n] = o->drum_prob[i][k][old];
            if (version >= VERSION_24) for (unsigned k = 0; k < 3; ++k) d->drum_vol[i][k][n] = o->drum_vol[i][k][old];
        }
    if (version >= VERSION_24) memcpy(d->drum_pitch, o->drum_pitch, sizeof d->drum_pitch);   /* by pitch slot: unchanged */
}

static int el_store_load_held(el_engine_t *e)
{
    if (!fm1_flash_size()) return FM1_FLASH_UNAVAILABLE;
    uint32_t a = read_valid(0), b = read_valid(1);
    if (!a && !b) return 1;
    if ((int32_t)(a - b) > 0 && !read_valid(0)) return 1; /* reload the newer one */
    unsigned version = record.version;
    const payload_t *d = &record.data;
    if (version == VERSION_26) memset(record.data.chain, 0, sizeof record.data.chain);   /* the part 26 did not have */
    if (version < VERSION_26) {
        from_v25(upgrade(), version, &buffer.up.conv);
        d = &buffer.up.conv;
    }
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
    /* the fills (N12): records before 26 have none, they are made when needed */
    for (unsigned i = 0; i < DR_PATTERNS; ++i) { m->fill[i] = d->fill[i]; m->fill[i].used = d->fill[i].used ? 1 : 0; }
    m->fill_auto = d->fill_auto == 8 || d->fill_auto == 16 || d->fill_auto == 32 ? d->fill_auto : 0;
    /* each kit as last set (N1) */
    m->kit_mem_used = (uint8_t)(d->kit_mem_used & ((1u << DR_KITS) - 1u));
    for (unsigned k = 0; k < DR_KITS; ++k) {
        tr808_sound_t *snd = &m->kit_mem[k];
        dr_kit_sound(snd, k);
        if (!((m->kit_mem_used >> k) & 1u)) continue;
        for (unsigned part = 0; part < DR_PARTS; ++part)
            for (int p = 0; p < tr808_nparams((int)part) && p < PARAMS; ++p) tr808_sound_set(snd, (int)part, p, d->kit_mem[k].params[part][p]);
        for (int p = 0; p < KIT_PARAMS; ++p) tr808_sound_set(snd, D8_KIT, p, d->kit_mem[k].kit_params[p]);
    }
    /* Audio is not running yet: the kit can be set directly. DR_CMD_KIT also
     * stores the kit in the current pattern, so select that one first. */
    m->current = m->next = (uint8_t)(d->current % DR_PATTERNS);
    dr_post(m, DR_CMD_KIT, (uint8_t)(d->kit % DR_KITS), 0, 0, 0);
    dr_process_commands(m);
    /* Each slot's sound (records before 21: as from_v25 set them). */
    for (unsigned i = 0; i < DR_PATTERNS; ++i) {
        tr808_sound_t *snd = &m->sound[i];
        dr_kit_sound(snd, m->pattern[i].kit);
        const drum_sound_payload_t *ds = &d->drum_sound[i];
        for (unsigned part = 0; part < DR_PARTS; ++part)
            for (int p = 0; p < tr808_nparams((int)part) && p < PARAMS; ++p) tr808_sound_set(snd, (int)part, p, ds->params[part][p]);
        for (int p = 0; p < KIT_PARAMS; ++p) tr808_sound_set(snd, D8_KIT, p, ds->kit_params[p]);
    }
    /* step conditions, chances and locks (0 in records before 22, 23, 24) */
    memcpy(m->cond, d->drum_cond, sizeof m->cond);
    memcpy(m->prob, d->drum_prob, sizeof m->prob);
    memcpy(m->pitch, d->drum_pitch, sizeof m->pitch);
    memcpy(m->vol, d->drum_vol, sizeof m->vol);
    /* Drum key and Follow (KIT parameters 4, 5) live in the former reserved
     * field: older records hold 0 there = key OFF (docs/adr/0016). */
    tr808_set(&m->kit, D8_KIT, 4, el_clampi(d->reserved & 0xffu, 0, 12));
    tr808_set(&m->kit, D8_KIT, 5, (d->reserved >> 8) & 1u);
    m->level_value = (uint8_t)el_clampi(d->level, 0, 127);
    m->level = m->level_value / 127.0f;
    if (version >= VERSION_18) {   /* older records keep el_init's options */
        /* Before version 20 only the first options existed; the compressor
         * keeps el_init's settings. */
        unsigned count = version >= VERSION_20 ? EL_OPTIONS_STORED : EL_OPT_COMP_THRESHOLD;
        for (unsigned i = 0; i < count; ++i) e->option[i] = (uint8_t)el_clampi(d->option[i], 0, el_option_max(i));
        if (version >= VERSION_25)
            for (unsigned i = EL_OPTIONS_STORED; i < EL_OPTIONS; ++i)
                e->option[i] = (uint8_t)el_clampi(d->option[i], 0, el_option_max(i));
        else if (e->option[EL_OPT_COMP_RATIO] % EL_COMP_RATIOS == EL_COMP_CLUB)
            e->option[EL_OPT_COMP_DRY] = 6;   /* CLUB kept 30 % of the dry signal before it was a setting */
        el_apply_comp(e);
        e->mix.enabled = e->option[EL_OPT_MASTER] ? 1 : 0;
        e->master = e->option[EL_OPT_OUT_LEVEL] / 127.0f;
    }
    /* OPTIONS POWER-ON: every area on slot A instead of where it was. */
    int slot_a = e->option[EL_OPT_START] == EL_START_SLOT_A;
    m->current = m->next = (uint8_t)(slot_a ? 0u : d->current % DR_PATTERNS);
    tr808_sound_apply(&m->kit, &m->sound[m->current]);
    m->kit_index = (uint8_t)(m->pattern[m->current].kit % DR_KITS);
    dr_share_kit(m);   /* one kit for every slot: older records take the playing slot's (feedback 12.11.) */
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
                t->pattern[p].step[k].chord = (uint8_t)((type > MKS_CHORDS ? 0u : type) | (inv > 2u ? 0u : inv) << 4 |
                                                        (old_chords ? 0u : chord & 0xc0u));   /* bits 6-7: the probability */
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
        st_keep_sound(t, 1);   /* before version 21: the one sound in every slot */
        if (version >= VERSION_21)
            for (unsigned p = 0; p < ST_PATTERNS; ++p) {
                const synth_sound_payload_t *ss = &d->synth_sound[i][p];
                st_sound_t *snd = &t->sound[p];
                snd->engine = ss->engine < SYN_ENGINES ? ss->engine : SYN_MKS;
                for (unsigned k = 0; k < SYN_VALUES; ++k)
                    snd->v[k] = k < syn_value_count(snd->engine) ? (uint8_t)el_clampi(ss->v[k], 0, syn_param_max(snd->engine, k)) : 0;
                int u = ss->preset >= EL_USER_SOUND && ss->preset < EL_USER_SOUND + EL_USER_SOUNDS;
                snd->preset = (uint8_t)(ss->preset < syn_sound_count() || u ? ss->preset : 0);
            }
        t->level_value = (uint8_t)el_clampi(s->level, 0, 127);
        t->muted = s->muted ? 1 : 0;
        t->chord = (uint8_t)el_clampi(old_chords && s->chord < 14u ? st_chord_v15[s->chord] : s->chord, 0, MKS_CHORDS);
        st_enter(t, slot_a ? 0u : s->current % ST_PATTERNS);
        t->next = t->current;
    }
    for (unsigned p = 0; p < FX_PARAMS; ++p) e->fx.v[p] = (uint8_t)el_clampi(d->fx[p], 0, fx_param_max(p));
    if (version >= VERSION_13)
        for (unsigned i = 0; i < EL_TRACKS; ++i)
            for (unsigned p = 0; p < ARP_PARAMS; ++p)
                e->track[i].arp.v[p] = (uint8_t)el_clampi(d->arp[i][p], p == ARP_GATE ? 5 : 0, arp_param_max(p));
    if (version >= VERSION_17)   /* older records keep el_init's channels */
        for (unsigned a = 0; a < EL_AREAS; ++a) e->midi_channel[a] = (uint8_t)el_clampi(d->midi_channel[a], 0, EL_MIDI_OFF);
    /* the chains (S16; 0 before version 27): each starts on its first link */
    for (unsigned a = 0; a < EL_AREAS; ++a) {
        unsigned links = d->chain[a][0], slots = 0;
        if (links < 2 || links > DR_CHAIN) continue;
        for (unsigned i = 0; i < links; ++i) slots |= (d->chain[a][1 + i] & 3u) << (2u * i);
        if (a == EL_AREA_BEAT) { dr_post(m, DR_CMD_CHAIN, 0, (uint8_t)links, 0, (int16_t)slots); dr_process_commands(m); }
        else st_chain(&e->track[a - 1u], links, slots);
    }
    return 0;
}

/* Whether copy `which` is erased throughout (then a save needs no erase,
 * which stops the sound ~50 ms a sector: docs/adr/0023). */
static int copy_blank(unsigned which)
{
    const uint32_t *w = (const uint32_t *)(const void *)&buffer;
    for (unsigned s = 0; s < SECTORS; ++s) {
        if (fm1_flash_read(el_store_address(which) + s * FM1_FLASH_SECTOR_BYTES, &buffer, FM1_FLASH_SECTOR_BYTES) != FM1_FLASH_OK)
            return 0;
        for (unsigned i = 0; i < FM1_FLASH_SECTOR_BYTES / 4u; ++i) if (w[i] != 0xffffffffu) return 0;
    }
    return 1;
}

/* The copy the next save writes (the older one) erased ahead, while it
 * costs no sound (the groove stopped): the newest valid copy stays. */
static int el_store_prepare_held(void)
{
    if (!fm1_flash_size()) return FM1_FLASH_UNAVAILABLE;
    uint32_t a = read_valid(0), b = read_valid(1);
    unsigned target = (int32_t)(a - b) > 0 ? 1u : 0u;
    if (!a && !b) target = 0;
    if (copy_blank(target)) return 0;
    int r = 0;
    for (unsigned s = 0; s < SECTORS && !r; ++s) r = fm1_flash_erase(el_store_address(target) + s * FM1_FLASH_SECTOR_BYTES);
    return r;
}

static int el_store_save_held(el_engine_t *e)
{
    uint32_t a = read_valid(0), b = read_valid(1);
    uint32_t sequence = ((int32_t)(a - b) > 0 ? a : b) + 1u;
    unsigned target = (int32_t)(a - b) > 0 ? 1u : 0u; /* overwrite the older one */
    if (!a && !b) target = 0;
    int blank = copy_blank(target);   /* (before the record: it uses the same buffer) */
    const dr_machine_t *m = &e->drums;
    memset(&record, 0, sizeof record);
    payload_t *d = &record.data;
    memcpy(d->pattern, m->pattern, sizeof d->pattern);
    for (unsigned i = 0; i < DR_PATTERNS; ++i) sound_to_payload(&m->sound[i], &d->drum_sound[i]);
    for (unsigned k = 0; k < DR_KITS; ++k) sound_to_payload(&m->kit_mem[k], &d->kit_mem[k]);
    d->kit_mem_used = m->kit_mem_used;
    memcpy(d->fill, m->fill, sizeof d->fill);
    d->fill_auto = m->fill_auto;
    memcpy(d->drum_cond, m->cond, sizeof d->drum_cond);
    memcpy(d->drum_prob, m->prob, sizeof d->drum_prob);
    memcpy(d->drum_pitch, m->pitch, sizeof d->drum_pitch);
    memcpy(d->drum_vol, m->vol, sizeof d->drum_vol);
    d->reserved = (uint16_t)(tr808_get(&m->kit, D8_KIT, 4) | tr808_get(&m->kit, D8_KIT, 5) << 8);
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
        for (unsigned p = 0; p < ST_PATTERNS; ++p) {
            synth_sound_payload_t *ss = &d->synth_sound[i][p];
            memcpy(ss->v, t->sound[p].v, sizeof ss->v);
            ss->engine = t->sound[p].engine;
            ss->preset = t->sound[p].preset;
        }
    }
    memcpy(d->fx, e->fx.v, sizeof d->fx);
    for (unsigned i = 0; i < EL_TRACKS; ++i) memcpy(d->arp[i], e->track[i].arp.v, ARP_PARAMS);
    for (unsigned a = 0; a < EL_AREAS; ++a) d->midi_channel[a] = e->midi_channel[a];
    for (unsigned i = 0; i < EL_OPTIONS; ++i) d->option[i] = e->option[i];
    for (unsigned a = 0; a < EL_AREAS; ++a) {   /* the chains (S16) */
        const uint8_t *links = a == EL_AREA_BEAT ? &m->chain_len : &e->track[a - 1u].chain_len;
        const uint8_t *slots = a == EL_AREA_BEAT ? m->chain : e->track[a - 1u].chain;
        d->chain[a][0] = *links;
        memcpy(&d->chain[a][1], slots, DR_CHAIN);
    }
    record.magic = MAGIC;
    record.sequence = sequence ? sequence : 1u;
    record.version = VERSION;
    record.bytes = sizeof(payload_t);
    record.crc = crc32(d, sizeof *d);
    uint32_t address = el_store_address(target);
    int r = 0;
    if (!blank)   /* erased ahead (el_store_prepare): no gap in the sound */
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

static int el_sound_get_held(unsigned slot, el_sound_t *out)
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

static int el_sound_put_held(unsigned slot, const el_sound_t *in)
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


int el_store_load(el_engine_t *e)
{
    if (!take()) return -1;
    int r = el_store_load_held(e);
    give();
    return r;
}

int el_store_prepare(void)
{
    if (!take()) return -1;
    int r = el_store_prepare_held();
    give();
    return r;
}

int el_store_save(el_engine_t *e)
{
    if (!take()) return -1;
    int r = el_store_save_held(e);
    give();
    return r;
}

#ifdef FM1_HOST_TEST
/* Host test: the newest record rewritten as version 20 wrote it. */
/* Today's record in version 25's layout (twelve parts, the toms empty). */
static void to_v25(const payload_t *d, payload_v25_t *o)
{
    memset(o, 0, sizeof *o);
    for (unsigned i = 0; i < DR_PATTERNS; ++i) {
        const dr_pattern_t *np = &d->pattern[i];
        dr_pattern_v25_t *op = &o->pattern[i];
        memcpy(op->name, np->name, sizeof op->name);
        op->length = np->length; op->swing = np->swing; op->kit = np->kit; op->style = np->style; op->accent = np->accent;
        for (unsigned old = 0; old < OLD_PARTS; ++old)
            if (new_part(old) >= 0) { op->hit[old] = np->hit[new_part(old)]; op->roll[old] = np->roll[new_part(old)]; }
    }
    unsigned cur = d->current % DR_PATTERNS;
    for (unsigned old = 0; old < OLD_PARTS; ++old)
        if (new_part(old) >= 0) memcpy(o->params[old], d->drum_sound[cur].params[new_part(old)], PARAMS);
    memcpy(o->kit_params, d->drum_sound[cur].kit_params, KIT_PARAMS);
    o->kit = d->pattern[cur].kit; o->level = d->level; o->current = d->current; o->muted = d->muted;
    o->bpm_x10 = d->bpm_x10; o->reserved = d->reserved;
    for (unsigned t = 0; t < EL_TRACKS; ++t) {
        const track_payload_t *nt = &d->track[t];
        track_payload_v25_t *ot = &o->track[t];
        for (unsigned i = 0; i < ST_PATTERNS; ++i) {
            ot->pattern[i].length = nt->pattern[i].length; ot->pattern[i].gate = nt->pattern[i].gate; ot->pattern[i].rate = nt->pattern[i].rate;
            for (unsigned k = 0; k < ST_STEPS; ++k) {
                ot->pattern[i].step[k].note = nt->pattern[i].step[k].note;
                ot->pattern[i].step[k].flags = nt->pattern[i].step[k].flags;
                ot->pattern[i].step[k].chord = nt->pattern[i].step[k].chord;
            }
        }
        memcpy(ot->v, nt->v, sizeof ot->v);
        ot->engine = nt->engine; ot->preset = nt->preset; ot->level = nt->level; ot->muted = nt->muted; ot->chord = nt->chord; ot->current = nt->current;
    }
    memcpy(o->fx, d->fx, sizeof o->fx);
    memcpy(o->arp, d->arp, sizeof o->arp);
    memcpy(o->midi_channel, d->midi_channel, sizeof o->midi_channel);
    memcpy(o->option, d->option, EL_OPTIONS_STORED);
    memcpy(o->option_ext, d->option + EL_OPTIONS_STORED, EL_OPTIONS - EL_OPTIONS_STORED);
    for (unsigned i = 0; i < DR_PATTERNS; ++i) {
        for (unsigned old = 0; old < OLD_PARTS; ++old) {
            int n = new_part(old);
            if (n < 0) continue;
            memcpy(o->drum_sound[i].params[old], d->drum_sound[i].params[n], PARAMS);
            for (unsigned k = 0; k < 3; ++k) { o->drum_cond[i][k][old] = d->drum_cond[i][k][n]; o->drum_vol[i][k][old] = d->drum_vol[i][k][n]; }
            for (unsigned k = 0; k < 4; ++k) o->drum_prob[i][k][old] = d->drum_prob[i][k][n];
        }
        memcpy(o->drum_sound[i].kit_params, d->drum_sound[i].kit_params, KIT_PARAMS);
    }
    memcpy(o->synth_sound, d->synth_sound, sizeof o->synth_sound);
    memcpy(o->drum_pitch, d->drum_pitch, sizeof o->drum_pitch);
}

/* The newest record rewritten as version `version` (20, 25 or 26) wrote it. */
int el_store_test_as(unsigned version)
{
    if (!take()) return -1;
    uint32_t a = read_valid(0), b = read_valid(1);
    unsigned newest = (int32_t)(a - b) > 0 || !b ? 0u : 1u;
    int r = -1;
    if ((a || b) && read_valid(newest) && record.version == VERSION) {
        static payload_v25_t o;
        size_t bytes = V26_BYTES;   /* 26: the same, without the chains */
        if (version < VERSION_26) {
            to_v25(&record.data, &o);
            bytes = version >= VERSION_25 ? V25_BYTES : V19_BYTES;
            memcpy(&record.data, &o, bytes);
        }
        record.version = (uint16_t)version;
        record.bytes = (uint16_t)bytes;
        record.crc = crc32(&record.data, bytes);
        uint32_t address = el_store_address(newest);
        r = 0;
        for (unsigned s = 0; s < SECTORS && !r; ++s) r = fm1_flash_erase(address + s * FM1_FLASH_SECTOR_BYTES);
        if (!r) r = fm1_flash_write(address, &record, offsetof(record_t, data) + bytes);
    }
    give();
    return r;
}
int el_store_test_as_v20(void) { return el_store_test_as(VERSION_20); }
#endif

int el_sound_get(unsigned slot, el_sound_t *out)
{
    if (!take()) return -1;
    int r = el_sound_get_held(slot, out);
    give();
    return r;
}

int el_sound_put(unsigned slot, const el_sound_t *in)
{
    if (!take()) return -1;
    int r = el_sound_put_held(slot, in);
    give();
    return r;
}
