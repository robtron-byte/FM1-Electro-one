/* SPDX-License-Identifier: GPL-3.0-only
 * Erzeugt von tools/cats.py aus engine/jp6_presets.c (Hauptordner): Gruppe je Werks-Sound. Nicht von Hand ändern. */
enum { NFC = 10 };   /* Gruppen der Werks-Sounds, 0 = INIT */
static const char *const jp_cat_names[NFC] = {"INIT", "BASS", "LEAD", "SEQUENCE", "ZAPS", "STAB", "KEYS", "STR+BRASS", "PAD", "FX+PERC"};
static const uint8_t jp_preset_cat[74] = {
    2,   /* SYNC SOLO */
    2,   /* REV SYNC */
    2,   /* XMOD SOLO */
    2,   /* OCTAVE LD */
    2,   /* BPF LEAD */
    2,   /* JUPI UNI */
    2,   /* PWM ENV LD */
    2,   /* TALK LEAD */
    2,   /* WHISTLE */
    3,   /* ARP 1983 */
    3,   /* ARP UPDN */
    3,   /* ARP RND BP */
    3,   /* ARP TRIPLT */
    3,   /* SEQ DETRO */
    3,   /* SEQ MOTOR */
    3,   /* SEQ PHRYG */
    3,   /* SEQ EBM */
    3,   /* SEQ XMOD */
    3,   /* COMPUTER */
    4,   /* ZAP DOWN */
    4,   /* ZAP UP */
    4,   /* SYNC BLAST */
    4,   /* XMOD PEW */
    4,   /* BPF ZAP */
    4,   /* LO-X ZAP */
    5,   /* DETROIT ST */
    5,   /* M9 DUB */
    5,   /* 4THS STAB */
    5,   /* POWER 5+8 */
    5,   /* ORCH HIT */
    5,   /* BRASS STAB */
    5,   /* HPF STAB */
    6,   /* SYNPOP KEY */
    6,   /* CHIME XMOD */
    6,   /* PULSE CLAV */
    6,   /* MARIMBA */
    6,   /* TINE PIANO */
    9,   /* ANALOG KCK */
    9,   /* NOISE SNR */
    9,   /* TICK HAT */
    9,   /* METAL TOM */
    9,   /* UFO */
    9,   /* RAVE ALARM */
    9,   /* WIND */
    7,   /* JP6 STRNG */
    7,   /* PULSE STR */
    7,   /* BRASS 1983 */
    7,   /* SYNC BRASS */
    7,   /* HPF STRNGS */
    7,   /* JP STRINGS */
    7,   /* PWM STRING */
    7,   /* JP SLOWSTR */
    7,   /* BPF SWEEP */
    7,   /* STR MACHN */
    7,   /* OCTAVE STR */
    7,   /* UNI STRING */
    7,   /* M9 STRINGS */
    7,   /* TAPE STR */
    8,   /* JUPI PAD */
    8,   /* BPF CHOIR */
    8,   /* XMOD GLASS */
    8,   /* S&H PAD */
    8,   /* NIGHTDRIVE */
    8,   /* HPF AIR */
    1,   /* JUPI BASS */
    1,   /* DETROIT B */
    1,   /* SYNC GROWL */
    1,   /* XMOD THUMP */
    1,   /* BPF BASS */
    1,   /* MIAMI SUB */
    1,   /* 12 VCO UNI */
    1,   /* EBM PULSE */
    1,   /* GATE BASS */
    1,   /* RUBBER */
};
