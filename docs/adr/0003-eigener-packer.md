# 0003: Eigener Packer mit 384-KB-Benutzerbereich

- **Status:** angenommen
- **Stand:** 2026-10-26

## Kontext

Neben FM1 Opens 524-KB-Benutzerbereich (Sample-Bank, Projekte) sind höchstens 444 KB für die Firmware frei. FM1-Electro ist 519 KB groß, braucht von dem Bereich aber nur 32 KB (Grooves, eigene Sounds). Das SDK prüft beim Start nur, dass die Größe im Flash-Verzeichnis mit `fm1_layout.user_bytes` in der Firmware übereinstimmt, nicht eine feste Zahl.

## Entscheidung

- `tools/package_fm1.py`: Python-Übersetzung von FM1 Opens `Package.cs`, nur Standardbibliothek. Gibt dem Benutzerbereich **384 KB** (`--user-kb`) und schreibt dieselbe Größe in `fm1_layout` der Firmware.
- Ablauf: `prepare_fm1.py` → `Build.ps1` **ohne** `-FirmwareOnly` (baut und prüft) → `package_fm1.py`.
- `fm1/el_store.c`: In einem Bereich kleiner als FM1 Opens liegen Grooves und Sounds in dessen **letzten acht Sektoren**. Der Bereich endet immer an derselben Flash-Adresse, die Daten bleiben bei einem anderen `--user-kb` erhalten.
- Ein Testbuild liegt in `firmware/`.

## Alternativen

- Funktionen streichen: Selbst die ganze Master-Kette bringt nur ~20 KB.
- Nur den Code verkleinern: siehe 0005, reicht allein nicht (513 KB).

## Folgen

- 78 KB Luft für künftige Funktionen.
- Beim Wechsel von FM1 Open oder der Original-Firmware gehen deren Projekte und Samples verloren; Bluetooth-Kopplungen können zurückgesetzt werden. Steht im README.
- `--user-kb` von Update zu Update gleich lassen.
- Für FM1 Opens eigene Aufteilung erzeugt der Packer dieselbe Datei wie die frühere Übersetzung; gegen das Windows-Original nicht bytegleich verglichen.

## Verlauf

- 2026-10-26: angelegt; im Emulator fm1-emu gestartet (Partition 393216 Bytes erkannt)
