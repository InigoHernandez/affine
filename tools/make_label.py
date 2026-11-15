#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""AFFINE: the record label for the VINYL pad (FACE_PX, the format LOAF BREAKER's face used): a 64 x 64 checker
texture mapped without perspective correction (the PS1's affine warp: the squares bend along the triangle's diagonal),
16 greys, two pixels a byte (high nibble first), 0 outside the circle.

  tools/make_label.py app/face_data.h
"""
import sys

N = 64


def tex(u, v):                       # an 8 x 8 checker, the darker squares Bayer-dithered
    c = (int(u * 8) + int(v * 8)) & 1
    return 13 if c else 4


def affine_quad(x, y):
    """the label as a quad drawn as two triangles, each interpolating u, v linearly (no 1/z): the warp the PS1 shows"""
    # the quad's corners on screen (a tilted square seen in perspective) and their texture coordinates
    P = [(4, 10), (60, 2), (62, 62), (2, 54)]
    T = [(0, 0), (1, 0), (1, 1), (0, 1)]
    for a, b, c in ((0, 1, 2), (0, 2, 3)):
        (x0, y0), (x1, y1), (x2, y2) = P[a], P[b], P[c]
        d = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
        l0 = ((y1 - y2) * (x - x2) + (x2 - x1) * (y - y2)) / d
        l1 = ((y2 - y0) * (x - x2) + (x0 - x2) * (y - y2)) / d
        l2 = 1 - l0 - l1
        if min(l0, l1, l2) >= -0.02:
            u = l0 * T[a][0] + l1 * T[b][0] + l2 * T[c][0]
            v = l0 * T[a][1] + l1 * T[b][1] + l2 * T[c][1]
            return tex(u, v)
    return 2


def main():
    out = sys.argv[1]
    data = []
    for y in range(N):
        for x in range(0, N, 2):
            v = []
            for k in (0, 1):
                dx, dy = x + k - 31.5, y - 31.5
                v.append(0 if dx * dx + dy * dy > 31.5 * 31.5 else max(1, affine_quad(x + k, y)))
            data.append(v[0] << 4 | v[1])
    with open(out, "w") as f:
        f.write("/* SPDX-License-Identifier: MPL-2.0 */\n")
        f.write("/* AFFINE: the VINYL pad's record label, made by tools/make_label.py: %d x %d, 16 greys, two pixels a byte\n"
                " * (0 outside the circle). Regenerate it with: tools/make_label.py app/face_data.h */\n" % (N, N))
        f.write("#pragma once\n#include <stdint.h>\n#define FACE_N %d\n" % N)
        f.write("static const uint8_t FACE_PX[%d] = {\n" % len(data))
        for i in range(0, len(data), 16):
            f.write("    " + ", ".join("0x%02X" % b for b in data[i:i + 16]) + ",\n")
        f.write("};\n")


if __name__ == "__main__":
    main()
