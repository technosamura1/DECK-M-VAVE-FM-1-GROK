#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""a contact sheet of the screenshots: python tools/sheet.py OUT.png A.png B.png ... (each at 2x, 4 a row, named)"""
import os
import sys
from PIL import Image, ImageDraw

out, files = sys.argv[1], sys.argv[2:]
cols, sc = 4, 2
w = 240 * sc + 16
rows = (len(files) + cols - 1) // cols
sheet = Image.new("RGB", (cols * w, rows * (240 * sc + 34)), (24, 24, 24))
d = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    im = Image.open(f).convert("RGB").resize((240 * sc, 240 * sc), Image.NEAREST)
    x, y = (i % cols) * w + 8, (i // cols) * (240 * sc + 34) + 26
    sheet.paste(im, (x, y))
    d.text((x, y - 18), os.path.basename(f), fill=(200, 200, 200))
sheet.save(out)
