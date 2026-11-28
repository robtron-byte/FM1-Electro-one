/* SPDX-License-Identifier: GPL-3.0-only */
/* MKS Ultra solo (Alpha Juno / MKS-50 „Ultra“, Rust-Nachbau) für das M-VAVE FM-1 (ADR 0045): der Baustein
 * bausteine/mksultra in voller Filterqualität (VCF 4×, DCO 2×) auf dem Gerüst bausteine/varianten/gemeinsam.
 *
 * Zwei Kerne im Fließband: Kern 1 rechnet die Stimmen von Block N (mksultra_voices), Kern 0 gleichzeitig die
 * Master-Kette (Chorus, Delay, Hall, EQ) von Block N−1 (mksultra_master). Das ist bitgleich mit dem Baustein, nur um
 * einen Block (1,45 ms) später. Effekte, Arpeggiator und Akkorde sind die des Bausteins (GM_FX 0).
 *
 * Werte: je Parameter ein Byte (Regler 0..200, Schalter ihre Stufen). Damit die Presets genau klingen, trägt jeder Sound
 * das Preset, aus dem er stammt (M_BASE_HI/LO, nicht sichtbar): kern_load lädt dieses Preset genau und setzt nur die
 * Werte, die von seiner Byte-Fassung abweichen. */
#define APP_NAME "MKS ULTRA"
#define GM_TITLE "MKS ULTRA"
#define GM_ACCENT RGB(51, 120, 255)             /* das Blau der MKS-50 */
#define GM_PARTS 1u
#define GM_NAMEMAX 10u
#define GM_SOUND_VER 1u
#define GM_JR_MAGIC 0x5453554Du                 /* "MUST": Journal dieser Firmware */
#define GM_FX 0                                 /* eigene Effekte; Kern 1 rechnet Stimmen */
#define GM_FAC_FUNCS 1
#define GM_NOWN 36u                             /* (ein Sound hat 87 Werte: 36 passen in einen Blob) */
#define GM_OUT_GAIN (32767.0f * 0.7f)
#include "gm_types.h"
#include "mksultra/mksultra.h"

/* ---- Parameter (Reihenfolge des Bausteins), dazu das Basis-Preset */
enum {M_Volume, M_DcoEnvMode, M_VcfEnvMode, M_VcaEnvMode, M_DcoWavePulse, M_DcoWaveSaw, M_DcoWaveSub, M_DcoRange, M_DcoSubLevel, M_Noise, M_HpfCutoff, M_DcoChorusSwitch, M_DcoLfoMod, M_DcoEnvMod, M_DcoAtMod, M_DcoPwmDepth, M_DcoPwmRate, M_VcfCutoff, M_VcfResonance, M_VcfLfoMod, M_VcfEnvMod, M_VcfKeyFollow, M_VcfAftertouch, M_VcaLevel, M_VcaAftertouch, M_LfoRate, M_LfoDelay, M_EnvAttackTime, M_EnvAttackLevel, M_EnvBreakTime, M_EnvBreakLevel, M_EnvDecayTime, M_EnvSustainLevel, M_EnvReleaseTime, M_EnvKeyFollow, M_ChorusRate, M_BenderRange, M_MasterFineTune, M_ModWheelVcfRange, M_ModWheelDcoLfo, M_ModWheelVcfLfo, M_NumberOfVoices, M_PortamentoMode, M_PortamentoTime, M_LfoWaveform, M_LfoSync, M_LfoTrigger, M_LfoInvert, M_mpsEnabled, M_mpeEnabled, M_sysexEnabled, M_resonanceBoost, M_resonanceBoostFrequency, M_specialSawVolume, M_specialPulseVolume, M_specialPulseSync, M_specialPulseTune, M_specialPulseFineTune, M_specialPulseDcoEnv, M_specialPulseDcoLfo, M_UnisonoVoices, M_UnisonoDetune, M_UnisonoSpread, M_RoundRobin, M_Octave, M_NoDcOffset, M_arprate, M_arprange, M_arpmode, M_arpsyncmode, M_reverbDryWet, M_reverbSize, M_reverbDelay, M_reverbTone, M_delayDryWet, M_delayTime, M_delaySync, M_delaySpread, M_delayTone, M_delayFeedback, M_eqBellFreq, M_eqBellQ, M_eqBellGain, M_paramPortamentoTimeMode, M_portamentoChordOriginalMode,
      M_BASE_HI, M_BASE_LO, GM_NP };
/* Anzeigen (laufen vor platform.c: eigene kleine Helfer) */
static void t_put(char *b, const char *s) { while ((*b++ = *s++) != 0) {} }
static void t_num(char *b, int v)
{
    char t[8];
    int n = 0;
    if (v < 0) { *b++ = '-'; v = -v; }
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 7);
    while (n) *b++ = t[--n];
    *b = 0;
}
static void t_int(uint8_t v, char *b) { t_num(b, v); }
static void t_pct(uint8_t v, char *b) { t_num(b, v / 2); }                 /* 0..100 */
static void t_bip(uint8_t v, char *b) { t_num(b, (int)v - 100); }          /* -100..100 */
static void t_db(uint8_t v, char *b) { int d = ((int)v * 48 + 100) / 200 - 24; if (d > 0) *b++ = '+'; t_num(b, d); }
static void t_x3(uint8_t v, char *b) { t_num(b, v * 3 / 200); while (*b) b++; *b++ = '.'; t_num(b, v * 30 / 200 % 10); }
static void t_oct(uint8_t v, char *b) { int d = (int)v - 2; if (d > 0) *b++ = '+'; t_num(b, d); }
static void t_pm24(uint8_t v, char *b) { int d = (int)v - 24; if (d > 0) *b++ = '+'; t_num(b, d); }
static void t_voices(uint8_t v, char *b) { t_num(b, v + 1); }
static void t_onoff(uint8_t v, char *b) { t_put(b, v ? "ON" : "OFF"); }
static void t_base(uint8_t v, char *b) { t_num(b, v); }

static const gm_param_t gm_params[GM_NP] = {
    {"VOLUME", 200, C_AMP, t_pct},  /* 0 Volume */
    {"ENV MODE", 3, C_MOD, t_int},  /* 1 DcoEnvMode */
    {"ENV MODE", 3, C_FLT, t_int},  /* 2 VcfEnvMode */
    {"VCA MODE", 3, C_AMP, t_int},  /* 3 VcaEnvMode */
    {"PULSE", 4, C_SRC, t_int},  /* 4 DcoWavePulse */
    {"SAW", 5, C_SRC, t_int},  /* 5 DcoWaveSaw */
    {"SUB", 5, C_SRC, t_int},  /* 6 DcoWaveSub */
    {"RANGE", 3, C_SRC, t_int},  /* 7 DcoRange */
    {"SUB LVL", 200, C_SRC, t_x3},  /* 8 DcoSubLevel */
    {"NOISE", 200, C_SRC, t_x3},  /* 9 Noise */
    {"HPF", 3, C_FLT, t_int},  /* 10 HpfCutoff */
    {"CHORUS", 1, C_FX, t_onoff},  /* 11 DcoChorusSwitch */
    {"LFO", 200, C_MOD, t_pct},  /* 12 DcoLfoMod */
    {"ENV", 200, C_MOD, t_pct},  /* 13 DcoEnvMod */
    {"AFTERT", 200, C_MOD, t_pct},  /* 14 DcoAtMod */
    {"PWM", 200, C_SRC, t_pct},  /* 15 DcoPwmDepth */
    {"PWM RATE", 200, C_SRC, t_pct},  /* 16 DcoPwmRate */
    {"CUTOFF", 200, C_FLT, t_pct},  /* 17 VcfCutoff */
    {"RESO", 200, C_FLT, t_pct},  /* 18 VcfResonance */
    {"LFO", 200, C_MOD, t_pct},  /* 19 VcfLfoMod */
    {"ENV", 200, C_FLT, t_pct},  /* 20 VcfEnvMod */
    {"KYBD", 200, C_FLT, t_pct},  /* 21 VcfKeyFollow */
    {"AFTERT", 200, C_MOD, t_pct},  /* 22 VcfAftertouch */
    {"LEVEL", 200, C_AMP, t_pct},  /* 23 VcaLevel */
    {"AFTERT", 200, C_MOD, t_pct},  /* 24 VcaAftertouch */
    {"RATE", 200, C_MOD, t_pct},  /* 25 LfoRate */
    {"DELAY", 200, C_MOD, t_pct},  /* 26 LfoDelay */
    {"T1", 200, C_AMP, t_pct},  /* 27 EnvAttackTime */
    {"L1", 200, C_AMP, t_pct},  /* 28 EnvAttackLevel */
    {"T2", 200, C_AMP, t_pct},  /* 29 EnvBreakTime */
    {"L2", 200, C_AMP, t_pct},  /* 30 EnvBreakLevel */
    {"T3", 200, C_AMP, t_pct},  /* 31 EnvDecayTime */
    {"L3", 200, C_AMP, t_pct},  /* 32 EnvSustainLevel */
    {"T4", 200, C_AMP, t_pct},  /* 33 EnvReleaseTime */
    {"KEY FLW", 200, C_AMP, t_pct},  /* 34 EnvKeyFollow */
    {"RATE", 200, C_FX, t_pct},  /* 35 ChorusRate */
    {"BEND", 12, C_MOD, t_int},  /* 36 BenderRange */
    {"FINE", 200, C_SRC, t_pct},  /* 37 MasterFineTune */
    {"MW VCF", 200, C_MOD, t_pct},  /* 38 ModWheelVcfRange */
    {"MW PITCH", 200, C_MOD, t_pct},  /* 39 ModWheelDcoLfo */
    {"MW WAH", 200, C_MOD, t_pct},  /* 40 ModWheelVcfLfo */
    {"VOICES", 11, C_AMP, t_voices},  /* 41 NumberOfVoices */
    {"MODE", 2, C_AMP, t_int},  /* 42 PortamentoMode */
    {"TIME", 200, C_AMP, t_pct},  /* 43 PortamentoTime */
    {"WAVE", 5, C_MOD, t_int},  /* 44 LfoWaveform */
    {"SYNC", 1, C_MOD, t_onoff},  /* 45 LfoSync */
    {"TRIGGER", 1, C_MOD, t_onoff},  /* 46 LfoTrigger */
    {"INVERT", 1, C_MOD, t_onoff},  /* 47 LfoInvert */
    {"MPS", 1, C_AMP, t_onoff},  /* 48 mpsEnabled */
    {"MPE", 1, C_AMP, t_onoff},  /* 49 mpeEnabled */
    {"SYSEX", 1, C_AMP, t_onoff},  /* 50 sysexEnabled */
    {"BOOST", 2, C_FLT, t_int},  /* 51 resonanceBoost */
    {"BOOST HZ", 1, C_FLT, t_onoff},  /* 52 resonanceBoostFrequency */
    {"SAW VOL", 200, C_SRC, t_pct},  /* 53 specialSawVolume */
    {"PLS VOL", 200, C_SRC, t_pct},  /* 54 specialPulseVolume */
    {"SYNC", 1, C_SRC, t_onoff},  /* 55 specialPulseSync */
    {"TUNE", 48, C_SRC, t_pm24},  /* 56 specialPulseTune */
    {"FINE", 200, C_SRC, t_bip},  /* 57 specialPulseFineTune */
    {"ENV", 200, C_MOD, t_bip},  /* 58 specialPulseDcoEnv */
    {"LFO", 200, C_MOD, t_bip},  /* 59 specialPulseDcoLfo */
    {"UNISON", 200, C_SRC, t_pct},  /* 60 UnisonoVoices */
    {"DETUNE", 200, C_SRC, t_pct},  /* 61 UnisonoDetune */
    {"SPREAD", 200, C_SRC, t_pct},  /* 62 UnisonoSpread */
    {"R-ROBIN", 1, C_AMP, t_onoff},  /* 63 RoundRobin */
    {"OCTAVE", 4, C_SRC, t_oct},  /* 64 Octave */
    {"NO DC", 1, C_AMP, t_onoff},  /* 65 NoDcOffset */
    {"RATE", 200, C_FX, t_pct},  /* 66 arprate */
    {"RANGE", 3, C_FX, t_int},  /* 67 arprange */
    {"MODE", 5, C_FX, t_int},  /* 68 arpmode */
    {"SYNC", 2, C_FX, t_int},  /* 69 arpsyncmode */
    {"MIX", 200, C_FX, t_pct},  /* 70 reverbDryWet */
    {"SIZE", 200, C_FX, t_pct},  /* 71 reverbSize */
    {"PREDLY", 200, C_FX, t_pct},  /* 72 reverbDelay */
    {"TONE", 200, C_FX, t_pct},  /* 73 reverbTone */
    {"MIX", 200, C_FX, t_pct},  /* 74 delayDryWet */
    {"TIME", 200, C_FX, t_pct},  /* 75 delayTime */
    {"SYNC", 1, C_FX, t_onoff},  /* 76 delaySync */
    {"SPREAD", 200, C_FX, t_pct},  /* 77 delaySpread */
    {"TONE", 200, C_FX, t_pct},  /* 78 delayTone */
    {"FEEDBK", 200, C_FX, t_pct},  /* 79 delayFeedback */
    {"EQ FREQ", 200, C_FLT, t_pct},  /* 80 eqBellFreq */
    {"EQ Q", 200, C_FLT, t_pct},  /* 81 eqBellQ */
    {"EQ GAIN", 200, C_FLT, t_db},  /* 82 eqBellGain */
    {"TIMEMODE", 1, C_AMP, t_onoff},  /* 83 paramPortamentoTimeMode */
    {"CHORDORG", 1, C_AMP, t_onoff},  /* 84 portamentoChordOriginalMode */
    {"BASE", 255, C_AMP, t_base}, {"BASE", 255, C_AMP, t_base},
};
static uint8_t gm_init_v[GM_NP];                /* INIT = Preset „default“ der Vorlage (beim Start berechnet) */

/* ---- Werkssounds: Ron (vorn, Preset 1–6), Bank A, Bank B, MKS, emptyvessel (zusammen 213; das Gerüst zählt
 * Werkssounds in einem Byte, mehr passen nicht; die übrigen Bänke: offen, ADR 0045) */
enum { CAT_RON, CAT_A, CAT_B, CAT_MKS, CAT_EV, GM_NCAT };
static const char *const gm_cat_name[GM_NCAT] = {"RON", "BANK A", "BANK B", "MKS", "VESSEL"};
static const char *const cat_dir[GM_NCAT] = {"Ron/", "Factory Bank A/", "Factory Bank B/", "Factory MKS/", "Factory emptyvessel/"};
static uint16_t fac_idx[254];
static uint8_t fac_c[254];
static unsigned fac_n;
static int default_idx = -1;
#define GM_NFAC fac_n
static uint8_t fac_buf[GM_NP];

static float val_of(unsigned k, uint8_t b)      /* Byte → Wert in Einheiten des Bausteins */
{
    float mn, mx, d;
    int it;
    mksultra_param_range((int)k, &mn, &mx, &d, &it);
    if (it) return mn + (float)b;
    return mn + (mx - mn) * ((float)b * (1.0f / 200.0f));
}
static uint8_t byte_of(unsigned k, float x)
{
    float mn, mx, d, t;
    int it;
    mksultra_param_range((int)k, &mn, &mx, &d, &it);
    x = x < mn ? mn : x > mx ? mx : x;
    t = it ? x - mn : (x - mn) / (mx - mn) * 200.0f;     /* (mx > mn für alle Parameter) */
    return (uint8_t)(t + 0.5f);
}
static float preset_val(const mksultra_preset_t *p, unsigned k)   /* wie mksultra_load_preset */
{
    float mn, mx, d;
    int it;
    mksultra_param_range((int)k, &mn, &mx, &d, &it);
    float x = (p->have[k / 32] >> (k % 32)) & 1u ? p->v[k] : d;
    if (p->version > 0 && p->version < 8 && k == M_resonanceBoost) x += 1.0f;
    return x;
}
static void bytes_of(unsigned pi, uint8_t *v)
{
    const mksultra_preset_t *p = &mksultra_presets[pi];
    for (unsigned k = 0; k < MKSULTRA_PARAMS; k++) v[k] = byte_of(k, preset_val(p, k));
    v[M_BASE_HI] = (uint8_t)(pi >> 8); v[M_BASE_LO] = (uint8_t)pi;
}
static const char *gm_fac_name(unsigned i) { return mksultra_presets[fac_idx[i]].name; }
static unsigned gm_fac_cat(unsigned i) { return fac_c[i]; }
static const uint8_t *gm_fac_v(unsigned i) { bytes_of(fac_idx[i], fac_buf); return fac_buf; }

/* ---- Seiten */
enum { ST_ENV_PIC = ST_USER };
static const page_t gm_pages[] = {
    {G_HOME, ST_HOME, "PLAY", {PRM("CUTOFF", M_VcfCutoff), PRM("RESO", M_VcfResonance), PRM("ENV", M_VcfEnvMod), PRM("RELEASE", M_EnvReleaseTime)}},
    {G_HOME, ST_NAME, "PERFORM", {PRM("VOLUME", M_Volume), PRM("GLIDE", M_PortamentoTime), PRM("OCTAVE", M_Octave), PRM("CHORUS", M_DcoChorusSwitch)}},
    {G_EDIT, ST_NAME, "DCO", {PRM("PULSE", M_DcoWavePulse), PRM("SAW", M_DcoWaveSaw), PRM("SUB", M_DcoWaveSub), PRM("RANGE", M_DcoRange)}},
    {G_EDIT, ST_NAME, "DCO 2", {PRM("SUB LVL", M_DcoSubLevel), PRM("NOISE", M_Noise), PRM("PWM", M_DcoPwmDepth), PRM("PWM RATE", M_DcoPwmRate)}},
    {G_EDIT, ST_NAME, "DCO MOD", {PRM("LFO", M_DcoLfoMod), PRM("ENV", M_DcoEnvMod), PRM("ENV MODE", M_DcoEnvMode), PRM("AFTERT", M_DcoAtMod)}},
    {G_EDIT, ST_VVCF, "VCF", {PRM("CUTOFF", M_VcfCutoff), PRM("RESO", M_VcfResonance), PRM("ENV", M_VcfEnvMod), PRM("KYBD", M_VcfKeyFollow)}},
    {G_EDIT, ST_VVCF, "VCF 2", {PRM("LFO", M_VcfLfoMod), PRM("ENV MODE", M_VcfEnvMode), PRM("HPF", M_HpfCutoff), PRM("AFTERT", M_VcfAftertouch)}},
    {G_EDIT, ST_NAME, "VCF 3", {PRM("BOOST", M_resonanceBoost), PRM("BOOST HZ", M_resonanceBoostFrequency), PRM("NO DC", M_NoDcOffset), PRM("LEVEL", M_VcaLevel)}},
    {G_EDIT, ST_NAME, "SPECIAL", {PRM("SAW VOL", M_specialSawVolume), PRM("PLS VOL", M_specialPulseVolume), PRM("SYNC", M_specialPulseSync), PRM("TUNE", M_specialPulseTune)}},
    {G_EDIT, ST_NAME, "SPECIAL 2", {PRM("FINE", M_specialPulseFineTune), PRM("ENV", M_specialPulseDcoEnv), PRM("LFO", M_specialPulseDcoLfo), NO}},
    {G_EDIT, ST_NAME, "UNISON", {PRM("UNISON", M_UnisonoVoices), PRM("DETUNE", M_UnisonoDetune), PRM("SPREAD", M_UnisonoSpread), PRM("R-ROBIN", M_RoundRobin)}},
    {G_ENV, ST_ENV_PIC, "ENV", {PRM("T1", M_EnvAttackTime), PRM("L1", M_EnvAttackLevel), PRM("T2", M_EnvBreakTime), PRM("L2", M_EnvBreakLevel)}},
    {G_ENV, ST_ENV_PIC, "ENV 2", {PRM("T3", M_EnvDecayTime), PRM("L3", M_EnvSustainLevel), PRM("T4", M_EnvReleaseTime), PRM("KEY FLW", M_EnvKeyFollow)}},
    {G_ENV, ST_NAME, "VCA", {PRM("VCA MODE", M_VcaEnvMode), PRM("LEVEL", M_VcaLevel), PRM("AFTERT", M_VcaAftertouch), NO}},
    {G_LFO, ST_VLFO, "LFO", {PRM("RATE", M_LfoRate), PRM("DELAY", M_LfoDelay), PRM("WAVE", M_LfoWaveform), PRM("SYNC", M_LfoSync)}},
    {G_LFO, ST_VLFO, "LFO 2", {PRM("TRIGGER", M_LfoTrigger), PRM("INVERT", M_LfoInvert), NO, NO}},
    {G_LFO, ST_NAME, "CONTROL", {PRM("BEND", M_BenderRange), PRM("MW VCF", M_ModWheelVcfRange), PRM("MW PITCH", M_ModWheelDcoLfo), PRM("MW WAH", M_ModWheelVcfLfo)}},
    {G_LFO, ST_NAME, "GLIDE", {PRM("MODE", M_PortamentoMode), PRM("TIME", M_PortamentoTime), PRM("TIMEMODE", M_paramPortamentoTimeMode), PRM("CHORDORG", M_portamentoChordOriginalMode)}},
    {G_FX, ST_NAME, "CHORUS + EQ", {PRM("CHORUS", M_DcoChorusSwitch), PRM("RATE", M_ChorusRate), PRM("EQ FREQ", M_eqBellFreq), PRM("EQ GAIN", M_eqBellGain)}},
    {G_FX, ST_VDLY, "DELAY", {PRM("MIX", M_delayDryWet), PRM("TIME", M_delayTime), PRM("FEEDBK", M_delayFeedback), PRM("SYNC", M_delaySync)}},
    {G_FX, ST_VDLY, "DELAY 2", {PRM("SPREAD", M_delaySpread), PRM("TONE", M_delayTone), PRM("EQ Q", M_eqBellQ), NO}},
    {G_FX, ST_VREV, "REVERB", {PRM("MIX", M_reverbDryWet), PRM("SIZE", M_reverbSize), PRM("PREDLY", M_reverbDelay), PRM("TONE", M_reverbTone)}},
    {G_ARP, ST_NAME, "ARP", {PRM("MODE", M_arpmode), PRM("RATE", M_arprate), PRM("RANGE", M_arprange), PRM("SYNC", M_arpsyncmode)}},
    {G_SEQ, ST_SEQ, "SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
    {G_SEQ, ST_LIVE, "LOOP", {LVK("LOOP", LV_SET_ON), LVK("BARS", LV_SET_BARS), LVK("LONGER", LV_SET_GROW), LVK("SHORTER", LV_SET_CUT)}},
    {G_GLO, ST_TEMPO, "TEMPO", {{"TEMPO", K_TEMPO, 0}, {"CLOCK", K_CLK, 0}, {"SEND", K_CLK, 1}, {"MIDI CH", K_CHAN, 0}}},
    {G_GLO, ST_NAME, "VOICES", {PRM("VOICES", M_NumberOfVoices), PRM("FINE", M_MasterFineTune), PRM("BEND", M_BenderRange), {"LEVEL", K_LEVEL, 0}}},
    {G_GLO, ST_SYS, "SYSTEM", {{"LEVEL", K_LEVEL, 0}, NO, NO, NO}},
    {G_SEL, ST_BROWSE, "SOUNDS", {NO, NO, NO, NO}},
};
static const layer_t gm_layers[NGROUP] = {
    [G_EDIT] = {"DCO", {PRM("PULSE", M_DcoWavePulse), PRM("SAW", M_DcoWaveSaw), PRM("SUB", M_DcoWaveSub), PRM("PWM", M_DcoPwmDepth)}},
    [G_ENV] = {"ENV", {PRM("T1", M_EnvAttackTime), PRM("T3", M_EnvDecayTime), PRM("L3", M_EnvSustainLevel), PRM("T4", M_EnvReleaseTime)}},
    [G_LFO] = {"LFO", {PRM("RATE", M_LfoRate), PRM("DELAY", M_LfoDelay), PRM("PITCH", M_DcoLfoMod), PRM("FILTER", M_VcfLfoMod)}},
    [G_FX] = {"EFFECTS", {PRM("CHORUS", M_DcoChorusSwitch), PRM("DELAY", M_delayDryWet), PRM("TIME", M_delayTime), PRM("REVERB", M_reverbDryWet)}},
    [G_ARP] = {"ARP", {PRM("MODE", M_arpmode), PRM("RATE", M_arprate), PRM("RANGE", M_arprange), PRM("SYNC", M_arpsyncmode)}},
    [G_GLO] = {"PERFORM", {PRM("VOLUME", M_Volume), PRM("GLIDE", M_PortamentoTime), PRM("OCTAVE", M_Octave), PRM("VOICES", M_NumberOfVoices)}},
    [G_SEQ] = {"SEQUENCER", {SQK("SEQ", SQ_ON), SQK("RATE", SQ_DIV), SQK("GATE", SQ_GATE), SQK("SHIFT", SQ_SHIFT)}},
};

/* ---- der Klang */
#ifndef MKU_STATE_BYTES
#define MKU_STATE_BYTES 284000u
#endif
static uint8_t mku[MKU_STATE_BYTES] __attribute__((section(".pool"), aligned(16)));
#if MKSULTRA_DELAY_EXTERN
static uint8_t mku_dl[MKU_DELAY_BYTES] __attribute__((section(".pool"), aligned(4)));   /* Delay links: Pool */
static uint8_t mku_dr[MKU_DELAY_BYTES] __attribute__((aligned(4)));                     /* Delay rechts: RAM */
#endif
static uint8_t mku_ok;
static uint8_t eng_v[GM_NP];
static volatile float mku_bpm;
#define DSPF __attribute__((section(".dsp_text"), noinline))
static void kern_boot(void)
{
    for (unsigned c = 0; c < GM_NCAT; c++)
        for (int i = 0; i < mksultra_preset_count && fac_n < 254u; i++) {
            const char *p = mksultra_presets[i].path, *d = cat_dir[c];
            unsigned k = 0;
            while (d[k] && p[k] == d[k]) k++;
            if (!d[k]) { fac_idx[fac_n] = (uint16_t)i; fac_c[fac_n] = (uint8_t)c; fac_n++; }
        }
    for (int i = 0; i < mksultra_preset_count; i++) {
        const char *p = mksultra_presets[i].path, *d = "default.mks";
        unsigned k = 0;
        while (d[k] && p[k] == d[k]) k++;
        if (!d[k] && !p[k]) default_idx = i;
    }
    bytes_of(default_idx >= 0 ? (unsigned)default_idx : 0u, gm_init_v);
    if (mksultra_baustein.state_size > sizeof mku) return;   /* (Seite SYSTEM meldet es) */
#if MKSULTRA_DELAY_EXTERN
    if (mksultra_delay_bytes() > MKU_DELAY_BYTES) return;
    mksultra_delay_memory(mku_dl, mku_dr);
#endif
    mksultra_init(mku, 44100.0f);
    mku_ok = (uint8_t)mksultra_fx_ok(mku);
    for (unsigned i = 0; i < GM_NP; i++) eng_v[i] = 0xFFu;
}
static DSPF void kern_set(unsigned p, unsigned i, uint8_t v)
{
    (void)p;
    if (!mku_ok || i >= MKSULTRA_PARAMS) return;
    eng_v[i] = v;
    mksultra_set_param(mku, (int)i, val_of(i, v));
}
static DSPF void kern_load(unsigned p, const uint8_t *v)
{
    unsigned base = (unsigned)v[M_BASE_HI] << 8 | v[M_BASE_LO];
    (void)p;
    if (!mku_ok) return;
    if (base < (unsigned)mksultra_preset_count) {
        static uint8_t q[GM_NP];
        mksultra_load_preset(mku, &mksultra_presets[base]);          /* genau wie im Baustein */
        bytes_of(base, q);
        for (unsigned i = 0; i < MKSULTRA_PARAMS; i++) { eng_v[i] = q[i]; if (v[i] != q[i]) kern_set(p, i, v[i]); }
    } else {
        for (unsigned i = 0; i < MKSULTRA_PARAMS; i++) kern_set(p, i, v[i]);
    }
    eng_v[M_BASE_HI] = v[M_BASE_HI]; eng_v[M_BASE_LO] = v[M_BASE_LO];
}
static DSPF void kern_on(unsigned p, int note, int vel, int legato) { (void)p; (void)legato; if (mku_ok) mksultra_note_on(mku, note, vel ? vel : 1); }
static DSPF void kern_off(unsigned p, int note) { (void)p; if (mku_ok) mksultra_note_off(mku, note); }
static DSPF void kern_all_off(unsigned p) { (void)p; if (mku_ok) mksultra_all_notes_off(mku); }
static DSPF void kern_bend(unsigned p, float b) { (void)p; if (mku_ok) mksultra_pitch_bend(mku, b); }
static DSPF void kern_mod(unsigned p, float m) { (void)p; if (mku_ok) mksultra_mod_wheel(mku, m); }
static int kern_voices(unsigned p) { (void)p; return mku_ok ? mksultra_active_voices(mku) : 0; }
static void kern_render(unsigned p, float *l, float *r, unsigned n);
static int kern_stage(unsigned stage, unsigned p);
static int mku_viz(unsigned stage, unsigned p, float *v);
#define GM_VIZ(stage, p, v) mku_viz(stage, p, v)   /* Grafik-Kopf (ADR 0055) */

#include "gm_app.c"

/* Grafik-Kopf (ADR 0055): die Werte des Sounds (Effekte des Bausteins, nicht des Gerüsts) */
static float mku_n(const uint8_t *u, unsigned i) { return (float)u[i] / (float)(gm_params[i].max ? gm_params[i].max : 1u); }
static int mku_viz(unsigned stage, unsigned p, float *v)
{
    static const int8_t wave[6] = {6, 0, 2, 1, 4, 5};   /* Baustein: SINE TRI SAW RECT S&H NOISE */
    const uint8_t *u = up[p].v;
    switch (stage) {
    case ST_VLFO: {
        float a = mku_n(u, M_DcoLfoMod), b = mku_n(u, M_VcfLfoMod);
        v[0] = mku_n(u, M_LfoRate); v[1] = a > b ? a : b; v[2] = wave[u[M_LfoWaveform] % 6u]; v[3] = -1.0f;
        return 1;
    }
    case ST_VVCF: v[0] = mku_n(u, M_VcfCutoff); v[1] = mku_n(u, M_VcfResonance); v[2] = 0; v[3] = mku_n(u, M_VcfEnvMod); return 1;
    case ST_VDLY: v[0] = mku_n(u, M_delayTime); v[1] = mku_n(u, M_delayFeedback); v[2] = mku_n(u, M_delayDryWet); v[3] = mku_n(u, M_delaySpread) > 0.5f; return 1;
    case ST_VREV: v[0] = mku_n(u, M_reverbDelay); v[1] = mku_n(u, M_reverbSize); v[2] = 1.0f - mku_n(u, M_reverbTone); v[3] = mku_n(u, M_reverbDryWet); return 1;
    default: return 0;
    }
}

void *memmove(void *d, const void *s, unsigned n)
{
    uint8_t *a = d;
    const uint8_t *b = s;
    if (a < b) while (n--) *a++ = *b++;
    else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
}

/* ---- Fließband: Kern 1 Stimmen (Block N), Kern 0 Master (Block N−1) */
static float vbuf[2][2][GM_BLOCK], mbuf[2][GM_BLOCK];
static volatile uint32_t vcur;
volatile uint32_t mku_meas[4];                  /* Takte: Stimmen (Kern 1), Master (Kern 0), Spitzen (für den Emulator) */
static DSPF void c1_voices(uint32_t a)
{
    uint32_t t0 = FM1_C1_T4;
    mksultra_voices(mku, vbuf[a][0], vbuf[a][1], GM_BLOCK);
    t0 = FM1_C1_T4 - t0;
    mku_meas[0] = t0; if (t0 > mku_meas[2]) mku_meas[2] = t0;
    FM1_C1_SYNC();
}
static DSPF void kern_render(unsigned p, float *l, float *r, unsigned n)
{
    uint32_t cur = vcur, started = 0, t0;
    (void)p;
    if (!mku_ok || n != GM_BLOCK) return;
    if (bpm_now != mku_bpm) { mku_bpm = bpm_now; mksultra_set_tempo(mku, bpm_now); }
    if (cpu1_ok && fm1_c1_on) started = (uint32_t)fm1_cpu1_run(c1_voices, cur);
    t0 = fm1_ticks();
    for (unsigned i = 0; i < GM_BLOCK; i++) { mbuf[0][i] = vbuf[cur ^ 1u][0][i]; mbuf[1][i] = vbuf[cur ^ 1u][1][i]; }
    mksultra_master(mku, mbuf[0], mbuf[1], GM_BLOCK);
    t0 = fm1_ticks() - t0;
    mku_meas[1] = t0; if (t0 > mku_meas[3]) mku_meas[3] = t0;
    if (started) {
        if (fm1_cpu1_wait() != 0) { cpu1_ok = 0; fm1_cpu1_hold(); fm1_cpu1_bar(); mksultra_voices(mku, vbuf[cur][0], vbuf[cur][1], GM_BLOCK); }
    } else mksultra_voices(mku, vbuf[cur][0], vbuf[cur][1], GM_BLOCK);
    FM1_C1_SYNC();
    vcur = cur ^ 1u;
    for (unsigned i = 0; i < GM_BLOCK; i++) { l[i] += mbuf[0][i]; r[i] += mbuf[1][i]; }
}

/* ---- Bilder */
static int kern_stage(unsigned stage, unsigned p)
{
    if (stage != ST_ENV_PIC) return 0;
    {   /* die Hüllkurve mit vier Zeiten und drei Pegeln */
        const uint8_t *v = up[p].v;
        int32_t x0 = 10, x1 = x0 + 6 + v[M_EnvAttackTime] * 40 / 200, x2 = x1 + 6 + v[M_EnvBreakTime] * 40 / 200,
                x3 = x2 + 6 + v[M_EnvDecayTime] * 40 / 200, x4 = x3 + 30, x5 = x4 + 6 + v[M_EnvReleaseTime] * 40 / 200;
        int32_t yb = 140, h = 96, y1 = yb - h * v[M_EnvAttackLevel] / 200, y2 = yb - h * v[M_EnvBreakLevel] / 200,
                y3 = yb - h * v[M_EnvSustainLevel] / 200;
        LN(x0, yb, x1, y1, T_SEL); LN(x1, y1, x2, y2, T_SEL); LN(x2, y2, x3, y3, T_SEL); LN(x3, y3, x4, y3, T_SEL);
        LN(x4, y3, x5, yb, T_SEL);
        R(10, yb + 1, 220, 1, T_LINE);
        draw_sound_line(30);
    }
    return 1;
}
