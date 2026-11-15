#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""AFFINE's pixel font: MONO2K's 1-bit face (../mono2k/assets/fonts/mono2k-pixel/mono2k-font.otf: the owner's adaptation
of the FontStruction "Elektron Pixel Font" by savingaurora, CC BY-SA 3.0; MONO2K's bitmaps are CC BY-SA 4.0) rendered
at its native 8 px, characters 32..95 (capitals, figures, signs), each glyph cropped to its ink across and kept on a
common grid of rows. The firmware scales it by whole numbers when it draws (2x for every line, 3x-4x for big words),
so every stroke stays whole pixels. Writes app/aff_font.h; the bitmaps in it are under CC BY-SA 4.0 (that file only).

  ../mono2k/.venv/bin/python tools/gen_pixel_font.py [app/aff_font.h]
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FACE = os.path.join(HERE, "..", "..", "mono2k", "assets", "fonts", "mono2k-pixel", "mono2k-font.otf")
FIRST, LAST = 32, 95
SPACE_W = 2                                 # a space's design pixels


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "app", "aff_font.h")
    font = ImageFont.truetype(FACE, 8)
    ims = {}
    top, bot = 99, -1
    for c in range(FIRST, LAST + 1):
        im = Image.new("1", (24, 24), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = "1"
        d.text((4, 4), chr(c), font=font, fill=1)
        ims[c] = im
        bb = im.getbbox()
        if bb:
            top, bot = min(top, bb[1]), max(bot, bb[3])
    rows = bot - top
    assert rows <= 8, rows
    cap = ims[ord("A")].getbbox()
    widths, data = [], []
    for c in range(FIRST, LAST + 1):
        im = ims[c]
        bb = im.getbbox()
        if not bb:
            widths.append(SPACE_W)
            data += [0] * rows
            continue
        x0, x1 = bb[0], bb[2]
        w = x1 - x0
        assert w <= 8, (chr(c), w)
        widths.append(w)
        px = im.load()
        for y in range(top, bot):
            b = 0
            for x in range(x0, x1):
                if px[x, y]:
                    b |= 0x80 >> (x - x0)
            data.append(b)
    with open(out, "w") as f:
        f.write("/* SPDX-License-Identifier: CC-BY-SA-4.0 */\n")
        f.write("/* AFFINE's pixel font, made by tools/gen_pixel_font.py from MONO2K's mono2k-font.otf (an adaptation of the\n"
                " * FontStruction \"Elektron Pixel Font\" by savingaurora, CC BY-SA 3.0): characters %d..%d, %d rows each (the\n"
                " * capitals' top at row %d), a byte a row (bit 7 the glyph's left column), each glyph its ink's width. This\n"
                " * file's bitmaps are under CC BY-SA 4.0. Regenerate: tools/gen_pixel_font.py app/aff_font.h */\n"
                % (FIRST, LAST, rows, cap[1] - top))
        f.write("#pragma once\n#include <stdint.h>\n")
        f.write("#define AFF_FIRST %du\n#define AFF_LAST %du\n#define AFF_ROWS %du\n#define AFF_CAP_TOP %du\n#define AFF_CAP_H %du\n"
                % (FIRST, LAST, rows, cap[1] - top, cap[3] - cap[1]))
        f.write("static const uint8_t AFF_W[%d] = {%s};\n" % (len(widths), ", ".join(str(w) for w in widths)))
        f.write("static const uint8_t AFF_BITS[%d] = {\n" % len(data))
        for i in range(0, len(data), rows):
            ch = chr(FIRST + i // rows)
            f.write("    " + ", ".join("0x%02X" % b for b in data[i:i + rows]) + ",   /* %s */\n"
                    % (ch if ch not in "*/\\" else "'" + ch + "'"))
        f.write("};\n")
    print("wrote %s: %d glyphs, %d rows, %d bytes" % (out, LAST - FIRST + 1, rows, len(data) + len(widths)))


if __name__ == "__main__":
    main()
