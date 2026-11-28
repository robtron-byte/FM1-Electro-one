/* SPDX-License-Identifier: GPL-3.0-only
 * MKS-50 firmware for the M-VAVE FM-1: its build configuration. The shared part (sample rate, block,
 * SRAM placement) is ../../shared/syn_config.h; after FM1-Electro's engine/el_config.h
 * (github.com/robtron-byte/FM1-Electro-one, commit e7b3b14). */
#ifndef MK_CONFIG_H
#define MK_CONFIG_H
#include "syn_config.h"

#define MKS_BLOCK SYN_BLOCK

/* Voices in the pool (docs/adr/0002): 6, as the MKS-50 has; set by the user after the device measurements
 * (docs/adr/0003: 8 sounding voices of the heaviest sounds took 91 % of a block, peak 98 %). */
#ifndef MKS_POOL
#define MKS_POOL 6
#endif
#endif
