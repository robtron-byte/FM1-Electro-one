# 0007: BASS einstimmig und mono

- **Status:** angenommen
- **Stand:** 2026-10-26

## Kontext

Der BASS-Bereich hatte zwei Stimmen und einen Stereo-Ausgang (Chorus, Unison-Spreizung). Ein Bass gehört in die Mitte, und zwei Stimmen kosten Rechenzeit.

## Entscheidung

- Eine Stimme für BASS (`voices[EL_TRACKS] = {3, 4, 1}` in `el_init`): eine neue Note löst die alte ab.
- Der BASS-Ausgang wird vor allen Effekten auf die Mitte gelegt (`engine/electro.c`, nach `render_sources`).

## Folgen

- Weniger Rechenzeit; kaum Flash-Ersparnis.
- Chorus und Unison eines Bass-Sounds wirken nur noch als Klangfarbe, nicht als Breite.

## Verlauf

- 2026-10-26: angelegt
