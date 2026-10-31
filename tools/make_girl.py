#!/usr/bin/env python3
"""The PLAY picture from an image: cropped to 240x144, dithered to 4 greys (Bayer 4x4), run-length coded a row at a
time into tapes/overworld/girl_img.h (a byte a run: the grey in bits 6-7, the length - 1 in bits 0-5; a table of
where each row starts). The device moves the hair sideways as it draws (girl.c).
   python tools/make_girl.py [image] [x y width]    (the crop: 5:3, in the image's pixels)"""
import sys
import numpy as np
from PIL import Image

BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], float) / 16.0
src = sys.argv[1] if len(sys.argv) > 1 else "tapes/overworld/art/girl.jpg"
x0, y0, w = (int(v) for v in sys.argv[2:5]) if len(sys.argv) > 4 else (60, 130, 620)
h = int(w * 0.6)
im = Image.open(src).convert("L").crop((x0, y0, x0 + w, y0 + h)).resize((240, 144), Image.LANCZOS)
g = np.clip((np.asarray(im, float) / 255.0 - 0.04) * 1.08, 0, 1)
lv = np.clip(np.floor(g * 3 + np.tile(BAYER, (36, 60))), 0, 3).astype(int)
data, offs = [], []
for y in range(144):
    offs.append(len(data))
    x = 0
    while x < 240:
        v, n = lv[y, x], 1
        while x + n < 240 and lv[y, x + n] == v and n < 64:
            n += 1
        data.append(v << 6 | (n - 1))
        x += n
with open("tapes/overworld/girl_img.h", "w") as f:
    f.write("/* made by tools/make_girl.py from %s (crop %d %d %d): 240x144, 4 greys, a row of runs each */\n" % (src, x0, y0, w))
    f.write("static const uint16_t GIRL_ROW[144] = {%s};\n" % ", ".join(map(str, offs)))
    f.write("static const uint8_t GIRL_RLE[%d] = {\n" % len(data))
    for i in range(0, len(data), 24):
        f.write("    " + ", ".join(map(str, data[i:i + 24])) + ",\n")
    f.write("};\n")
print("girl: %d bytes of runs + 288 of rows (raw 2-bit would be 8640)" % len(data))
