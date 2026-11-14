# 0035: Versionsnummern statt Build-Zählung

- **Status:** angenommen
- **Stand:** 2026-11-14

## Kontext

Die Testbuilds wurden gezählt („52. Build“), die Plattform-Datei hieß immer `FM1_ELECTRO_PLATTFORM.fwsc`. Der Nutzer
wünscht für alle seine Firmwares Versionsnummern im Alpha-Stadium, wie in fm1-synths (MKS-50, JX-8P: dort
`mks50/docs/adr/0014`), und hat für FM1-Electro **0.5.2-alpha** gewählt (14.11.2026), passend zum 52. Build.

## Entscheidung

- **Datei `VERSION`** im Wurzelordner, Form `X.Y.Z-suffix` (eine Ziffer je Stelle, wie die Plattform sie verlangt).
- **`tools/build_platform.py` liest sie** und baut wie `--release`: Paket `platform/build/fm1-electro-0.5.2-alpha.fwsc`,
  Kennung `FM-1_96Y` (Y = zweite Stelle, hier `FM-1_965`), `APP_VER` und `EL_VERSION` mit dem ganzen Text.
- **Im Gerät sichtbar:** unten rechts auf der Seite OPTIONS (HOME halten + SAVE). Rechner-Builds zeigen „DEV“.
- **Testbuild in `firmware/`:** `FM1_ELECTRO-<Version>.fwsc` (Plattform). Der ältere SDK-Build `FM1_ELECTRO.fwsc`
  bleibt, wie er ist.
- **Zählen:** Jeder weitere Testbuild erhöht Z (0.5.3-alpha …); eine neue Funktionsgruppe erhöht Y und setzt Z auf 0.
  Nach 0.5.9 folgt 0.6.0. „beta“, wenn der Nutzer es sagt.
- **Tags und GitHub-Releases** (vom Nutzer gewünscht, 14.11.2026): `.github/workflows/release.yml` legt beim Push für jede neue Version Tag `v<Version>` und ein Release (Vorabversion bei „-alpha“) mit der Datei an.

## Folgen

- Die Kennung wechselt von `FM-1_960` zu `FM-1_965`; Speicher und Grooves hängen nicht daran.
- Der Inhalt von 0.5.2-alpha ist der 52. Build, nur mit der Versionsanzeige.

## Verlauf

- 2026-11-14: angelegt; 0.5.2-alpha
- 2026-11-14: Tags und Releases per GitHub Actions (Wunsch des Nutzers)
