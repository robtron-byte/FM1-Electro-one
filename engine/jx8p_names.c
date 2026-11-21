/* SPDX-License-Identifier: GPL-3.0-only
 * JX-8P engine: names of its parameters and values for the display (control task, Flash), apart from
 * engine/jx8p.c so they do not take audio SRAM on the device. From fm1-synths (github.com/robtron-byte/
 * fm1-synths, commit 2a7b4fa, jx8p/engine/jx.c, GPL-3.0-only, robtron-byte), names JP_ -> JX_. */
#include "el_config.h"
#if EL_SYNTH_JX
#include "jx8p.h"

const char *jx8p_param_name(unsigned p)
{
    switch (p) {
    case JX_DCO1_RANGE: return "1 RANGE"; case JX_DCO1_WAVE: return "1 WAVE"; case JX_DCO1_TUNE: return "1 TUNE";
    case JX_DCO1_LFO: return "1 LFO"; case JX_DCO1_ENV: return "1 ENV";
    case JX_DCO2_RANGE: return "2 RANGE"; case JX_DCO2_WAVE: return "2 WAVE"; case JX_DCO2_XMOD: return "X-MOD";
    case JX_DCO2_TUNE: return "2 TUNE"; case JX_DCO2_FINE: return "2 FINE"; case JX_DCO2_LFO: return "2 LFO";
    case JX_DCO2_ENV: return "2 ENV"; case JX_DCO_DYNA: return "DCO DYN"; case JX_DCO_ENV_MODE: return "DCO EMD";
    case JX_MIX_DCO1: return "DCO1"; case JX_MIX_DCO2: return "DCO2"; case JX_MIX_ENV: return "MIX ENV";
    case JX_MIX_DYNA: return "MIX DYN"; case JX_MIX_ENV_MODE: return "MIX EMD"; case JX_HPF: return "HPF";
    case JX_CUTOFF: return "CUTOFF"; case JX_RESO: return "RESO"; case JX_VCF_LFO: return "VCF LFO";
    case JX_VCF_ENV: return "VCF ENV"; case JX_VCF_KEY: return "KEY FLW"; case JX_VCF_DYNA: return "VCF DYN";
    case JX_VCF_ENV_MODE: return "VCF EMD"; case JX_VCA_LEVEL: return "LEVEL"; case JX_VCA_DYNA: return "VCA DYN";
    case JX_CHORUS: return "CHORUS"; case JX_LFO_WAVE: return "LFO WAVE"; case JX_LFO_DELAY: return "LFO DLY";
    case JX_LFO_RATE: return "LFO RATE";
    case JX_E1_ATTACK: return "1 ATTACK"; case JX_E1_DECAY: return "1 DECAY"; case JX_E1_SUSTAIN: return "1 SUST";
    case JX_E1_RELEASE: return "1 REL"; case JX_E1_KEY: return "1 KEY";
    case JX_E2_ATTACK: return "2 ATTACK"; case JX_E2_DECAY: return "2 DECAY"; case JX_E2_SUSTAIN: return "2 SUST";
    case JX_E2_RELEASE: return "2 REL"; case JX_E2_KEY: return "2 KEY"; case JX_VCA_ENV_MODE: return "VCA MODE";
    case JX_BENDER: return "BEND"; case JX_BEND_LFO: return "BEND LFO"; case JX_AFTER_VIB: return "AT VIB";
    case JX_AFTER_BRIL: return "AT BRIL"; case JX_AFTER_VOL: return "AT VOL"; case JX_CHORUS_RATE: return "CHO RATE";
    case JX_CHORUS_MODE: return "CHO MODE"; case JX_KEY_MODE: return "KEY MODE"; case JX_PORTA_TIME: return "GLIDE";
    case JX_PORTA: return "PORTA"; case JX_MOD_SENS: return "MOD SENS"; case JX_VOLUME: return "VOLUME";
    case JX_KEY_SHIFT: return "SHIFT"; case JX_DRIFT: return "DRIFT"; case JX_UNISON_DETUNE: return "UNI DET";
    case JX_LFO_SHAPE: return "LFO SHP"; case JX_LFO_SYNC: return "LFO SYNC"; case JX_LFO_DIV: return "LFO DIV";
    case JX_LFO_KEYSYNC: return "LFO KEY"; case JX_ENV_RETRIG: return "RETRIG"; case JX_ENV_SNAP: return "ENV SNAP";
    case JX_DELAY_SEND: return "DELAY"; case JX_REVERB_SEND: return "REVERB"; case JX_CHORD: return "CHORD";
    default: return "";
    }
}

const char *jx8p_param_value_name(unsigned p, int value)
{
    static const char *const ranges[] = {"16'", "8'", "4'", "2'"};
    static const char *const waves[] = {"NOISE", "SQUARE", "PULSE", "SAW"};
    static const char *const xmod[] = {"OFF", "SYNC1", "SYNC2", "XMOD"};
    static const char *const env_modes[] = {"2 DOWN", "2 UP", "1 DOWN", "1 UP"};
    static const char *const chorus[] = {"OFF", "I", "II"};
    static const char *const lfo_waves[] = {"RANDOM", "SQUARE", "SINE"};
    static const char *const vca[] = {"GATE", "ENV2"};
    static const char *const off_on[] = {"OFF", "ON"};
    static const char *const shapes[] = {"JX", "TRI", "SINE", "SAW UP", "SAW DN", "SQUARE", "S&H", "RANDOM"};
    static const char *const divs[] = {"1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4T",
                                       "1/4", "1/4.", "1/2", "1 BAR", "2 BARS", "4 BARS"};
    static const char *const key_modes[] = {"POLY", "CHORD", "MONO", "UNISON"};
    static const char *const porta[] = {"OFF", "ON", "AUTO"};
    static const char *const retrig[] = {"MULTI", "LEGATO"};
    static const char *const snap[] = {"ORIG", "SNAP"};
    static const char *const cho_modes[] = {"JX", "I+II", "FREE"};
    static const char *const tunes[] = {"-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0",
                                        "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "+11", "+12"};
    value = el_clampi(value, 0, jx8p_param_max(p));
    switch (p) {
    case JX_DCO1_RANGE: case JX_DCO2_RANGE: return ranges[value];
    case JX_DCO1_WAVE: case JX_DCO2_WAVE: return waves[value];
    case JX_DCO2_XMOD: return xmod[value];
    case JX_DCO_ENV_MODE: case JX_MIX_ENV_MODE: case JX_VCF_ENV_MODE: return env_modes[value];
    case JX_CHORUS: return chorus[value];
    case JX_LFO_WAVE: return lfo_waves[value];
    case JX_VCA_ENV_MODE: return vca[value];
    case JX_DCO1_TUNE: case JX_DCO2_TUNE: case JX_KEY_SHIFT: return tunes[value];
    case JX_LFO_SYNC: case JX_LFO_KEYSYNC: return off_on[value];
    case JX_LFO_SHAPE: return shapes[value];
    case JX_LFO_DIV: return divs[value];
    case JX_KEY_MODE: return key_modes[value];
    case JX_PORTA: return porta[value];
    case JX_ENV_RETRIG: return retrig[value];
    case JX_ENV_SNAP: return snap[value];
    case JX_CHORUS_MODE: return cho_modes[value];
    default: return 0;
    }
}

#endif
