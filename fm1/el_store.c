/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "el_store.h"
#include "fm1.h"
#include <string.h>

enum { MAGIC = 0x34444c45u /* "ELD4" */, VERSION = 4, PARAMS = 10, KIT_PARAMS = 3 };

typedef struct {
    st_pattern_t pattern[ST_PATTERNS];
    uint8_t v[MP_COUNT];
    uint8_t preset, level, muted, chord;
} track_payload_t;

typedef struct {
    dr_pattern_t pattern[DR_PATTERNS];
    uint8_t params[DR_PARTS][PARAMS];
    uint8_t kit_params[KIT_PARAMS];
    uint8_t kit, level, current, muted;
    uint16_t bpm_x10, reserved;
    track_payload_t track[EL_TRACKS];
    uint8_t fx[FX_PARAMS];
} payload_t;

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
_Static_assert(SECTORS <= 3, "record fits three sectors per copy");

static record_t record; /* static: too large for the control task stack */

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
    if (fm1_flash_read(el_store_address(which), &record, sizeof record) != FM1_FLASH_OK) return 0;
    if (record.magic != MAGIC || record.version != VERSION || record.bytes != sizeof(payload_t)) return 0;
    if (crc32(&record.data, sizeof record.data) != record.crc) return 0;
    return record.sequence;
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
        m->pattern[i] = p;
    }
    /* Audio is not running yet: the kit can be set directly. */
    dr_post(m, DR_CMD_KIT, (uint8_t)(d->kit % DR_KITS), 0, 0, 0);
    dr_process_commands(m);
    for (unsigned part = 0; part < DR_PARTS; ++part)
        for (int p = 0; p < tr808_nparams((int)part) && p < PARAMS; ++p)
            tr808_set(&m->kit, (int)part, p, d->params[part][p]);
    for (int p = 0; p < KIT_PARAMS; ++p) tr808_set(&m->kit, D8_KIT, p, d->kit_params[p]);
    m->level_value = (uint8_t)el_clampi(d->level, 0, 127);
    m->level = m->level_value / 127.0f;
    m->current = m->next = (uint8_t)(d->current % DR_PATTERNS);
    m->muted = d->muted ? 1 : 0;
    el_transport_set_bpm(&e->transport, el_clampi(d->bpm_x10, 300, 3000) * 0.1f);
    for (unsigned i = 0; i < EL_TRACKS; ++i) {
        st_track_t *t = &e->track[i];
        const track_payload_t *s = &d->track[i];
        for (unsigned p = 0; p < ST_PATTERNS; ++p) {
            t->pattern[p] = s->pattern[p];
            t->pattern[p].length = (uint8_t)el_clampi(t->pattern[p].length, 1, ST_STEPS);
            t->pattern[p].gate = (uint8_t)el_clampi(t->pattern[p].gate, 10, 100);
            for (unsigned k = 0; k < ST_STEPS; ++k)
                t->pattern[p].step[k].chord = (uint8_t)el_clampi(t->pattern[p].step[k].chord, 0, MKS_CHORDS);
        }
        uint8_t v[MP_COUNT];
        for (unsigned p = 0; p < MP_COUNT; ++p) v[p] = (uint8_t)el_clampi(s->v[p], 0, mks_param_max(p));
        mks_part_load(&t->part, v);
        t->preset = (uint8_t)(s->preset < mks_preset_count ? s->preset : 0);
        t->level_value = (uint8_t)el_clampi(s->level, 0, 127);
        t->muted = s->muted ? 1 : 0;
        t->chord = (uint8_t)el_clampi(s->chord, 0, MKS_CHORDS);
    }
    for (unsigned p = 0; p < FX_PARAMS; ++p) e->fx.v[p] = (uint8_t)el_clampi(d->fx[p], 0, fx_param_max(p));
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
        memcpy(s->v, t->part.v, sizeof s->v);
        s->preset = t->preset;
        s->level = t->level_value;
        s->muted = t->muted;
        s->chord = t->chord;
    }
    memcpy(d->fx, e->fx.v, sizeof d->fx);
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
