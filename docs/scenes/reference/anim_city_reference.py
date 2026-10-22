import math, random
from PIL import Image, ImageDraw, ImageFont, ImageFilter

SP = '/tmp/claude-0/-home-claude/5efead84-df8a-55bd-9fa1-656c206823d2/scratchpad/prev/'
base = Image.open(SP + 'city240.png').convert('RGB')
W = H = 240
px = base.load()

# 1. windows of the front building -> 6 tracks by column, height = level
wins = []
for y in range(112, 197):
    for x in range(92, 214):
        r, g, b = px[x, y]
        if (r + g + b) / 3 > 140:
            t = min(5, (x - 92) * 6 // 122)
            frac = (196 - y) / 84.0
            wins.append((x, y, t, frac))
print('window pixels', len(wins))

BB = (101, 33, 216, 81)  # billboard inner rect
font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 30)
chords = ['AM9', 'FMAJ7', 'CMAJ9', 'G6']

random.seed(3)
drops = [[random.uniform(0, W), random.uniform(0, 200), random.uniform(3, 6)] for _ in range(70)]
FPB = 6  # frames per beat
N = 96
frames = []
for f in range(N):
    beat = f / FPB
    ph = beat % 1.0
    kick = max(0.0, 1 - ph * 1.6)
    lv = [
        0.25 + 0.75 * kick,                                   # drums
        0.3 + 0.6 * max(0.0, 1 - ((beat * 2) % 1) * 1.4),     # bass 8ths
        0.55 + 0.25 * math.sin(f / 9.0),                      # pad
        0.2 + 0.7 * random.random(),                          # arp
        (0.7 + 0.2 * math.sin(f / 3.0)) if (int(beat) % 4) in (1, 2) else 0.15,  # lead phrase
        0.9 if (f % 48) in range(40, 48) else 0.1,            # fx riser
    ]
    im = base.copy()
    p = im.load()
    for (x, y, t, frac) in wins:
        if frac > lv[t]:
            r, g, b = p[x, y]
            p[x, y] = (int(r * 0.3), int(g * 0.3), int(b * 0.42))
        elif lv[t] - frac < 0.08:
            p[x, y] = (255, 255, 255)
    # 2. chord on the billboard, neon with flicker
    chord = chords[(f // (FPB * 4)) % 4]
    flick = (f % 37) in (5, 7)
    glow = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    tw = gd.textlength(chord, font=font)
    tx = (BB[0] + BB[2] - tw) / 2
    ty = (BB[1] + BB[3]) / 2 - 19
    if not flick:
        gd.text((tx, ty), chord, font=font, fill=(255, 42, 138, 255))
        glow = glow.filter(ImageFilter.GaussianBlur(3))
        im.paste(glow, (0, 0), glow)
        im.paste(glow, (0, 0), glow)
        d = ImageDraw.Draw(im)
        d.text((tx, ty), chord, font=font, fill=(255, 220, 240))
    else:
        d = ImageDraw.Draw(im)
        d.text((tx, ty), chord, font=font, fill=(120, 60, 110))
    # 3. rain on the glass, denser on hats (every half beat)
    d = ImageDraw.Draw(im)
    hat = ((beat * 2) % 1) < 0.34
    for i, dr in enumerate(drops):
        if not hat and i % 3 == 0:
            continue
        x, y, s = dr
        d.line([(x, y), (x - 1, y + s * 1.4)], fill=(200, 210, 255), width=1)
        dr[1] += s
        if dr[1] > 200:
            dr[0], dr[1] = random.uniform(0, W), random.uniform(-20, 0)
    # 4. cassette LED blinks on the kick
    if kick > 0.6:
        d.rectangle([52, 214, 53, 215], fill=(255, 42, 74))
    big = im.resize((480, 480), Image.NEAREST)
    bp = big.load()
    for yy in range(0, 480, 3):
        for xx in range(480):
            r, g, b = bp[xx, yy]
            bp[xx, yy] = (int(r * 0.8), int(g * 0.8), int(b * 0.8))
    frames.append(big.quantize(colors=128, method=Image.Quantize.MEDIANCUT))

frames[0].save(SP + 'city_anim.gif', save_all=True, append_images=frames[1:], duration=89, loop=0, optimize=True)
print('done')
