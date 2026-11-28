/* SPDX-License-Identifier: GPL-3.0-only
 * JX-8P firmware for the M-VAVE FM-1: its build configuration. The shared part (sample rate, block,
 * SRAM placement) is ../../shared/syn_config.h. */
#ifndef JX_CONFIG_H
#define JX_CONFIG_H
#include "syn_config.h"

#define JX_BLOCK SYN_BLOCK

/* Voices in the pool: 6, as the JX-8P has. The measuring firmware decides (docs/adr/0002). */
#ifndef JX_POOL
#define JX_POOL 6
#endif
#endif
