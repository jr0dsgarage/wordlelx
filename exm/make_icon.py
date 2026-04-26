#!/usr/bin/env python3
"""
make_icon.py — Generate WORDLELX.ICN for HP 200LX System Manager.

Icon spec (from EXM SDK ICON.TXT):
  44 x 32 pixels, monochrome (1=black, 0=white)
  G_ImageGet buffer format:
    8-byte header: 01 00 01 00 2C 00 20 00
                   (type, ?, width=44, height=32, little-endian words)
    6 bytes/row × 32 rows = 192 bytes pixel data
    Total: 200 bytes
  Named WORDLELX.ICN, placed alongside WORDLELX.EXM.

Design: large bold W, small italic "LX" badge in bottom-right
        overlapping the W's right strokes.
"""

import math, os

W, H = 44, 32
BPR  = (W + 7) // 8          # 6 bytes per row

bm = [[0] * W for _ in range(H)]


def dot(x, y):
    if 0 <= x < W and 0 <= y < H:
        bm[y][x] = 1


def clear(x, y):
    if 0 <= x < W and 0 <= y < H:
        bm[y][x] = 0


def thick_line(x0, y0, x1, y1, t):
    """Draw a thick line via perpendicular sampling."""
    dx, dy = x1 - x0, y1 - y0
    ln = math.hypot(dx, dy)
    if not ln:
        return
    nx, ny = -dy / ln, dx / ln          # perpendicular unit vector
    steps  = int(max(abs(dx), abs(dy)) * 2) + 1
    half   = t / 2.0
    for i in range(steps + 1):
        s  = i / steps
        cx = x0 + s * dx
        cy = y0 + s * dy
        j  = -int(half * 2) - 1
        while j <= int(half * 2) + 1:
            o = j / 2.0
            if abs(o) < half:
                dot(round(cx + nx * o), round(cy + ny * o))
            j += 1


# ── Big W ─────────────────────────────────────────────────────────────────────
# Four diagonal strokes, width 5px, running top-to-bottom.
# Outer strokes barely angle outward; inner pair converges to a centre peak.
SW = 5
thick_line( 2,  2, 10, 29, SW)   # left outer
thick_line(15,  2, 22, 29, SW)   # left inner
thick_line(27,  2, 22, 29, SW)   # right inner (meets left inner at bottom)
thick_line(40,  2, 33, 29, SW)   # right outer

# ── LX badge ──────────────────────────────────────────────────────────────────
# Small italic "LX" sits on a cleared (white) background that partially
# covers the W's right strokes, giving the "on top of" visual effect.

GH  = 9    # glyph height in rows
GY  = 21   # top row of glyphs (y = 21..29)
LX_ = 26   # L glyph left edge
XX_ = 33   # X glyph left edge

# 5-wide × 9-tall L (2px vertical bar, 2px base)
L_pix = [
    [1,1,0,0,0],
    [1,1,0,0,0],
    [1,1,0,0,0],
    [1,1,0,0,0],
    [1,1,0,0,0],
    [1,1,0,0,0],
    [1,1,0,0,0],
    [1,1,1,1,1],
    [1,1,1,1,1],
]

# 7-wide × 9-tall X (2px crossing diagonals)
X_pix = [
    [1,1,0,0,0,1,1],
    [1,1,0,0,0,1,1],
    [0,1,1,0,1,1,0],
    [0,0,1,1,1,0,0],
    [0,0,1,1,1,0,0],
    [0,1,1,0,1,1,0],
    [0,1,1,0,1,1,0],
    [1,1,0,0,0,1,1],
    [1,1,0,0,0,1,1],
]

# Italic: top rows shifted right more, bottom rows not at all.
def italic(row, max_shift=2):
    return (GH - 1 - row) * max_shift // (GH - 1)

# Clear a white badge region (1px padding around the glyph bounding box).
# This makes LX appear to sit "on top of" the W rather than merging with it.
badge_x0 = LX_ - 1
badge_x1 = XX_ + 7 + italic(0) + 1   # rightmost X pixel + max shift + margin
badge_y0 = GY  - 1
badge_y1 = GY  + GH

for y in range(badge_y0, badge_y1 + 1):
    for x in range(badge_x0, badge_x1 + 1):
        clear(x, y)

# Draw L and X glyphs with italic offset.
for r in range(GH):
    sh = italic(r)
    y  = GY + r
    for c, v in enumerate(L_pix[r]):
        if v:
            dot(LX_ + c + sh, y)
    for c, v in enumerate(X_pix[r]):
        if v:
            dot(XX_ + c + sh, y)

# ── Serialise to ICN ──────────────────────────────────────────────────────────
HEADER = bytes([0x01, 0x00, 0x01, 0x00, 0x2C, 0x00, 0x20, 0x00])
pixels = bytearray()
for row in bm:
    rb = bytearray(BPR)
    for x in range(W):
        if row[x]:
            rb[x // 8] |= 0x80 >> (x % 8)
    pixels.extend(rb)

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'WORDLELX.ICN')
with open(out, 'wb') as f:
    f.write(HEADER + pixels)

size = 8 + len(pixels)
print(f'Wrote {out}  ({size} bytes, expected 200)')

# ── ASCII preview ─────────────────────────────────────────────────────────────
print(f'\n  Preview ({W}×{H}):')
print('  ' + ''.join(str(x % 10) for x in range(W)))
for y, row in enumerate(bm):
    print(f'{y:2d}' + ''.join('█' if p else ' ' for p in row))
