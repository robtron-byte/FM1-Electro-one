"""Builds docs/FM1-Electro-Anleitung.pdf: a quick start page with the panel,
then the controls by topic. Usage: python3 tools/make_manual.py"""
import os
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, Flowable,
                                KeepTogether)

FONTS = '/usr/share/fonts/truetype/dejavu/'
pdfmetrics.registerFont(TTFont('Sans', FONTS + 'DejaVuSans.ttf'))
pdfmetrics.registerFont(TTFont('Sans-Bold', FONTS + 'DejaVuSans-Bold.ttf'))
pdfmetrics.registerFont(TTFont('Mono', FONTS + 'DejaVuSansMono.ttf'))
pdfmetrics.registerFontFamily('Sans', normal='Sans', bold='Sans-Bold', italic='Sans', boldItalic='Sans-Bold')

INK = colors.HexColor('#1d1d1f')
DIM = colors.HexColor('#6e6e73')
LINE = colors.HexColor('#d2d2d7')
SOFT = colors.HexColor('#f5f5f7')
AREA = {'BEAT': colors.HexColor('#f86820'), 'RHYTHM': colors.HexColor('#e0b800'),
        'STRINGS': colors.HexColor('#9a78f0'), 'BASS': colors.HexColor('#20b8ae')}

H1 = ParagraphStyle('h1', fontName='Sans-Bold', fontSize=20, leading=24, textColor=INK, spaceAfter=2)
SUB = ParagraphStyle('sub', fontName='Sans', fontSize=10, leading=13, textColor=DIM, spaceAfter=8)
H2 = ParagraphStyle('h2', fontName='Sans-Bold', fontSize=13, leading=16, textColor=INK, spaceBefore=10, spaceAfter=5)
BODY = ParagraphStyle('body', fontName='Sans', fontSize=9, leading=12.2, textColor=INK)
CELL = ParagraphStyle('cell', parent=BODY, fontSize=8.4, leading=11)
KEY = ParagraphStyle('key', parent=CELL, fontName='Sans-Bold')
STEP = ParagraphStyle('step', parent=BODY, fontSize=9.4, leading=12.6, leftIndent=14, firstLineIndent=-14, spaceAfter=3)


class Panel(Flowable):
    """The FM-1 front panel with what each control does in FM1-Electro."""

    def __init__(self, width):
        super().__init__()
        self.width, self.height = width, 100 * mm

    def control(self, c, x, y, w, h, name, note, fill=None, round_=False, text=INK):
        c.setStrokeColor(INK)
        c.setLineWidth(0.6)
        c.setFillColor(fill or colors.white)
        if round_:
            c.circle(x + w / 2, y + h / 2, min(w, h) / 2, stroke=1, fill=1)
        else:
            c.roundRect(x, y, w, h, 2, stroke=1, fill=1)
        c.setFillColor(text)
        size = 6.3 if c.stringWidth(name, 'Sans-Bold', 6.3) < w - 3 else 5.2
        c.setFont('Sans-Bold', size)
        c.drawCentredString(x + w / 2, y + h / 2 - size / 3, name)
        if note:
            c.setFillColor(DIM)
            c.setFont('Sans', 5.8)
            for i, line in enumerate(note.split('\n')):
                c.drawCentredString(x + w / 2, y - 7 - i * 6.6, line)

    def draw(self):
        c = self.canv
        W, H = self.width, self.height
        c.setFillColor(SOFT)
        c.setStrokeColor(LINE)
        c.roundRect(0, 0, W, H, 6, stroke=1, fill=1)
        top = H - 14 * mm
        # Left column: encoders and octave.
        enc = 12.5 * mm
        x0 = 6 * mm
        self.control(c, x0, top, enc, enc, 'MASTER', 'Lautstärke\n(Gerät)', round_=True)
        self.control(c, x0 + 17 * mm, top, enc, enc, 'SELECT', 'Platz A-D', round_=True)
        self.control(c, x0, top - 22 * mm, enc, enc, 'PRESETS', 'Sound / Kit', round_=True)
        self.control(c, x0 + 17 * mm, top - 22 * mm, enc, enc, 'ALGO', 'Tempo', round_=True)
        bw, bh = 12 * mm, 6 * mm
        self.control(c, x0 - 0.5 * mm, top - 40 * mm, bw, bh, 'OCT -', 'Oktave /\nSteps 1-16')
        self.control(c, x0 + 16.5 * mm, top - 40 * mm, bw, bh, 'OCT +', 'Oktave /\nSteps 17-32')
        # Display.
        dx, dw, dh = 40 * mm, 38 * mm, 38 * mm
        c.setFillColor(colors.HexColor('#111111'))
        c.roundRect(dx, top - dh + enc, dw, dh, 3, stroke=0, fill=1)
        c.setFont('Mono', 4.6)
        tabs = ['BEAT', 'RHYT', 'STRG', 'BASS']
        for i, (t, col) in enumerate(zip(tabs, AREA.values())):
            c.setFillColor(col)
            c.rect(dx + 2 + i * (dw - 4) / 4, top + enc - 7, (dw - 4) / 4 - 1, 1.2, stroke=0, fill=1)
            c.drawString(dx + 3 + i * (dw - 4) / 4, top + enc - 5, t + ' A')
        c.setFillColor(colors.white)
        c.setFont('Sans', 6.5)
        c.drawCentredString(dx + dw / 2, top - dh / 2 + enc - 2, 'Display')
        c.setFillColor(DIM)
        c.setFont('Sans', 5.8)
        c.drawCentredString(dx + dw / 2, top - dh + enc - 7, 'Reiter = Bereich + Platz')
        # Right: knobs, then two rows of buttons.
        rx = 84 * mm
        knob = 12 * mm
        for i in range(4):
            self.control(c, rx + 5 * mm + i * 21 * mm, top, knob, knob, 'KNOB %d' % (i + 1), '', round_=True)
        c.setFillColor(DIM)
        c.setFont('Sans', 5.8)
        c.drawCentredString(rx + 5 * mm + 1.5 * 21 * mm + knob / 2, top - 7, 'Klang  ·  in HOME: Pegel BEAT / RHYTHM / STRINGS / BASS')
        row1 = [('FX', 'BEAT', AREA['BEAT']), ('SEL', 'RHYTHM', AREA['RHYTHM']), ('ENV', 'STRINGS', AREA['STRINGS']),
                ('LFO', 'BASS', AREA['BASS']), ('EDIT', 'Umschalter\n(+Bereich:\nEditor)', None), ('GLO', 'Umschalter\n(+Bereich:\nstumm)', None)]
        row2 = [('HOME', 'Übersicht\n(halten+SAVE:\nOPTIONS)', None), ('SAVE', 'alles\nspeichern', None),
                ('ARP', 'tippen: Arp\nhalten:\nGenerator', None), ('SEQ', 'Tasten /\nStep-Modus', None),
                ('PLAY', 'Start /\nStop', None), ('REC', 'Aufnahme', None)]
        bw = 12.5 * mm
        for r, row in enumerate((row1, row2)):
            y = top - 21 * mm - r * 22 * mm
            for i, (name, note, col) in enumerate(row):
                self.control(c, rx + i * 15.5 * mm, y, bw, 6 * mm, name, note, fill=col,
                             text=colors.white if col is not None else INK)
        # Keyboard: 11 black pills (3-2-3-2-1) above 16 white pills.
        ky = 8 * mm
        kx, kw = 6 * mm, W - 12 * mm
        pw = kw / 16
        for i in range(16):
            c.setFillColor(colors.white)
            c.setStrokeColor(INK)
            c.roundRect(kx + i * pw + 1, ky, pw - 2, 6 * mm, 3, stroke=1, fill=1)
            c.setFillColor(DIM)
            c.setFont('Sans', 5.5)
            c.drawCentredString(kx + i * pw + pw / 2, ky + 2.1 * mm, str(i + 1))
        # Black pills sit between white ones as on the FM-1: 3-2-3-2-1.
        for i in (0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14):
            c.setFillColor(INK)
            c.roundRect(kx + (i + 1) * pw - pw * 0.4, ky + 8 * mm, pw * 0.8, 5 * mm, 2.5, stroke=0, fill=1)
        c.setFillColor(DIM)
        c.setFont('Sans', 5.8)
        c.drawString(kx, ky + 14.5 * mm, 'schwarze Tasten (11): Instrumente in BEAT, Noten in den Synth-Bereichen, in HOME 1-4: Bereich stumm')
        c.drawString(kx, ky - 4.5 * mm, 'weiße Tasten (16): Steps, Noten; in HOME 1-4 BEAT A-D, 5-8 RHYTHM, 9-12 STRINGS, 13-16 BASS')


def table(rows, widths, head=None):
    data = []
    if head:
        data.append([Paragraph(h, KEY) for h in head])
    for r in rows:
        data.append([Paragraph(r[0], KEY)] + [Paragraph(x, CELL) for x in r[1:]])
    t = Table(data, colWidths=widths, repeatRows=1 if head else 0)
    style = [('VALIGN', (0, 0), (-1, -1), 'TOP'), ('LINEBELOW', (0, 0), (-1, -1), 0.4, LINE),
             ('TOPPADDING', (0, 0), (-1, -1), 3), ('BOTTOMPADDING', (0, 0), (-1, -1), 3),
             ('LEFTPADDING', (0, 0), (-1, -1), 4), ('RIGHTPADDING', (0, 0), (-1, -1), 4)]
    if head:
        style += [('BACKGROUND', (0, 0), (-1, 0), SOFT)]
    t.setStyle(TableStyle(style))
    return t


def footer(c, doc):
    c.saveState()
    c.setFont('Sans', 7)
    c.setFillColor(DIM)
    c.drawString(18 * mm, 10 * mm, 'FM1-Electro  ·  Bedienungsanleitung')
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, 'Seite %d' % doc.page)
    c.restoreState()


def build(path):
    doc = SimpleDocTemplate(path, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=15 * mm,
                            bottomMargin=17 * mm, title='FM1-Electro Anleitung', author='FM1-Electro')
    W = A4[0] - 36 * mm
    s = []
    # ---- page 1: quick start --------------------------------------------------
    s.append(Paragraph('FM1-Electro', H1))
    s.append(Paragraph('Kurzübersicht: Electro-Groovebox mit 808-Drums und drei Synth-Bereichen (MKS-50 / JP-6) auf dem M-VAVE FM-1', SUB))
    s.append(Panel(W))
    s.append(Spacer(1, 6))
    s.append(Paragraph('In fünf Schritten loslegen', H2))
    steps = [
        '<b>1  Abspielen.</b> PLAY drückt Start/Stop. Beim ersten Einschalten liegt auf Platz A ein Groove, danach kommt immer dein gespeicherter Stand.',
        '<b>2  Bereich wählen.</b> FX = BEAT, SEL = RHYTHM, ENV = STRINGS, LFO = BASS. Die Tasten spielen den Bereich, KNOB 1-4 formen den Klang, nochmal drücken blättert die Klangseiten durch, PRESETS wählt Sound bzw. Kit.',
        '<b>3  Pattern bauen.</b> In BEAT: schwarze Taste wählt das Instrument, weiße Tasten setzen Steps. In den Synth-Bereichen schaltet SEQ auf Step-Eingabe; oder REC und live einspielen.',
        '<b>4  Variieren.</b> ARP halten + weiße Taste 1 = Variation, Taste 2 = neues Pattern (KNOB 1 Stil, KNOB 2 Stärke). Zweimal drücken bestätigt. In HOME gilt es für alle Bereiche.',
        '<b>5  Mischen und speichern.</b> HOME: KNOB 1-4 sind die Pegel der vier Bereiche. SAVE speichert alles. HOME halten + SAVE öffnet OPTIONS (MIDI, Kompressor, Ausgang).',
    ]
    for t in steps:
        s.append(Paragraph(t, STEP))
    s.append(Spacer(1, 4))
    s.append(Paragraph('<b>Merksatz:</b> links die Struktur (Platz, Tempo, Sound), rechts der Klang (KNOB 1-4). '
                       '<b>EDIT</b> und <b>GLO</b> sind die beiden Umschalter für Zweitfunktionen.', BODY))
    s.append(Paragraph('Die wichtigsten Griffe', H2))
    grips = [['Bereich stumm', 'GLO + Bereichstaste'], ['Synth-Editor', 'EDIT + Bereichstaste'],
             ['Arpeggiator an/aus', 'ARP antippen'], ['Pattern löschen', 'EDIT + REC'],
             ['Pattern-Länge', 'GLO + ALGORITHM'], ['Swing / Notenlänge', 'EDIT + ALGORITHM'],
             ['Andere Engine (MKS/JP)', 'EDIT + PRESETS'], ['Verzerrung', 'HOME, dann EDIT halten']]
    half = (len(grips) + 1) // 2
    rows = [[Paragraph(a[0], CELL), Paragraph(a[1], KEY), Paragraph(b[0], CELL), Paragraph(b[1], KEY)]
            for a, b in zip(grips[:half], grips[half:])]
    g = Table(rows, colWidths=[W * 0.22, W * 0.28, W * 0.22, W * 0.28])
    g.setStyle(TableStyle([('LINEBELOW', (0, 0), (-1, -1), 0.4, LINE), ('TOPPADDING', (0, 0), (-1, -1), 3),
                           ('BOTTOMPADDING', (0, 0), (-1, -1), 3), ('LEFTPADDING', (0, 0), (-1, -1), 4)]))
    s.append(g)
    s.append(PageBreak())

    # ---- page 2: controls everywhere ------------------------------------------
    s.append(Paragraph('Bedienung überall', H2))
    s.append(table([
        ['FX / SEL / ENV / LFO', 'Bereich BEAT / RHYTHM / STRINGS / BASS. Nochmal drücken: nächste Klangseite.'],
        ['GLO + Bereichstaste', 'Bereich stumm / wieder an.'],
        ['SELECT', 'Platz A-D des Bereichs (wechselt am nächsten Takt). In HOME: alle Bereiche zugleich.'],
        ['ALGORITHM', 'Tempo. GLO + ALGORITHM: Länge des Patterns. EDIT + ALGORITHM: Swing (BEAT), Notenlänge (RHYTHM, BASS), Schrittlänge 1/16 bis 2 Takte (STRINGS).'],
        ['PRESETS', 'Sound des Bereichs (EDIT + PRESETS: andere Engine, MKS-50 / JP-6); in BEAT das Kit (808, ELECTRO, MIAMI, KRAFT, PO-12).'],
        ['KNOB 1-4', 'Klang des Bereichs bzw. Instruments.'],
        ['PLAY', 'Start / Stop.'],
        ['REC', 'Aufnahme in den aktuellen Bereich. EDIT + REC löscht dessen Pattern.'],
        ['SAVE', 'Alles speichern: alle Plätze, Klänge, Einstellungen. Generierte Patterns speichern sich nach 2 s selbst.'],
        ['ARP antippen', 'Arpeggiator an/aus (RHYTHM, STRINGS, BASS). EDIT + ARP: HOLD. Einstellungen auf der letzten Klangseite (Modus, Rate, Oktaven, Gate).'],
        ['ARP halten', 'Generator: KNOB 1 Stil, KNOB 2 AMOUNT, weiße Taste 1 Variation, weiße Taste 2 neues Pattern. Erst fragt das Display, der zweite Druck führt aus; jede andere Taste bricht ab.'],
    ], [38 * mm, W - 38 * mm]))

    s.append(Paragraph('Vier Plätze pro Bereich', H2))
    s.append(Paragraph('Jeder Bereich hat die Plätze <b>A-D</b> und wechselt unabhängig, immer am nächsten Takt. '
                       'Die Reiter oben im Display zeigen den Platz jedes Bereichs (orange: wechselt gleich). '
                       'So baust du Teile eines Tracks: z. B. Beat A durchlaufen lassen und den Bass zwischen A und B wechseln.', BODY))

    s.append(Paragraph('HOME: Übersicht und Mischpult', H2))
    s.append(table([
        ['HOME', 'Übersicht ein/aus: alle vier Spuren, Pegel, Mute.'],
        ['Weiße Tasten', '1-4 BEAT A-D, 5-8 RHYTHM A-D, 9-12 STRINGS A-D, 13-16 BASS A-D. Die LEDs zeigen die Plätze, der nächste blinkt.'],
        ['Schwarze Tasten 1-4', 'Bereich stumm / an.'],
        ['KNOB 1-4', 'Pegel von BEAT, RHYTHM, STRINGS, BASS.'],
        ['GLO + KNOB 1-4', 'Delay-Zeit, Feedback, Hallgröße, Hallzeit.'],
        ['PRESETS / GLO + PRESETS', 'Hall-Anteil / Delay-Anteil.'],
        ['GLO + ALGORITHM', 'Tempo fein.'],
        ['EDIT halten', 'Verzerrung: KNOB 1-4 Send pro Bereich, PRESETS Typ (OD, DIST, RAT, TUBE), ALGORITHM Drive, SELECT Ton.'],
        ['ARP halten + weiß 1 / 2', 'Variation bzw. neuer Groove für alle Bereiche (zweimal drücken).'],
    ], [38 * mm, W - 38 * mm]))

    s.append(Paragraph('MIDI', H2))
    s.append(Paragraph('Jeder Bereich hört auf seinem Kanal (einstellbar in OPTIONS), mit Anschlagstärke. Drums: General-MIDI-Noten, '
                       'ZAP Note 67, PULSE Note 75. MIDI-Clock, Start, Stop und Continue werden angenommen (OPTIONS: MIDI CLOCK).', BODY))
    s.append(PageBreak())
    s.append(Paragraph('OPTIONS (HOME halten + SAVE)', H2))
    s.append(Paragraph('Für Einstellungen, die man einmal macht. SELECT oder ALGORITHM wählt die Zeile, PRESETS oder ein KNOB ändert den Wert, HOME oder SAVE schließt und speichert.', BODY))
    s.append(Spacer(1, 3))
    s.append(table([
        ['MIDI CH ...', 'Kanal je Bereich (1-16 oder OFF). Ab Werk BEAT 10, RHYTHM 1, STRINGS 2, BASS 3. Zwei Bereiche auf einem Kanal spielen beide.'],
        ['MIDI CLOCK', 'FOLLOW: externer Clock und Start/Stop; IGNORE.'],
        ['MASTER FX', 'Mix- und Master-Bearbeitung an/aus.'],
        ['COMP ...', 'Master-Kompressor: THRESHOLD 0 bis -30 dB, RATIO 1:1 (aus) bis 10:1, ATTACK 1-120 ms, RELEASE 50-1200 ms, MAKEUP 0 bis +12 dB. Oben zeigt COMP GR die Absenkung.'],
        ['OUTPUT LEVEL', 'Gesamtlautstärke.'],
        ['POWER-ON', 'LAST: jeder Bereich auf seinem letzten Platz; SLOTS A: alle auf A.'],
        ['HELP LINE', 'Hilfezeile unten im Display an/aus (Meldungen bleiben).'],
    ], [38 * mm, W - 38 * mm]))

    # ---- page 3: options and areas ---------------------------------------------
    s.append(Paragraph('BEAT (808)', H2))
    s.append(table([
        ['Schwarze Taste', 'Instrument wählen und spielen: BD SD LT HT RS CP CB CY OH CH ZAP. SEQ: zweite Ebene mit PULSE auf Taste 1.'],
        ['Weiße Tasten', 'Steps setzen. EDIT + weiß: Akzent. GLO + weiß: 32tel-Roll.'],
        ['EDIT / GLO + schwarz', 'Instrument löschen / stumm.'],
        ['KNOB 1-4', 'Klang des Instruments; Bereichstaste nochmal: nächste Seite.'],
        ['GLO + SELECT', 'Instrument wählen, ohne es zu spielen (auch PULSE).'],
        ['OCT - / +', 'Steps 1-16 / 17-32.'],
    ], [38 * mm, W - 38 * mm]))

    s.append(Paragraph('RHYTHM, STRINGS, BASS', H2))
    s.append(Paragraph('Zwei Modi, <b>SEQ</b> schaltet um.', BODY))
    s.append(Spacer(1, 3))
    s.append(table([
        ['Tasten', 'Alle 27 Tasten spielen den Sound; die LEDs zeigen, was die Sequenz spielt.', 'Weiße Tasten schalten Steps an/aus und wählen sie. EDIT + weiß Akzent, GLO + weiß Tie.'],
        ['OCT - / +', 'Oktave.', 'Steps 1-16 / 17-32. GLO + SELECT wählt den Step.'],
        ['KNOB 1-4', 'Klangseiten (Filter, Hüllkurve, Oszillator, Modulation, FX), zuletzt ARP.', 'Note, Oktave, Akkord, Umkehrung (STRINGS) bzw. Akzent, Tie.'],
        ['REC', 'Läuft: live einspielen (quantisiert). Steht: Noten Schritt für Schritt, EDIT + Taste = Pause.', '-'],
        ['STRINGS', 'GLO + weiße Taste 1-9: Akkord fürs Live-Spiel (1 = Einzelnoten; m7, SUS2, MIN, 5+8, SUS4, 7TH, MAJ, m7b5).', 'KNOB 3 Akkordtyp, KNOB 4 Umkehrung.'],
    ], [26 * mm, (W - 26 * mm) / 2, (W - 26 * mm) / 2], head=['', 'KEYS (SEQ aus)', 'STEP (SEQ an)']))

    s.append(Paragraph('Synth-Editor (EDIT + Bereichstaste)', H2))
    s.append(table([
        ['SELECT', 'Sektion wie am Original-Panel (MKS-50: DCO, VCF, VCA, ENV, LFO, CHORUS, KEY, FX; JP-6: VCO-1, VCO-2, MOD, VCF, VCA, ENV-1, ENV-2, LFO, KEY, FX).'],
        ['ALGORITHM / KNOB 1-4', 'Seite der Sektion / die vier Regler der Seite.'],
        ['PRESETS', 'Sound; EDIT + PRESETS: andere Engine.'],
        ['SAVE', 'Eigenen Sound speichern (U01-U32): PRESETS/SELECT Platz, KNOB 1 Cursor, KNOB 2 Buchstabe, SAVE nochmal speichert, andere Taste bricht ab.'],
        ['Zurück', 'Bereichstaste nochmal, oder HOME / SEQ.'],
    ], [38 * mm, W - 38 * mm]))

    doc.build(s, onFirstPage=footer, onLaterPages=footer)


if __name__ == '__main__':
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs', 'FM1-Electro-Anleitung.pdf')
    build(os.path.normpath(out))
    print(os.path.normpath(out))
