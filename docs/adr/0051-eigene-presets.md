# 0051: Eigene Presets des Nutzers aus den iPhone-Apps in allen Bausteinen

- **Status:** vorgeschlagen
- **Stand:** 2026-11-28

## Kontext

Der Nutzer hat in seinen iOS-Apps (MKS-50, JX-8P, JP-6) und im TAL-Pha-Nachbau (MKS Ultra) eigene Sounds gebaut, die
ihm wichtiger sind als die Werks-Sounds (Wunsch 28.11.2026). Es kommen laufend neue dazu. Der Firmware-Build soll aber
nicht von der Dropbox oder dem iPhone abhängen.

## Entscheidung

- **Quellen** (Dropbox, „Programmieren/Musik/MKS50 App/“): `Eigene Presets iPhone/<app>/` mit den JSON der Apps
  (`mks50/`, `jx8p/`, `jp6/`, Format je App, siehe `LIES MICH.txt` dort); für mksultra `TAL Pha Nachbau/app/presets/Ron/`.
- **Holen:** `tools/iphone_presets_pull.sh [mks50|jx8p|jp6]` kopiert per `xcrun devicectl device copy from`
  (`appDataContainer`, `Library/Application Support/UserPresets`) vom iPhone in diesen Ordner. Es überschreibt, löscht
  nichts, und ruft danach das Werkzeug jedes Bausteins auf, der eins hat.
- **Werkzeug je Baustein:** `bausteine/<name>/tools/eigene2c.py` (mksultra: `tools/mks2c.py`) liest **alle** Dateien
  des Quellordners, bildet sie auf die Parameter des Kerns ab und schreibt eine C-Tabelle, die eingecheckt wird.
  Presets, die nur eine unveränderte Kopie von INIT sind, fallen weg. Reihenfolge nach Dateinummer.
- **Reihenfolge im Gerät:** eigene Presets zuerst (Preset 1 …), dann die Werks-Sounds.
- Erster Baustein mit iPhone-Presets: jx8p (7 eigene, ADR 0046); mksultra hat seine 6 Ron-Presets (ADR 0043).
  mks50 und jp6 folgen beim Herauslösen (ADR 0047, 0048) nach demselben Muster.

## Alternativen

- **JSON zur Laufzeit lesen:** das FM-1 hat kein Dateisystem für so etwas, und der Build wäre von der Dropbox abhängig.
- **Presets von Hand übertragen:** fehleranfällig, neue Presets kämen nicht nach.

## Folgen

- Nach neuen Presets auf dem iPhone: Skript laufen lassen, `make -C bausteine test`, Tabellen einchecken, Firmware neu bauen.
- Was ein Kern nicht kennt (z. B. Arp, Sequencer, Distortion der Apps), fällt beim Umsetzen weg; das Werkzeug nennt es
  im Kopf der erzeugten Datei bzw. im README des Bausteins.
- Ein auf dem iPhone gelöschtes Preset bleibt im Ordner, bis man es dort löscht.

## Verlauf

- 2026-11-28: angelegt (jx8p-Chat): Skript `tools/iphone_presets_pull.sh`, Werkzeug für jx8p. Skript am iPhone
  „Phonie“ geprüft: jx8p 7 Presets (gleich dem Dropbox-Stand), mks50 23; jp6 ist nicht mehr auf dem iPhone (Fehlermeldung,
  Ordner bleibt bei der Sicherung vom 05.11.).
