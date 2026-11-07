# 0029: Master-Kompressor CLUB

- **Status:** vorgeschlagen
- **Stand:** 2026-11-07

## Kontext

Wunsch des Nutzers: herausfinden, wie der Kompressor im EP-133 K.O. II arbeitet, und ihn auf den Master übertragen oder als Option anbieten; am besten einfacher und nicht rechenintensiv, aber gut im Club.

Die Anleitung von Teenage Engineering (teenage.engineering/guides/ep-133) war aus der Sitzung nicht erreichbar (Netz gesperrt); im Netz fanden sich nur Forenbeiträge ohne technische Angaben. Ein Nachbau des EP-133-Algorithmus ist deshalb nicht möglich. Stattdessen eine Club-Einstellung nach den üblichen Regeln für Bus-Kompression.

## Entscheidung

COMP RATIO in OPTIONS hat einen neunten Wert **CLUB** (`EL_COMP_CLUB`, `engine/electro.c`, `mb_master_club` in `engine/mixbus.c`):

- 4:1, Attack 10 ms (der Anschlag kommt durch), Schwelle und Make-up wie eingestellt; Attack und Release aus OPTIONS gelten dann nicht.
- **Release nach dem Programm:** Ein langsamer Mittelwert (600 ms) folgt dem Pegel; liegt die Hüllkurve mehr als 3 dB darüber (ein Schlag), lässt sie mit 80 ms los, sonst mit 700 ms (kein Pumpen auf dem Teppich).
- **Parallel:** 30 % des unkomprimierten Signals bleiben (`mb_comp_out`): Punch bei dichter Mischung.
- Kosten: der Detektor wie bisher alle 4 Samples (ein log2, ein exp2), dazu eine Mittelwert-Zeile und eine Multiplikation pro Sample.
- Kein neues Speicherformat: CLUB ist ein Wert der vorhandenen Option.

`make comp-ab`: der Standard-Groove mit dem Werks-Kompressor (A, 1,8:1) und mit CLUB (B). Im Groove: gleicher Pegel (RMS −8,6 dBFS, Spitze −1 dBFS), stärkste Reduktion 0,9 dB (A) gegen 2,7 dB (B).

## Alternativen

- **Den EP-133-Kompressor nachbauen:** erst mit der Anleitung oder Aufnahmen; dafür müsste teenage.engineering im Netz der Sitzung freigegeben werden.
- **Ein eigener, noch billigerer Kompressor im linearen Bereich (ohne log/exp):** spart wenig, der Detektor läuft schon nur alle 4 Samples.

## Folgen

- Die Bassdrum geht standardmäßig am Master-Kompressor vorbei (`MB_KICK_THROUGH_MASTER`), auch bei CLUB.
- Ein gespeicherter CLUB-Wert lädt in älteren Firmwares als ungültig (auf ihren Höchstwert begrenzt).

## Verlauf

- 2026-11-07: angelegt auf Wunsch des Nutzers
