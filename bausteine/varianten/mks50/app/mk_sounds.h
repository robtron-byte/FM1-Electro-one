/* SPDX-License-Identifier: GPL-3.0-only
 * Erzeugt von bausteine/varianten/mks50/tools/sounds.py: Gruppen der Soundliste (Reihenfolge wie
 * mks50_bs_preset_*: eigene zuerst, dann die Werks-Sounds). Nicht von Hand ändern. */
#define MK_NCAT 9u
#define MK_NSOUND 79u
static const char *const mk_cat_names[MK_NCAT] = {"EIGENE", "BASS", "SEQUENCE", "ZAPS", "STAB", "KEYS", "STR+BRASS", "PAD", "FX+PERC"};
static const uint8_t mk_sound_cat[MK_NSOUND] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 5, 5, 8, 5, 4, 2, 2, 4, 3,
    8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 4, 8, 8, 3, 3, 3,
    3, 3, 3, 3, 3, 6, 7, 6, 7, 6, 7, 7, 7, 6, 6, 6,
    6, 6, 6, 6, 6, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1,
};
/* 0: WOBBLE 01; 1: INIT TO 01; 2: ZAP 01; 3: TRANS E 01; 4: PEW PEW 01; 5: JX STRI 01; 6: SWEEP S 01; 7: NUMBERS 01; 8: WOBBLE 02; 9: TRANS E 02; 10: TRANS E 03; 11: TRANS E 04; 12: ROBO CH 01; 13: ROBO CH 02; 14: S&H BLE 01; 15: INIT TO 02; 16: INIT TO 03; 17: INIT TO 04; 18: INIT TO 05; 19: INIT TO 06; 20: INIT TO 07; 21: SAW SWE 01; 22: BOOM 80 01; 23: PLUCK KEYS; 24: GLASS BELL; 25: NOISE SWP; 26: ORGAN; 27: HOUSE STAB; 28: NUMBERS; 29: CALCULATR; 30: ELEC STAB; 31: ZAP; 32: ELEC HAT; 33: BOOM 808; 34: SEQ KRAFT; 35: SEQ ROTHER; 36: ARP ROBOT; 37: ARP UPDOWN; 38: S&H BLEEP; 39: OCT BOUNCE; 40: SAW SWEEP; 41: TRIPLETZAP; 42: GATE CHORD; 43: COMP TALK; 44: S&H NOISE; 45: ZAP CLASSC; 46: LASER; 47: PEW PEW; 48: RISE ZAP; 49: SYNC ZAP; 50: NOISE ZAP; 51: RES ZAP; 52: DUB ZAP; 53: JUNO BRASS; 54: WARM PAD; 55: STRINGS; 56: DARK DRONE; 57: TRANS EURO; 58: MACHINE PD; 59: ROBO CHOIR; 60: DRIFT PAD; 61: JX STRINGS; 62: JX ENSEMBL; 63: STR STAB; 64: DARK STR; 65: SWEEP STR; 66: BRIGHT STR; 67: STR CHORD; 68: SLOW STR; 69: FAT BASS; 70: SUB BASS; 71: ACID LINE; 72: WOBBLE; 73: ROBO BASS; 74: AUTO BASS; 75: ACID ELEC; 76: ARP BASS; 77: S&H BASS; 78: WOBBLE 1/8 */
