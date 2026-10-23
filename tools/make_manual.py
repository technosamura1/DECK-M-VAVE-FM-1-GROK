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
S_STEP = ParagraphStyle("step", fontName="Light", fontSize=26, leading=28, textColor=G)


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
        rows = [[Paragraph(n, ParagraphStyle("stn", parent=S_STEP, textColor=col)), [Paragraph(h, S_H2), Paragraph(t, S_P)]]
                for n, h, t in items]
        t = Table(rows, colWidths=[12 * mm, (wd - 12) * mm])
        t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.4, LINE),
                               ("LEFTPADDING", (0, 0), (-1, -1), 0), ("TOPPADDING", (0, 0), (-1, -1), 4),
                               ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
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

    def lesson(num, title, lead, col, shot_name, items, after=None, sw=70):
        out = section(num, title, lead, col)
        right = [steps(items, col, CW - sw - 6)]
        out.append(two(shot(shot_name, sw), right, sw, CW - sw, 6))
        if after:
            out.append(Spacer(1, 5 * mm))
            out += after
        out.append(PageBreak())
        return out

    # ---- contents
    toc = [("01", "Установка и откат"), ("02", "Как устроен экран и панель"), ("03", "Урок 1. Играть: дорожки, пресеты, сад"),
           ("04", "Урок 2. Тональность и аккорды"), ("05", "Урок 3. Записать петлю, стереть, отменить"),
           ("06", "Урок 4. Арпеджиатор"), ("07", "Урок 5. Секвенсоры"), ("08", "Урок 6. Звук"),
           ("09", "Урок 7. Свести"), ("10", "Урок 8. Песня из частей"), ("11", "Урок 9. Проекты и сохранение"),
           ("12", "GLOBAL, ATMO, COOK и RANDOM"), ("13", "Звуки OVERWORLD"), ("14", "В компьютер"),
           ("15", "Шпаргалка"), ("16", "Если что-то не так")]
    story += section("СОДЕРЖАНИЕ", "Что внутри", "Читай по порядку, с синтом в руках: каждый урок — несколько шагов, "
                     "сделал шаг — увидел результат.")
    story.append(table([[c(n, "g"), t] for n, t in toc], [14, CW - 14], key_col=False, pad=5))
    story.append(Spacer(1, 8 * mm))
    story.append(card([Paragraph(f'{c("Как читать", "y")}', S_H2),
                       Paragraph(f"Названия кнопок — как на панели FM-1: {k('PLAY')}, {k('HOME')}, {k('REC')}. "
                                 f"«Белая 5» — пятая белая клавиша слева, «чёрная 2» — вторая чёрная. "
                                 f"{k('HOME')}&nbsp;+&nbsp;{k('PLAY')} — держишь первую и нажимаешь вторую. "
                                 f"{c('K1', 'g')} {c('K2', 'b')} {c('K3', 'y')} {c('K4', 'o')} — четыре верхние ручки; "
                                 "на экране каждое значение отмечено точкой цвета своей ручки.", S_P)], CW, Y))
    story.append(PageBreak())

    # ---- 01 install
    story += section("01", "Установка", "Один двойной клик. Лучше ставить на второй, экспериментальный FM-1.")
    story.append(steps([
        ("1", "Подготовка", "Зарядить FM-1. Закрыть Reaper и всё, что работает с MIDI. Не дать компьютеру уснуть."),
        ("2", "Кабель", "FM-1 — к компьютеру напрямую, без хаба, кабелем с передачей данных."),
        ("3", "Запуск", f"Дважды кликнуть {k('install-deck.bat')} (рядом {k('deck-overworld.fwsc')} и {k('fm1_install.py')}). "
                        "Свежая сборка — GitHub → Releases → latest или папка deck-diag."),
        ("4", "Ждать", "Не трогать кабель, пока окно не скажет, что готово. Оборвалось — запустить ещё раз."),
        ("5", "Готово", f"FM-1 перезагрузится в сад, откроется последняя песня (сначала — демо FIRST LIGHT). Нажми {k('PLAY')}.")]))
    story.append(Spacer(1, 6 * mm))
    story.append(note("Откат на OVERWORLD", f"{k('fm1\\overworld\\install-overworld.bat')} — вернётся старая прошивка "
                      "(её петли будут пустыми: DECK хранит проекты там же). Чёрный экран — Pico и Transporter, "
                      f"{k('Desktop\\FM1Trasmitter-for-pico\\ВОССТАНОВЛЕНИЕ_FM-1.txt')}.", R))
    story.append(Spacer(1, 4 * mm))
    story.append(note("Если тормозит", f"{k('diag-deck.bat')} минуту слушает синт, пока ты играешь, и пишет "
                      f"{k('deck_diag.txt')}: нагрузку, пропуски звука, долгие кадры. Его можно прислать.", B))
    story.append(PageBreak())

    # ---- 02 screen and panel
    story += section("02", "Экран и панель", "На каждой странице одно и то же: шапка, большая картинка, четыре значения внизу.")
    story.append(two(shot("p02_sound_engine.png", 62), bullets([
        "Шапка: ▶ играет / ● пишет / ■ стоит, буква и цвет части (A–H), такт.доля, имя страницы или дорожки, "
        "темп и шесть точек-дорожек (текущая белая, звучащие цветные, заглушённая — кольцо).",
        "Когда играешь — в шапке на секунду видно ноты или имя аккорда.",
        f"Внизу четыре значения — их крутят {c('K1', 'g')} {c('K2', 'b')} {c('K3', 'y')} {c('K4', 'o')}. Кольцо — "
        "число, точка — выбор из списка. На странице звука вместо них — четыре нарисованные ручки.",
        "Под ними — подсказка: видна 3 с после смены страницы и пока держишь HOME."]), 62, CW - 62, 6))
    story.append(Spacer(1, 4 * mm))
    story.append(RLImage(encoders_png(), width=CW * mm, height=CW * mm * 330 / 1500))
    story.append(Paragraph(f"Ручки слева — помощники, одинаковые везде: {c('SELECT', 'g')} — дорожка, "
                           f"{c('PRESET', 'y')} — пресет дорожки, {c('ALGO', 'b')} — ещё один параметр страницы "
                           f"(значение всплывает на экране). {k('HOME')} + {c('SELECT', 'g')} — темп.", S_SM))
    story.append(PageBreak())
    story += section("02", "Кнопки", "Нажать — одно, держать — другое, с HOME — третье. Кнопка страницы горит, пока ты на ней.")
    btn = [[k("HOME"), "сад (главная)", "SHIFT", "белая 1–16 пресет · чёрная 1–6 дорожка · 7–11 клип"],
           [k("SEL"), "ATMO: дождь, костёр, птицы, море, ветер, сверчки, пыль, лента", "—", "—"],
           [k("EDIT"), "KEYS: тональность, аккорды · ещё раз — ряд", "UNDO", "REDO"],
           [k("REC"), "запись со счётом", "ERASE: 1 с часть, 2,5 с вся дорожка", "метроном вкл/выкл"],
           [k("PLAY"), "старт / стоп (сразу)", "PART ↔ SONG", "тапы: темп"],
           [k("ARP"), "арпеджиатор: вкл и его страница · ещё раз — ряд", "+ белая A–H: часть", "ARP выкл"],
           [k("SEQ"), "секвенсор дорожки · ещё раз — ряд", "+ белая 1–5: секвенсор", "BAKE: DREAM → STEP"],
           [k("FX"), "звук: ENG / ENV / FX / MOD", "—", "—"],
           [k("LFO"), "пульт MIX", "+ ручка на SOUND: модулятор", "—"],
           [k("ENV"), "песня-лента ROAD", "гараж частей", "—"],
           [k("SAVE"), "проекты · ещё раз — сохранить", "+ белая 13–16: свой пресет", "—"],
           [k("GLO"), "GLOBAL: темп, пыль, метроном, MIDI", "FILL — сбивка", "—"],
           [k("OCT− / OCT+"), "октава (кнопка горит, если сдвинута)", "—", "клип на ступень лада"],
           [k("SEQ + REC"), "стереть клип дорожки сразу", "", ""]]
    story.append(table(btn, [26, 62, 46, CW - 134], head=["КНОПКА", "НАЖАТЬ", "ДЕРЖАТЬ", "С HOME"]))
    story.append(PageBreak())

    # ---- 03 lesson 1
    story += lesson("03 · УРОК 1", "Играть", "Сад — главная страница. Здесь просто играешь.", G, "p01_play.png", [
        ("1", "Сад", f"{k('HOME')} — сад. Цветок растёт от нот всех дорожек, день сменяет ночь. {k('PLAY')} — играет песня."),
        ("2", "Дорожка", f"Ручка {c('SELECT', 'g')} — следующая/предыдущая: посреди экрана на секунду шесть кружков, "
                         f"выбранный крупно с именем. Или {k('HOME')} + чёрная 1–6 — сразу нужная."),
        ("3", "Шесть дорожек", "1 PIANO · 2 KEYS (электропиано, колокольчики, калимба) · 3 PAD · 4 BASS · 5 DRUMS · 6 NATURE."),
        ("4", "Пресет", f"Ручка {c('PRESET', 'y')} листает пресеты дорожки. {k('HOME')} + белая 1–12 — сразу нужный, "
                        "13–16 — твои."),
        ("5", "Звук прямо на саде", f"{c('K1', 'g')} MACRO — главная ручка пресета, {c('K2', 'b')} ревер, "
                                    f"{c('K3', 'y')} эхо, {c('K4', 'o')} громкость дорожки."),
        ("6", "Настроение", f"{c('ALGO', 'b')} — MOOD: луг, дождь, пещера, снег, океан, ночь, костёр, вечер. Меняет сад, "
                            "лад, темп и звуки разом.")],
        after=[two(shot("p20_trackbar.png", 56), [notew(CW - 62, "Октавы", f"{k('OCT−')} / {k('OCT+')} — октава дорожки. "
                   "Горит та кнопка, куда сдвинуто. Обе погасли — исходная октава.", O)], 56, CW - 56, 6)])

    # ---- 04 lesson 2
    story += lesson("04 · УРОК 2", "Тональность и аккорды", "Клавиатура не фальшивит: играет только ноты выбранного лада.",
                    Y, "p11_keys.png", [
        ("1", "Открыть", f"{k('EDIT')} — страница KEYS: крупно аккорд, под ним ступень и тональность."),
        ("2", "Тональность", f"{c('K1', 'g')} — тоника (C, D…), {c('K2', 'b')} — лад (MAJOR, MINOR, DORIAN…). "
                             "Выбранное видно внизу слева и крупно на экране."),
        ("3", "Ноты или аккорды", f"{c('K3', 'y')} WHITE: NOTES — белые играют ноты лада, как транспонированная "
                                  "клавиатура: клавиша До (слева от двух чёрных) — всегда тоника. Фа минор: До играет Фа, "
                                  "Ре — Соль, Ми — Ля♭. Играешь «как в до мажоре», звучит в своей тональности. CHORDS — "
                                  "каждая белая играет аккорд своей ступени. Подписи на клавишах подскажут."),
        ("4", "Какой аккорд", f"{c('K4', 'o')} — тип: от TRIAD до MAJ9, MIN11, QUARTAL. {c('ALGO', 'b')} — голосоведение."),
        ("5", "Дальше", f"{k('EDIT')} ещё раз — ряд 2: что делают чёрные (CHORDS / PERFORM / NOTES), качество, "
                        "голосоведение, STRUM. Ряд 3: SPLIT, бас, дорожка гармонии, октава. Ряд 4: SCALE / CHROMA.")],
        after=[kb("kb_play.png", ["I", "II", "III", "IV", "V", "VI", "VII", "I'", "II'", "III'", "IV'", "V'", "VI'",
                                  "VII'", "I''", "II''"],
                  ["I", "II", "III", "IV", "V", "VI", "VII", "iv", "bVII", "II", "IV/I"],
                  "белые: аккорды ступеней", "чёрные: ступени + 4 «щемящих»")])

    # ---- 04b keys explained
    story += section("04 · УРОК 2", "Что делают чёрные, SPLIT, BASS, HARMONY", "Ряды 2–4 страницы KEYS (EDIT ещё раз).", Y)
    story.append(table([
        ["BLACK = CHORDS", "Чёрные играют аккорды: 1–7 — ступени лада (I…VII), 8–11 — четыре «щемящих» заимствованных "
                           "аккорда кассеты (в мажоре iv, bVII, II, IV над тоникой). Белые при этом — мелодия или тоже аккорды."],
        ["BLACK = PERFORM", "Чёрные — эффекты на всю песню, пока держишь: TAPESTOP, REWIND, VINYL STOP, LOW PASS, RAIN UP, "
                            "DREAM, HALF TIME, ECHO THROW, MUTE DRUMS, SLEEP, REVERSE. Пока держишь — имя эффекта горит "
                            "оранжевым в шапке."],
        ["BLACK = NOTES", "Чёрные играют свои ноты — диезы и бемоли, как на обычном пианино. Для мелодий вне лада."],
        ["QUALITY", "DIATONIC — аккорды из лада (в до мажоре на Ре — Dm7); FIXED — все одного типа (параллельные, как в чиллвейве)."],
        ["VOICING · STRUM", "Как разложен аккорд (CLOSE, OPEN, SPREAD, DROP2, POWER) и насколько «размазан», как у пианиста (0–120 мс)."],
        ["LEAD", "Аккорды сами ищут ближайшее обращение к прошлому — переходы плавные, рука не прыгает."],
        ["SPLIT", "Аккорды уходят на дорожку гармонии (HARMONY), а одиночные ноты — на текущую. Пример: стоишь на KEYS, "
                  "чёрными играешь аккорды — они звучат пианино, белыми — мелодию колокольчиками. Две руки — два инструмента."],
        ["HARMONY", "Та самая дорожка гармонии для SPLIT (по умолчанию 1 — пианино)."],
        ["BASS", "К каждому аккорду добавляется его основа октавой ниже — аккорд звучит полнее."],
        ["OCTAVE · DIM SKIP", "Октава клавиатуры для всех дорожек; DIM SKIP — уменьшённый аккорд ступени VII заменяется на "
                              "более мягкий (в эмбиенте он режет слух)."],
        ["SCALE / CHROMA", "SCALE — белые всегда в ладу (не сфальшивить). CHROMA — обычная клавиатура, все 27 нот как есть."]],
        [36, CW - 36], pad=4))
    story.append(PageBreak())

    # ---- 05 lesson 3
    story += lesson("05 · УРОК 3", "Записать петлю", "Запись идёт по кругу: каждый проход — новый слой поверх.", R,
                    "p13_tape_count.png", [
        ("1", "Дорожка", f"Выбери, куда писать: ручка {c('SELECT', 'g')} или {k('HOME')} + чёрная. Секвенсор — LIVE (так по умолчанию)."),
        ("2", "REC", f"{k('REC')} — откроется кассета, метроном отсчитает такт: на наклейке 4…3…2…1. Играй с первой доли."),
        ("3", "Слои", "Петля крутится и дописывается. Ноты нового прохода — красные, головка мигает на каждой ноте."),
        ("4", "Стоп", f"{k('REC')} ещё раз. Кассета остаётся на экране: {c('K1', 'g')} квантование, {c('K2', 'b')} какая "
                      f"дорожка пишется, {c('K3', 'y')} громкость метронома, {c('K4', 'o')} режим метронома."),
        ("5", "Отменить", f"Держи {k('EDIT')} — убрать последний проход. {k('HOME')} + {k('EDIT')} — вернуть."),
        ("6", "Длина петли", f"На кассете {c('K1', 'g')} BARS. AUTO (в пустой части): пишешь сколько хочешь, жмёшь "
                             f"{k('REC')} — петля закрывается на ближайшей тактовой черте, вся часть стала такой длины. "
                             f"Или поставь число заранее (1–16 тактов). {c('K4', 'o')} — метроном: OFF / ON REC / ALWAYS, "
                             "на наклейке CLICK ON / OFF.")],
        after=[note("Стереть: держи REC", f"Держи {k('REC')} — на экране окно ERASE с именем дорожки и двумя полосками. "
                    "Через 1 секунду стирается эта дорожка в текущей части, держишь дальше до 2,5 с — эта дорожка во "
                    "всех 8 частях. Отпустил раньше — ничего не стёрто. На странице гаража (держи ENV) так стирается "
                    f"вся часть (все 6 дорожек), на ленте ROAD — вся лента. Передумал — держи {k('EDIT')}: вернётся всё "
                    f"разом. Быстро без окна: {k('SEQ')} + {k('REC')}.", R),
               Spacer(1, 3 * mm),
               note("Метроном", f"Щёлкает при записи и счёте. {k('HOME')} + {k('REC')} — вкл/выкл. Звук, деление, "
                    "счёт в 2 такта, режим ALWAYS — на странице GLOBAL. В экспорт метроном не попадает.", Y)])

    # ---- 06 lesson 4
    story += lesson("06 · УРОК 4", "Арпеджиатор", "Как в обычных синтах: включил, зажал аккорд — он его перебирает.", B,
                    "p16_seq_arp.png", [
        ("1", "Включить", f"{k('ARP')} — арп включён (кнопка горит) и открыта его страница. На барабанах не работает."),
        ("2", "Играть", "Зажми аккорд. На экране ритм: точки — паузы, кружки — ноты на своей высоте, текущий шаг светится."),
        ("3", "Настроить", f"{c('K1', 'g')} MODE (UP, DOWN, UPDN, RAND, ORDER, CHORD), {c('K2', 'b')} RATE 1/4…1/32 "
                           f"с триолями, {c('K3', 'y')} RHYTHM (STRAIGHT, PULSE, OFFBEAT, DOTTED, GALLOP, RISE, SKIP), "
                           f"{c('K4', 'o')} LATCH — ноты держатся сами."),
        ("4", "Ряд 2", f"{k('ARP')} ещё раз: октавы, GATE (длина нот), PLAYS — KEYS (зажатые) или CHORD (аккорд песни: "
                       "клавиши можно не держать), вкл/выкл."),
        ("5", "Записать", f"Включи запись {k('REC')} — в клип ляжет то, что сыграл арп. {k('HOME')} + {k('ARP')} — выключить.")],
        after=[note("Скопировать арпеджио в сетку", f"Зажми или защёлкни (LATCH) аккорд и нажми {k('SEQ')} — арп "
                    "запишет то, что сыграл бы, нотами в клип и переключит дорожку на STEP. Дальше правишь по сетке, "
                    f"ничего не кладёшь руками. Не понравилось — {k('SEQ')} + {k('OCT−')} вернёт как было.", B)])

    # ---- 07 lesson 5
    story += section("07 · УРОК 5", "Секвенсоры", "У каждой дорожки свой способ играть клип. Держи SEQ и нажми белую 1–5.", Y)
    story.append(table([
        ["1 STEP", "Шаги. Барабаны: белые — 16 шагов, чёрные — инструменты кита. K1 инструмент, K2 сила, K3 вероятность, "
                   "K4 рэтчет, ALGO — страница шагов. Держи шаг и крути K1 — у шага своё значение (p-lock). "
                   "Мелодия: K1 сдвиг всего клипа по шагам, K2 длина нот с клавиш, K3 сила, K4 вероятность. "
                   "Писать ноты — в шаговом редакторе (ниже)."],
        ["2 LIVE", "Петля из того, что сыграл с REC. K1 SHIFT — сдвиг петли по шагам, K2 MOVE — вся петля на ступень лада вверх/вниз, K3 громкость, K4 ревер. Октава — кнопки OCT−/OCT+."],
        ["3 DREAM", "Автопилот: нажми PLAY — дорожка играет сама под аккорды песни. K1 энергия (сколько нот), K2 стиль "
                    "(C418 — редко и воздушно, LOFI — бит и аккорды, AMBIENT, CAMPFIRE — гитарная пульсация), K3 фантазия, "
                    "K4 громкость. Сыграл фразу — помолчит и ответит. Раз в 8 тактов дышит (пауза). HOME+SEQ — «запечь» в "
                    "STEP и править руками. В PERFORM 6-я чёрная глушит все DREAM-дорожки и включает обратно."],
        ["4 FINGER", "Каждая белая — готовый рисунок на такт (на экране он нарисован). Держишь — играет, две — складываются. "
                     "Чёрные — вариации. K1 LATCH — держится сам, K2 громкость, K3 ревер, K4 эхо."],
        ["5 TOMBOLA", "Ноты — шарики в шестиугольнике, звучат от ударов о стенки. K1 вращение, K2 гравитация, K3 отскок, "
                      "K4 GRID — привязка к сетке. Держи REC — вытряхнуть."]], [26, CW - 26], pad=5))
    story.append(Spacer(1, 4 * mm))
    story.append(note("Такты (BARS) — одна длина на часть", "1 такт = 16 шагов = один экран сетки. Часть длиной "
                      "4 такта — 4 экрана (сверху BAR 1/4, листать HOME + K1). Длина одна для всей части и всех её "
                      "дорожек: ставится в SEQ (ряд 2, K2), в гараже или на кассете — везде это одно и то же число. "
                      "ARP → SEQ заполняет ровно эту длину. Нужна часть длиннее 16 тактов — пиши следующую часть и "
                      "собери их на ленте ROAD.", Y))
    story.append(Spacer(1, 3 * mm))
    story.append(Paragraph(f"{k('SEQ')} ещё раз — ряд 2, общий для всех секвенсоров: тип, длина части в тактах, свинг, квантование записи. "
                           f"{k('SEQ')} + {k('REC')} — стереть клип.", S_P))
    story.append(Spacer(1, 2 * mm))
    story.append(note("Барабаны — каждый звук отдельно", f"На барабанах у SEQ три ряда: шаги · KIT · настройки. В ряду KIT "
                      f"{c('K1', 'g')} выбирает звук (KICK, SNARE…), {c('K2', 'b')} — его громкость, {c('K3', 'y')} — "
                      f"строй, {c('K4', 'o')} — длина, {c('ALGO', 'b')} — панорама. На экране у каждого звука полоска громкости.", Y))
    story.append(Spacer(1, 3 * mm))
    story.append(two(shot("p07_seq_step_drums.png", 54), shot("p18_seq_drive.png", 54), 54, CW - 54, 4))
    story.append(PageBreak())

    # ---- 07b the step editor
    story += lesson("07 · УРОК 5 · ЧАСТЬ 2", "Шаговый редактор", "Ноты по сетке ручками, как в грувбоксе. Для мелодий на STEP и LIVE.", G,
                    "p26_step_edit.png", [
        ("1", "Открыть", f"Страница SEQ, {k('SEQ')} ещё два раза — третий ряд STEP EDIT. Сверху — номер такта, "
                         "в окне 16 шагов текущего такта, ноты — полоски на своей высоте, шаг под курсором подсвечен."),
        ("2", "Ходить", f"{c('K1', 'g')} STEP — по шагам (1.1 … 1.16, 2.1 …). {k('HOME')} + {c('K1', 'g')} — по тактам."),
        ("3", "Нота", f"{c('K2', 'b')} вправо — поставить ноту на шаг (пустая рамка показывает какую). Ещё вправо — "
                      "к ней терция по ладу: растёт аккорд. Влево — убрать верхнюю ноту."),
        ("4", "Длина", f"{c('K3', 'y')} вправо — протянуть: последняя нота тянется до шага под курсором, дальше — "
                       "по шагу. Влево — разрыв: нота, звучащая сюда, обрывается на этом шаге; начатая здесь — короче."),
        ("5", "Высота", f"{c('K4', 'o')} — нота на шаге вверх/вниз по ладу (мимо лада не попадёшь), "
                        f"{k('HOME')} + {c('K4', 'o')} — на октаву. На пустом шаге K4 выбирает, какую ноту поставит K2."),
        ("6", "Клавишами", "Нажатая клавиша пишет свою ноту (или аккорд) на шаг под курсором, курсор шагает дальше. "
                           f"Та же ещё раз — стирает. Ошибся — держи {k('EDIT')}: отмена (серия поворотов одной ручкой — один шаг).")],
        after=[note("Как набить мелодию", f"{c('K2', 'b')}→ нота · {c('K1', 'g')}→ на 4 шага · {c('K3', 'y')}→ нота протянулась · "
                    f"{c('K4', 'o')}→ выше · {c('K2', 'b')}→ следующая. Нажми {k('PLAY')} — слушаешь, пока правишь.", G)])

    # ---- 08 lesson 6
    story += lesson("08 · УРОК 6", "Звук", "Модуль синта: в окошке пресет, ниже четыре ручки — это K1–K4.", B,
                    "p02_sound_engine.png", [
        ("1", "Открыть", f"{k('FX')} — модуль текущей дорожки. Сверху имя движка (FELT, AIRPAD…), в окошке — пресет."),
        ("2", "ENG", f"Вкладка движка: K1–K4 — его главные ручки (у пианино TONE, FELT, DECAY, KNOCK), "
                     f"{c('ALGO', 'b')} — пятая."),
        ("3", "ENV · FX · MOD", f"{k('FX')} ещё раз листает: ENV — атака, затухание, отпускание, громкость; FX — эффект "
                                "(VINYL, TAPE, LOFI, CHORUS, FILTER, TREMOLO, SPRING) и его ручки; MOD — модулятор."),
        ("4", "Быстрый модулятор", f"Держи {k('LFO')} и крути ручку на вкладке ENG — модуляция садится прямо на неё."),
        ("5", "Сохранить свой", f"Держи {k('SAVE')} + белая 13–16 — пресет в свой слот. Позвать: {k('HOME')} + белая 13–16."),
        ("6", "Другой инструмент", f"{k('HOME')} + ручка PRESET — сменить сам инструмент дорожки: FELT, TINE, AIRPAD, "
                                   "GLASS, SOFTBASS, KALIMBA, WINDS. У барабанов и природы — свои, не меняются.")],
        after=[note("WINDS — флейты и медь", "Новый инструмент: FLUTE, PAN FLUTE, OCARINA, SHAKUHACHI, CLARINET, TRUMPET, "
                    "MUTED (труба с сурдиной), HORN, BRASS (секция), FLUGEL, MORNING, OLD BAND. Ручки: BRIGHT — яркость, "
                    "BREATH — сколько воздуха, VIBRATO — вибрато (вступает не сразу, как у живого духовика), TYPE, SWELL — "
                    "как медленно раздувается нота. Играй одноголосно и держи ноты — так звучит естественнее.", B),
               Spacer(1, 3 * mm),
               note("Барабаны: эффект на весь кит или на один звук", f"На вкладке FX у барабанов {c('K1', 'g')} FOR: "
                    "WHOLE KIT — эффект на всю дорожку (как у всех); KICK, SNARE… — только этот звук: "
                    f"{c('K2', 'b')} свой ревер, {c('K3', 'y')} своё эхо, {c('K4', 'o')} CRUSH (8 бит: грубее и ниже "
                    f"частота), {c('ALGO', 'b')} — громкость. Кик посуше, снейр в ревер, хет восьмибитный — легко.", B),
               Spacer(1, 3 * mm),
               note("Запись движений", "Крутишь ручку на этой странице во время записи — движение пишется в клип и "
                    "повторяется каждый круг.", B)])

    # ---- 09 lesson 7
    story += lesson("09 · УРОК 7", "Свести", "Пульт: у каждой дорожки панорама, фейдер, огоньки уровня.", G,
                    "p08_mix.png", [
        ("1", "Открыть", f"{k('LFO')} — пульт. Сверху имя выбранной дорожки, её полоска внизу светлее."),
        ("2", "Ручки", f"{c('K1', 'g')} громкость, {c('K2', 'b')} панорама, {c('K3', 'y')} ревер, {c('K4', 'o')} эхо. "
                       f"{c('ALGO', 'b')} — пространство: ROOM, HALL, CAVE, SHIMMER."),
        ("3", "Мьют и соло", "Белые 1–6 — заглушить дорожку (полоска станет серой с M), чёрные 1–6 — соло (S)."),
        ("4", "Стереть", "Держи белую 1–6 секунду — стирается эта дорожка в текущей части.")])

    # ---- 10 lesson 8
    story += lesson("10 · УРОК 8", "Песня из частей", "Восемь частей A–H, из них на ленте собирается песня.", B,
                    "p09_road.png", [
        ("1", "Части", f"У каждой дорожки в каждой части свой клип. Держи {k('ARP')} + белая A–H — перейти к части "
                       "(сменится с конца такта)."),
        ("2", "Лента", f"{k('ENV')} — песня-лента: блоки частей — цветные куски на плёнке, едут под головкой; "
                       "внизу вся песня одной полоской."),
        ("3", "Собрать", "Белые A–H — добавить блок. K1 какой блок, K2 его часть, K3 повторы, K4 PART / SONG."),
        ("4", "Трюки", "Чёрные: HALF, DOUBLE, REVERSE, +12, −12, TAPESTOP, DROP (заглушить дорожку на блоке), "
                       "удалить, дублировать, вырезать, вставить. Трюки — жёлтые точки под блоком."),
        ("5", "Играть песню", f"Держи {k('PLAY')} — SONG: едем по ленте с выбранного блока (K1). PART — крутится часть "
                              "выбранного блока. "
                              f"Держи {k('ENV')} — гараж: имя (INTRO, CHORUS…) и длина каждой части.")],
        after=[note("Запись ленты вживую", f"В режиме PART на ленте нажми {k('REC')} и переключай части — они сами "
                    "лягут в песню.", B)])

    # ---- 11 lesson 9
    story += lesson("11 · УРОК 9", "Проекты и сохранение", "Четыре проекта — четыре кассеты на полке.", G,
                    "p12_project.png", [
        ("1", "Открыть", f"{k('SAVE')} — полка. Открытая кассета выдвинута, её бобины крутятся, пока играет."),
        ("2", "Сохранить", f"{k('SAVE')} ещё раз — сохранено сейчас («SAVED»). Жёлтая точка в углу — есть несохранённое."),
        ("3", "Само", "Сохраняется само раз в полторы минуты, когда синт стоит и ты не играешь. При выключении FM-1 сохранить не успеет — "
                      "перед выключением нажми SAVE дважды."),
        ("4", "Другой проект", "Белые 1–4 — открыть. Белая 5 — новый по шаблону (K2: EMPTY, POP, CLUB, AMBIENT), "
                               "белая 6 — демо. K3 — стиль автопилота."),
        ("5", "Наружу", "Чёрная 1 — EXPORT в .mid, чёрная 2 — STEMS (раздел 14).")])

    # ---- 12 global
    story += lesson("12", "GLOBAL", "Всё, что касается всей песни сразу.", Y, "p19_global.png", [
        ("1", "Открыть", f"{k('GLO')} — деревянный метроном качается в темп, рядом темп крупно."),
        ("2", "Ряд 1", "K1 темп, K2 свинг, K3 DUST (пыль кассеты), K4 SLEEP (всё темнее и дольше — для концовки). "
                       "ALGO — SIDECHAIN (пад приседает от бочки)."),
        ("3", "Ряд 2", f"{k('GLO')} ещё раз — метроном: режим (OFF / REC / ALWAYS), счёт, звук, громкость; ALGO — деление."),
        ("4", "Ряды 3–4", "MIDI OUT, CLOCK, MIDI IN · эхо, загрузка CPU, версия."),
        ("5", "FILL", f"Держи {k('GLO')} — сбивка всех дорожек, пока держишь.")])

    # ---- 12b ATMO
    story += lesson("12", "ATMO — атмосфера", "Место под музыкой: включаешь и смешиваешь, что нужно.", G, "p23_atmo.png", [
        ("1", "Открыть", f"{k('SEL')} — восемь плиток: дождь, костёр, птицы, море · ветер, сверчки, пыль, лента."),
        ("2", "Ряд 1", f"{c('K1', 'g')} RAIN, {c('K2', 'b')} FIRE, {c('K3', 'y')} BIRDS, {c('K4', 'o')} OCEAN. "
                       "Плитка оживает, когда слой звучит; полоска — сколько его."),
        ("3", "Ряд 2", f"{k('SEL')} ещё раз: WIND, CRICKETS, DUST (треск и шип винила, всё чуть мутнее), "
                       "TAPE — лента: плывёт и дрожит высота, тёплый низ, мягкая перегрузка, сжатие, шип."),
        ("4", "Общая громкость", f"{c('ALGO', 'b')} — AIR LEVEL: громкость всех слоёв сразу."),
        ("5", "С песней", "Атмосфера сохраняется в проекте и в экспорт не мешает: это звук синта, а не дорожки.")])

    # ---- 12c clip tools
    story += section("12", "COOK и RANDOM", "Держи SEQ — и кнопки становятся инструментами для клипа.", O)
    story.append(steps([
        ("1", "COOK", f"Держи {k('SEQ')} + чёрная 1 — клип чуть меняется в ладу: пара нот на ступень выше или ниже, "
                      "дышит громкость, на барабанах — сдвиг и призрачные удары. Ещё раз — ещё чуть-чуть."),
        ("2", "Пример", f"Скопировал пианино из части A в B ({k('HOME')} + чёрные 7 и 8), в B нажал COOK пару раз — "
                        "та же мелодия, но живее, не копия."),
        ("3", "RANDOM", f"Держи {k('SEQ')} + чёрная 2 — тот же ритм, новые ноты лада (аккорды двигаются целиком). "
                        "На барабанах — столько же ударов, разбросаны заново."),
        ("4", "Назад и вперёд", f"Держи {k('SEQ')} + {k('OCT−')} — шаг назад, {k('OCT+')} — вперёд. Можно вернуться к "
                                "самому первому варианту.")], O))
    story.append(PageBreak())

    # ---- 13 sounds
    story += section("13", "Звуки OVERWORLD", "Восемь движков по 12 пресетов, 4 своих слота на каждый.")
    eng = [["FELT", "пианино с войлоком — главный инструмент", "TONE · FELT · DECAY · KNOCK"],
           ["TINE", "лоу-фай электропиано с тремоло", "TINE · TREMOLO · DRIFT · BARK"],
           ["AIRPAD", "тёплые пады: SWEDEN, AIR, WARM, SHIMMER, STRINGS", "BRIGHT · SWELL · AIR · TYPE"],
           ["GLASS", "колокольчики: стекло, челеста, маримба, шкатулка", "TYPE · DECAY · SHINE · OCTAVE"],
           ["KALIMBA", "язычок, щипок, деревянный корпус", "TONE · DECAY · WOOD"],
           ["SOFTBASS", "мягкий бас: SUB, UPRIGHT, MOOG, с глайдом", "TONE · PLUCK · GLIDE · TYPE"],
           ["DUSTKIT", "лоу-фай барабаны", "TUNE · DECAY · DUST · KIT"],
           ["WORLD", "дождь, ветер, костёр, ручей, птицы, океан, гроза", "TYPE · AMOUNT · TONE"]]
    story.append(table(eng, [24, 86, CW - 110], head=["ДВИЖОК", "ЧТО ЭТО", "РУЧКИ K1–K4"], pad=3))
    story.append(Spacer(1, 4 * mm))
    story.append(Paragraph("Дорожка KEYS листает 12 пресетов из трёх движков сразу: 4 электропиано (TINE), 4 колокольчика "
                           "(GLASS), 4 калимбы.", S_P))
    moods = [["PLAINS", "76", "мажор · C418", "луг, птицы"], ["RAIN", "72", "дорийский · LOFI", "дождь"],
             ["CAVE", "64", "минор · AMBIENT", "капли"], ["SNOW", "68", "лидийский · AMBIENT", "ветер, шаги"],
             ["OCEAN", "70", "миксолидийский · C418", "океан"], ["NIGHT", "60", "мажор · C418", "сверчки"],
             ["CAMPFIRE", "82", "пентатоника · CAMPFIRE", "костёр"], ["STUDY", "84", "дорийский · LOFI", "дождь, винил"]]
    story.append(Spacer(1, 3 * mm))
    story.append(table(moods, [30, 16, 62, CW - 108], head=["MOOD", "BPM", "ЛАД · СТИЛЬ", "МИР"], pad=2))
    story.append(PageBreak())

    # ---- 14 computer
    story += section("14", "В компьютер", "Reaper, MIDI и дорожки по отдельности.", G)
    story += bullets([
        f"EXPORT в .mid: открыть в Chrome web\\export.html из репозитория, выбрать FM-1, Listen. На синте {k('SAVE')}, "
        "чёрная 1. Или: python tools\\deck_export.py --port \"FM-1\" song.mid.",
        "В Reaper — шесть MIDI-дорожек с именами, маркеры частей, трюки ленты уже применены, движения ручек — CC74 и CC7.",
        "DREAM экспортируется после BAKE (HOME + SEQ).",
        "MIDI OUT: каждая дорожка на своём канале 1–6 + clock, старт, стоп. MIDI IN: каналы 1–6 (10 — барабаны, CC64 — педаль).",
        "STEMS (SAVE, чёрная 2): песня 7 раз — полный микс и каждая дорожка соло, между проходами щелчок. Пишешь выход одним "
        "дублем и режешь по щелчкам."])
    story.append(PageBreak())

    # ---- 15 cheat sheet
    story += section("15", "Шпаргалка", "Одна страница — всё под рукой.", Y)
    cheat = [[k("HOME"), "сад · держать: SHIFT"], [k("HOME") + " + белая", "пресет (13–16 — свои)"],
             [k("HOME") + " + чёрная 1–6", "дорожка"], [k("HOME") + " + чёрная 7–11", "клип: копир. · вставить · удвоить · вариация · гуманизация"],
             [k("SEL"), "ATMO: слои атмосферы, пыль, лента · ещё раз: ряд"],
             [k("SEQ") + " + чёрная 1 / 2", "COOK (чуть иначе в ладу) / RANDOM"], [k("SEQ") + " + OCT− / OCT+", "шаг назад / вперёд (UNDO / REDO)"],
             [k("EDIT"), "KEYS (тональность, аккорды) · держать: UNDO · HOME+: REDO"],
             [k("REC"), "запись · держать: ERASE (1 с часть, 2,5 с вся дорожка) · HOME+: метроном"], [k("SEQ") + " + " + k("REC"), "стереть клип сразу"],
             [k("PLAY"), "старт/стоп · держать: PART/SONG · HOME+: тап-темп"],
             [k("ARP"), "арп вкл и страница · ещё раз: ряд · HOME+: выкл · держать + белая: часть"],
             [k("SEQ"), "секвенсор · держать + белая 1–5: тип · HOME+: BAKE"], [k("FX"), "звук · вкладки"],
             [k("LFO"), "пульт · держать + ручка: модулятор"], [k("ENV"), "песня-лента · держать: гараж"],
             [k("SAVE"), "проекты · ещё раз: сохранить · держать + белая 13–16: свой пресет"],
             [k("GLO"), "GLOBAL · держать: FILL"], [k("OCT− / +"), "октава · HOME+: клип на ступень"],
             ["K1–K4", "четыре поля страницы (сад: MACRO · REVERB · ECHO · VOLUME)"],
             ["SELECT · ALGO · PRESET", "дорожка · ещё параметр (сад: MOOD) · пресет; HOME + SELECT — темп"],
             ["MIX: держать белую", "стереть дорожку в части"]]
    story.append(table(cheat, [54, CW - 54], pad=4))
    story.append(PageBreak())

    # ---- 16 trouble
    story += section("16", "Если что-то не так", lead=None, col=R)
    tr = [["Нет звука", "Покрути MASTER. Проверь мьют и соло на пульте (LFO)."],
          ["Клавиши играют не то", f"На ленте, полке, пульте и в секвенсоре клавиши управляют. {k('HOME')} — в сад, там просто играют."],
          ["Меняется пресет", f"Зажата {k('HOME')}. Отпусти её перед игрой."],
          ["Не понимаю, где я", f"Держи {k('HOME')} — внизу подсказка страницы. Горящая кнопка — твоя страница."],
          ["Синт моргнул", "Это автосохранение (раз в 1,5 минуты, в тишине): звук на миг плавно уходит в ноль."],
          ["Звук прерывается", "При перегрузе синт гасит тихие хвосты. Меньше длинных пэдов одновременно. "
                               "Загрузку видно на GLOBAL (ряд 4) и в diag-deck.bat."],
          ["Не сохранилось", "Само — раз в 1,5 минуты в тишине. Перед выключением — SAVE, SAVE."],
          ["Экспорт не приходит", "Закрой другие MIDI-программы. Запасной путь — MIDI OUT вживую в Reaper."]]
    story.append(table(tr, [48, CW - 48], pad=5))
    story.append(Spacer(1, 12 * mm))
    story.append(Paragraph("Сделано на fm1-base и felucca-bsp (Felucca — Leo Kuroshita, Hügelton Instruments). Ревер, "
                           "голосоведение и звук OVERWORLD — по мотивам AMB-1 (Charles Vestal). Лицензия MPL-2.0.", S_SM))

    doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=M, rightMargin=M, topMargin=16 * mm, bottomMargin=18 * mm,
                            title="DECK · OVERWORLD — инструкция", author="DECK")
    doc.build(story, onFirstPage=page_bg, onLaterPages=page_bg)
    print(OUT)


if __name__ == "__main__":
    build()
