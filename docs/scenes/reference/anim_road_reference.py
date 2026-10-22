from PIL import Image, ImageDraw, ImageFont, ImageFilter

SP = '/tmp/claude-0/-home-claude/5efead84-df8a-55bd-9fa1-656c206823d2/scratchpad/prev/'
U = '/root/.claude/uploads/5efead84-df8a-55bd-9fa1-656c206823d2/'
fb = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
chords = ['AM9', 'FMAJ7', 'CMAJ9', 'G6']

CFG = [
    dict(file='a2943c00-image.jpg', top=80, bot=940,
         signs=[(212, 182, 385, 305), (400, 182, 623, 305), (638, 182, 810, 305)],
         lamps=[(232, 343), (332, 415), (782, 343), (690, 415)],
         tails=[(330, 775, 445, 830), (580, 775, 695, 830)],
         car=(305, 655, 718, 918),
         lines=[((470, 600), (-20, 812)), ((554, 600), (1044, 812))]),
    dict(file='07d6bef9-image.jpg', top=100, bot=956,
         signs=[(312, 182, 712, 298)],
         lamps=[(285, 322), (757, 322)],
         tails=[(322, 745, 435, 795), (590, 745, 705, 795)],
         car=(300, 632, 722, 895),
         lines=[((470, 548), (-20, 740)), ((552, 548), (1044, 740))]),
]


def prep(c):
    src = Image.open(U + c['file']).convert('RGB')
    S = src.size[0]
    k = 240 / S
    h = round((c['bot'] - c['top']) * k)
    oy = (240 - h) // 2
    img = src.crop((0, c['top'], S, c['bot'])).resize((240, h), Image.LANCZOS)
    img = img.quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert('RGB')
    base = Image.new('RGB', (240, 240), (10, 6, 22))
    base.paste(img, (0, oy))
    P = lambda x, y: (round(x * k), round((y - c['top']) * k) + oy)
    R = lambda r: P(r[0], r[1]) + P(r[2], r[3])
    bp = base.load()
    lane = []
    for a, b in c['lines']:
        (ax, ay), (bx, by) = P(*a), P(*b)
        dx, dy = bx - ax, by - ay
        L2 = dx * dx + dy * dy
        for y in range(min(ay, by), min(239, max(ay, by)) + 1):
            for x in range(240):
                t = ((x - ax) * dx + (y - ay) * dy) / L2
                if 0 <= t <= 1:
                    qx, qy = ax + t * dx, ay + t * dy
                    if (x - qx) ** 2 + (y - qy) ** 2 <= 2.5 and sum(bp[x, y]) / 3 > 120:
                        lane.append((x, y, t))
    road = bp[120, R(c['car'])[3] - 2]
    tails = []
    for r in c['tails']:
        x0, y0, x1, y1 = R(r)
        for y in range(y0, y1):
            for x in range(x0, x1):
                rr, g, b = bp[x, y]
                if rr > 150 and rr > g * 1.6:
                    tails.append((x, y))
    return dict(base=base, signs=[R(s) for s in c['signs']], lamps=[P(*l) for l in c['lamps']],
                car=R(c['car']), lane=lane, road=road, tails=tails)


def neon(im, rect, text, col, maxfs):
    x0, y0, x1, y1 = rect
    w = x1 - x0
    pad = max(3, w // 10)
    d = ImageDraw.Draw(im)
    fs = maxfs
    while fs > 6:
        f = ImageFont.truetype(fb, fs)
        if d.textlength(text, font=f) <= w - 2 * pad and fs <= (y1 - y0) * 0.7:
            break
        fs -= 1
    f = ImageFont.truetype(fb, fs)
    tw = d.textlength(text, font=f)
    tx = x0 + (w - tw) / 2
    ty = (y0 + y1) / 2 - fs * 0.62
    g = Image.new('RGBA', im.size, (0, 0, 0, 0))
    ImageDraw.Draw(g).text((tx, ty), text, font=f, fill=col + (255,))
    g = g.filter(ImageFilter.GaussianBlur(2))
    im.paste(g, (0, 0), g)
    im.paste(g, (0, 0), g)
    ImageDraw.Draw(im).text((tx, ty), text, font=f, fill=tuple(min(255, v // 2 + 140) for v in col))


def frame(s, fr):
    FPB = 6
    beat = fr / FPB
    ph = beat % 1
    kick = max(0.0, 1 - ph * 1.8)
    snare = (int(beat) % 2 == 1) and ph < 0.35
    im = s['base'].copy()
    p = im.load()
    phase = fr * 0.22
    for (x, y, t) in s['lane']:
        if ((t ** 0.6) * 7 - phase) % 1 < 0.45:
            p[x, y] = s['road']
    bob = kick > 0.55
    if bob:
        im.paste(s['base'].crop(s['car']), (s['car'][0], s['car'][1] - 1))
        p = im.load()
    if snare:
        for (x, y) in s['tails']:
            p[x, y - (1 if bob else 0)] = (255, 130, 150)
    glow = Image.new('RGBA', (240, 240), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for (lx, ly) in s['lamps']:
        r = 4 + 4 * kick
        gd.ellipse([lx - r, ly - r, lx + r, ly + r], fill=(255, 80, 220, int(50 + 130 * kick)))
    glow = glow.filter(ImageFilter.GaussianBlur(2))
    im.paste(glow, (0, 0), glow)
    bar = (fr // (FPB * 4)) % 4
    flick = (fr % 41) in (9, 11)
    if len(s['signs']) == 3:
        neon(im, s['signs'][0], 'CHORUS', (0, 229, 255), 12)
        if not flick:
            neon(im, s['signs'][1], chords[bar], (255, 42, 138), 22)
        neon(im, s['signs'][2], '> ' + chords[(bar + 1) % 4], (255, 180, 60), 12)
    elif not flick:
        neon(im, s['signs'][0], chords[bar], (255, 42, 138), 22)
    return im


scenes = [prep(c) for c in CFG]
print([len(s['lane']) for s in scenes], [len(s['tails']) for s in scenes])
frames = []
for fr in range(96):
    sheet = Image.new('RGB', (980, 500), (18, 12, 30))
    for i, s in enumerate(scenes):
        sheet.paste(frame(s, fr).resize((480, 480), Image.NEAREST), (10 + i * 490, 10))
    frames.append(sheet.quantize(colors=200, method=Image.Quantize.MEDIANCUT))
frames[0].save(SP + 'road_variants.gif', save_all=True, append_images=frames[1:], duration=89, loop=0, optimize=True)
frames[30].convert('RGB').save(SP + 'road_var_f30.png')
print('ok')
