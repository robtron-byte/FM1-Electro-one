/* SPDX-License-Identifier: GPL-3.0-only */
/* FM1-Electro's app on its own platform (docs/adr/0021): one unit with the
 * platform (platform/firmware/src/platform.c, Dinghy / Felucca by Leo
 * Kuroshita, Hügelton Instruments, GPL-3.0-only). It answers the few calls
 * FM1-Electro made of FM1 Open's SDK (panel, LEDs, screen bands, flash, time)
 * and runs FM1-Electro (el_glue.c) from the platform's main loop and audio
 * interrupt. */
#define APP_NAME "FM1-ELECTRO"
static void app_idle(void);
#define LCD_IDLE app_idle
#include "platform.c"
#include "el_glue.h"

uint32_t el_platform_ticks(void) { return fm1_ticks(); }

/* ---- time */
uint32_t fm1_millis(void) { return fm1_ms; }

/* ---- LEDs: 0..13 the buttons (the same order as the platform's BTN_*),
 * 14..40 the 27 note keys */
void fm1_led_set(unsigned key, int on)
{
    if (key < 14u) fm1_led_key(key, on);
    else if (key < 41u) fm1_led_key(KEY_LED(key - 14u), on);
}

/* ---- screen: FM1-Electro paints 8-row bands of native RGB565 and sends them
 * by DMA while it draws the next (it asks pending first) */
int fm1_screen_submit(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t *pixels, size_t count)
{
    if (!w || !h || x + w > 240u || y + h > 240u || count < (size_t)w * h) return -1;
    /* the panel takes the high byte first; the band is native (FM1 Open's SDK
     * swapped it in place too) */
    for (size_t i = 0; i < (size_t)w * h; ++i) pixels[i] = (uint16_t)(pixels[i] >> 8 | pixels[i] << 8);
    lcd_blit(x, y, w, h, pixels);
    return 0;
}
int fm1_screen_pending(void)
{
    if (!lcd_busy) return 0;
    if (!fm1_lcd_done()) return 1;
    lcd_sync();   /* finished: release the panel */
    return 0;
}

/* ---- flash: FM1-Electro's own store (fm1/el_store.c: two copies of the
 * grooves, two of the sound bank, 8 sectors) at the start of the platform's
 * slots (store.h, 0x97000.., inside its allow-list FL_RANGE_OK) */
#define EL_STORE_AT SLOTS_AT
#define EL_STORE_BYTES (8u * 0x1000u)
_Static_assert(EL_STORE_BYTES <= NSLOT * SLOT_SECT * 0x1000u, "FM1-Electro's store fits the slots");
uint32_t fm1_flash_size(void) { return flash_ok ? EL_STORE_BYTES : 0u; }
int fm1_flash_read(uint32_t offset, void *data, size_t bytes)
{
    if (!flash_ok || offset > EL_STORE_BYTES || bytes > EL_STORE_BYTES - offset) return FM1_FLASH_INVALID;
    return st_read(EL_STORE_AT + offset, data, (uint32_t)bytes) ? FM1_FLASH_IO : FM1_FLASH_OK;
}
int fm1_flash_erase(uint32_t offset)
{
    uint32_t took;
    if (!flash_ok || (offset & 0xFFFu) || offset >= EL_STORE_BYTES) return FM1_FLASH_INVALID;
    fm1_wdt_feed();   /* (~50 ms an erase) */
    return fl_erase4k(EL_STORE_AT + offset, &took) ? FM1_FLASH_IO : FM1_FLASH_OK;
}
int fm1_flash_write(uint32_t offset, const void *data, size_t bytes)
{
    if (!flash_ok || offset > EL_STORE_BYTES || bytes > EL_STORE_BYTES - offset) return FM1_FLASH_INVALID;
    return fl_write(EL_STORE_AT + offset, data, (uint32_t)bytes) ? FM1_FLASH_IO : FM1_FLASH_OK;
}

/* ---- the store's buffer (fm1/el_store.c takes it while it reads or writes,
 * one at a time): from the pool, there is no heap */
static uint8_t store_buf[3u * 0x1000u] __attribute__((section(".pool"), aligned(4)));
static uint8_t store_buf_taken;
void *malloc(size_t n)
{
    if (store_buf_taken || n > sizeof store_buf) return 0;
    store_buf_taken = 1;
    return store_buf;
}
void free(void *p)
{
    if (p == store_buf) store_buf_taken = 0;
}

/* ---- C library bits the engine and UI use beyond platform/libc.c */
void *memmove(void *d, const void *s, size_t n)
{
    uint8_t *dp = d;
    const uint8_t *sp = s;
    if (dp < sp) while (n--) *dp++ = *sp++;
    else { dp += n; sp += n; while (n--) *--dp = *--sp; }
    return d;
}
char *strchr(const char *s, int c)
{
    for (;; ++s) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return 0;
    }
}
int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) ++a, ++b;
    return (unsigned char)*a - (unsigned char)*b;
}
char *strncpy(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; i < n && s[i]; ++i) d[i] = s[i];
    for (; i < n; ++i) d[i] = 0;
    return d;
}
/* (EL_FENCE / __sync_synchronize: the compiler's own on this target) */

/* ---- sound: the platform's audio interrupt, 256 frames at 44.1 kHz */
static void app_audio(int32_t *out, uint32_t n) { el_glue_render(out, n); }

/* ---- MIDI in: whole messages to the engine */
static uint8_t idle_ok;
static void midi_in(void)
{
    uint8_t st, d1, d2;
    while (midi_get(&st, &d1, &d2) != 0) {
        uint8_t m[3] = {st, d1, d2};
        unsigned ty = st & 0xF0u;
        size_t len = st >= 0xF8u ? 1u : (ty == 0xC0u || ty == 0xD0u) ? 2u : 3u;
        el_glue_midi(m, len);
    }
}
static void app_idle(void)
{
    if (idle_ok) midi_in();
}

/* ---- the main loop: the panel as FM1 Open's fm1_controls_read gave it */
static void app_main(void)
{
    struct fm1_controls c;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    el_glue_start();
    for (;;) {
        platform_poll();
        midi_in();
        c.keys = fm1_in.notes;
        c.buttons = fm1_in.buttons;
        for (unsigned i = 0; i < 7u; ++i) c.encoders[i] = (int16_t)fm1_enc_take(i);
        {
            int a = fm1_adc_read(FM1_ADC_MASTER);
            c.knob = (uint16_t)(a < 0 ? 1023 : a);
        }
        c.battery = 0;
        idle_ok = 1;   /* the screen's DMA waits may read MIDI meanwhile */
        el_glue_control(&c, fm1_ms);
        idle_ok = 0;
    }
}
