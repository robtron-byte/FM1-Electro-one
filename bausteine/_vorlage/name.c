/* SPDX-License-Identifier: GPL-3.0-only */
#include "name.h"

static const char *pname(unsigned p) { (void)p; return ""; }
static void init(void *s, double rate) { ((name_t *)s)->rate = rate; }
static void note_on(void *s, int note, int vel) { (void)s; (void)note; (void)vel; }
static void note_off(void *s, int note) { (void)s; (void)note; }
static void set_param(void *s, unsigned p, double v) { (void)s; (void)p; (void)v; }
static void render(void *s, float *l, float *r, int n) { (void)s; (void)l; (void)r; (void)n; }
static int active(const void *s) { (void)s; return 0; }

const baustein_t name_baustein = {"name", "<Titel>", sizeof(name_t), 0, pname, init, note_on, note_off, set_param, render, active};
