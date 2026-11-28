/* Host-Test (ADR 0054): übersetzt ota.c der gemeinsamen Plattform (bausteine/varianten/gemeinsam) und schickt ihm
 * eine Kennungs- und eine Info-Abfrage (cmd 0x11, 0x12); gibt jede Antwort als Hex-Zeile aus. test.mjs vergleicht
 * sie mit dem, was fm1ota.js erwartet. Aufruf: ota_info_host HEX-NACHRICHT... (F0 .. F7 je Argument). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef FELUCCA_ID
#define FELUCCA_ID "FM-1_941"
#endif
static uint8_t frame[700];
static uint32_t frame_n, have;
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) printf("%02x", p[i]);
    printf("\n");
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!have) return 0; *p = frame; *n = frame_n; return 1; }
static void ota_frame_done(void) { have = 0; }
static uint32_t ota_now_ms(void) { return 0; }
static void ota_idle(void) {}
static int ota_erase(uint32_t off) { (void)off; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "ota.c"
int main(int argc, char **argv)
{
    for (int a = 1; a < argc; a++) {
        size_t L = strlen(argv[a]) / 2;
        frame_n = 0;
        for (size_t i = 1; i + 1 < L; i++) {            /* ohne F0 und F7 */
            unsigned v; sscanf(argv[a] + 2 * i, "%2x", &v); frame[frame_n++] = (uint8_t)v;
        }
        have = 1;
        ota_service();
        printf("--\n");
    }
    (void)ota_session; (void)ota_boot_cleanup;
    return 0;
}
