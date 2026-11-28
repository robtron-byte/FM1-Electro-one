/* SPDX-License-Identifier: GPL-3.0-only */
/* Grafik-Kopf (ADR 0055), Teil der Einheit gm_app.c (eingebunden von gm_ui.c): oben auf der Seite ein Bild, das zeigt, wo
 * man ist, und live auf den gerade gedrehten Regler antwortet. Bilder: LFO (Wellenform mit Rate, Tiefe und laufendem
 * Punkt), ENV (ADSR mit dem Punkt, wo die gespielte Note gerade steht), VCF (Filterkurve aus Cutoff und Reso), DELAY
 * (Echos als abnehmende Striche), REVERB (Abklingen), DIST (Kennlinie mit Modus). Alle anderen Seiten: der Name; wird
 * gedreht, für 1,5 s groß Regler und Wert.
 *
 * Die Variante setzt die Bilder als Stage ihrer Seiten (ST_VLFO … ST_VDIST) und liefert die Werte über
 * GM_VIZ(stage, part, float v[8]) (1 = gefüllt). Delay und Hall nimmt das Gerüst ohne Haken aus seinen Effekten (GM_FX).
 * Gezeichnet wird in die Streifen der Plattform (nur Linien und Rechtecke, kein eigener Puffer). */
#ifndef GM_VIZ
#define GM_VIZ(stage, p, v) 0
#endif
enum { VZ_X0 = 8, VZ_X1 = 232, VZ_Y0 = 52, VZ_Y1 = 146, VZ_HOT_MS = 1500 };
static uint8_t viz_hot = 0xffu, viz_hot_pg;
static uint32_t viz_hot_t, viz_now;            /* viz_now: Zeit des Bilds (alle Streifen gleich, draw_all) */
static void viz_touch(unsigned k) { viz_hot = (uint8_t)k; viz_hot_pg = page; viz_hot_t = fm1_ms; }
static int viz_is_hot(unsigned k) { return viz_hot == k && viz_hot_pg == page && viz_now - viz_hot_t < VZ_HOT_MS; }
static uint16_t viz_c(unsigned k) { return viz_is_hot(k) ? T_ACC : T_SEL; }   /* (ENV: die Felder sind A D S R) */
static uint16_t viz_any(void) { return viz_hot < 4u && viz_hot_pg == page && viz_now - viz_hot_t < VZ_HOT_MS ? T_ACC : T_SEL; }
static float viz_clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
static float viz_frac(float x) { return x - (float)(int32_t)x; }
static void viz_dot(int32_t x, int32_t y, uint16_t c) { R(x - 3, y - 3, 7, 7, c); }
static void viz_title(const page_t *pg)   /* oben links der Name der Seite; gerade gedreht: Regler und Wert */
{
    if (viz_hot < 4u && viz_hot_pg == page && viz_now - viz_hot_t < VZ_HOT_MS && pg->k[viz_hot].kind != K_NONE) {
        char b[28], *q = put(b, pg->k[viz_hot].label);
        *q++ = ' ';
        k_text(&pg->k[viz_hot], k_get(&pg->k[viz_hot]), q);
        Q(4, 32, 2, b, T_ACC);
    } else Q(4, 32, 2, pg->title, T_DIM);
    draw_sound_line(32);
}

/* LFO: v0 Rate 0…1, v1 Tiefe 0…1, v2 Welle (0 Dreieck, 1 Rechteck, 2 Säge auf, 3 Säge ab, 4 Zufall S&H, 5 Rauschen,
 * 6 Sinus), v3 Phase 0…1 (< 0: aus der Zeit nach der Rate) */
static float viz_wave(int w, float ph, uint32_t seg)
{
    switch (w) {
    case 1: return ph < 0.5f ? 1.0f : -1.0f;
    case 2: return 2.0f * ph - 1.0f;
    case 3: return 1.0f - 2.0f * ph;
    case 4: case 5: { uint32_t h = (seg + 1u) * 2654435761u; h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return (float)(h & 1023u) * (2.0f / 1023.0f) - 1.0f; }
    case 6: { float t = ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f; return t * (1.5f - 0.5f * t * t); }
    default: return ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f;
    }
}
static void viz_lfo(const float *v)
{
    float rate = viz_clamp01(v[0]), depth = 0.15f + 0.85f * viz_clamp01(v[1]), cyc = 1.0f + 4.0f * rate;
    float hz = 0.1f + 20.0f * rate * rate, mid = (float)(VZ_Y0 + VZ_Y1) * 0.5f, amp = (float)(VZ_Y1 - VZ_Y0) * 0.5f * depth;
    int w = (int)v[2], step = (w == 4 || w == 5) ? 1 : 2;
    uint16_t c = viz_any();
    int32_t px = VZ_X0, py = 0;
    R(VZ_X0, (int32_t)mid, VZ_X1 - VZ_X0, 1, T_LINE);
    for (int32_t x = VZ_X0; x <= VZ_X1; x += step) {
        float t = (float)(x - VZ_X0) * cyc / (float)(VZ_X1 - VZ_X0), ph = viz_frac(t);
        uint32_t seg = (uint32_t)(t * (w == 5 ? 24.0f : 1.0f));
        int32_t y = (int32_t)(mid - amp * viz_wave(w, ph, seg));
        if (x > VZ_X0) LN(px, py, x, y, c);
        px = x; py = y;
    }
    {   /* der laufende Punkt: einmal über das Bild in cyc Schwingungen */
        float ph = v[3] >= 0.0f ? v[3] : viz_frac((float)(viz_now % 100000u) * 0.001f * hz / cyc);
        float t = ph * cyc;
        int32_t x = VZ_X0 + (int32_t)(ph * (float)(VZ_X1 - VZ_X0));
        viz_dot(x, (int32_t)(mid - amp * viz_wave(w, viz_frac(t), (uint32_t)(t * (w == 5 ? 24.0f : 1.0f)))), T_LIVE);
    }
}
/* ENV: v0–v3 A D S R (0…1), v4 Abschnitt der klingenden Note (−1 keine, 0 A, 1 D/S, 3 R), v5 Pegel 0…1 */
static void viz_env(const float *v)
{
    float a = viz_clamp01(v[0]), d = viz_clamp01(v[1]), s = viz_clamp01(v[2]), r = viz_clamp01(v[3]);
    int32_t xa = VZ_X0 + 4 + (int32_t)(a * 52.0f), xd = xa + 4 + (int32_t)(d * 56.0f), xs = xd + 40, xr = xs + 4 + (int32_t)(r * 60.0f);
    int32_t yb = VZ_Y1, yt = VZ_Y0, ys = yb - (int32_t)((float)(yb - yt) * s);
    R(VZ_X0, yb + 1, VZ_X1 - VZ_X0, 1, T_LINE);
    LN(VZ_X0, yb, xa, yt, viz_c(0)); LN(xa, yt, xd, ys, viz_c(1)); LN(xd, ys, xs, ys, viz_c(2)); LN(xs, ys, xr, yb, viz_c(3));
    if (viz_is_hot(2)) R(xd, ys - 1, xs - xd, 3, T_ACC);
    if (v[4] >= 0.0f) {
        float lv = viz_clamp01(v[5]);
        int32_t st = (int32_t)v[4], x, y = yb - (int32_t)((float)(yb - yt) * lv);
        if (st == 0) x = VZ_X0 + (int32_t)((float)(xa - VZ_X0) * lv);
        else if (st == 3) x = xs + (int32_t)((float)(xr - xs) * viz_clamp01(1.0f - lv / (s > 0.05f ? s : 0.05f)));   /* (Nenner nie 0) */
        else if (lv > s + 0.01f && s < 0.99f) x = xa + (int32_t)((float)(xd - xa) * viz_clamp01((1.0f - lv) / (s < 0.95f ? 1.0f - s : 0.05f)));
        else x = xd + (int32_t)((float)(xs - xd) * viz_frac((float)(viz_now % 4000u) * (1.0f / 4000.0f)));   /* hält: wandert */
        viz_dot(x, y, T_LIVE);
    }
}
/* VCF: v0 Cutoff 0…1, v1 Reso 0…1, v2 Art (0 Tiefpass 24 dB, 1 Tiefpass 12 dB, 2 Hochpass), v3 Hüllkurven-Anteil 0…1 */
static void viz_vcf(const float *v)
{
    float fc = viz_clamp01(v[0]), q = viz_clamp01(v[1]), slope = v[2] == 1.0f ? 2.0f : 4.0f;
    int hp = v[2] == 2.0f;
    int32_t y0 = VZ_Y0 + 24, py = 0;
    uint16_t c = viz_any();
    R(VZ_X0, y0, VZ_X1 - VZ_X0, 1, T_LINE);
    for (int32_t x = VZ_X0; x <= VZ_X1; x += 2) {
        float f = (float)(x - VZ_X0) / (float)(VZ_X1 - VZ_X0), dd = hp ? fc - f : f - fc, g = dd > 0.0f ? -dd * slope * 60.0f : 0.0f;
        float bump = 1.0f / (1.0f + dd * dd * 900.0f);
        int32_t y = y0 - (int32_t)(q * 22.0f * bump) - (int32_t)g;
        y = y > VZ_Y1 ? VZ_Y1 : y < VZ_Y0 - 4 ? VZ_Y0 - 4 : y;
        if (x > VZ_X0) LN(x - 2, py, x, y, c);
        py = y;
    }
    {   /* Cutoff als Strich, der Hüllkurven-Anteil als Pfeil darüber */
        int32_t x = VZ_X0 + (int32_t)(fc * (float)(VZ_X1 - VZ_X0));
        R(x, VZ_Y1 - 8, 1, 8, c);
        if (v[3] > 0.01f) { int32_t xe = x + (int32_t)(viz_clamp01(v[3]) * 60.0f); R(x, VZ_Y1 - 4, xe - x, 1, T_LIVE); R(xe - 2, VZ_Y1 - 6, 2, 5, T_LIVE); }
    }
}
/* DELAY: v0 Zeit 0…1, v1 Rückkopplung 0…1, v2 Pegel 0…1, v3 Ping-Pong (0/1) */
static void viz_dly(const float *v)
{
    float gap = 10.0f + viz_clamp01(v[0]) * 34.0f, fb = viz_clamp01(v[1]) * 0.92f, h = 0.35f + 0.65f * viz_clamp01(v[2]);
    int32_t mid = (VZ_Y0 + VZ_Y1) / 2, x = VZ_X0 + 4;
    R(VZ_X0, mid, VZ_X1 - VZ_X0, 1, T_LINE);
    R(x, VZ_Y0, 4, VZ_Y1 - VZ_Y0, T_DIM);       /* der trockene Ton */
    for (unsigned i = 1; i < 12u; i++) {
        int32_t xi = x + (int32_t)(gap * (float)i), hh = (int32_t)(h * (float)(VZ_Y1 - VZ_Y0) * 0.5f);
        if (xi > VZ_X1 - 3 || hh < 2) break;
        if (v[3] > 0.5f) { if (i & 1u) R(xi, mid - hh, 3, hh, viz_any()); else R(xi, mid + 1, 3, hh, viz_any()); }
        else R(xi, mid - hh, 3, 2 * hh, viz_any());
        h *= fb;
        if (fb <= 0.0f) break;
    }
}
/* REVERB: v0 Größe 0…1, v1 Abklingen 0…1, v2 Dämpfung 0…1, v3 Pegel 0…1 */
static void viz_rev(const float *v)
{
    float len = 30.0f + viz_clamp01(v[1]) * 180.0f, pre = 2.0f + viz_clamp01(v[0]) * 16.0f, damp = viz_clamp01(v[2]);
    float h0 = (0.35f + 0.65f * viz_clamp01(v[3])) * (float)(VZ_Y1 - VZ_Y0);
    R(VZ_X0, VZ_Y1, VZ_X1 - VZ_X0, 1, T_LINE);
    R(VZ_X0, VZ_Y0, 3, VZ_Y1 - VZ_Y0, T_DIM);
    for (int32_t x = VZ_X0 + (int32_t)pre; x <= VZ_X1; x += 2) {
        float t = (float)(x - VZ_X0) - pre, e = 1.0f - t / len, k;
        uint32_t hsh = (uint32_t)x * 2654435761u;
        if (e <= 0.0f) break;
        k = e * e * (0.7f + 0.3f * (float)(hsh >> 28) / 15.0f);
        R(x, VZ_Y1 - (int32_t)(h0 * k), 1, (int32_t)(h0 * k), (x & 2) && damp > 0.5f ? T_DIM : viz_any());
    }
}
/* DIST: v0 Modus (0 TUBE, 1 OD, 2 DIST, 3 RAT), v1 Drive 0…1, v2 Tone 0…1, v3 Pegel 0…1 */
static float viz_shape(int m, float x)
{
    float a = x < 0.0f ? -x : x, y;
    switch (m) {
    case 1: y = x / (1.0f + a); break;                                       /* weich */
    case 2: y = x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x; break;            /* hart */
    case 3: y = a > 0.8f ? (x < 0.0f ? -1.0f : 1.0f) * (0.8f + 0.1f * (a - 0.8f) / (1.0f + a)) : x; break;
    default: { float t = x > 3.0f ? 3.0f : x < -3.0f ? -3.0f : x; y = t * (27.0f + t * t) / (27.0f + 9.0f * t * t); y += 0.12f * y * y; } break;   /* TUBE: schief */
    }
    return y > 1.0f ? 1.0f : y < -1.0f ? -1.0f : y;
}
static void viz_dist(const float *v)
{
    static const char *const nm[4] = {"TUBE", "OD", "DIST", "RAT"};
    int m = (int)v[0] & 3;
    float g = 1.0f + viz_clamp01(v[1]) * 9.0f, lv = 0.4f + 0.6f * viz_clamp01(v[3]);
    int32_t cx = 80, cy = (VZ_Y0 + VZ_Y1) / 2, half = (VZ_Y1 - VZ_Y0) / 2, py = 0;
    R(cx - half, cy, 2 * half, 1, T_LINE); R(cx, cy - half, 1, 2 * half, T_LINE);
    for (int32_t i = -half; i <= half; i += 2) {
        float x = (float)i / (float)half;
        int32_t y = cy - (int32_t)(viz_shape(m, x * g) * lv * (float)half);
        if (i > -half) LN(cx + i - 2, py, cx + i, y, viz_any());
        py = y;
    }
    PC(190, 70, 3, nm[m], T_INK);
    {   /* Tone als Balken: links dunkel, rechts hell */
        int32_t w = (int32_t)(viz_clamp01(v[2]) * 80.0f);
        R(150, 110, 80, 3, T_LINE); R(150, 110, w, 3, viz_any());
        Q(150, 118, 1, "TONE", T_DIM);
    }
}
/* das Bild der Seite (1) oder nichts (0: die Seite zeigt ihren Namen) */
static int viz_stage(const page_t *pg)
{
    float v[8] = {0, 0, 0, 0, -1.0f, 0, 0, 0};
    unsigned st = pg->stage;
    if (st < ST_VLFO || st > ST_VDIST) return 0;
    if (!GM_VIZ(st, sel, v)) {
        if (st == ST_VDLY) { v[0] = fxv[FX_DLY_TIME] / 127.0f; v[1] = fxv[FX_DLY_FB] / 127.0f; v[2] = fxv[FX_DLY_LEVEL] / 127.0f; v[3] = fxv[FX_DLY_PING] ? 1.0f : 0.0f; }
        else if (st == ST_VREV) { v[0] = fxv[FX_REV_SIZE] / 127.0f; v[1] = fxv[FX_REV_DECAY] / 127.0f; v[2] = fxv[FX_REV_DAMP] / 127.0f; v[3] = fxv[FX_REV_LEVEL] / 127.0f; }
        else return 0;
    }
    viz_title(pg);
    switch (st) {
    case ST_VLFO: viz_lfo(v); break;
    case ST_VENV: viz_env(v); break;
    case ST_VVCF: viz_vcf(v); break;
    case ST_VDLY: viz_dly(v); break;
    case ST_VREV: viz_rev(v); break;
    default: viz_dist(v); break;
    }
    return 1;
}
/* alle anderen Seiten: der Name; gerade gedreht: Regler und Wert groß */
static int viz_value(const page_t *pg)
{
    const kn_t *kn;
    char b[12];
    if (viz_hot > 3u || viz_hot_pg != page || viz_now - viz_hot_t >= VZ_HOT_MS || layer_shown()) return 0;
    kn = &pg->k[viz_hot];
    if (kn->kind == K_NONE) return 0;
    k_text(kn, k_get(kn), b);
    Q(4, 32, 2, pg->title, T_DIM);
    draw_sound_line(32);
    QC(120, 58, 2, kn->label, T_DIM);
    PC(120, 82, 5, b, k_colour(kn));
    return 1;
}
