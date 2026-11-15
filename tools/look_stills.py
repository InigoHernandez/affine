#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""AFFINE: three looks for the screen, drawn as stills of HOME (240 x 240, the FM-1's screen) for the owner to pick
from before any UI is built (the handbook: settle the look cheaply). The same content in each: the break's 16 slices
with the one playing lit, the four gestures (FILTER, CRUSH, REDUX, VERB), the tempo, the pattern, the pads.

  A POLYGON   low-poly shapes, an affine-mapped floor (the textures bend), ordered dither, vertices on the grid
  B MEMCARD   the era's console menus: save blocks with little icons, deep gradients, bevelled bars
  C DISC      the break as a spinning disc, its sectors the slices, a CD player's face

Type: MONO2K's pixel font (assets/fonts/mono2k-pixel in ../mono2k, CC BY-SA), at 2x and 3x, whole pixels.
  ../mono2k/.venv/bin/python tools/look_stills.py OUTDIR
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FACE = os.path.join(HERE, "..", "..", "mono2k", "assets", "fonts", "mono2k-pixel", "mono2k-font.otf")
W = H = 240
BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
_font = ImageFont.truetype(FACE, 8)
_glyphs = {}


def glyph(ch):
    if ch not in _glyphs:
        im = Image.new("1", (24, 24), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = "1"
        d.text((4, 4), ch, font=_font, fill=1)
        bb = im.getbbox()
        if bb is None:
            _glyphs[ch] = (None, 2, 0)
        else:
            _glyphs[ch] = (im.crop((bb[0], 0, bb[2], 24)), bb[2] - bb[0], 0)
    return _glyphs[ch]


def text_w(s, z=2):
    w = 0
    for ch in s:
        g, gw, _ = glyph(ch)
        w += (gw + 1) * z
    return max(0, w - z)


def text(img, x, y, s, col, z=2, align="l"):
    """MONO2K's pixel font: each glyph at its native 8 px, scaled z times (whole pixels), 1 design pixel apart"""
    if align == "r":
        x -= text_w(s, z)
    elif align == "c":
        x -= text_w(s, z) // 2
    px = img.load()
    IW, IH = img.size
    for ch in s:
        g, gw, _ = glyph(ch)
        if g is not None:
            gp = g.load()
            for gy in range(24):
                for gx in range(gw):
                    if gp[gx, gy]:
                        for dy in range(z):
                            for dx in range(z):
                                X, Y = x + gx * z + dx, y + (gy - 6) * z + dy
                                if 0 <= X < IW and 0 <= Y < IH:
                                    px[X, Y] = col
        x += (gw + 1) * z
    return x


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def dither(t, x, y, levels=4):
    """t 0..1 to one of `levels` steps through the 4x4 ordered matrix: the visible bit texture"""
    v = t * (levels - 1)
    i = int(v)
    return min(levels - 1, i + (1 if (v - i) * 16 > BAYER4[y & 3][x & 3] else 0)) / (levels - 1)


def dgrad(img, box, c0, c1, vertical=True, levels=5):
    x0, y0, x1, y1 = box
    px = img.load()
    for y in range(y0, y1):
        for x in range(x0, x1):
            t = (y - y0) / max(1, y1 - y0 - 1) if vertical else (x - x0) / max(1, x1 - x0 - 1)
            px[x, y] = mix(c0, c1, dither(t, x, y, levels))


def tri_fill(img, pts, shade_fn):
    """a flat triangle, pixel centres inside; shade_fn(x, y) -> colour"""
    (x0, y0), (x1, y1), (x2, y2) = pts
    px = img.load()
    minx, maxx = max(0, int(min(x0, x1, x2))), min(W - 1, int(max(x0, x1, x2)) + 1)
    miny, maxy = max(0, int(min(y0, y1, y2))), min(H - 1, int(max(y0, y1, y2)) + 1)
    d = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
    if d == 0:
        return
    for y in range(miny, maxy + 1):
        for x in range(minx, maxx + 1):
            l0 = ((y1 - y2) * (x + 0.5 - x2) + (x2 - x1) * (y + 0.5 - y2)) / d
            l1 = ((y2 - y0) * (x + 0.5 - x2) + (x0 - x2) * (y + 0.5 - y2)) / d
            l2 = 1 - l0 - l1
            if l0 >= 0 and l1 >= 0 and l2 >= 0:
                c = shade_fn(x, y, l0, l1, l2)
                if c is not None:
                    px[x, y] = c


# the content every look shows
SLICES = 16
PLAYING = 6
GEST = [("FILTER", "1.2K", -0.45), ("CRUSH", "6 BIT", 0.62), ("REDUX", "11K", 0.48), ("VERB", "SPACE", 0.71)]
# (FILTER is bipolar: -1 low-pass .. 0 open .. +1 high-pass; its bar grows from the middle)
PADS = ["R8", "R16", "R32", "REV", "HLF", "STP", "FRZ", "SKP", "VRB", "CRS", "FLT"]
PADS_ON = {7: "held", 8: "latch"}
BREAK = "JUNGLE 175"
SLICE_ENV = [0.95, 0.35, 0.6, 0.3, 0.9, 0.4, 0.75, 0.3, 0.92, 0.38, 0.55, 0.85, 0.5, 0.3, 0.8, 0.45]


# ===================================================================================================== A POLYGON ---
def look_a():
    BG0, BG1 = (6, 8, 20), (30, 26, 64)                   # night: the page
    INK, DIM = (236, 230, 214), (120, 118, 150)           # text, labels
    HOT = (255, 92, 40)                                   # what plays now (one colour, one job)
    COOL = (70, 214, 200)                                 # the effects' amounts
    FLOOR0, FLOOR1 = (34, 32, 74), (64, 58, 120)
    img = Image.new("RGB", (W, H), BG0)
    dgrad(img, (0, 0, W, 130), BG0, BG1, levels=6)
    d = ImageDraw.Draw(img)
    # the floor: 16 columns (the slices) x 6 rows, in four strips of four slices, each a quad drawn as two triangles
    # with affine texture coordinates (no 1/z): inside each strip the checker bends along the diagonal, the PS1's tell
    # (games cut their floors into strips like this to tame it)
    NL, NR, FL, FR, YN, YF = -40, 280, 72, 168, 190, 110

    def floor_shade(Tq, tri, u0):
        def f(x, y, l0, l1, l2):
            u = u0 + 0.25 * (l0 * Tq[tri[0]][0] + l1 * Tq[tri[1]][0] + l2 * Tq[tri[2]][0])
            v = l0 * Tq[tri[0]][1] + l1 * Tq[tri[1]][1] + l2 * Tq[tri[2]][1]
            col, row = min(SLICES - 1, int(u * SLICES)), int(v * 6)
            if col == PLAYING:
                base = mix(HOT, (255, 200, 120), 0.0 if (col + row) & 1 else 0.35)
                return mix(BG0, base, dither(0.35 + 0.65 * v, x, y, 4))
            c = FLOOR1 if (col + row) & 1 else FLOOR0
            return mix(BG0, c, dither(0.2 + 0.8 * v, x, y, 4))
        return f
    for strip in range(4):
        ua, ub = strip / 4, (strip + 1) / 4
        P = [(NL + (NR - NL) * ua, YN), (NL + (NR - NL) * ub, YN), (FL + (FR - FL) * ub, YF), (FL + (FR - FL) * ua, YF)]
        Tq = [(0, 1), (1, 1), (1, 0), (0, 0)]
        for tri in ((0, 1, 2), (0, 2, 3)):
            tri_fill(img, [P[i] for i in tri], floor_shade(Tq, tri, ua))
    # the horizon line
    d.line([(0, 110), (W, 110)], fill=DIM)
    # a low-poly gem above the playing slice, its vertices snapped to whole pixels (they jitter as it turns)
    cx, cy, r = 120, 64, 30
    ang = 0.6
    top, bot = (0, -1.25, 0), (0, 1.0, 0)
    ring = [(math.cos(ang + k * math.pi / 3), 0, math.sin(ang + k * math.pi / 3)) for k in range(6)]
    faces = []
    for k in range(6):
        a, b = ring[k], ring[(k + 1) % 6]
        faces.append((top, a, b))
        faces.append((bot, b, a))
    light = (-0.5, -0.8, -0.4)

    def proj(p):
        x, y, z = p
        y2 = y * math.cos(0.35) - z * math.sin(0.35)
        z2 = y * math.sin(0.35) + z * math.cos(0.35)
        s = 1.0 / (1.0 + 0.25 * z2)
        return (round(cx + x * r * s), round(cy + y2 * r * s)), z2
    drawn = []
    for f in faces:
        pts = [proj(p) for p in f]
        z = sum(p[1] for p in pts) / 3
        (ax, ay), (bx, by), (qx, qy) = [p[0] for p in pts]
        cross = (bx - ax) * (qy - ay) - (by - ay) * (qx - ax)
        if cross <= 0:
            continue
        ux, uy, uz = [f[1][i] - f[0][i] for i in range(3)]
        vx, vy, vz = [f[2][i] - f[0][i] for i in range(3)]
        n = (uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx)
        ln = math.sqrt(sum(c * c for c in n)) or 1
        lum = max(0.0, -sum(n[i] / ln * light[i] for i in range(3)))
        drawn.append((z, [p[0] for p in pts], lum))
    for z, pts, lum in sorted(drawn, key=lambda t: -t[0]):
        tri_fill(img, pts, lambda x, y, *_ , lum=lum: mix((60, 20, 30), HOT if lum > 0.55 else mix(HOT, BG1, 0.5),
                                                         dither(0.3 + 0.7 * lum, x, y, 4)))
    # the header: the break and the tempo; the pattern, the slice, the bar
    text(img, 8, 8, BREAK, INK)
    text(img, 232, 8, "175", INK, align="r")
    d.rectangle((196, 10, 197, 15), fill=HOT)            # the beat, lit on the one
    text(img, 8, 24, "A01", DIM)
    text(img, 120, 24, "SLICE %02d" % (PLAYING + 1), HOT, align="c")
    text(img, 232, 24, "3.2", DIM, align="r")
    # the four gestures, two by two: label left, value right, a dithered bar under
    d.rectangle((0, 192, W, H), fill=BG0)
    d.line([(0, 192), (W, 192)], fill=DIM)
    for i, (lab, val, amt) in enumerate(GEST):
        x, y = 8 + (i % 2) * 120, 198 + (i // 2) * 21
        text(img, x, y, lab, DIM)
        text(img, x + 106, y, val, INK, align="r")
        for yy in range(y + 13, y + 16):
            for xx in range(x, x + 106):
                t = (xx - x) / 105
                if amt < 0:                      # from the middle, left: low-pass
                    on, k = 0.5 + amt / 2 <= t <= 0.5, (0.5 - t) / max(-amt / 2, 0.01)
                else:
                    on, k = t <= amt, t / max(amt, 0.01)
                if on:
                    img.putpixel((xx, yy), mix(BG1, COOL, dither(0.3 + 0.7 * k, xx, yy, 4)))
                elif (xx + yy) & 1 == 0:
                    img.putpixel((xx, yy), BG1)
        if amt < 0:
            d.line([(x + 53, y + 12), (x + 53, y + 16)], fill=INK)
    return img


# ===================================================================================================== B MEMCARD ---
def look_b():
    BG0, BG1 = (8, 14, 48), (40, 70, 150)                 # the era's menu blue, deep to light
    INK, DIM = (240, 240, 232), (150, 168, 210)
    HOT = (255, 214, 80)                                  # what plays now: the cursor's gold
    COOL = (120, 230, 255)                                # the effects' amounts
    BEV_HI, BEV_LO = (170, 190, 240), (10, 18, 50)
    img = Image.new("RGB", (W, H), BG0)
    dgrad(img, (0, 0, W, H), BG1, BG0, levels=7)
    d = ImageDraw.Draw(img)

    def bevel(box, fill=None, inset=False):
        x0, y0, x1, y1 = box
        if fill:
            d.rectangle(box, fill=fill)
        d.line([(x0, y0), (x1, y0)], fill=BEV_LO if inset else BEV_HI)
        d.line([(x0, y0), (x0, y1)], fill=BEV_LO if inset else BEV_HI)
        d.line([(x0, y1), (x1, y1)], fill=BEV_HI if inset else BEV_LO)
        d.line([(x1, y0), (x1, y1)], fill=BEV_HI if inset else BEV_LO)
    # title bar
    bevel((4, 4, 235, 36), fill=(20, 34, 96))
    text(img, 12, 12, BREAK, INK)
    text(img, 228, 12, "175", HOT, align="r")
    # the slices as 16 save blocks, 8 x 2, each with a 20 x 14 icon: the slice's hit, drawn as the era's sprites
    for i in range(SLICES):
        col, row = i % 8, i // 8
        x, y = 6 + col * 29, 44 + row * 42
        lit = i == PLAYING
        bevel((x, y, x + 26, y + 26), fill=(30, 50, 120) if not lit else (110, 84, 20), inset=lit)
        e = SLICE_ENV[i]
        for k in range(20):
            amp = e * 6.5 * math.exp(-k / (4 + 6 * (1 - e))) * (1 if k % 2 else 0.7)
            for yy in range(-int(amp), int(amp) + 1):
                img.putpixel((x + 3 + k, y + 13 + yy), HOT if lit else (COOL if abs(yy) < amp - 1 else mix(COOL, BG0, 0.45)))
        text(img, x + 14, y + 30, "%d" % (i + 1), HOT if lit else DIM, align="c")
    # the four gestures as "blocks used" meters
    y0 = 136
    for i, (lab, val, amt) in enumerate(GEST):
        y = y0 + i * 18
        text(img, 10, y + 2, lab, DIM)
        text(img, 228, y + 2, val, INK, align="r")
        bevel((84, y, 166, y + 12), fill=(14, 24, 70), inset=True)
        nseg = 10
        for sg in range(nseg):
            on = sg / nseg < amt if amt >= 0 else (nseg / 2 + amt * nseg / 2 <= sg < nseg / 2)
            if on:
                d.rectangle((87 + sg * 8, y + 3, 92 + sg * 8, y + 9), fill=COOL)
        if amt < 0:
            d.line([(125, y + 1), (125, y + 11)], fill=INK)
    # the pads: eleven tiles (the black keys), lit held, ringed latched; the ones on named
    bevel((4, 212, 235, 235), fill=(20, 34, 96))
    for j in range(len(PADS)):
        x = 9 + j * 11
        st = PADS_ON.get(j)
        if st == "held":
            d.rectangle((x, 218, x + 7, 229), fill=HOT)
        elif st == "latch":
            d.rectangle((x, 218, x + 7, 229), outline=HOT)
        else:
            d.rectangle((x, 218, x + 7, 229), fill=(36, 58, 130))
    text(img, 228, 218, "SKIP VERB", HOT, align="r")
    return img


# ======================================================================================================== C DISC ---
def look_c():
    BG = (10, 10, 12)                                     # black: the disc's side of the case
    INK, DIM = (232, 232, 226), (110, 112, 120)
    HOT = (60, 255, 140)                                  # what plays now: an LCD's green
    COOL = (255, 120, 200)                                # the effects' amounts
    DISC0, DISC1 = (24, 22, 30), (70, 66, 90)
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    cx, cy, R0, R1 = 84, 110, 18, 70
    px = img.load()
    rot = 0.4
    for y in range(cy - R1 - 1, cy + R1 + 2):
        for x in range(cx - R1 - 1, cx + R1 + 2):
            dx, dy = x + 0.5 - cx, y + 0.5 - cy
            rr = math.hypot(dx, dy)
            if rr > R1 or rr < 6:
                continue
            a = (math.atan2(dy, dx) - rot) % (2 * math.pi)
            sec = int(a / (2 * math.pi) * SLICES)
            if rr < R0:                                   # the hub: dithered grey
                px[x, y] = mix(DISC0, DISC1, dither(0.5 + 0.5 * math.cos(a * 2), x, y, 3))
                continue
            # the sector's hit as its brightness, the grooves as fine rings, a sheen across (no rainbow: one hue)
            e = SLICE_ENV[sec] * (1 - (rr - R0) / (R1 - R0) * 0.5)
            groove = 0.12 if int(rr) % 3 == 0 else 0.0
            sheen = 0.35 * max(0.0, math.cos(a * 2 - 0.8)) ** 6
            if sec == PLAYING:
                px[x, y] = mix(BG, HOT, dither(min(1, 0.35 + 0.6 * e), x, y, 4))
            else:
                px[x, y] = mix(DISC0, DISC1, dither(min(1, 0.15 + 0.4 * e + sheen - groove), x, y, 4))
            if abs(((a / (2 * math.pi) * SLICES) % 1) - 0) < 0.02:
                px[x, y] = BG                             # the sector lines
    d.ellipse((cx - R1, cy - R1, cx + R1, cy + R1), outline=DIM)
    # the CD player's face: track = slice, the pattern and the bar as a time
    text(img, 8, 8, BREAK, INK)
    text(img, 232, 8, "175", HOT, align="r")
    text(img, 196, 46, "TRACK", DIM, align="c")
    text(img, 196, 62, "%02d" % (PLAYING + 1), HOT, z=4, align="c")
    text(img, 196, 110, "A01", DIM, align="c")
    text(img, 196, 126, "3.2.1", INK, align="c")
    # the four gestures in four columns: the value, a dial, the name
    d.line([(0, 186), (W, 186)], fill=(40, 40, 48))
    for i, (lab, val, amt) in enumerate(GEST):
        x = 30 + i * 60
        dx, dy = x, 210
        text(img, x, 192, val, INK, align="c")
        for k in range(60):
            t = k / 59
            ang = math.pi * (0.75 + 1.5 * t)
            on = t <= amt if amt >= 0 else 0.5 + amt / 2 <= t <= 0.5
            d.point((round(dx + math.cos(ang) * 8), round(dy + math.sin(ang) * 8)), fill=COOL if on else (46, 46, 54))
        a = amt if amt >= 0 else 0.5 + amt / 2
        ang = math.pi * (0.75 + 1.5 * a)
        d.line([(dx, dy), (round(dx + math.cos(ang) * 6), round(dy + math.sin(ang) * 6))], fill=INK)
        text(img, x, 226, lab, DIM, align="c")
    return img


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "build/review/look"
    os.makedirs(out, exist_ok=True)
    looks = [("A-polygon", look_a()), ("B-memcard", look_b()), ("C-disc", look_c())]
    for name, im in looks:
        im.resize((W * 3, H * 3), Image.NEAREST).save(os.path.join(out, name + ".png"))
    # the three side by side, 2x, captioned
    sheet = Image.new("RGB", (3 * (W * 2 + 24) + 24, H * 2 + 70), (18, 18, 22))
    for i, (name, im) in enumerate(looks):
        x = 24 + i * (W * 2 + 24)
        sheet.paste(im.resize((W * 2, H * 2), Image.NEAREST), (x, 24))
        cap = Image.new("RGB", (W, 20), (18, 18, 22))
        text(cap, W // 2, 8, name.upper(), (230, 230, 230), align="c")
        sheet.paste(cap.resize((W * 2, 40), Image.NEAREST), (x, H * 2 + 28))
    sheet.save(os.path.join(out, "looks.png"))
    print("wrote", out)


if __name__ == "__main__":
    main()
