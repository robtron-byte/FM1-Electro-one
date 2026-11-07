/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * The 808's bass drum of tr808.c (bass_drum()) in fixed point, for the own
 * platform (docs/adr/0022, 0006: the calibrated model stays the sound).
 * Included by tr808.c after tr808_metal_fixed.h (its helpers).
 *
 * The same model, sample for sample: the trigger pulse, the resonator
 * (raw voltage of the TPT state-variable filter, its frequency and Q set
 * every 8 samples as the float code does, by the float svf_set), the
 * retrigger pulse, the sigh, the octave body, the feedback buffer's
 * non-linearity, the tone path and the DC block. Signals Q24 (the
 * resonator stays below about 0.35), coefficients Q28, envelopes Q30.
 * Floats per render call, every 8 samples (the resonator's coefficients)
 * and, only with DRIVE up, the tanh. */
#if TR808_FIXED && TR808_BD_FIXED

#define DB_ONE24 (1 << 24)
static inline EL_DSP_HELPER int32_t db_q24(float x) { return (int32_t)(x * 16777216.0f); }

/* the band-pass's raw voltage v1 (svf_bp(..) / k) */
static inline EL_DSP_HELPER int32_t db_bp_raw(dm_svf_t *f, int32_t x)
{
    int32_t v3 = x - f->ic2, v1 = DM28(f->a1, f->ic1) + DM28(f->a2, v3), v2 = f->ic2 + DM28(f->a2, f->ic1) + DM28(f->a3, v3);
    f->ic1 = 2 * v1 - f->ic1;
    f->ic2 = 2 * v2 - f->ic2;
    return v1;
}

static struct {
    dm_svf_t r1, r2;
    int32_t pulse_lp, aux, out_lp, dc_x, dc_y, env, pitch, env3;
    float r1_hz, r1_q;          /* the resonator as last set (skip when it does not move) */
    float r2_hz;                /* the body band-pass as last set */
    tr_memo_t memo[DM_MEMOS];
    uint32_t age;
    uint8_t active;
} db;
static float db_y[EL_BLOCK];
static void db_reset(void) { memset(&db, 0, sizeof db); dm_memo_init(db.memo, DM_MEMOS); }

/* Renders the bass drum of this call into db_y; returns 1 if it sounds. */
static EL_DSP_CODE int db_render(tr808_t *t, int frames)
{
    const int part = D8_BD;
    tr_voice_t *tv = &t->v[part];
    if (po_engine(t, part) || !tv->active) { db.active = 0; return 0; }
    const float *k = tr808_bd_cal;
    if (tv->age == 0 || !db.active) {           /* a hit: the resonator keeps its state (no machine gun) */
        db.active = 1;
        db.age = 0;
        db.pulse_lp = 0;
        db.aux = 0;
        db.env = db.pitch = db.env3 = DM_ONE30;
    }
    /* ---- per call, as bass_drum() works them out */
    float f0 = k[0] * (t->key_on ? t->key_r[part] : tolerance[part] * spread(t, part, P_TUNE)) * t->shift[part];
    float kd = knob(t, part, P_DECAY);
    float q = k[11] * m_exp2(&db.memo[1], k[12] * m_powf(&db.memo[0], kd, 0.8f));
    float punch = knob(t, part, P_SOUND);
    uint32_t attack = (uint32_t)MS(k[5] * (1.0f + punch));
    float sigh0 = k[1] + k[2] * tv->accent;
    const int32_t c_env = dm_q30(m_decay(&db.memo[2], q / (DSP_PI * f0) * EL_SAMPLE_RATE));
    const int32_t c_pitch = dm_q30(m_decay(&db.memo[3], MS(k[13])));
    const int32_t c_aux = dm_q30(0.99955f), c_plp = dm_q28(0.2035f), c_fade = dm_q30(0.9925f);
    const int32_t volts = db_q24(tv->volts), clamp = db_q24(-0.05f), aux0 = db_q24(-k[7] * 0.2f * tv->volts);
    const int32_t g_in = dm_q28(k[6]), body = dm_q28(TR808_BD_BODY);
    const float x_scale = k[14];
    const int32_t c_x = dm_q28(k[14]), c_h2 = dm_q28(k[8]), c_h3 = dm_q28(k[9]), c_xinv = dm_q28(1.0f / k[14]);
    float drive = knob(t, part, P_SNAPPY), d = 1.0f + 6.0f * drive, d_norm = drive > 0 ? 1.0f / dsp_sqrt(d) : 0.0f;
    const int32_t c_drive = dm_q28(d * (1.0f / 16.0f)), c_dnorm = dm_q28(d_norm);   /* (Q24 in, Q20 for the table) */
    float tone = knob(t, part, P_TONE);
    float cutoff = 305.0f * m_exp2(&db.memo[4], 4.56f * tone);
    const int32_t c_click = dm_q28(k[10] + 0.18f * tone);
    const int32_t c_lp = dm_q30(dsp_min(1.0f, 2.0f * DSP_PI * cutoff / EL_SAMPLE_RATE)), c_dc = dm_q30(0.99952f);
    const uint32_t pulse_len = (uint32_t)MS(1.0f), quiet_after = (uint32_t)MS(30.0f);
    const int32_t quiet = dm_q30(1e-4f);
    float att_hz = k[3] * tolerance[part] * (1.0f + punch);
    if (TR808_BD_BODY > 0 && 2.0f * f0 != db.r2_hz) { dm_svf_set(&db.r2, 2.0f * f0, 0.85f); db.r2_hz = 2.0f * f0; }
    (void)x_scale;
    for (int n = 0; n < frames; ++n) {
        if (!db.active) { db_y[n] = 0; continue; }
        uint32_t age = db.age;
        /* trigger_pulse(): the 1 ms pulse through the shaper, the diode */
        int32_t rect = age < pulse_len ? volts : 0;
        db.pulse_lp += DM28(rect - db.pulse_lp, c_plp);
        int32_t pulse = rect - db.pulse_lp;
        if (pulse < clamp) pulse = clamp;
        if (age == attack) db.aux = aux0;                        /* retrigger pulse */
        db.aux = DM30(db.aux, c_aux);                            /* 50 ms */
        if ((age & 7u) == 0) {
            /* as the float code: the attack's corner, then the sigh gliding to f0. Its
             * coefficients only when the corner moves by more than 2e-6 (the sigh has
             * settled after ~0.25 s; the float code works them out every time). */
            float hz, qq;
            if (age < attack) { hz = att_hz; qq = k[4]; }
            else {
                float sigh = sigh0 * ((float)db.pitch * (1.0f / DM_ONE30)) + TR808_BD_SINK * ((float)db.env * (1.0f / DM_ONE30));
                hz = f0 * (1.0f + sigh); qq = q;
            }
            float dhz = hz - db.r1_hz;
            if (qq != db.r1_q || dhz > 2e-6f * hz || dhz < -2e-6f * hz) {
                dm_svf_set(&db.r1, hz, qq);
                db.r1_hz = hz; db.r1_q = qq;
            }
        }
        db.env = DM30(db.env, c_env);
        db.pitch = DM30(db.pitch, c_pitch);
        int32_t y = db_bp_raw(&db.r1, DM28(pulse + db.aux, g_in));
        if (TR808_BD_BODY > 0) y += DM28(dm_bp(&db.r2, y < 0 ? -y : y), body);
        /* feedback buffer non-linearity: 2nd and 3rd harmonics */
        int32_t x = DM28(y, c_x), x2 = dm_mul(x, x, 24), x3 = dm_mul(x2, x, 24);
        y = DM28(x + DM28(x2, c_h2) + DM28(x3, c_h3), c_xinv);
        if (drive > 0) y = dm_mul(dr_tanh(DM28(y, c_drive)), c_dnorm, 24);   /* tanh(y d) / sqrt(d): Q24 -> Q20 -> Q24 */
        /* tone: the click through the 1-pole low-pass (out = out_lp) */
        int32_t click = DM28(pulse, c_click);
        db.out_lp += DM30(y + click - db.out_lp, c_lp);
        int32_t out = db.out_lp;
        int32_t o = out - db.dc_x + DM30(db.dc_y, c_dc);          /* DC block ~3.4 Hz */
        db.dc_x = out; db.dc_y = o;
        if (age > quiet_after && DM30(db.env, volts << 6) < quiet) {   /* env * volts (Q30) */
            db.env3 = DM30(db.env3, c_fade);
            if (db.env3 < dm_q30(1e-3f)) db.active = 0;
        }
        db_y[n] = (float)DM30(o, db.env3) * (1.0f / DB_ONE24);
        ++db.age;
    }
    if (!db.active) tv->active = 0;
    return 1;
}
#endif
