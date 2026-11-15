#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""AFFINE: a contact sheet of the simulator's screenshots (app/build/scenarios), for checking the screen page by page.
  ../mono2k/.venv/bin/python tools/contact_sheet.py OUT.png NAME [NAME ...]   (names without .png; z: scale, default 1)"""
import sys
from PIL import Image, ImageDraw
out, names = sys.argv[1], sys.argv[2:]
z = 1
if names and names[0].startswith("z="):
    z = int(names[0][2:]); names = names[1:]
cols = 6 if len(names) > 6 else len(names)
rows = (len(names) + cols - 1) // cols
W, H = 240 * z, 240 * z
s = Image.new("RGB", (cols * (W + 6) + 6, rows * (H + 18) + 6), (16, 16, 20))
d = ImageDraw.Draw(s)
for i, n in enumerate(names):
    x, y = 6 + (i % cols) * (W + 6), 6 + (i // cols) * (H + 18)
    try:
        im = Image.open("app/build/scenarios/%s.png" % n).convert("RGB").resize((W, H), Image.NEAREST)
        s.paste(im, (x, y))
    except FileNotFoundError:
        pass
    d.text((x, y + H + 3), n, fill=(200, 200, 200))
s.save(out)
