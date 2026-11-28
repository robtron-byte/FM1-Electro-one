# 0014: Versionsnummern statt TEST-Builds

- **Status:** angenommen
- **Stand:** 2026-11-14

## Kontext

Die Testbuilds hießen `MKS50_TEST_01` … `_12` (JX-8P: `JX8P_MESS_01` … `_06`). Der Nutzer wünscht Versionsnummern,
derzeit im Alpha-Stadium, für alle Firmwares.

## Entscheidung

- **Eine Datei je Firmware:** `mks50/VERSION`, `jx8p/VERSION`, Form `X.Y.Z-suffix` (eine Ziffer je Stelle, wie die
  Plattform sie verlangt). Start: **0.1.0-alpha** für beide.
- **`tools/build.py` liest sie** und baut wie `--release`: Paket `build/mks50-0.1.0-alpha.fwsc` (zusätzlich
  `build/mks50.fwsc` für die Emulator-Skripte), Kennung `FM-1_95Y` bzw. `FM-1_97Y` mit Y = zweite Stelle, der ganze
  Text als `APP_VER` im Gerät. `--release` überschreibt die Datei.
- **Im Gerät sichtbar:** beim Einschalten kurz „MKS-50 V0.1.0-ALPHA“ (bzw. „JX-8P …“), dauerhaft oben auf der Seite SYSTEM.
- **Testbuild in `firmware/`:** `MKS50-<Version>.fwsc`, `JX8P-<Version>.fwsc`.
- **Zählen:** Jeder weitere Testbuild erhöht Z (0.1.1-alpha …). Eine neue Funktionsgruppe erhöht Y und setzt Z auf 0.
  Der Zusatz wird „beta“, wenn der Nutzer es sagt; ohne Zusatz ist es eine Freigabe.
- **Tags und GitHub-Releases** (Repo-ADR 0001; vom Nutzer ausdrücklich gewünscht, 14.11.2026): `.github/workflows/release.yml` legt beim Push für jede neue Version Tag `<ordner>-v<Version>` und ein Release (Vorabversion bei „-alpha“) mit der `.fwsc`-Datei und `firmware/README.md` an; ein vorhandener Tag bleibt.

## Folgen

- Die Kennung der MKS-50-Firmware wechselt von `FM-1_950` zu `FM-1_951` (JX-8P: `FM-1_960` → `FM-1_971`, weil `FM-1_96N` FM1-Electro gehört; 0.1.1-alpha). Speicher,
  eigene Sounds, Sets und der Stand vom Ausschalten hängen nicht daran und bleiben.
- FM1-Electro (eigenes Repository) ist seit 14.11.2026 ebenso versioniert: 0.5.2-alpha, Kennung `FM-1_965` (dort ADR 0035).

## Verlauf

- 2026-11-14: angelegt auf Wunsch des Nutzers; 0.1.0-alpha für MKS-50 und JX-8P
- 2026-11-14: JX-8P auf Kennung FM-1_97Y (FM-1_96Y gehört FM1-Electro), 0.1.1-alpha; FM1-Electro 0.5.2-alpha
- 2026-11-14: Tags und Releases per GitHub Actions (Wunsch des Nutzers)
