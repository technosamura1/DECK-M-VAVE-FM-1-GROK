"""Pixelize OUTRUN scene art for the FM-1 screen and write zones.json.

Usage: python3 make_scenes2.py <src_dir> <out_dir>
src_dir holds <name>.jpg/png (1024x1024 from the generator). Coordinates in SCENES are in source pixels.
Output: <name>_240.png, <name>_120.png (64 colors, art fit to width, dark bands top/bottom for header/footer),
zones.json (all rects converted to 240x240 screen coordinates), preview_sheet.png.
"""
import json, os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

SCENES = {
    # name: crop (top, bottom), signs [(x0,y0,x1,y1)...] with role, eq mode + region, sprites/anim hints
    'city': dict(crop=(82, 936), parts=['INTRO'],
                 signs=[('chord', (402, 395, 646, 524))],
                 eq=('windows', (0, 110, 1024, 790)),
                 anim=['rain on glass (denser on hats)', 'synth keys on the windowsill light up with the notes you play',
                       'synth display blinks on the kick', 'cassette LED on the kick'],
                 hot=dict(synth_keys=(298, 852, 772, 922), synth_display=(504, 812, 582, 826), cassette=(810, 870, 940, 910))),
    'road': dict(crop=(82, 936), parts=['VERSE'],
                 signs=[('part', (214, 184, 384, 303)), ('chord', (402, 184, 621, 303)), ('next', (640, 184, 808, 303))],
                 eq=('lamps', None),
                 anim=['lane lines run toward the viewer (speed = tempo)', 'car bobs 1px on the kick',
                       'taillights flash on the snare', 'street lamp glows pulse on the kick'],
                 hot=dict(car=(305, 655, 718, 918), tail_l=(330, 775, 445, 830), tail_r=(580, 775, 695, 830),
                          lamps=[(232, 343), (332, 415), (782, 343), (690, 415)],
                          lane_l=((470, 600), (-20, 812)), lane_r=((554, 600), (1044, 812)))),
    'grid': dict(crop=(82, 936), parts=['CHORUS'],
                 signs=[('chord', (374, 140, 652, 275))],
                 eq=('sun_stripes', (298, 420, 726, 640)),
                 anim=['grid floor scrolls toward the viewer (palette cycling of the horizontal lines)',
                       'each sun stripe = one track level (6 stripes bottom-up)', 'stars twinkle on KEYS/ARP notes',
                       'grid flashes brighter on the kick'],
                 hot=dict(grid_floor=(0, 660, 1024, 936), sun=(298, 310, 726, 640))),
    'vhsroom': dict(crop=(0, 921), parts=['BRIDGE'],
                    signs=[('chord', (250, 448, 470, 662))],
                    eq=('bars', (758, 598, 926, 670)),
                    anim=['the graphic equalizer on the stereo = 6 tracks (literal bars, split the panel into 6 columns)',
                          'chord on the TV screen with CRT glow + rolling scanline', 'speakers pump on the kick/bass',
                          'VCR display blinks on the beat', 'city windows outside blink with KEYS notes'],
                    hot=dict(speaker_l=(650, 585, 742, 752), speaker_r=(942, 585, 1024, 752),
                             vcr=(182, 752, 488, 800), window=(722, 190, 1024, 528))),
    'drivein': dict(crop=(0, 1000), parts=['PRE'],
                    signs=[('chord', (210, 345, 815, 665))], sign_style='film',
                    eq=('car_lights', [(0, 760, 290, 960), (370, 760, 660, 960), (750, 760, 1024, 960),
                                       (225, 705, 400, 800), (450, 705, 585, 770), (630, 705, 800, 800)]),
                    anim=['the movie screen shows the chord as a film title (dark letters on the light screen, projector flicker)',
                          'taillights of 6 cars = 6 tracks (brighter = louder)', 'stars twinkle with KEYS',
                          'palm trees sway once per bar'],
                    hot=dict(projector_beam=(400, 0, 620, 340))),
    'arcade': dict(crop=(133, 921), parts=['DROP'],
                   signs=[('chord', (432, 340, 595, 433))],
                   eq=('screens', [(100, 285, 215, 455), (235, 330, 305, 455), (300, 350, 345, 450),
                                   (740, 360, 800, 465), (835, 395, 915, 540), (935, 380, 1024, 560)]),
                   anim=['neon sign on the back wall shows the chord', 'arcade screens = 6 tracks (screen brightness/flash)',
                         'checkered floor reflects the kick (brief brightness pulse)', 'little sprite blips on screens on hats'],
                   hot=dict(floor=(150, 580, 880, 921))),
    'diner': dict(crop=(0, 920), parts=['BREAK'],
                  signs=[('chord', (98, 192, 368, 362)), ('part', (528, 462, 772, 530))],
                  eq=('windows', (360, 575, 1024, 750)),
                  anim=['pole sign shows the chord, roof sign shows the part name', 'diner windows = 6 tracks (split into 6 columns)',
                        'car taillights on the snare', 'stars twinkle, a distant car passes on the road every 8 bars'],
                  hot=dict(car=(668, 655, 958, 802), road=(0, 690, 360, 880))),
    'sunrise': dict(crop=(0, 937), parts=['OUTRO'],
                    signs=[('chord', (162, 795, 304, 866))],
                    eq=('waves', (0, 560, 1024, 800)),
                    anim=['sun slowly rises over the length of the OUTRO part', 'seagulls glide across (sprites)',
                          'waves roll, wave crests brighten with track levels', 'reflection shimmers'],
                    hot=dict(sun=(420, 420, 604, 512), gulls=[(230, 390), (295, 440), (720, 355), (770, 390), (670, 420)])),
}

FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
DEMO = {'chord': ('AM9', (255, 42, 138)), 'part': ('CHORUS', (0, 229, 255)), 'next': ('> FMAJ7', (255, 180, 60))}


def find_src(src_dir, name):
    for ext in ('.png', '.jpg', '.jpeg', '.webp'):
        p = os.path.join(src_dir, name + ext)
        if os.path.exists(p):
            return p
    return None


def fit_text(d, text, rect, maxfs):
    x0, y0, x1, y1 = rect
    w, h = x1 - x0, y1 - y0
    pad = max(3, w // 10)
    fs = maxfs
    while fs > 6:
        f = ImageFont.truetype(FONT, fs)
        if d.textlength(text, font=f) <= w - 2 * pad and fs <= h * 0.7:
            return f, fs
        fs -= 1
    return ImageFont.truetype(FONT, 6), 6


def neon(im, rect, text, col, maxfs):
    d = ImageDraw.Draw(im)
    f, fs = fit_text(d, text, rect, maxfs)
    x0, y0, x1, y1 = rect
    tw = d.textlength(text, font=f)
    tx, ty = x0 + (x1 - x0 - tw) / 2, (y0 + y1) / 2 - fs * 0.62
    g = Image.new('RGBA', im.size, (0, 0, 0, 0))
    ImageDraw.Draw(g).text((tx, ty), text, font=f, fill=col + (255,))
    g = g.filter(ImageFilter.GaussianBlur(2))
    im.paste(g, (0, 0), g)
    im.paste(g, (0, 0), g)
    ImageDraw.Draw(im).text((tx, ty), text, font=f, fill=tuple(min(255, v // 2 + 140) for v in col))


def film(im, rect, text):
    """Film-title style for light screens: dark letters, soft shadow."""
    d = ImageDraw.Draw(im)
    f, fs = fit_text(d, text, rect, 34)
    x0, y0, x1, y1 = rect
    tw = d.textlength(text, font=f)
    tx, ty = x0 + (x1 - x0 - tw) / 2, (y0 + y1) / 2 - fs * 0.62
    d.text((tx + 1, ty + 1), text, font=f, fill=(200, 170, 220))
    d.text((tx, ty), text, font=f, fill=(58, 22, 92))


def main(src_dir, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    zones, tiles = {}, []
    for name, c in SCENES.items():
        p = find_src(src_dir, name)
        if not p:
            print('missing', name)
            continue
        src = Image.open(p).convert('RGB')
        S = src.size[0]
        top, bot = c['crop']
        art = src.crop((0, top, S, bot))
        for res in (240, 120):
            h = round((bot - top) * res / S)
            s = art.resize((res, h), Image.LANCZOS)
            s = s.quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert('RGB')
            fr = Image.new('RGB', (res, res), (10, 6, 22))
            oy = (res - h) // 2
            fr.paste(s, (0, oy))
            fr.save(os.path.join(out_dir, f'{name}_{res}.png'))
            if res != 240:
                continue
            k = 240 / S
            P = lambda x, y: [round(x * k), round((y - top) * k) + oy]
            Rr = lambda r: P(r[0], r[1]) + P(r[2], r[3])

            def conv(v):
                if isinstance(v, tuple) and len(v) == 4 and all(isinstance(a, (int, float)) for a in v):
                    return Rr(v)
                if isinstance(v, tuple) and len(v) == 2 and all(isinstance(a, (int, float)) for a in v):
                    return P(*v)
                if isinstance(v, (list, tuple)):
                    return [conv(a) for a in v]
                return v
            zones[name] = dict(
                parts=c['parts'], image_y=oy, image_h=h,
                signs=[dict(role=r, rect=Rr(rc), style=c.get('sign_style', 'neon')) for r, rc in c['signs']],
                eq=dict(mode=c['eq'][0], region=conv(c['eq'][1]) if c['eq'][1] else None),
                hot={kk: conv(vv) for kk, vv in c['hot'].items()},
                anim=c['anim'])
            pv = fr.copy()
            d = ImageDraw.Draw(pv)
            d.rectangle([0, 0, 239, 17], fill=(5, 2, 20))
            d.rectangle([0, 220, 239, 239], fill=(5, 2, 20))
            d.text((6, 3), 'PLAY  ' + name.upper(), font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 10), fill=(255, 255, 255))
            for sg in zones[name]['signs']:
                txt, col = DEMO[sg['role']]
                if sg['style'] == 'film':
                    film(pv, sg['rect'], txt)
                else:
                    neon(pv, sg['rect'], txt, col, 22 if sg['role'] == 'chord' else 12)
            tiles.append(pv.resize((480, 480), Image.NEAREST))
    json.dump(zones, open(os.path.join(out_dir, 'zones.json'), 'w'), indent=1)
    rows = (len(tiles) + 2) // 3
    sheet = Image.new('RGB', (3 * 490 + 10, rows * 490 + 10), (18, 12, 30))
    for i, t in enumerate(tiles):
        sheet.paste(t, (10 + (i % 3) * 490, 10 + (i // 3) * 490))
    sheet.save(os.path.join(out_dir, 'preview_sheet.png'))
    print('scenes:', list(zones))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
