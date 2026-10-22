/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Persistent drum patterns, kit sound and tempo in the FM1 user Flash.
 * Two sectors alternate (newest valid sequence wins), so a power cut while
 * saving keeps the previous save. Control task only. */
#ifndef EL_STORE_H
#define EL_STORE_H
#include "electro.h"

/* Sectors used: after FM1 Open's area when the partition has room there,
 * otherwise FM1 Open project slots 12-15 (unused while FM1-Electro runs). */
uint32_t el_store_address(unsigned which);
/* Before audio starts: load the newest valid save into the engine.
 * Returns 0 when loaded, 1 when nothing was saved yet, <0 on Flash errors. */
int el_store_load(el_engine_t *e);
/* Save the current state. Briefly silences audio while Flash is written. */
int el_store_save(el_engine_t *e);
#endif
