/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca memory map.
 * Code + constants execute in place from XIP (app.bin offset 0 = 0x02000120).
 * 0x01C00000..0x01C06000 holds .ram_text (code that must run while the flash
 * is busy: the flash driver); 0x01C06000..0x01C08000 stays for RAM-run debug.
 * RAM  : .data/.bss (96 KiB)
 * POOL : big zero-initialised buffers (FX lines, canvases), section .pool
 * STACKS with 256-byte guard bands; NOINIT survives resets and UBOOT entry.
 * 0x01C7FD50.. is reserved: boot_info, boot-state mailbox, vector table. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAMTEXT (rwx): ORIGIN = 0x01C00000, LENGTH = 0x6000
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 96K
    POOL   (rw)  : ORIGIN = 0x01C20000, LENGTH = 0x54000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x3D50
}
SECTIONS
{
    .text : { KEEP(*(.text.crt0)) *(EXCLUDE_FILE(*e_arp.o *e_dist.o *e_drums.o *e_electro.o *e_fx.o *e_jp6.o *e_mixbus.o *e_mks.o *e_po12.o *e_synth.o *e_synth_track.o *e_tr808.o *e_transport.o) .text*) *(.rodata*) . = ALIGN(4); _etext = .; } > XIP
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) . = ALIGN(4); } > RAMTEXT AT > XIP
    _rt_load = LOADADDR(.ram_text);
    /* FM1-Electro (from SLOOP 2.8.5 app.ld): the second core's waiting loop (hal/fm1_cpu1.h) right after the
     * flash driver's RAM code, copied with it at boot; the guard's RAM code window covers both */
    .c1_text : { _c1_start = .; KEEP(*(.c1_text*)) . = ALIGN(4); _c1_end = .; } > RAMTEXT AT > XIP
    _c1_load = LOADADDR(.c1_text);
    _rt_copy_end = _c1_end;
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); _bss_end = .; } > RAM
    /* FM1-Electro: the audio code and its constants (engine/el_config.h EL_DSP_*, .dsp_text /
     * .dsp_rodata), with all code of the engine's sound files (FM1-Electro's build names them e_*.o), at the start of the pool's SRAM, copied at boot: a taken branch costs half
     * there of what it costs from the flash's cache. The guard's RAM code window runs from
     * .ram_text to its end (so it takes in .data/.bss: the window is one range). */
    .dsp_text : { . = ALIGN(4); _dsp_start = .; KEEP(*(.dsp_text*)) *e_arp.o(.text*) *e_dist.o(.text*) *e_drums.o(.text*) *e_electro.o(.text*) *e_fx.o(.text*) *e_jp6.o(.text*) *e_mixbus.o(.text*) *e_mks.o(.text*) *e_po12.o(.text*) *e_synth.o(.text*) *e_synth_track.o(.text*) *e_tr808.o(.text*) *e_transport.o(.text*) *(.dsp_rodata*) . = ALIGN(4); _dsp_end = .; } > POOL AT > XIP
    _dsp_load = LOADADDR(.dsp_text);
    _rt_end = _dsp_end;
    .pool (NOLOAD) : { _pool_start = .; *(.pool*) . = ALIGN(4); _pool_end = .; } > POOL
    .noinit (NOLOAD) : { *(.noinit) *(.noinit.*) } > NOINIT
    _guard0 = 0x01C74000; _ustack_lo = 0x01C74100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    /DISCARD/ : { *(.comment) *(.note*) }
}
