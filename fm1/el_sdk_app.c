/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * Device build only: what FM1 Open's startup expects from its app layer,
 * for the parts FM1-Electro replaces. tools/prepare_fm1.py moves the SDK's
 * own screen, presets, projects, samples, FM library and project transfer
 * out of the build (they would keep the SDK's synth engine, effects and
 * buffers linked: about 100 KB of SRAM). Board drivers, audio output and
 * the MIDI router stay as they are. */
#include "instrument.h"
#include "fm_patch.h"
#include "fm1.h"
#include "el_fm1.h"
#include <stdlib.h>
#include <string.h>

/* FM1 Open's Build.ps1 checks that the firmware still holds the SDK's
 * Bluetooth/BLE controls and project/tuning entry points. Their users
 * (screen, transfer) are out of the build; this table keeps them linked. */
static const void *const kept[] = {
    (const void *)fm1_ble_set_role, (const void *)fm1_ble_scan, (const void *)fm1_ble_connect,
    (const void *)fm1_ble_disconnect, (const void *)fm1_ble_get_status, (const void *)fm1_ble_get_device,
    (const void *)fm1_bluetooth_configure, (const void *)fm1_bluetooth_get_status,
    (const void *)groove_tuning_apply, (const void *)groove_project_apply_edit,
};

/* Panel and LCD come up as in the SDK's screen; FM1-Electro draws itself.
 * The SDK's startup has just given its eight tracks their default
 * instruments (two DX7 and more: about 60 KB of heap). They never sound
 * here (FM1-Electro renders the audio), so the tracks are switched off and
 * the heap is free again before the SDK starts its remaining tasks (with
 * them, its app task finds no room for its stack and the start halts). */
void groove_screen_init(void)
{
    for (unsigned t = 0; t < GROOVE_TRACKS; ++t) (void)groove_track_role_set(t, GROOVE_TRACK_OFF);
    /* The SDK scales the output by its master level (fm1_output_mix), which
     * FM1 Open's screen set from the MASTER knob. FM1-Electro applies MASTER
     * itself (EL_CMD_MASTER), so the SDK's level stays at full scale. */
    (void)groove_master_update(1023);
    (void)*(const void *const volatile *)&kept[0];
    fm1_controls_init();
    for (unsigned key = 0; key < 41; ++key) fm1_led_set(key, 0);
    fm1_screen_init();
}

void groove_screen_control(void) { (void)el_fm1_control(); }

/* No SDK projects: FM1-Electro keeps its own store (el_store.c). */
void groove_project_init(void) {}
void groove_project_poll(void) {}
void groove_project_name(char name[13]) { memcpy(name, "FM1-ELECTRO", 12); }
void groove_project_metadata(const struct groove_project *p) { (void)p; }

/* While starting (groove_track_role_set above) the heap is full: the
 * engine's still unused memory serves. Later only allocated if the SDK
 * ever asks for it. */
struct groove_project *groove_project_scratch(void)
{
    static struct groove_project *scratch;
    struct groove_project *boot = el_fm1_boot_scratch(sizeof *boot);
    if (boot) return boot;
    if (!scratch) scratch = malloc(sizeof *scratch);
    return scratch;
}

/* No project transfer: application SysEx is ignored (firmware updates are
 * handled by the SDK before this hook). */
void fm1_app_sysex(const uint8_t *message, size_t bytes)
{
    (void)message;
    (void)bytes;
}

/* No sample bank, FM library or project transfer. */
void groove_samples_init(void) {}
void groove_samples_poll(void) {}
void groove_transfer_init(void) {}
void groove_transfer_poll(void) {}
void groove_fm_library_init(void) {}
FM1_DSP_CODE void groove_fm_program(uint8_t patch[156], unsigned program)
{
    (void)program;
    memset(patch, 0, 156);
}
