/* Erzeugt fx/gm_dist_tab.c: die Kurven der Distortion (FM1-Electros dist.c, Festkomma Q24) als Konstanten im Flash,
 * dieselbe Rechnung wie dist_init_tables auf dem Gerät. Aufruf: tools/gen_dist_tab.sh */
#define DIST_FIXED 1
#include "../fx/gm_dist.c"
#include <stdio.h>
int main(void)
{
    dist_init_tables();
    printf("/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception\n"
           " * Erzeugt von tools/gen_dist_tab.c aus fx/gm_dist.c (dist_init_tables). Nicht von Hand ändern. */\n"
           "#include <stdint.h>\nconst int32_t gm_dist_tab[4][%d] = {\n", DIST_TAB + 1);
    for (int m = 0; m < 4; m++) {
        printf("{");
        for (int i = 0; i <= DIST_TAB; i++) printf("%ld,%s", (long)dist_tabq[m][i], i % 12 == 11 ? "\n" : "");
        printf("},\n");
    }
    printf("};\n");
    return 0;
}
