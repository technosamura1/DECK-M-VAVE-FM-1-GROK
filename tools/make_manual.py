#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""DECK · OVERWORLD — the user's manual: a PDF in Russian, dark like the device's screen, with the simulator's
screenshots (out/screens: run tests/run_tests.sh first).   python tools/make_manual.py [out/DECK_инструкция.pdf]"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.pdfmetrics import registerFontFamily
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (Flowable, Image as RLImage, KeepTogether, PageBreak, Paragraph, SimpleDocTemplate,
                                Spacer, Table, TableStyle)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = os.path.join(ROOT, "out", "screens")
TMP = os.path.join(ROOT, "build", "manual")
os.makedirs(TMP, exist_ok=True)
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "out", "DECK_инструкция.pdf")

FONTS = r"C:\Windows\Fonts"
if os.path.exists(os.path.join(FONTS, "segoeui.ttf")):      # Windows: Segoe UI + Consolas
    FF = {"Light": "segoeuil.ttf", "Body": "segoeui.ttf", "Bold": "segoeuib.ttf", "Mono": "consola.ttf", "MonoB": "consolab.ttf",
          "Semi": "seguisb.ttf" if os.path.exists(os.path.join(FONTS, "seguisb.ttf")) else "segoeuib.ttf"}
else:                                                        # Linux (the cloud): DejaVu
    FONTS = "/usr/share/fonts/truetype/dejavu"
    FF = {"Light": "DejaVuSans-ExtraLight.ttf", "Body": "DejaVuSans.ttf", "Semi": "DejaVuSans-Bold.ttf",
          "Bold": "DejaVuSans-Bold.ttf", "Mono": "DejaVuSansMono.ttf", "MonoB": "DejaVuSansMono-Bold.ttf"}
    if not os.path.exists(os.path.join(FONTS, FF["Light"])):   # no ExtraLight in this image: the plain face
        FF["Light"] = "DejaVuSans.ttf"
for _n, _f in FF.items():
    pdfmetrics.registerFont(TTFont(_n, os.path.join(FONTS, _f)))
registerFontFamily("Body", normal="Body", bold="Semi", italic="Body", boldItalic="Semi")

# the tape's colours: black, a warm white, the three encoders (green, blue, yellow), the greys, the record's red
BG = colors.HexColor("#000000")
FG = colors.HexColor("#ece8dc")
DIM = colors.HexColor("#8a8a8a")
FAINT = colors.HexColor("#4a4a4a")
LINE = colors.HexColor("#262626")
CARD = colors.HexColor("#0e0e0e")
G = colors.HexColor("#7ccc60")
B = colors.HexColor("#6ebaf4")
Y = colors.HexColor("#f6d65c")
R = colors.HexColor("#e84840")
P_ = colors.HexColor("#f4aac8")
O = colors.HexColor("#fa9646")
HEX = {"g": "#7ccc60", "b": "#6ebaf4", "y": "#f6d65c", "r": "#e84840", "p": "#f4aac8", "d": "#8a8a8a", "f": "#ece8dc",
       "o": "#fa9646"}

W, H = A4
M = 18 * mm

S_KICK = ParagraphStyle("kick", fontName="Mono", fontSize=8.5, leading=11, textColor=DIM, spaceAfter=2)
S_H1 = ParagraphStyle("h1", fontName="Light", fontSize=34, leading=38, textColor=FG, spaceAfter=6)
S_H2 = ParagraphStyle("h2", fontName="Semi", fontSize=13, leading=17, textColor=FG, spaceBefore=10, spaceAfter=4)
S_P = ParagraphStyle("p", fontName="Body", fontSize=10.2, leading=15, textColor=FG, spaceAfter=5)
S_LEAD = ParagraphStyle("lead", fontName="Light", fontSize=14, leading=20, textColor=FG, spaceAfter=10)
S_SM = ParagraphStyle("sm", fontName="Body", fontSize=8.8, leading=12.4, textColor=DIM, spaceAfter=3)
S_CELL = ParagraphStyle("cell", fontName="Body", fontSize=9.2, leading=13, textColor=FG)
S_CELLD = ParagraphStyle("celld", parent=S_CELL, textColor=DIM)
S_KEY = ParagraphStyle("key", fontName="MonoB", fontSize=9.2, leading=13, textColor=FG)
S_CAP = ParagraphStyle("cap", fontName="Mono", fontSize=7.8, leading=10, textColor=DIM, alignment=TA_CENTER)
S_STEP = ParagraphStyle("step", fontName="Light", fontSize=22, leading=24, textColor=G)
S_STH = ParagraphStyle("sth", parent=S_H2, spaceBefore=0, spaceAfter=2)
S_STP = ParagraphStyle("stp", parent=S_P, leading=14, spaceAfter=2)


def c(text, col):
    return f'<font color="{HEX[col]}">{text}</font>'


def k(text):
    """a button's name, as on the panel"""
    return f'<font name="MonoB" color="#ece8dc">{text}</font>'


def page_bg(canv, doc):
    canv.saveState()
    canv.setFillColor(BG)
    canv.rect(0, 0, W, H, fill=1, stroke=0)
    n = canv.getPageNumber()
    if n > 1:                                   # the foot: three dots in the encoders' colours, the page
        canv.setFont("Mono", 7.5)
        canv.setFillColor(FAINT)
        canv.drawString(M, 10 * mm, "DECK · OVERWORLD")
        canv.drawRightString(W - M, 10 * mm, f"{n:02d}")
        for i, col in enumerate((G, B, Y, O)):
            canv.setFillColor(col)
            canv.circle(W / 2 - 12 + i * 8, 11.3 * mm, 1.6, fill=1, stroke=0)
    canv.restoreState()


class Rule(Flowable):
    """a thin line, a short coloured stroke at its start"""
    def __init__(self, col=G, w=None):
        super().__init__()
        self.col, self.w = col, w

    def wrap(self, aw, ah):
        self.aw = self.w or aw
        return self.aw, 8

    def draw(self):
        self.canv.setStrokeColor(LINE)
        self.canv.setLineWidth(0.6)
        self.canv.line(0, 4, self.aw, 4)
        self.canv.setStrokeColor(self.col)
        self.canv.setLineWidth(1.6)
        self.canv.line(0, 4, 26, 4)


def shot(name, w_mm, crop=None):
    """a screenshot at 4x (the pixels kept sharp), a thin frame"""
    path = os.path.join(SHOTS, name)
    im = Image.open(path).convert("RGB")
    if crop:
        im = im.crop(crop)
    big = im.resize((im.width * 4, im.height * 4), Image.NEAREST)
    fr = Image.new("RGB", (big.width + 8, big.height + 8), (52, 52, 52))
    fr.paste(big, (4, 4))
    out = os.path.join(TMP, name.replace(".png", "") + ("_c" if crop else "") + ".png")
    fr.save(out)
    w = w_mm * mm
    return RLImage(out, width=w, height=w * fr.height / fr.width)


def table(rows, widths, head=None, key_col=True, pad=4):
    data = []
    if head:
        data.append([Paragraph(f'<font name="Mono" color="#8a8a8a">{h}</font>', S_CELL) for h in head])
    for r in rows:
        data.append([Paragraph(str(x), S_KEY if (key_col and i == 0) else S_CELL) for i, x in enumerate(r)])
    t = Table(data, colWidths=[w * mm for w in widths], repeatRows=1 if head else 0)
    st = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.4, LINE),
          ("LEFTPADDING", (0, 0), (-1, -1), 0), ("RIGHTPADDING", (0, 0), (-1, -1), 8),
          ("TOPPADDING", (0, 0), (-1, -1), pad), ("BOTTOMPADDING", (0, 0), (-1, -1), pad + 1)]
    t.setStyle(TableStyle(st))
    return t


def card(flows, w_mm, col=G):
    """a dark card with a coloured edge on its left"""
    t = Table([[flows]], colWidths=[w_mm * mm])
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), CARD), ("LINEBEFORE", (0, 0), (0, -1), 2, col),
                           ("LEFTPADDING", (0, 0), (-1, -1), 10), ("RIGHTPADDING", (0, 0), (-1, -1), 10),
                           ("TOPPADDING", (0, 0), (-1, -1), 8), ("BOTTOMPADDING", (0, 0), (-1, -1), 8)]))
    return t


def two(left, right, wl, wr, gap=8):
    t = Table([[left, right]], colWidths=[wl * mm, wr * mm])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0),
                           ("LEFTPADDING", (1, 0), (1, -1), gap * mm), ("RIGHTPADDING", (0, 0), (-1, -1), 0),
                           ("TOPPADDING", (0, 0), (-1, -1), 0), ("BOTTOMPADDING", (0, 0), (-1, -1), 0)]))
    return t


def bullets(items, col="g"):
    return [Paragraph(f'{c("—", col)}&nbsp;&nbsp;{t}', ParagraphStyle("b", parent=S_P, leftIndent=12, firstLineIndent=-12,
                                                                      spaceAfter=3)) for t in items]


def section(kick, title, lead=None, col=G):
    out = [Paragraph(kick, S_KICK), Paragraph(title, S_H1), Rule(col), Spacer(1, 6)]
    if lead:
        out.append(Paragraph(lead, S_LEAD))
    return out


# ------------------------------------------------------------------ figures --- */
def keyboard_png(name, white, black, title_w="", title_b=""):
    """the 27 keys, F3..G5: what the white and the black do (labels under each)"""
    sc = 3
    Wd, Hd = 1700, 500
    im = Image.new("RGB", (Wd, Hd), (0, 0, 0))
    d = ImageDraw.Draw(im)
    fm = ImageFont.truetype(os.path.join(FONTS, FF["MonoB"]), 30)
    fs = ImageFont.truetype(os.path.join(FONTS, FF["Mono"]), 22)
    kw, x0, y0, kh = 100, 50, 80, 300
    blacks_after = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]          # the white key a black one sits after (F3..G5)
    for i in range(16):
        x = x0 + i * kw
        d.rectangle([x, y0, x + kw - 6, y0 + kh], outline=(90, 90, 90), width=2)
        lab = white[i] if i < len(white) else ""
        col = (124, 204, 96) if lab else (60, 60, 60)
        for j, line in enumerate(str(lab).split("\n")):
            tw = d.textlength(line, font=fm if j == 0 else fs)
            d.text((x + (kw - 6 - tw) / 2, y0 + kh - 72 + j * 32), line, font=fm if j == 0 else fs, fill=col if j == 0 else (150, 150, 150))
    for j, a in enumerate(blacks_after):
        x = x0 + a * kw + kw - 30
        d.rectangle([x, y0, x + 48, y0 + 165], fill=(14, 14, 14), outline=(140, 140, 140), width=2)
        lab = black[j] if j < len(black) else ""
        for i, line in enumerate(str(lab).split("\n")):
            tw = d.textlength(line, font=fs)
            d.text((x + 24 - tw / 2, y0 + 118 + i * 18 - (len(str(lab).split("\n")) - 1) * 18), line, font=fs, fill=(110, 186, 244))
    d.text((x0, 18), title_w, font=fm, fill=(124, 204, 96))
    tw = d.textlength(title_b, font=fm)
    d.text((Wd - 50 - tw, 18), title_b, font=fm, fill=(110, 186, 244))
    out = os.path.join(TMP, name)
    im.save(out)
    return out


def kb(name, white, black, tw="", tb="", w_mm=None):
    p = keyboard_png(name, white, black, tw, tb)
    w = (w_mm or (W - 2 * M) / mm) * mm
    return RLImage(p, width=w, height=w * 500 / 1700)


def encoders_png():
    """the four knobs K1..K4 in the screen's colours, and what each turns"""
    im = Image.new("RGB", (1500, 330), (0, 0, 0))
    d = ImageDraw.Draw(im)
    fb = ImageFont.truetype(os.path.join(FONTS, FF["Light"]), 44)
    fs = ImageFont.truetype(os.path.join(FONTS, FF["Mono"]), 26)
    items = [((124, 204, 96), "K1", "поле 1 · на саде — MACRO"), ((110, 186, 244), "K2", "поле 2 · REVERB"),
             ((246, 214, 92), "K3", "поле 3 · ECHO"), ((236, 232, 220), "K4", "поле 4 · VOLUME")]
    for i, (col, nm, what) in enumerate(items):
        cx = 120 + i * 370
        d.ellipse([cx - 70, 40, cx + 70, 180], outline=(70, 70, 70), width=6)
        d.arc([cx - 70, 40, cx + 70, 180], start=-90, end=150 - i * 60, fill=col, width=8)
        d.ellipse([cx - 8, 102, cx + 8, 118], fill=col)
        d.text((cx + 84, 84), nm, font=fb, fill=(236, 232, 220))
        for j, part in enumerate(what.split(" · ")):
            d.text((cx - 70, 205 + j * 34), part, font=fs, fill=(150, 150, 150) if j == 0 else col)
    out = os.path.join(TMP, "encoders.png")
    im.save(out)
    return out


# ------------------------------------------------------------------- content --- */
def build():
    story = []
    CW = (W - 2 * M) / mm                         # the column, mm

    # ---- the cover
    cover = Image.open(os.path.join(SHOTS, "g_plains_day.png")).convert("RGB").crop((0, 28, 240, 172))
    cover = cover.resize((cover.width * 6, cover.height * 6), Image.NEAREST)
    cover_p = os.path.join(TMP, "cover.png")
    cover.save(cover_p)
    story += [Spacer(1, 22 * mm),
              Paragraph("M-VAVE FM-1 · ПРОШИВКА", ParagraphStyle("k0", parent=S_KICK, alignment=TA_CENTER, fontSize=9)),
              Spacer(1, 6),
              Paragraph("DECK", ParagraphStyle("t", fontName="Light", fontSize=72, leading=76, textColor=FG, alignment=TA_CENTER)),
              Paragraph(f'кассета {c("OVERWORLD", "g")}', ParagraphStyle("t2", fontName="Light", fontSize=22, leading=28,
                                                                           textColor=FG, alignment=TA_CENTER)),
              Spacer(1, 14 * mm),
              RLImage(cover_p, width=CW * mm, height=CW * mm * 144 / 240),
              Spacer(1, 12 * mm),
              Paragraph("Карманная студия в духе OP-1 и тихий сад в духе музыки C418: войлочное пианино, тёплые пады, "
                        "стеклянные колокольчики, пыльные лоу-фай барабаны, дождь и птицы. Много воздуха, мало нот, всё "
                        "немного плывёт.", ParagraphStyle("cv", parent=S_LEAD, alignment=TA_CENTER, textColor=DIM)),
              PageBreak()]

    # ---- helpers for the lessons
    def steps(items, col=G, width=None):
        """numbered steps: a big number, a title, what to do"""
        wd = width or CW
        rows = [[Paragraph(n, ParagraphStyle("stn", parent=S_STEP, textColor=col)), [Paragraph(h, S_STH), Paragraph(t, S_STP)]]
                for n, h, t in items]
        t = Table(rows, colWidths=[10 * mm, (wd - 10) * mm])
        t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.4, LINE),
                               ("LEFTPADDING", (0, 0), (-1, -1), 0), ("TOPPADDING", (0, 0), (-1, -1), 3),
                               ("BOTTOMPADDING", (0, 0), (-1, -1), 4)]))
        return t

    def knobs(k1, k2, k3, k4, extra=None):
        """the four knobs of a page, in their colours (and ALGO's extra)"""
        row = [[Paragraph(f'{c("●", col)} <font name="Mono" color="#8a8a8a">{n}</font>', S_CELL) for n, col in
                (("K1", "g"), ("K2", "b"), ("K3", "y"), ("K4", "o"))],
               [Paragraph(x, S_CELL) for x in (k1, k2, k3, k4)]]
        t = Table(row, colWidths=[CW / 4 * mm] * 4)
        t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0),
                               ("TOPPADDING", (0, 0), (-1, -1), 1), ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
                               ("LINEBELOW", (0, 1), (-1, 1), 0.4, LINE)]))
        out = [t]
        if extra:
            out.append(Paragraph(extra, S_SM))
        return out

    def note(title, text, col=Y):
        return card([Paragraph(c(title, {G: "g", B: "b", Y: "y", R: "r", O: "o"}[col]), S_H2), Paragraph(text, S_P)], CW, col)

    def notew(w, title, text, col=Y):
        return card([Paragraph(c(title, {G: "g", B: "b", Y: "y", R: "r", O: "o"}[col]), S_H2), Paragraph(text, S_P)], w, col)

    def lesson(num, title, lead, col, shot_name, items, after=None, sw=60, pb=True):
        out = section(num, title, lead, col)
        right = [steps(items, col, CW - sw - 6)]
        out.append(two(shot(shot_name, sw), right, sw, CW - sw, 6))
        if after:
            out.append(Spacer(1, 5 * mm))
            out += after
        if pb:
            out.append(PageBreak())
        return out

    # ---- contents + how to read
    toc = [("1", "Установка и откат"), ("2", "Экран, ручки, кнопки"), ("3", "Клавиши: что делают на каждой странице"),
           ("4", "Играть: дорожки, пресеты, тональность"), ("5", "Записать: REC, метроном, автоматизация, UNDO"),
           ("6", "TRACKS и EDIT-список: правка и стирание клипа"), ("7", "Секвенсоры и арпеджиатор"),
           ("8", "Звук: FX, эффекты, пресеты"), ("9", "Пульт MIX, части и песня ROAD"),
           ("10", "Проекты, автосохранение, снимок"), ("11", "GLOBAL: все настройки"), ("12", "ATMO — атмосфера"),
           ("13", "Звуки OVERWORLD"), ("14", "Что убрано и чем заменено (было → стало)"), ("15", "Шпаргалка и если что-то не так")]
    story += section("СОДЕРЖАНИЕ", "Что внутри", "Коротко и по делу. Версия -12: комбинаций кнопок стало меньше, "
                     "правила одинаковые на всех страницах.")
    story.append(table([[c(n, "g"), t] for n, t in toc], [12, CW - 12], key_col=False, pad=3))
    story.append(Spacer(1, 6 * mm))
    story.append(card([Paragraph(c("Как читать", "y"), S_H2),
                       Paragraph(f"Кнопки — как на панели: {k('PLAY')}, {k('HOME')}, {k('REC')}. «Белая 5» — пятая белая клавиша "
                                 f"слева, «чёрная 2» — вторая чёрная. «Тап» — нажал и отпустил. «Держать» — "
                                 f"около полсекунды и дольше: кнопка, которую держат, показывает подсказку внизу экрана. "
                                 f"{c('K1', 'g')} {c('K2', 'b')} {c('K3', 'y')} {c('K4', 'o')} — четыре верхние ручки, "
                                 f"на экране нарисованы теми же цветами.", S_P)], CW, Y))
    story.append(Spacer(1, 8 * mm))

    # ---- 1 install
    story.append(PageBreak())
    story += section("01", "Установка и откат", "Один двойной клик. Лучше ставить на второй, экспериментальный FM-1.")
    story.append(steps([
        ("1", "Подготовка", "Зарядить FM-1. Закрыть Reaper и всё, что работает с MIDI. Не давать компьютеру уснуть."),
        ("2", "Кабель", "FM-1 — к компьютеру напрямую, без хаба, кабелем с передачей данных."),
        ("3", "Запуск", f"Дважды кликнуть {k('install-deck.bat')} (рядом лежат {k('deck-overworld.fwsc')} и {k('fm1_install.py')})."),
        ("4", "Ждать", "Не трогать кабель, пока окно не скажет, что готово. Оборвалось — запустить ещё раз."),
        ("5", "Готово", f"FM-1 перезагрузится в сад и откроет последнюю песню (первый раз — демо FIRST LIGHT). Нажми {k('PLAY')}.")]))
    story.append(Spacer(1, 6 * mm))
    story.append(note("Откат", f"{k('fm1\\overworld\\install-overworld.bat')} — вернётся прежняя прошивка OVERWORLD (её петли будут "
                      "пустыми: DECK хранит проекты в том же месте). Чёрный экран — Pico и Transporter, "
                      f"{k('Desktop\\FM1Trasmitter-for-pico\\ВОССТАНОВЛЕНИЕ_FM-1.txt')}.", R))
    story.append(Spacer(1, 4 * mm))
    story.append(note("Не находит синт", f"Установщик сам ищет FM-1 среди MIDI-портов, в том числе под чужим именем (например «Clipper»): "
                      "другую технику (клавиатуры, Minilab) он не трогает, а на остальные порты шлёт только безопасный запрос имени. Не нашёл — в окне "
                      "будет список портов, которые он пробовал. Тогда запусти вручную: "
                      f"{k('py -3 fm1_install.py deck-overworld.fwsc --port ИМЯ')} (ИМЯ — часть имени порта из списка). "
                      "Часто помогает другой кабель: нужен кабель с передачей данных.", Y))
    story.append(Spacer(1, 4 * mm))
    story.append(note("Если тормозит", f"{k('diag-deck.bat')} минуту слушает синт, пока ты играешь, и пишет {k('deck_diag.txt')}: "
                      "нагрузку, пропуски звука, долгие кадры. Файл можно прислать.", B))
    story.append(PageBreak())

    # ---- 2 screen, knobs, buttons
    story += section("02", "Экран, ручки, кнопки", "Сверху шапка, под ней модуль страницы: окно и четыре ручки.")
    story.append(two(shot("p02_sound_engine.png", 50), bullets([
        "Шапка: ▶ играет / ● пишет / ■ стоит, часть A–H, такт.доля, имя страницы или дорожки, темп, шесть точек-дорожек "
        "(текущая белая, звучащие цветные, заглушённая — кольцо). Слева сверху точка — есть несохранённое.",
        "Модуль: сверху имя страницы и её состояние, в окне картинка, внизу четыре ручки "
        f"{c('K1', 'g')} {c('K2', 'b')} {c('K3', 'y')} {c('K4', 'o')}: над ручкой имя, под ней значение.",
        "Внизу строка подсказки: видна 3 секунды после смены страницы и пока держишь кнопку.",
        "Кнопка страницы горит, пока ты на ней. Тап по ней ещё раз — следующий ряд ручек этой страницы."], ), 50, CW - 50, 6))
    story.append(Spacer(1, 3 * mm))
    story.append(Paragraph(f"Три ручки слева — помощники, одинаковые везде: {c('SELECT', 'g')} — дорожка, {c('PRESET', 'y')} — пресет "
                           f"дорожки, {c('ALGO', 'b')} — пятое поле страницы (значение всплывает на экране). "
                           f"{k('HOME')} + {c('SELECT', 'g')} — темп, {k('HOME')} + {c('PRESET', 'y')} — другой инструмент дорожки.", S_SM))
    story.append(Spacer(1, 3 * mm))
    btn = [[k("HOME"), "сад; на саде — TRACKS; ещё раз — назад", "SHIFT (держишь)", "белая — пресет · чёрная 1–6 — дорожка"],
           [k("FX"), "SOUND; ещё раз — ряд (FAV · MORE · FX · TYPE · MOD)", "+ белая 1–4: выкл. ступень цепочки", "следующий блок пресетов"],
           [k("SEQ"), "секвенсор дорожки; ещё раз — ряд", "—", "BAKE (DREAM → STEP)"],
           [k("ARP"), "арпеджиатор вкл + его страница; ещё раз — ряд", "+ белая A–H: часть", "ARP выкл."],
           [k("EDIT"), "KEYS; на TRACKS — EDIT-список", "UNDO (0,7 с)", "REDO"],
           [k("REC"), "запись (со счётом); при записи — punch-out", "держать 0,6 с: стереть свежий дубль; + ручка SOUND/MIX: автоматизация", "метроном вкл/выкл"],
           [k("PLAY"), "старт / стоп сразу", "PART ↔ SONG (0,8 с)", "тапы — TAP TEMPO"],
           [k("LFO"), "MIX (пульт); ещё раз — MASTER", "+ ручка SOUND: модулятор MOD 1", "—"],
           [k("ENV"), "ROAD (песня-лента)", "гараж частей (0,6 с)", "—"],
           [k("SEL"), "ATMO (атмосфера)", "—", "предыдущий блок пресетов"],
           [k("GLO"), "GLOBAL (настройки); ещё раз — ряд", "FILL, пока держишь", "—"],
           [k("SAVE"), "PROJECT; ещё раз — сохранить", "+ белая 1 снимок, 2 вернуть, 13–16 свой пресет", "—"],
           [k("OCT− / +"), "октава дорожки", "—", "клип на ступень лада"]]
    story.append(table(btn, [22, 56, 52, CW - 130], head=["КНОПКА", "ТАП", "ДЕРЖАТЬ", "С HOME"], pad=2))
    story.append(PageBreak())

    # ---- 3 keys
    story += section("03", "Клавиши", "Одно правило на MIX и TRACKS, у остальных страниц своё. Ниже — всё по порядку.", B)
    story.append(kb("kb_tracks.png",
                    ["1", "2", "3", "4", "5", "6", "", "", "", "", "", "", "", "", "", ""],
                    ["M1", "M2", "M3", "M4", "M5", "M6", "SOLO", "ARM", "", "", ""],
                    "белая 1–6: выбрать дорожку", "чёрная 1–6: мьют, 7: соло, 8: на запись"))
    story.append(Paragraph(f"{c('MIX и TRACKS — одно правило.', 'b')} Белая 1–6 только выбирает дорожку. Чёрная 1–6 — мьют дорожки "
                           f"{c('в этой части', 'y')} (пока играет, срабатывает с конца такта; кнопка мигает). Чёрная 7 — соло "
                           f"выбранной дорожки, чёрная 8 — взвести выбранную на запись (сначала выбери, потом взводи). "
                           f"Временного мьюта и «держи белую — сотрёт» больше нет.", S_P))
    story.append(Spacer(1, 2 * mm))
    rows = [["PLAY, KEYS, SOUND…", "играют ноты (на KEYS — по ладу или аккордами). Держишь HOME — клавиши выбирают пресеты и дорожки."],
            [k("HOME") + " + белая 1–16", "пресет дорожки (13–16 — твои). " + k("HOME") + " + чёрная 1–6 — выбрать дорожку."],
            ["EDIT-список (TRACKS + EDIT)", "белые 1–14: QUANTIZE, COPY BAR, PASTE BAR, DOUBLE, VARIATE, HUMANIZE, COOK, RANDOM, "
                                            "COPY CLIP, PASTE CLIP, LAYERS, ERASE NOTES, ERASE AUTO, ERASE ALL (раздел 6)."],
            ["Кнопка + клавиша/ручка", "Если держишь кнопку и нажал клавишу или крутнул ручку — это только комбо: при отпускании страница или список этой кнопки не откроются."],
            [k("ARP") + " (держать) + белая A–H", "перейти к части; с конца такта. Пустая часть получает копию текущей (NEW PART)."],
            [k("FX") + " (держать) + белая 1–4", "выкл./вкл. ступень цепочки: 1 эффект, 2 эхо, 3 ревер, 4 тейп."],
            [k("SAVE") + " (держать) + белая", "1 — снимок части, 2 — вернуть снимок, 13–16 — сохранить свой пресет."],
            ["PROJECT", "белые 1–4 — открыть проект, 5 — новый (шаблон), 6 — демо; чёрные 1 EXPORT, 2 STEMS, 3 сохранить, "
                        "4 CLEAR ALL (нажать дважды)."],
            ["ROAD", "белые A–H — добавить блок части; чёрные — трюки, удалить, дублировать, вырезать, вставить."],
            ["SEQ STEP", "белые — шаги (барабаны) или ноты на шаг под курсором; чёрные — инструменты кита."]]
    story.append(table(rows, [52, CW - 52], pad=3))
    story.append(PageBreak())

    # ---- 4 play
    story += lesson("04", "Играть", "Сад — главная страница. Здесь просто играешь.", G, "p01_play.png", pb=False, items=[
        ("1", "Сад", f"{k('HOME')} — сад: цветок растёт от нот всех дорожек. {k('PLAY')} — песня играет."),
        ("2", "Дорожка", f"{c('SELECT', 'g')} — следующая; или {k('HOME')} + чёрная 1–6. Шесть дорожек: 1 PIANO, 2 KEYS, 3 PAD, "
                         "4 BASS, 5 DRUMS, 6 NATURE."),
        ("3", "Пресет", f"{c('PRESET', 'y')} листает пресеты; {k('HOME')} + белая — сразу нужный. {k('HOME')} + {c('PRESET', 'y')} — другой инструмент."),
        ("4", "Ручки сада", f"{c('K1', 'g')} MACRO (главная ручка пресета), {c('K2', 'b')} ревер, {c('K3', 'y')} эхо, "
                            f"{c('K4', 'o')} громкость. {c('ALGO', 'b')} — MOOD: луг, дождь, пещера, снег, океан, ночь, костёр, учёба."),
        ("5", "Октава", f"{k('OCT−')} / {k('OCT+')}; горит та, куда сдвинуто. {k('HOME')} + эти кнопки — клип на ступень лада.")])
    story += section("04", "Тональность и аккорды", "Клавиатура не фальшивит: играет ноты выбранного лада.", Y)
    story.append(two(shot("p11_keys.png", 50), bullets([
        f"{k('EDIT')} — страница KEYS. {c('K1', 'g')} тоника (ROOT), {c('K2', 'b')} лад (MAJOR, MINOR, DORIAN…). Клавиши "
        f"тональность не ставят — только ручка K1.",
        f"{c('K3', 'y')} WHITE: NOTES — белые играют ноты лада (клавиша До — всегда тоника); CHORDS — каждая белая играет аккорд "
        f"своей ступени. {c('K4', 'o')} — тип аккорда (TRIAD … MAJ9, QUARTAL).",
        f"{k('EDIT')} ещё раз — ряд 2: что делают чёрные (CHORDS / PERFORM / NOTES), качество, голосоведение, STRUM. "
        "Ряд 3: SPLIT (аккорды — на дорожку гармонии, ноты — на текущую), BASS, октава, DIM SKIP. Ряд 4: SCALE / CHROMA. "
        f"PERFORM: пока держишь чёрную — эффект на всю песню (TAPESTOP, REWIND, LOW PASS, ECHO THROW…)."]), 50, CW - 50, 6))
    story.append(PageBreak())

    # ---- 5 record
    story += lesson("05", "Записать", "Запись идёт по кругу: каждый проход — новый слой поверх. Одно нажатие, без подтверждений.", R,
                    "p13_tape_count.png", pb=False, items=[
        ("1", "Выбери дорожку", f"{c('SELECT', 'g')} или {k('HOME')} + чёрная. Писать можно на любой секвенсор (LIVE по умолчанию)."),
        ("2", "Тап REC", f"Откроется кассета, метроном отсчитает такт (COUNT IN, на GLOBAL), играй с первой доли. "
                         f"Начал чуть раньше: всё, что сыграно после последнего щелчка отсчёта, ляжет в начало петли. "
                         f"В GLOBAL ON NOTE = ждать первую ноту вместо счёта."),
        ("3", "Слои", "Петля крутится, каждый проход дописывается. До 7 слоёв (LAYERS), чтобы убрать — EDIT-список → LAYERS."),
        ("4", "Стоп", f"{k('REC')} ещё раз — punch-out: запись кончилась, петля играет дальше. Кассета остаётся: "
                      f"{c('K1', 'g')} BARS (длина), {c('K2', 'b')} QUANT, {c('K3', 'y')} REC MOD (OVERDUB / REPLACE), "
                      f"{c('K4', 'o')} METRO (OFF / ON REC / ALWAYS)."),
        ("5", "Метроном", f"{k('HOME')} + {k('REC')} — вкл/выкл; звук, громкость и деление — на GLOBAL."),
        ("6", "Стереть свежий дубль", f"Сыграл не то? Держи {k('REC')} 0,6 с и ничего не крути: сотрутся ноты последнего "
                                      f"нажатия REC (все проходы, все дорожки, на которые писалось). Старые ноты и автоматизация остаются. "
                                      f"Во время записи — сотрётся то, что уже записано, а запись идёт дальше. Если дубля нет — «NO TAKE». "
                                      f"Один шаг UNDO. В режиме REPLACE ноты, которые дубль заменил, вернёт ещё один UNDO."),
        ("7", "UNDO", f"Держи {k('EDIT')} (0,7 с) — отменить. {k('HOME')} + {k('EDIT')} — вернуть (REDO). Серия поворотов одной "
                      "ручкой — один шаг.")],
        after=[note("REC: тап или держать", f"Тап — запись (срабатывает при отпускании). Держишь REC и крутишь ручку — это автоматизация, "
                    f"записи нот нет. Подержал дольше 0,45 с — появится подсказка; подержал 0,6 с и ничего не крутил — "
                    f"сотрётся дубль, который ты только что записал (см. шаг 7). Остальное стирается в EDIT-списке (раздел 6).", R)])
    story.append(two(shot("auto_rec.png", 48), [
        Paragraph(c("Автоматизация ручки", "y"), S_H2),
        Paragraph(f"Пока играет музыка, держи {k('REC')} и крути ручку на SOUND (или громкость, панораму, ревер, эхо на MIX). "
                  f"Движение запишется плавной кривой в клип этой дорожки в этой части и будет повторяться по кругу. На ручке "
                  f"появится волнистая метка. Крутнул рукой сам — твоё значение звучит до конца такта, потом кривая продолжается. "
                  f"Кривая своя у каждой ручки и дорожки, копируется вместе с частью и хранится в проекте. "
                  f"Стереть: EDIT-список → ERASE AUTO (ноты остаются).", S_P)], 48, CW - 48, 6))
    story.append(PageBreak())

    # ---- 6 tracks + edit list
    story += section("06", "TRACKS и EDIT-список", f"{k('HOME')} на саде — шесть дорожек в строках. Здесь выбор, мьют, соло и правка клипа.", O)
    story.append(two(shot("p31_tracks_play.png", 52), [
        Paragraph(f"{c('K1', 'g')} VOL, {c('K2', 'b')} PAN выбранной дорожки, {c('K3', 'y')} REC MODE (OVERDUB / REPLACE), "
                  f"{c('K4', 'o')} REC ARM. Справа в строке — клип дорожки в этой части, головка идёт по нему.", S_P),
        Paragraph(f"Клавиши — как на MIX: белая 1–6 выбор, чёрная 1–6 мьют в части, чёрная 7 соло, чёрная 8 на запись. "
                  f"Несколько дорожек можно взвести — REC запишет их разом (REC MODE на TRACKS и на кассете).", S_P),
        Paragraph(f"{k('EDIT')} на TRACKS — {c('EDIT-список', 'o')}; ещё раз {k('EDIT')} или {k('HOME')} — назад.", S_P)], 52, CW - 52, 6))
    story.append(Spacer(1, 3 * mm))
    story.append(two(shot("p42_edit_list.png", 52), [
        Paragraph(c("14 пунктов — белые 1–14", "o"), S_H2),
        table([["1 QUANTIZE", "выровнять ноты по сетке (шаг — QUANT дорожки, 0 = нечего)"], ["2–3 COPY / PASTE BAR", "такт клипа (играющий сейчас) в буфер и из буфера"],
               ["4 DOUBLE", "клип вдвое длиннее, с повтором нот"], ["5 VARIATE", "несколько нот сдвигаются на ступень лада, гармония сохраняется"],
               ["6 HUMANIZE", "чуть живее время и сила нот"], ["7 COOK", "ноты чуть иначе в ладу, дышит громкость; можно повторять"],
               ["8 RANDOM", "тот же ритм, новые ноты лада"], ["9–10 COPY / PASTE CLIP", "клип целиком"],
               ["11 LAYERS", "список слоёв дублей: мьют, удалить слой"]], [38, CW - 90], pad=2, key_col=True)], 52, CW - 52, 6))
    story.append(Spacer(1, 3 * mm))
    story.append(two(shot("p41_edit_erase_ask.png", 52), [
        Paragraph(c("Стирание: 12–14, нажать дважды", "r"), S_H2),
        Paragraph(f"{c('12 ERASE NOTES', 'r')} — ноты клипа (автоматизация остаётся). {c('13 ERASE AUTO', 'r')} — только кривые "
                  f"ручек. {c('14 ERASE ALL', 'r')} — эта дорожка во всех восьми частях: ноты и автоматизация. Первое нажатие "
                  f"спрашивает («AGAIN = ERASE», 3 секунды), второе — стирает. Любое стирание — один шаг UNDO "
                  f"({k('EDIT')} держать), на экране «UNDO BRINGS IT BACK».", S_P)], 52, CW - 52, 6))
    story.append(PageBreak())

    # ---- 7 seq + arp
    story += section("07", "Секвенсоры и арпеджиатор", f"{k('SEQ')} — как дорожка играет клип. Тип выбирается на странице SEQ ручкой K1 (ряд 2).", Y)
    story.append(table([
        ["STEP", "Шаги. Барабаны: белые — 16 шагов, чёрные — инструменты кита; K1 инструмент, K2 сила, K3 вероятность, K4 рэтчет. "
                 "Держи шаг и крути K1 — у шага своё значение. Мелодия: K1 сдвиг клипа, K2 длина нот, K3 сила, K4 вероятность."],
        ["LIVE", "Петля из того, что сыграл с REC. K1 SHIFT (сдвиг), K2 MOVE (весь клип на ступень лада), K3 громкость, K4 ревер."],
        ["DREAM", "Автопилот: на PLAY дорожка играет сама под аккорды песни. K1 энергия, K2 стиль (C418, LOFI, AMBIENT, "
                  "CAMPFIRE), K3 фантазия, K4 громкость. HOME + SEQ — «запечь» в STEP и править руками."],
        ["FINGER", "Каждая белая — готовый рисунок на такт (16 штук от простых к плотным). Держишь — играет, две складываются. "
                   "Чёрные — вариации. K1 BANK (LOFI, 8BIT, SYNTH, CINEMA), K2 LATCH (держится сам), K3 громкость, K4 ревер, ALGO эхо."]],
        [22, CW - 22], pad=4))
    story.append(Spacer(1, 3 * mm))
    story.append(Paragraph(f"{k('SEQ')} ещё раз — ряд 2, общий: SEQUENCER (тип), BARS (длина части в тактах, одна на всю часть), "
                           f"SWING (свой у дорожки), QUANT (квантование записи). Мелодии: третий тап — {c('STEP EDIT', 'g')}.", S_P))
    story.append(two(shot("p26_step_edit.png", 50), bullets([
        f"{c('STEP EDIT', 'g')}: {c('K1', 'g')} шаг, {c('K2', 'b')} ноту на шаг (вправо — поставить, дальше — аккорд; влево — убрать), "
        f"{c('K3', 'y')} длину, {c('K4', 'o')} высоту (по ладу; с HOME — на октаву). Нажатая клавиша пишет свою ноту на шаг под курсором.",
        "Клип можно править кнопками EDIT-списка (раздел 6): QUANTIZE, DOUBLE, COOK, RANDOM и другие.",
        "1 такт = 16 шагов = экран сетки. Длина части одна для всех дорожек."]), 50, CW - 50, 6))
    story.append(Spacer(1, 3 * mm))
    story.append(two(shot("p16_seq_arp.png", 50), bullets([
        f"{k('ARP')} включает арпеджиатор и открывает его страницу. Зажми аккорд — он перебирается. На барабанах не работает.",
        f"{c('K1', 'g')} MODE (UP, DOWN, UPDN, RAND, ORDER, CHORD), {c('K2', 'b')} RATE (1/4…1/32, триоли), {c('K3', 'y')} RHYTHM, "
        f"{c('K4', 'o')} LATCH. {k('ARP')} ещё раз — ряд 2: OCT, GATE, PLAYS (KEYS или аккорд песни), ON.",
        f"{k('SEQ')} на странице ARP — записать то, что он играл, нотами в клип (дорожка станет STEP). "
        f"{k('HOME')} + {k('ARP')} — арп выкл."]), 50, CW - 50, 6))
    story.append(PageBreak())

    # ---- 8 sound
    story += lesson("08", "Звук", f"{k('FX')} — модуль синта выбранной дорожки: в окне пресет и цепочка эффектов.", B, "fx_fav.png", [
        ("1", "FAV и MORE", f"FAV — четыре главные ручки этого пресета (то, что двигает MACRO); {c('ALGO', 'b')} — MACRO. "
                            f"{k('FX')} ещё раз — MORE: остальные (атака, затухание, отпускание…)."),
        ("2", "FX", f"Тон, глубина, ревер, эхо: {c('K1', 'g')} тон, {c('K2', 'b')} глубина, {c('K3', 'y')} ревер, {c('K4', 'o')} эхо. "
                    f"У FILTER K2 = CUTOFF (Гц), K1 = RES."),
        ("3", "TYPE и MOD", "TYPE — выбор эффекта (VINYL, TAPE, LOFI, CHORUS, FILTER, TREMOLO, SPRING) и его вторая ручка. "
                            f"MOD — модулятор; {k('LFO')} (держать) + ручка на FAV/MORE — модулятор прямо на неё."),
        ("4", "Выкл. ступень", f"Держи {k('FX')} + белая 1–4: 1 эффект, 2 эхо, 3 ревер, 4 тейп — выключается сразу, без щелчка; ещё раз — включена."),
        ("5", "Свой пресет", f"Держи {k('SAVE')} + белая 13–16 — сохранить в слот; позвать: {k('HOME')} + белая 13–16."),
        ("6", "Блоки пресетов", f"{k('HOME')} + {k('FX')} — следующий блок (AMBIENT LOFI, 8-16 BIT, SYNTHWAVE, CINEMATIC), "
                               f"{k('HOME')} + {k('SEL')} — предыдущий; место внутри блока то же.")],
        after=[note("Барабаны", f"На SEQ у барабанов ряд KIT: {c('K1', 'g')} звук (KICK, SNARE…), {c('K2', 'b')} его громкость, "
                    f"{c('K3', 'y')} строй, {c('K4', 'o')} длина. FX у барабанов: ALGO FOR — на весь кит или на один звук.", B)])
    story.append(PageBreak())

    # ---- 9 mix + road
    story += lesson("09", "Пульт MIX", f"{k('LFO')} — у каждой дорожки панорама, фейдер, огонёк уровня.", G, "p08_mix.png", pb=False, items=[
        ("1", "Ручки", f"{c('K1', 'g')} громкость, {c('K2', 'b')} панорама, {c('K3', 'y')} ревер, {c('K4', 'o')} эхо; "
                       f"{c('ALGO', 'b')} — пространство (ROOM, HALL, CAVE, SHIMMER). {k('LFO')} ещё раз — MASTER."),
        ("2", "Клавиши", "Белая 1–6 — выбрать дорожку, чёрная 1–6 — мьют в части, 7 — соло, 8 — на запись (как на TRACKS)."),
        ("3", "SIDECHAIN", "На GLOBAL: пады приседают от бочки.")])
    story += lesson("09", "Части и песня ROAD", "Восемь частей A–H; из них на ленте собирается песня.", B, "p09_road.png", [
        ("1", "Части", f"У каждой дорожки в каждой части свой клип. Держи {k('ARP')} + белая A–H — перейти (с конца такта). "
                       f"Пустая часть при игре получает копию текущей (NEW PART на GLOBAL: COPY / EMPTY)."),
        ("2", "Гараж", f"{k('ENV')} держать — гараж: {c('K1', 'g')} какая часть, {c('K2', 'b')} имя, {c('K3', 'y')} BARS. "
                       f"Белая 1–8 — скопировать часть сюда."),
        ("3", "Лента", f"{k('ENV')} — лента: белые A–H добавляют блок, {c('K1', 'g')} блок, {c('K2', 'b')} часть, "
                       f"{c('K3', 'y')} повторы, {c('K4', 'o')} PART / SONG. Чёрные — трюки (HALF, DOUBLE, REVERSE, ±12, TAPESTOP, DROP) и правка блоков."),
        ("4", "Играть", f"Держи {k('PLAY')} (0,8 с) — SONG: песня по ленте. PART — крутится одна часть. На ленте {k('REC')} "
                        f"записывает переходы частей вживую.")])

    # ---- 10 projects
    story += lesson("10", "Проекты, автосохранение, снимок", "Четыре проекта — четыре кассеты на полке.", G, "p12_project.png", [
        ("1", "Открыть", f"{k('SAVE')} — полка. Белые 1–4 открыть, 5 — новый ({c('K2', 'b')} TEMPLATE: EMPTY, POP, CLUB, AMBIENT, DEMO), "
                         f"6 — демо, {c('K3', 'y')} — стиль автопилота."),
        ("2", "Сохранить", f"{k('SAVE')} ещё раз — сохранено (SAVED). Чёрная 3 — то же. Справа сверху SAVED / NOT SAVED."),
        ("3", "Автосохранение", "Раз в 5 минут (первый раз сразу), когда синт стоит и ты не играешь и не держишь кнопку. "
                                "Звук в этот миг плавно уходит в ноль и возвращается. Перед выключением нажми SAVE дважды."),
        ("4", "Снимок", f"Держи {k('SAVE')} + белая 1 — снимок части («SNAPSHOT PART A») перед экспериментом. Белая 2 — "
                        f"вернуть его (это шаг UNDO)."),
        ("5", "Наружу", "Чёрная 1 — EXPORT в .mid (шесть дорожек, маркеры частей, ручки — CC74/CC7; Chrome → web/export.html "
                        "или tools/deck_export.py). Чёрная 2 — STEMS: песня 7 раз (микс и каждая дорожка соло, между проходами щелчок). "
                        "Чёрная 4 — CLEAR ALL (дважды)."),
        ("6", "MEM в шапке", "Память песни заполнена на 90 % (оранжевый) или 97 % (красный): сотри лишнее в EDIT-списке.")])
    story.append(PageBreak())

    # ---- 11 global
    story += section("11", "GLOBAL — все настройки", f"{k('GLO')} — деревянный метроном качается в темп. Тап ещё раз — следующий ряд (всего 5). "
                     f"Пятое поле ряда — {c('ALGO', 'b')}.", Y)
    story.append(two(shot("p19_global.png", 52), [
        Paragraph(f"Держи {k('GLO')} — FILL: сбивка, пока держишь. Темп, свинг, пыль, сон и сайдчейн хранятся в песне; метроном, MIDI, REC LAT, REC MODE и остальное — настройки устройства, они сохраняются сами.", S_P),
        Paragraph(f"{k('HOME')} + {c('SELECT', 'g')} — темп с любой страницы; {k('HOME')} + {k('PLAY')} (тапы) — TAP TEMPO.", S_P)], 52, CW - 52, 6))
    story.append(Spacer(1, 3 * mm))
    story.append(table([
        ["1 · время и воздух", "TEMPO 40–200 BPM", "SWING 0–60", "DUST 0–100 (шипение пыли и кассеты)", "SLEEP (всё темнее и медленнее — для концовки)", "SIDECHAIN"],
        ["2 · метроном", "METRO OFF / ON REC / ALWAYS", "COUNT IN 1–2 такта", "CLICK: CLICK, WOOD, HAT, COWBELL, RIM", "M VOL громкость", "M DIV: 1/4, 1/8, 1/16"],
        ["3 · MIDI и части", "MIDI OUT (ноты на каналах 1–6)", "CLOCK (MIDI-такт, старт, стоп)", "MIDI IN (каналы 1–6, 10 — барабаны)", "ON NOTE: REC ждёт первую ноту", "NEW PART: COPY / EMPTY"],
        ["4 · запись и эхо", "REC LAT 0–30 мс (по умолч. 9)", "ECHO TIME: 1/8. · 1/4 · 1/8 · 1/4.", "CPU (нагрузка, только показ)", "VERSION", "LOCK TAKE: ВКЛ/ВЫКЛ"],
        ["5 · дубли", "REC MODE: OVERDUB / REPLACE", "REC ARM: ВЫКЛ / ВКЛ", "—", "—", "—"]],
        [24, 28, 24, 36, 36, CW - 148], head=["РЯД", "K1", "K2", "K3", "K4", "ALGO"], pad=3, key_col=True))
    story.append(Spacer(1, 3 * mm))
    story += bullets([
        f"{c('REC LAT', 'y')} — нота, сыгранная на клавише, пишется на столько мс раньше: звук идёт через буфер и ЦАП. Подстрой на слух.",
        f"{c('LOCK TAKE', 'y')} (ВКЛ) — в конце дубля звук дорожки записывается вместе с нотами: смена пресета, MOOD или MACRO потом "
        f"не меняет записанное. ВЫКЛ — записанное играет живым звуком дорожки.",
        f"{c('REC ARM', 'y')} ВЫКЛ — один тап REC сразу пишет (как у OP-1). ВКЛ — REC взводит (мигает), PLAY запускает, REC ещё раз отпускает. "
        f"При играющем транспорте REC пишет сразу в обоих случаях.",
        f"{c('REC MODE', 'y')}: OVERDUB — слои поверх, REPLACE — новый проход заменяет ноты, которые ты сыграл. То же на TRACKS (K3).",
        f"{c('NEW PART', 'y')} COPY — пустая часть при переходе во время игры получает копию текущей. Непустую часть не трогает никогда."])
    story.append(PageBreak())

    # ---- 12 ATMO
    story += lesson("12", "ATMO — атмосфера", f"{k('SEL')} — место под музыкой: включаешь и смешиваешь, что нужно.", G, "p23_atmo.png", pb=False, items=[
        ("1", "Ряд 1", f"{c('K1', 'g')} RAIN, {c('K2', 'b')} FIRE, {c('K3', 'y')} BIRDS, {c('K4', 'o')} OCEAN. Плитка оживает, когда слой звучит."),
        ("2", "Ряд 2", f"{k('SEL')} ещё раз: WIND, CRICKETS, DUST (треск винила), TAPE (лента: плывёт высота, тёплый низ, мягкая перегрузка, шип)."),
        ("3", "AIR LEVEL", f"{c('ALGO', 'b')} — громкость всех слоёв сразу."),
        ("4", "С песней", "Атмосфера хранится в проекте. В экспорт .mid и STEMS не входит: это звук синта, а не дорожки.")])
    story.append(Spacer(1, 4 * mm))

    # ---- 13 sounds
    story += section("13", "Звуки OVERWORLD", "Одиннадцать движков, четыре блока пресетов, 4 своих слота на каждый.")
    eng = [["FELT", "пианино с войлоком — главный инструмент", "TONE · FELT · DECAY · KNOCK"],
           ["TINE", "лоу-фай электропиано с тремоло", "TINE · TREMOLO · DRIFT · BARK"],
           ["AIRPAD", "тёплые пады: SWEDEN, AIR, WARM, SHIMMER, STRINGS", "BRIGHT · SWELL · AIR · TYPE"],
           ["GLASS", "колокольчики: стекло, челеста, маримба, шкатулка", "TYPE · DECAY · SHINE · OCTAVE"],
           ["KALIMBA", "язычок, щипок, деревянный корпус", "TONE · DECAY · WOOD"],
           ["SOFTBASS", "мягкий бас: SUB, UPRIGHT, MOOG, глайд", "TONE · PLUCK · GLIDE · TYPE"],
           ["DUSTKIT", "барабаны: лоу-фай киты и эмуляции 808 / 909 / 606 / CR-78 / 707", "TUNE · DECAY · DUST · KIT"],
           ["WORLD", "дождь, ветер, костёр, ручей, птицы, океан, гроза", "TYPE · AMOUNT · TONE"],
           ["WINDS", "флейты, окарина, кларнет, труба, валторна, медь", "BRIGHT · BREATH · VIBRATO · TYPE"],
           ["SYNTH", "чип и synthwave: SAW, PULSE, SQUARE, TRI, SUPER, фильтр", "WAVE · CUTOFF · RESO · SWEEP"],
           ["PLUCK", "щипковые: лютня, арфа, нейлон, сталь, кото", "TYPE · BRIGHT · DECAY · POSITION"]]
    story.append(table(eng, [24, 86, CW - 110], head=["ДВИЖОК", "ЧТО ЭТО", "РУЧКИ K1–K4"], pad=2))
    story.append(Spacer(1, 3 * mm))
    blk = [["AMBIENT LOFI", "132", "то, что было, и новые: SOFT SAW, DUST LEAD, DRONE-пады, пыльные ударные"],
           ["8-16 BIT", "50", "меандр, пульс, треугольник, шум; чип-арпеджио, GAME OVER, POWER-UP, SNES STRINGS"],
           ["SYNTHWAVE", "74", "NIGHT DRIVE, OUTRUN LEAD, SUNSET SAW, MIAMI BRASS, барабаны 808 и 909"],
           ["CINEMATIC", "88", "дроны, хор, рога, струнные, боевые барабаны WAR, 808 SUB BOOM"]]
    story.append(table(blk, [30, 14, CW - 44], head=["БЛОК", "ПРЕСЕТОВ", "ЧТО ВНУТРИ"], pad=2))
    story.append(Paragraph("Барабанные киты — синтез-эмуляции, а не семплы оригиналов: похожий характер, не те самые звуки. "
                           "Дорожка KEYS листает 12 пресетов из TINE, GLASS и KALIMBA.", S_SM))
    story.append(PageBreak())

    # ---- 14 was -> now
    story += section("14", "Что убрано и чем заменено", "В версии -11 комбинаций стало меньше. Что было и куда делось.", R)
    story.append(table([
        ["SEQ + чёрная 1–5", "COOK, RANDOM, QUANTIZE и т. п. — теперь EDIT-список на TRACKS (белые 1–14)"],
        ["HOME + чёрная 7–11", "копировать / вставить / удвоить клип, вариация, гуманизация — EDIT-список (9, 10, 4, 5, 6)"],
        ["SEQ + OCT−/OCT+", "UNDO / REDO — держать EDIT / HOME + EDIT"],
        ["SEQ + белая", "тип секвенсора — на странице SEQ, K1 (ряд 2)"],
        ["SEQ + белая 1–6 (временный мьют)", "убран; мьют дорожки в части — чёрные 1–6 на MIX/TRACKS"],
        ["MIX: белая 1–6 мьют · чёрная 1–6 соло", "белая — выбрать, чёрная 1–6 — мьют, 7 — соло, 8 — на запись"],
        ["MIX: держать белую — стереть", "ERASE NOTES / AUTO / ALL в EDIT-списке"],
        ["REC держать 1 с / 2,5 с — стереть клип / всё", "убрано; вместо этого: держать REC 0,6 с — стереть только свежий дубль; остальное — EDIT-список"],
        ["REC + FX (стереть автоматизацию)", "EDIT-список → ERASE AUTO"],
        ["SEQ + REC (стереть клип сразу)", "EDIT-список → ERASE NOTES (нажать дважды)"],
        ["HOME + чёрная 1–6", "осталось: выбрать дорожку"]], [62, CW - 62], pad=3, key_col=False))
    story.append(Spacer(1, 3 * mm))
    story.append(Paragraph("Тап REC — запись; держать REC + ручка — автоматизация; держать REC 0,6 с без ручки — стереть только что записанный дубль; остальное стирание — только в EDIT-списке и всегда «нажать дважды»; "
                           "всё это отменяется одним UNDO.", S_P))

    # ---- 15 cheat sheet
    story += section("15", "Шпаргалка", None, Y)
    cheat = [[k("HOME"), "сад · на саде TRACKS · держать SHIFT"], [k("HOME") + " + белая", "пресет (13–16 — свои)"],
             [k("HOME") + " + чёрная 1–6", "выбрать дорожку"], [k("HOME") + " + FX / SEL", "блок пресетов вперёд / назад"],
             [k("PLAY"), "старт/стоп · держать PART↔SONG · HOME+ тапы: темп"],
             [k("REC"), "запись · punch-out · держать 0,6 с: стереть свежий дубль · +ручка: автоматизация · HOME+: метроном"],
             [k("EDIT"), "KEYS · на TRACKS: EDIT-список · держать: UNDO · HOME+: REDO"],
             ["MIX / TRACKS", "белая 1–6 выбор · чёрная 1–6 мьют в части · 7 соло · 8 на запись"],
             [k("ARP") + " + белая A–H", "перейти к части"], [k("FX") + " + белая 1–4", "ступень цепочки вкл/выкл"],
             [k("SAVE") + " + белая", "1 снимок · 2 вернуть · 13–16 свой пресет"],
             [k("GLO"), "настройки (5 рядов) · держать: FILL"]]
    story.append(table(cheat, [48, CW - 48], pad=2))
    story.append(Spacer(1, 3 * mm))
    tr = [["Нет звука", "Проверь мьют и соло на MIX (LFO), громкость MASTER."],
          ["Клавиши играют не то", f"На ленте, полке, пульте клавиши управляют. {k('HOME')} — в сад, там просто играют."],
          ["Меняется пресет", f"Зажата {k('HOME')}. Отпусти её перед игрой."],
          ["Не понимаю, где я", f"Держи любую кнопку чуть дольше — внизу подсказка. Горящая кнопка — твоя страница."],
          ["Синт моргнул", "Автосохранение (раз в 5 минут, только в тишине): звук на миг уходит в ноль."],
          ["Звук прерывается", "Перегруз: меньше длинных падов сразу. Загрузка видна на GLOBAL (ряд 4, CPU) и в diag-deck.bat."],
          ["Экспорт не приходит", "Закрой другие MIDI-программы. Запасной путь — MIDI OUT вживую в Reaper."]]
    story.append(table(tr, [44, CW - 44], pad=3))
    story.append(Spacer(1, 6 * mm))
    story.append(Paragraph("Сделано на fm1-base и felucca-bsp (Felucca — Leo Kuroshita, Hügelton Instruments). Ревер, "
                           "голосоведение и звук OVERWORLD — по мотивам AMB-1 (Charles Vestal). Лицензия MPL-2.0.", S_SM))

    doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=M, rightMargin=M, topMargin=16 * mm, bottomMargin=18 * mm,
                            title="DECK · OVERWORLD — инструкция (-11)", author="DECK")
    doc.build(story, onFirstPage=page_bg, onLaterPages=page_bg)
    print(OUT)


if __name__ == "__main__":
    build()
