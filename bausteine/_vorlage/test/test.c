/* Host-Test: rendert WAVs nach argv[2] und vergleicht mit den Referenzen in argv[1]. Exit 0 = bestanden. */
#include "name.h"
#include <stdio.h>
int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    static name_t s;
    name_baustein.init(&s, 48000.0);
    printf("name: Vorlage, noch nichts geprüft\n");
    return 0;
}
