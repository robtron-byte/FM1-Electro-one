# Vorlage für einen neuen Baustein

1. Ordner `bausteine/<name>/` anlegen, diese Dateien hineinkopieren, `name` überall durch den Namen ersetzen,
   `Makefile.vorlage` in `Makefile` umbenennen.
2. Abschnitt in `../ARBEITSTAFEL.md` anlegen und sich eintragen (pushen, bevor man weiterarbeitet).
3. `make -C bausteine <name>` muss bestehen. Das Makefile im Ordner `bausteine/` findet den neuen Baustein selbst.

Die Schnittstelle steht in `../baustein.h`.
