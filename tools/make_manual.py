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
HEX = {"g": "#7ccc60", "b": "#6ebaf4", "y": "#f6d65c", "r": "#e84840", "p": "#f4aac8", "d": "#8a8a8a", "f": "#ece8dc"}

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
        for i, col in enumerate((G, B, Y)):
            canv.setFillColor(col)
            canv.circle(W / 2 - 8 + i * 8, 11.3 * mm, 1.6, fill=1, stroke=0)
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
    items = [((124, 204, 96), "K1", "поле 1 · на PLAY — DUST"), ((110, 186, 244), "K2", "поле 2 · SIDECHAIN"),
             ((246, 214, 92), "K3", "поле 3 · SLEEP"), ((236, 232, 220), "K4", "поле 4 · SWING")]
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

    # ---- contents
    toc = [("01", "Установка и откат"), ("02", "Панель: кнопки, ручки, клавиши"), ("03", "Первые пять минут"),
           ("04", "Страницы: PLAY · SOUND · SEQ · MIX · ROAD · KEYS · PROJECT · REC"), ("05", "Восемь секвенсоров"),
           ("06", "Клавиатура, которая не фальшивит"), ("07", "Звуки кассеты OVERWORLD"), ("08", "Запись, метроном, отмена"),
           ("09", "Песня целиком: части, трасса, трюки"), ("10", "В компьютер: Reaper, MIDI, STEMS"),
           ("11", "Шпаргалка"), ("12", "Если что-то не так")]
    story += section("СОДЕРЖАНИЕ", "Что внутри")
    story.append(table([[c(n, "g"), t] for n, t in toc], [14, CW - 14], key_col=False, pad=6))
    story.append(Spacer(1, 12 * mm))
    story.append(card([Paragraph(f'{c("Как читать", "y")}', S_H2),
                       Paragraph(f"Названия кнопок — как на панели FM-1: {k('PLAY')}, {k('HOME')}, {k('REC')}. "
                                 f"«Белая 5» — пятая белая клавиша слева, «чёрная 2» — вторая чёрная. "
                                 f"{k('HOME')}&nbsp;+&nbsp;{k('PLAY')} — держишь первую и нажимаешь вторую. "
                                 f"Цвета {c('зелёный', 'g')}, {c('голубой', 'b')}, {c('жёлтый', 'y')} — это три ручки: "
                                 "на экране значение всегда рисуется цветом той ручки, которая его крутит.", S_P)], CW, Y))
    story.append(PageBreak())

    # ---- 01 install
    story += section("01", "Установка", "Пять минут и один двойной клик. Лучше ставить на второй, экспериментальный FM-1.")
    steps = [("1", "Подготовка", "Зарядить FM-1 полностью. Закрыть Reaper и всё, что работает с MIDI. Не дать компьютеру уснуть."),
             ("2", "Кабель", "FM-1 — к компьютеру напрямую, без хаба, кабелем с передачей данных (не только зарядным)."),
             ("3", "Запуск", f"Скачать {k('deck-overworld.zip')} (GitHub → Releases → latest), распаковать и дважды кликнуть {k('install-deck.bat')}. "
                             "Он сам поставит нужное и запишет прошивку."),
             ("4", "Ждать", "Не трогать кабель, пока окно не скажет, что готово. Если связь оборвалась — FM-1 не выключать, "
                            "просто запустить установщик ещё раз."),
             ("5", "Готово", "FM-1 перезагрузится: цветок вишни и надпись OVERWORLD, потом сразу откроется демо-песня. "
                             f"Нажми {k('PLAY')}.")]
    rows = []
    for n, h, t in steps:
        rows.append([Paragraph(n, S_STEP), [Paragraph(h, S_H2), Paragraph(t, S_P)]])
    t = Table(rows, colWidths=[16 * mm, (CW - 16) * mm])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.4, LINE),
                           ("LEFTPADDING", (0, 0), (-1, -1), 0), ("TOPPADDING", (0, 0), (-1, -1), 6),
                           ("BOTTOMPADDING", (0, 0), (-1, -1), 8)]))
    story.append(t)
    story.append(Spacer(1, 8 * mm))
    story.append(card([Paragraph(c("Откат на OVERWORLD", "r"), S_H2),
                       Paragraph(f"Дважды кликнуть {k('fm1\\overworld\\install-overworld.bat')} — вернётся старая прошивка. "
                                 "Петли старого OVERWORLD при этом будут пустыми: DECK хранит проекты там же. "
                                 "Если экран совсем чёрный — восстановление через Pico и Transporter "
                                 f"({k('Desktop\\FM1Trasmitter-for-pico\\ВОССТАНОВЛЕНИЕ_FM-1.txt')}).", S_P),
                       Paragraph("Прошивка новая и на настоящем FM-1 ещё не запускалась. Если что-то тормозит или "
                                 "ведёт себя странно — это ожидаемо, напиши, что именно.", S_SM)], CW, R))
    story.append(PageBreak())

    # ---- 02 panel
    story += section("02", "Панель", "Одна кнопка — одно действие. Удержание — второе действие. С HOME — третье.")
    story.append(RLImage(encoders_png(), width=CW * mm, height=CW * mm * 330 / 1500))
    story.append(Spacer(1, 4))
    story.append(Paragraph(f"Как у OP-1: внизу каждой страницы четыре значения, и верхние ручки {k('K1')}–{k('K4')} "
                           "крутят именно их (цвет точки = ручка). Зашёл в звук — крутят звук, в микшер — громкости. "
                           f"Ручки слева — помощники: {c('SELECT', 'g')} — дорожка, {c('PRESET', 'y')} — пресет дорожки, "
                           f"{c('ALGO', 'b')} — ещё один параметр страницы (покажет на экране). На саде: SELECT — темп, "
                           "ALGO — MACRO, PRESET — MOOD; K1–K4 — DUST, SIDECHAIN, SLEEP, SWING.", S_SM))
    story.append(Spacer(1, 6))
    btn = [[k("FX"), "страница SOUND · ещё раз — следующая вкладка", "—", "—"],
           [k("SEQ"), "страница SEQ · ещё раз — следующий ряд", "+ белая 1–5: секвенсор", "BAKE: DRIVE → STEP"],
           [k("ENV"), "страница ROAD (трасса)", "гараж частей", "—"],
           [k("LFO"), "страница MIX · ещё раз — ряд MASTER", "+ ручка на SOUND: модулятор на неё", "—"],
           [k("PLAY"), "старт / стоп", "PART ↔ SONG", "тапы: TAP TEMPO"],
           [k("REC"), "запись (со счётом)", "1 с: очистить клип", "метроном вкл/выкл"],
           [k("SEL"), "следующая дорожка", "мьют дорожки", "—"],
           [k("ARP"), "арпеджио на дорожке · ещё раз — LATCH", "+ белая A–H: часть", "ARP выкл"],
           [k("GLO"), "FILL (сбивка всех дорожек), пока держишь", "—", "—"],
           [k("EDIT"), "страница KEYS · ещё раз — ряд", "UNDO", "REDO"],
           [k("SAVE"), "страница PROJECT · ещё раз — ряд", "+ белая 13–16: сохранить пресет", "—"],
           [k("HOME"), "страница PLAY (сад)", "SHIFT", "белая 1–16 пресет · чёрная 1–6 дорожка"],
           [k("OCT− / OCT+"), "октава дорожки", "—", "клип на ступень лада"]]
    story.append(table(btn, [26, 56, 52, CW - 134], head=["КНОПКА", "НАЖАТЬ", "ДЕРЖАТЬ", "С HOME"]))
    story.append(PageBreak())
    story += section("02", "Клавиши и HOME", "27 клавиш: 16 белых и 11 чёрных. С HOME они становятся пультом.")
    story.append(kb("kb_home.png", [f"{i + 1}\nпресет" for i in range(12)] + [f"{i + 13}\nсвой" for i in range(4)],
                    ["1", "2", "3", "4", "5", "6", "COPY", "PASTE", "DBL", "VAR", "HUM"],
                    "HOME + белая: пресет дорожки", "HOME + чёрная: дорожка · клип"))
    story.append(Spacer(1, 8))
    story += bullets([f"{k('HOME')} + белая 1–12 — заводские пресеты дорожки, 13–16 — твои. Сохранить свой: держи "
                      f"{k('SAVE')} и нажми белую 13–16.",
                      f"{k('HOME')} + чёрная 1–6 (или ручка SELECT) — дорожка: 1 пианино, 2 пэд, 3 клавиши (электропиано, колокольчики, калимба), 4 бас, 5 барабаны, 6 природа.",
                      f"{k('HOME')} + чёрная 7–11 — клип: копировать, вставить, удвоить, «варьировать» (чуть меняет ноты в ладу), "
                      "«очеловечить» (чуть сдвигает время и силу).",
                      f"Держи {k('ARP')} + белая 1–8 — перейти к части A–H. Держи {k('SEQ')} + белая 1–5 — секвенсор."])
    story.append(Spacer(1, 8))
    story.append(kb("kb_play.png", ["I", "II", "III", "IV", "V", "VI", "VII", "I'", "II'", "III'", "IV'", "V'", "VI'",
                                    "VII'", "I''", "II''"],
                    ["I", "II", "III", "IV", "V", "VI", "VII", "iv", "bVII", "II", "IV/I"],
                    "белые: аккорды ступеней лада", "чёрные: ступени + 4 «щемящих»"))
    story.append(Paragraph("Так клавиши звучат по умолчанию: белые и чёрные играют аккорды выбранного лада. Сфальшивить "
                           "невозможно. Переключается на странице KEYS.", S_CAP))
    story.append(PageBreak())

    # ---- 03 quick start
    story += section("03", "Первые пять минут", "От включения до своей первой петли.", Y)
    qs = [("1", "Послушать демо", f"Включи FM-1 — откроется песня FIRST LIGHT. {k('PLAY')} — играть, ещё раз — стоп. "
                                 "Цветок в саду растёт от каждой ноты."),
          ("2", "Поиграть аккорды", f"Дорожка 1 — пианино, она выбрана сразу. Нажимай белые клавиши: каждая — красивый "
                                    "аккорд тональности, мягко «размазанный», как у пианиста."),
          ("3", "Сменить звук", f"Ручка {c('PRESET', 'y')} (или {k('HOME')} + белая 1…12) — пресеты пианино. На саде та же ручка "
                                "меняет настроение (MOOD): дождь, снег, океан, костёр…"),
          ("4", "Записать петлю", f"{k('REC')}: метроном отсчитает такт (4-3-2-1 на кассете), дальше играй. "
                                  f"Петля крутится и дописывается проходами. {k('REC')} ещё раз — стоп. "
                                  f"Не понравился проход — держи {k('EDIT')}: он исчезнет."),
          ("5", "Включить автопилот", f"{k('HOME')} + чёрная 5 (барабаны), держи {k('SEQ')} + белая 3 — секвенсор DRIVE. "
                                      f"{k('PLAY')}: барабаны играют сами по стилю. Ручкой {c('K2', 'b')} на странице SEQ "
                                      "выбери стиль LOFI.")]
    rows = [[Paragraph(n, ParagraphStyle("st2", parent=S_STEP, textColor=Y)), [Paragraph(h, S_H2), Paragraph(t, S_P)]] for n, h, t in qs]
    t = Table(rows, colWidths=[16 * mm, (CW - 16) * mm])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.4, LINE),
                           ("LEFTPADDING", (0, 0), (-1, -1), 0), ("TOPPADDING", (0, 0), (-1, -1), 6),
                           ("BOTTOMPADDING", (0, 0), (-1, -1), 8)]))
    story.append(t)
    story.append(Spacer(1, 6 * mm))
    story.append(card([Paragraph("Всё сохраняется само — через несколько секунд тишины после изменений. "
                                 "Никаких «сохранить?».", S_P)], CW, G))
    story.append(PageBreak())

    # ---- 04 pages
    pages = [
        ("PLAY", "p01_play.png", G, f"{k('HOME')}",
         ["Сад кассеты: большой цветок растёт от нот всех шести дорожек, открывается лепестками, облетает в тишине.",
          "День и ночь сменяются сами; MOOD — это биом: луг, дождь, пещера, снег, океан, ночь, костёр, вечер.",
          "В шапке: игра / запись / стоп, часть (буква и цвет), такт.доля, название части, текущий аккорд, темп, шесть точек-дорожек.",
          f"Ручки: {c('TEMPO', 'g')} · {c('MACRO', 'b')} (главная ручка пресета) · {c('MOOD', 'y')}."]),
        ("SOUND", "p02_sound_engine.png", B, f"{k('FX')}",
         ["Четыре вкладки — ENGINE, ENV, FX, MOD. Повторный тап FX листает их.",
          "ENGINE: K1–K4 — четыре параметра движка, ALGO — пятый (тип, ширина…).",
          "ENV: атака, затухание, отпускание, громкость. FX: эффект, количество, A, B. MOD: тип, глубина, скорость, цель.",
          f"Быстрый модулятор: держи {k('LFO')} и крути ручку на ENGINE — модуляция садится прямо на неё."]),
        ("SEQ", "p07_seq_step_drums.png", Y, f"{k('SEQ')}",
         ["Вид зависит от секвенсора дорожки: сетка шагов, мини-ролл, DRIVE, FINGER, TOMBOLA; с ARP — сначала его ряды.",
          "Барабаны в STEP: белые клавиши — 16 шагов, чёрные — дорожки кита (бочка, щётка, шейкер…).",
          "Держишь шаг и крутишь ручку 1 — p-lock: этот шаг звучит со своим значением.",
          f"Следующий ряд ({k('SEQ')} ещё раз): секвенсор, длина клипа, свинг, квантование."]),
        ("MIX", "p08_mix.png", G, f"{k('LFO')}",
         ["Шесть фейдеров с индикаторами. Под каждым — номер дорожки в её цвете, имя выбранной — сверху. Выбрать: SEL или HOME + чёрная.",
          "Белые 1–6 — мьют, чёрные 1–6 — соло. Держи белую 1 секунду — стереть эту дорожку в части.",
          f"K1–K4: громкость, панорама, реверб, эхо (ALGO — тип пространства). Ряд MASTER ({k('LFO')} ещё раз): "
          "DUST, SIDECHAIN, SLEEP, SWING."]),
        ("ROAD", "p09_road.png", B, f"{k('ENV')}",
         ["Песня — ночная дорога: блоки частей уходят к горизонту, машинка едет по ним.",
          "Белые A–H — добавить блок части. Чёрные — трюки: HALF, DOUBLE, REVERSE, +12, −12, TAPESTOP, DROP, "
          "удалить, дублировать, вырезать, вставить.",
          f"Держи {k('ENV')} — гараж частей: метка (INTRO, CHORUS…) и длина каждой части."]),
        ("KEYS", "p11_keys.png", Y, f"{k('EDIT')}",
         ["Крупно — аккорд, который звучит, и его ступень в тональности.",
          f"Ряды полей ({k('EDIT')} листает): тоника/лад/тип аккорда · белые/чёрные/качество · голосоведение/страм/LEAD · "
          "раскладка/SPLIT/бас · октава/DIM SKIP/дорожка гармонии.",
          "Тонику проще всего выбрать так: повернул ручку ROOT — и нажми клавишу нужной ноты."]),
        ("PROJECT", "p12_project.png", G, f"{k('SAVE')}",
         ["Четыре проекта. Белые 1–4 — открыть, белая 5 — новый по шаблону (EMPTY, POP, CLUB, AMBIENT), белая 6 — демо.",
          "Чёрная 1 — EXPORT в .mid, чёрная 2 — STEMS, чёрная 3 — сохранить сейчас.",
          "Ряды: проект/шаблон/темп/стиль · метроном (режим, счёт, звук, деление; ALGO — громкость) · MIDI · эхо/CPU/версия."]),
        ("REC", "p14_tape_rec.png", R, f"{k('REC')}",
         ["Появляется сам, когда пишешь. Кассета: бобины крутятся в темпе, лента перематывается по позиции.",
          "Головка горит красным и мигает на каждой записанной ноте; ноты нового прохода — красные.",
          f"{k('REC')} — новый проход поверх, держи {k('EDIT')} — отменить проход. Ручки: квантование, какая дорожка "
          "пишется, громкость метронома."]),
    ]
    first = True
    for i in range(0, len(pages), 2):
        if first:
            story += section("04", "Страницы", "Восемь страниц. На каждой — одна большая вещь и три значения внизу.")
            first = False
        for name, png, col, btn_, items in pages[i:i + 2]:
            right = [Paragraph(f'{name}&nbsp;&nbsp;<font name="Mono" size="9" color="#8a8a8a">кнопка</font> {btn_}',
                               ParagraphStyle("pn", parent=S_H2, fontName="Light", fontSize=22, leading=26, spaceBefore=0))]
            right += bullets(items, {G: "g", B: "b", Y: "y", R: "r"}[col])
            story.append(KeepTogether([two(shot(png, 76), right, 76, CW - 76, 7), Spacer(1, 9 * mm)]))
        story.append(PageBreak())

    # ---- 05 sequencers
    story += section("05", "Пять секвенсоров и ARP", "У каждой дорожки свой. Держи SEQ и нажми белую 1–5. ARP — поверх любого.")
    seqs = [["1 STEP", "Шаги шестнадцатыми. Курсор ручкой, клавиша ставит ноту или аккорд; та же нота ещё раз — стирает. "
                       "У шага: сила, вероятность, рэтчет, p-lock."],
            ["2 LIVE", "Играешь — пишется петля, с квантованием 0–100 %. Каждый проход отменяется отдельно."],
            ["3 DRIVE", "Автопилот DREAM: играет сам по стилю (C418, LOFI, AMBIENT, CAMPFIRE) и по текущей гармонии. "
                        "ENERGY — плотность, VARIATION — фантазия. Сыграл фразу — он ответит. HOME+SEQ — «запечь» в STEP."],
            ["4 FINGER", "Каждая белая — готовый рисунок на такт (грув или фигура). Держишь — играет, две — складываются. "
                         "Чёрные — вариации: HALF, DOUBLE, ACCENT, GHOST, сдвиг."],
            ["5 TOMBOLA", "В шестиугольнике падают шарики-ноты и звучат от ударов о стенки. SPIN, GRAVITY, BOUNCE. "
                          "Держи REC — вытряхнуть. Бесконечная, никогда не повторяющаяся мелодия."],
            ["ARP", "Не секвенсор, а переключатель поверх игры, как в обычных синтах. Кнопка ARP: тап — включить, "
                    "ещё тап — LATCH (ноты держатся сами), HOME+ARP — выключить. Зажал аккорд — играет; нажал REC — "
                    "пишется то, что сыграл арп. Ряды: MODE (UP, DOWN, UPDN, RAND, ORDER, CHORD), RATE 1/4…1/32 с "
                    "триолями, OCTAVES, RHYTHM (STRAIGHT, PULSE, OFFBEAT, DOTTED, GALLOP, RISE, SKIP); GATE, PLAYS "
                    "(KEYS — зажатые ноты, CHORD — аккорд песни), LATCH."]]
    story.append(table(seqs, [30, CW - 30], pad=6))
    story.append(Spacer(1, 6 * mm))
    story.append(two(shot("p16_seq_arp.png", 78), shot("p15_seq_tombola.png", 78), 78, 86, 8))
    story.append(Paragraph("ARP (ритм и ноты на своей высоте) и TOMBOLA на странице SEQ", S_CAP))
    story.append(PageBreak())

    # ---- 06 keyboard
    story += section("06", "Клавиатура, которая не фальшивит", "Узкие клавиши FM-1 неудобны — поэтому клавиатура умная.")
    story += bullets([f"{c('SCALE', 'g')} (по умолчанию): белые играют только ноты выбранного лада подряд. "
                      f"{c('CHROMA', 'd')} — обычная клавиатура.",
                      f"{c('WHITE = CHORDS', 'g')}: каждая белая — аккорд своей ступени. В режиме DIATONIC качество берётся из лада "
                      "(в до мажоре на ре — Dm7), в FIXED все аккорды одного типа (параллельные, как в чиллвейве).",
                      f"{c('BLACK', 'b')}: CHORDS — семь ступеней и четыре «щемящих» заимствованных аккорда кассеты "
                      "(в мажоре iv, bVII, II, IV над тоникой); PERFORM — эффекты; OCTAVE — октавы и FILL.",
                      "23 типа аккордов: от TRIAD и SUS2 до MAJ9, MIN11, 13, QUARTAL и CLUSTER. "
                      "Голосоведение: CLOSE, OPEN, SPREAD, DROP2, POWER; LEAD — аккорды сами ищут ближайшее обращение.",
                      "STRUM 0–120 мс — аккорд «размазывается», как у пианиста. BASS — корень октавой ниже.",
                      f"{c('SPLIT', 'y')}: аккорды уходят на дорожку гармонии (пианино), мелодия — на текущую. Обе руки сразу.",
                      "Лады: MAJOR, MINOR, DORIAN, PHRYGIAN, LYDIAN, MIXOLYD, HARM MIN, MEL MIN, PENTA MAJ, PENTA MIN, BLUES."])
    story.append(Spacer(1, 6))
    story.append(kb("kb_perform.png", [], ["TAPE\nSTOP", "RE-\nWIND", "VINYL\nSTOP", "LOW\nPASS", "RAIN\nUP", "DREAM",
                                           "HALF\nTIME", "ECHO\nTHROW", "MUTE\nDRUMS", "SLEEP", "REV"],
                    "", "BLACK = PERFORM: эффекты, пока держишь"))
    story.append(PageBreak())

    # ---- 07 sounds
    story += section("07", "Звуки OVERWORLD", "Восемь движков по 12 пресетов. Послушать всё: out\\listen.html.")
    eng = [["FELT", "пианино с войлоком — главный инструмент: мягкая атака, срез верха, слышный стук клавиши",
            "TONE · FELT · DECAY", "MEMORY"],
           ["TINE", "лоу-фай электропиано с тремоло и «плаванием»", "TINE · TREMOLO · DRIFT", "DUSTY"],
           ["AIRPAD", "тёплые пады: SWEDEN, AIR, WARM, SHIMMER, STRINGS", "BRIGHT · SWELL · AIR", "SKY"],
           ["GLASS", "колокольчики: стекло, челеста, маримба, шкатулка, вибрафон", "TYPE · DECAY · SHINE", "SNOW"],
           ["KALIMBA", "язычок, щипок пальцем, деревянный корпус", "TONE · DECAY · WOOD", "CAMPFIRE"],
           ["SOFTBASS", "мягкий бас: SUB, UPRIGHT, MOOG, с глайдом", "TONE · PLUCK · GLIDE", "DEEP"],
           ["DUSTKIT", "лоу-фай барабаны: бочка, щётка, рим, шейкер, хет, клэп, дерево", "TUNE · DECAY · DUST", "TAPE"],
           ["WORLD", "дождь, ветер, костёр, ручей, капли, сверчки, птицы, океан, гроза, снег", "TYPE · AMOUNT · TONE",
            "WEATHER"]]
    story.append(table(eng, [24, 74, 46, CW - 144], head=["ДВИЖОК", "ЧТО ЭТО", "РУЧКИ", "MACRO"], pad=3))
    story.append(Spacer(1, 2 * mm))
    story.append(Paragraph("Эффекты дорожки", S_H2))
    story.append(Paragraph("VINYL (треск, шип) · TAPE (вау, сатурация) · LOFI (биты, частота) · CHORUS · FILTER · TREMOLO · "
                           "SPRING (пружинный ревер). Посылы: ECHO — плёночное эхо, REVERB — ROOM, HALL, CAVE, SHIMMER. "
                           "На мастере: DUST (0 — чисто, 100 — кассета из детства) и SLEEP (всё темнее и дольше — для аутро).", S_P))
    story.append(Paragraph("Настроения (MOOD)", S_H2))
    moods = [["PLAINS", "76", "мажор · C418", "луг, птицы"], ["RAIN", "72", "дорийский · LOFI", "дождь"],
             ["CAVE", "64", "минор · AMBIENT", "капли, гул"], ["SNOW", "68", "лидийский · AMBIENT", "ветер, шаги"],
             ["OCEAN", "70", "миксолидийский · C418", "океан"], ["NIGHT", "60", "мажор · C418", "сверчки"],
             ["CAMPFIRE", "82", "пентатоника · CAMPFIRE", "костёр"], ["STUDY", "84", "дорийский · LOFI", "дождь, винил"]]
    story.append(table(moods, [30, 16, 62, CW - 108], head=["MOOD", "BPM", "ЛАД · СТИЛЬ", "МИР"], pad=2))
    story.append(PageBreak())

    # ---- 08 recording
    story += section("08", "Запись, метроном, отмена", "Всё отменяемо. Ничего не надо сохранять руками.", R)
    story.append(two(shot("p13_tape_count.png", 66), bullets([
        f"{k('REC')} на остановленном синте — счёт в один такт: метроном щёлкает, на наклейке кассеты крупно 4…3…2…1, "
        "запись начинается ровно с первой доли.",
        f"Если уже играет — запись начинается сразу. {k('REC')} ещё раз — стоп; через такт вернётся страница, с которой пришёл.",
        "Петля дописывается по кругу: каждый проход — отдельный слой. Держи EDIT — отменить последний проход, HOME+EDIT — вернуть.",
        "Крутишь ручку на SOUND во время записи — движение пишется в клип.",
        f"Держи {k('REC')} 1 секунду — очистить клип."], "r"), 66, CW - 66, 7))
    story.append(Spacer(1, 6 * mm))
    story.append(card([Paragraph(c("Метроном", "y"), S_H2),
                       Paragraph(f"По умолчанию щёлкает только при записи и счёте — мягким деревянным WOOD. {k('HOME')} + "
                                 f"{k('REC')} — вкл/выкл. На кассете ручка {c('K3', 'y')} — его громкость. Всё остальное "
                                 "(режим ALWAYS, счёт в 2 такта, звук CLICK/HAT/COWBELL/RIM, деление 1/8 и 1/16) — на странице "
                                 "PROJECT. В экспорт и в STEMS метроном не попадает. TAP TEMPO: "
                                 f"{k('HOME')} + {k('PLAY')} три раза и больше.", S_P)], CW, Y))
    story.append(PageBreak())

    # ---- 09 song
    story += section("09", "Песня целиком", "Части, трасса, трюки — как монтаж ленты у OP-1.", B)
    story += bullets([
        "Восемь частей A–H. У каждой — метка (INTRO, VERSE, PRE, CHORUS, DROP, BRIDGE, BREAK, OUTRO) и длина 1–16 тактов. "
        "Внутри части у каждой дорожки свой клип; клип короче части крутится по кругу.",
        f"Держи {k('ARP')} + белая A–H — перейти к части (переключится с конца такта).",
        "PART — крутится текущая часть. SONG — едем по трассе. Переключение: держи PLAY.",
        "Трасса (страница ROAD): цепочка до 64 блоков «часть × повторы + трюки». Трюки: HALF (полтемпа нот), DOUBLE, "
        "REVERSE (ноты задом наперёд), +12/−12, DROP (выключить дорожку на этом блоке), TAPESTOP в конце блока.",
        f"Запись трассы вживую: в режиме PART на странице ROAD нажми {k('REC')} и переключай части — они сами лягут в трассу.",
        "Шаблоны нового проекта: POP, CLUB, AMBIENT — пустые части с метками и готовая трасса."], "b")
    story.append(Spacer(1, 4 * mm))
    story.append(two(shot("p09_road.png", 78), shot("p10_garage.png", 78), 78, 86, 8))
    story.append(Paragraph("Трасса с машинкой и гараж частей", S_CAP))
    story.append(PageBreak())

    # ---- 10 computer
    story += section("10", "В компьютер", "Reaper, MIDI и дорожки по отдельности.", G)
    story.append(Paragraph(c("EXPORT в .mid — главное", "g"), S_H2))
    story += bullets([
        "Самый простой путь: открыть в Chrome файл web\\export.html из репозитория, выбрать FM-1, нажать Listen.",
        f"На FM-1: {k('SAVE')} (страница PROJECT), потом чёрная 1 — EXPORT. Страница соберёт файл — кнопка «Save the .mid».",
        "Или из командной строки: python tools\\deck_export.py --port \"FM-1\" song.mid.",
        "В Reaper: перетащить .mid — шесть MIDI-дорожек с именами (движок + пресет), маркеры частей на таймлайне, "
        "трюки трассы уже применены, движения ручек — CC74 и CC7.",
        "Дорожки в DRIVE экспортируются после BAKE (HOME + SEQ) — до этого автопилот играет «вживую»."])
    story.append(Paragraph(c("MIDI вживую", "b"), S_H2))
    story += bullets(["MIDI OUT: каждая дорожка на своём канале 1–6, плюс clock, старт и стоп — Reaper пишет все шесть сразу.",
                      "MIDI IN: каналы 1–6 играют дорожки FM-1 (канал 10 — барабаны, CC64 — педаль)."], "b")
    story.append(Paragraph(c("STEMS", "y"), S_H2))
    story += bullets(["PROJECT, чёрная 2: после счёта песня проигрывается 7 раз — полный микс и каждая дорожка соло, "
                      "между проходами такт тишины со щелчком. Пишешь выход синта одним дублем и режешь по щелчкам."], "y")
    story.append(PageBreak())

    # ---- 11 cheat sheet
    story += section("11", "Шпаргалка", "Одна страница — всё под рукой.", Y)
    cheat = [[k("HOME"), "сад · SHIFT"], [k("HOME") + " + белая", "пресет (13–16 — свои)"],
             [k("HOME") + " + чёрная 1–6", "дорожка"], [k("HOME") + " + чёрная 7–11", "копир. · вставить · удвоить · вариация · гуманизация"],
             [k("FX"), "SOUND · вкладки"], [k("SEQ"), "SEQ · держать + белая 1–5: секвенсор · HOME+: BAKE"], [k("ENV"), "ROAD · держать: гараж"],
             [k("LFO"), "MIX · ещё раз: MASTER · держать + ручка: модулятор"], [k("EDIT"), "KEYS · держать: UNDO · HOME+: REDO"],
             [k("SAVE"), "PROJECT · держать + белая 13–16: свой пресет"], [k("PLAY"), "старт/стоп · держать: PART/SONG · HOME+: тап-темп"],
             [k("REC"), "запись · держать: очистить · HOME+: метроном"], [k("SEL"), "дорожка → · держать: мьют"],
             [k("ARP"), "ARP вкл · ещё раз: LATCH · HOME+: выкл · держать + белая: часть"], [k("GLO"), "FILL, пока держишь"], [k("OCT− / +"), "октава · HOME+: транспонировать клип"],
             ["K1–K4", "четыре поля страницы (на саде: DUST · SIDECHAIN · SLEEP · SWING)"],
             ["SELECT · ALGO · PRESET", "дорожка · ещё параметр · пресет (на саде: темп · MACRO · MOOD)"],
             ["MIX: держать белую", "стереть дорожку в части"]]
    story.append(table(cheat, [54, CW - 54], pad=5))
    story.append(PageBreak())

    # ---- 12 trouble
    story += section("12", "Если что-то не так", lead=None, col=R)
    tr = [["Нет звука", "Покрути MASTER. Проверь мьют (страница MIX: белые 1–6) и соло (чёрные 1–6)."],
          ["Клавиши играют не то", f"Ты на странице PROJECT, ROAD, MIX или SEQ — там клавиши управляют. {k('HOME')} — "
                                   "вернуться к саду, там клавиши просто играют."],
          ["Нажимаю — меняется пресет", f"Зажата {k('HOME')}. Отпусти её перед игрой."],
          ["Звук прерывается", "Синт урезает хвосты нот при перегрузе. Меньше одновременных пэдов и пианино с длинным "
                               "DECAY. CPU % — на странице PROJECT."],
          ["Не сохранилось", "Сохранение — в тишине через несколько секунд после остановки. Или PROJECT → чёрная 3."],
          ["Экспорт не приходит", "Закрой другие MIDI-программы, проверь, что выбрал FM-1. Запасной путь — MIDI OUT вживую в Reaper."],
          ["Всё сломалось", "Откат на OVERWORLD — раздел 01. Расскажи, что случилось: прошивка ещё не проверялась на железе."]]
    story.append(table(tr, [48, CW - 48], pad=6))
    story.append(Spacer(1, 14 * mm))
    story.append(Paragraph("Сделано на fm1-base и felucca-bsp (Felucca — Leo Kuroshita, Hügelton Instruments). Пластинчатый "
                           "ревер, голосоведение и звук OVERWORLD — по мотивам AMB-1 (Charles Vestal). Лицензия MPL-2.0.", S_SM))

    doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=M, rightMargin=M, topMargin=16 * mm, bottomMargin=18 * mm,
                            title="DECK · OVERWORLD — инструкция", author="DECK")
    doc.build(story, onFirstPage=page_bg, onLaterPages=page_bg)
    print(OUT)


if __name__ == "__main__":
    build()
