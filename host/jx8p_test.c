/* SPDX-License-Identifier: GPL-3.0-only
 * Host test of the JX-8P engine (engine/jx8p.c, docs/adr/0039), built with EL_SYNTH_JX=1 (make test-jx):
 * every factory sound sounds and ends, poly / mono / unison take the voices they should, values stay in
 * their ranges, the sound list has every area. Writes one WAV per area's first sound to the given folder. */
#include "synth.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#if !EL_SYNTH_JX
#error "build with -DEL_SYNTH_JX=1"
#endif

static float bl[EL_BLOCK], br[EL_BLOCK], dl[EL_BLOCK], rv[EL_BLOCK];

/* Renders ms milliseconds; returns the peak, counts NaNs. */
static float run(jx8p_part_t *p, unsigned ms, unsigned *nans, FILE *wav)
{
    float peak = 0;
    unsigned blocks = (unsigned)(ms * EL_SAMPLE_RATE / 1000.0f) / EL_BLOCK + 1u;
    for (unsigned b = 0; b < blocks; ++b) {
        memset(bl, 0, sizeof bl); memset(br, 0, sizeof br); memset(dl, 0, sizeof dl); memset(rv, 0, sizeof rv);
        jx8p_render(p, bl, br, dl, rv, EL_BLOCK, 120.0f);
        for (unsigned i = 0; i < EL_BLOCK; ++i) {
            if (isnan(bl[i]) || isnan(br[i])) ++*nans;
            float a = fabsf(bl[i]) > fabsf(br[i]) ? fabsf(bl[i]) : fabsf(br[i]);
            if (a > peak) peak = a;
            if (wav) {
                int16_t s[2] = {(int16_t)(fmaxf(-1, fminf(1, bl[i])) * 32767), (int16_t)(fmaxf(-1, fminf(1, br[i])) * 32767)};
                fwrite(s, sizeof s, 1, wav);
            }
        }
    }
    return peak;
}

static FILE *wav_open(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/jx8p_%s.wav", dir, name);
    for (char *c = path + strlen(dir) + 1; *c; ++c) if (*c == ' ' || *c == '.' || *c == '/') *c = '_';
    memcpy(path + strlen(path) - 4, ".wav", 4);
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    uint8_t h[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0,
                     0x44, 0xac, 0, 0, 0x10, 0xb1, 2, 0, 4, 0, 16, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0};
    fwrite(h, 1, sizeof h, f);
    return f;
}

static void wav_close(FILE *f)
{
    if (!f) return;
    long n = ftell(f);
    uint32_t riff = (uint32_t)(n - 8), data = (uint32_t)(n - 44);
    fseek(f, 4, SEEK_SET); fwrite(&riff, 4, 1, f);
    fseek(f, 40, SEEK_SET); fwrite(&data, 4, 1, f);
    fclose(f);
}

static unsigned busy(const jx8p_part_t *p)
{
    unsigned n = 0;
    for (unsigned i = 0; i < p->voices; ++i) n += p->voice[i].active;
    return n;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    static jx8p_part_t p;
    int ok = 1;

    /* the list: every area has sounds, names fit, values in range */
    unsigned area[4] = {0, 0, 0, 0};
    for (unsigned s = 0; s < jx8p_preset_count; ++s) {
        const jx8p_preset_t *x = &jx8p_presets[s];
        ok = ok && x->area >= 1 && x->area <= 3 && strlen(x->name) <= 10;
        if (x->area <= 3) ++area[x->area];
        for (unsigned k = 0; k < JX_COUNT; ++k)
            if (x->v[k] > jx8p_param_max(k)) { printf("  %s: %s %u > %d\n", x->name, jx8p_param_name(k), x->v[k], jx8p_param_max(k)); ok = 0; }
    }
    ok = ok && (int)JX_COUNT <= (int)SYN_VALUES && area[1] && area[2] && area[3];
    printf("jx8p list: %u sounds (rhythm %u, strings %u, bass %u), values in range %s\n", jx8p_preset_count, area[1], area[2],
           area[3], ok ? "ok" : "FAIL");

    /* every factory sound: a note sounds (not too loud), no NaN, ends after the key goes up */
    int sounds_ok = 1;
    unsigned written[4] = {0, 0, 0, 0};
    float loudest = 0, quietest = 10;
    const char *loud_name = "", *quiet_name = "";
    for (unsigned s = 0; s < jx8p_preset_count; ++s) {
        const jx8p_preset_t *x = &jx8p_presets[s];
        unsigned nans = 0;
        jx8p_part_init(&p, x->area == 3 ? 1u : 3u, 1234u + s);
        jx8p_part_load(&p, x->v);
        int note = x->area == 3 ? 36 : x->area == 2 ? 60 : 64;
        FILE *w = written[x->area]++ ? 0 : wav_open(dir, x->name);
        jx8p_note_on(&p, note, 0.8f, 0);
        if (x->area == 2) { jx8p_note_on(&p, note + 4, 0.8f, 0); jx8p_note_on(&p, note + 7, 0.8f, 0); }
        float held = run(&p, 1500, &nans, w);
        jx8p_note_off(&p, note);
        if (x->area == 2) { jx8p_note_off(&p, note + 4); jx8p_note_off(&p, note + 7); }
        run(&p, 14000, &nans, w);   /* the longest JX-8P release is about 12 s */
        wav_close(w);
        int ended = !busy(&p);
        if (held > loudest) { loudest = held; loud_name = x->name; }
        if (held < quietest) { quietest = held; quiet_name = x->name; }
        if (nans || held < 0.005f || held > 2.0f || !ended) {
            printf("  %s: peak %.3f nans %u ended %d\n", x->name, held, nans, ended);
            sounds_ok = 0;
        }
    }
    printf("jx8p sounds: every one sounds and ends, peaks %.3f (%s) .. %.3f (%s) %s\n", quietest, quiet_name, loudest,
           loud_name, sounds_ok ? "ok" : "FAIL");

    /* key modes: poly takes a voice per note, mono one, unison all; mono goes back to the held note */
    uint8_t v[JX_COUNT];
    memcpy(v, jx8p_presets[jx8p_preset_count - 3].v, sizeof v);   /* INIT RHY */
    jx8p_part_init(&p, 3, 7);
    jx8p_part_load(&p, v);
    jx8p_note_on(&p, 60, 0.8f, 0); jx8p_note_on(&p, 64, 0.8f, 0); jx8p_note_on(&p, 67, 0.8f, 0);
    unsigned nans = 0;
    run(&p, 50, &nans, 0);
    int modes = busy(&p) == 3;
    jx8p_note_on(&p, 72, 0.8f, 0);   /* a fourth note takes the oldest voice */
    run(&p, 20, &nans, 0);
    modes = modes && busy(&p) == 3;
    jx8p_all_off(&p);
    run(&p, 3000, &nans, 0);
    modes = modes && !busy(&p);
    jx8p_part_set(&p, JX_KEY_MODE, 2);   /* mono */
    jx8p_note_on(&p, 48, 0.8f, 0); jx8p_note_on(&p, 55, 0.8f, 0);
    run(&p, 20, &nans, 0);
    modes = modes && busy(&p) == 1 && p.voice[0].key == 55;
    jx8p_note_off(&p, 55);
    run(&p, 20, &nans, 0);
    modes = modes && busy(&p) == 1 && p.voice[0].key == 48 && p.voice[0].gate;
    jx8p_note_off(&p, 48);
    run(&p, 3000, &nans, 0);
    jx8p_part_set(&p, JX_KEY_MODE, 3);   /* unison: every voice of the part */
    jx8p_note_on(&p, 48, 0.8f, 0);
    run(&p, 20, &nans, 0);
    modes = modes && busy(&p) == 3 && !nans;
    jx8p_part_set(&p, JX_CUTOFF, 300);   /* clamped */
    modes = modes && p.v[JX_CUTOFF] == 127;
    printf("jx8p key modes: poly 3 voices, steal, mono back to the held note, unison all, clamp %s\n", modes ? "ok" : "FAIL");

    /* the engine slot: synth.h plays it as engine 0, with the chord memory */
    static syn_part_t sp;
    syn_init(&sp, SYN_MKS, 3, 9);
    int chord_sound = -1;
    for (unsigned s = 0; s < syn_sound_count() && chord_sound < 0; ++s)
        if (syn_sound_engine(s) == SYN_MKS && !strcmp(syn_sound_name(s), "HI STRINGS")) chord_sound = (int)s;
    int slot = chord_sound >= 0;
    if (slot) {
        memcpy(v, syn_sound_values((unsigned)chord_sound), sizeof v);
        v[JX_KEY_MODE] = 1; v[JX_CHORD] = 6;   /* CHORD, MAJ */
        syn_load(&sp, SYN_MKS, v);
        slot = syn_chord_memory(&sp) == 7u && !strcmp(syn_engine_names[SYN_MKS], "JX-8P");
    }
    printf("jx8p slot: engine 0 is the JX-8P, chord memory %s\n", slot ? "ok" : "FAIL");

    return ok && sounds_ok && modes && slot ? 0 : 1;
}
