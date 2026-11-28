/* SPDX-License-Identifier: GPL-3.0-only
 * <name>: <Klangerzeuger>. Vorlage: <Dropbox-Pfad / Repository, Stand>. Siehe README.md. */
#ifndef NAME_H
#define NAME_H
#include "baustein.h"

typedef struct {
    double rate;
    /* Zustand des Klangerzeugers */
} name_t;

extern const baustein_t name_baustein;
#endif
