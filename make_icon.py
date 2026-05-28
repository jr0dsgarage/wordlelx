#!/usr/bin/env python3
"""
make_icon.py — Generate WORDLELX.ICN for HP 200LX System Manager.

Usage:
    python3 make_icon.py [output_dir_or_primary_icon_path]

Icon spec (from EXM SDK ICON.TXT):
  44 x 32 pixels, monochrome (1=black, 0=white)
  G_ImageGet buffer format:
    8-byte header: 01 00 01 00 2C 00 20 00
                   (type, ?, width=44, height=32, little-endian words)
    6 bytes/row × 32 rows = 192 bytes pixel data
    Total: 200 bytes
    Generates both WORDLELX.ICN and WORDLDOS.ICN in the target directory.

Design: large 3D W, with badge variants in the bottom-right
    overlapping the W's right strokes.
"""

import math, os, sys

W, H = 44, 32
BPR  = (W + 7) // 8          # 6 bytes per row

def new_bitmap():
    return [[0] * W for _ in range(H)]


def dot(bm, x, y):
    if 0 <= x < W and 0 <= y < H:
        bm[y][x] = 1


def clear(bm, x, y):
    if 0 <= x < W and 0 <= y < H:
        bm[y][x] = 0


def thick_line(bm, x0, y0, x1, y1, t):
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
                dot(bm, round(cx + nx * o), round(cy + ny * o))
            j += 1


# ── Big W (3D) ────────────────────────────────────────────────────────────────
# Build a true "down-up-down-up" W from connected segments:
#   \\       //
#    \\ /\\ //
# Then add a small offset copy to create an extruded 3D look.

def draw_polyline(bm, points, width):
    for i in range(len(points) - 1):
        x0, y0 = points[i]
        x1, y1 = points[i + 1]
        thick_line(bm, x0, y0, x1, y1, width)


W_front = [
    (3, 4),
    (10, 25),
    (18, 10),
    (25, 25),
    (39, 4),
]

EXTRUDE_DX = 2
EXTRUDE_DY = 2
W_back = [(x + EXTRUDE_DX, y + EXTRUDE_DY) for x, y in W_front]

def draw_w(bm):
    # Back face first, then connector bridges, then front face.
    draw_polyline(bm, W_back, 5)
    for (fx, fy), (bx, by) in zip(W_front, W_back):
        thick_line(bm, fx, fy, bx, by, 3)
    draw_polyline(bm, W_front, 5)

# ── Badge variants ────────────────────────────────────────────────────────────

GH = 9
GY = 21


def italic(row, height, max_shift=2):
    return (height - 1 - row) * max_shift // (height - 1)


def draw_badge_lx(bm):
    lx = 26
    xx = 33
    l_pix = [
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
    x_pix = [
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

    badge_x0 = lx - 1
    badge_x1 = xx + 7 + italic(0, GH) + 1
    badge_y0 = GY - 1
    badge_y1 = GY + GH

    for y in range(badge_y0, badge_y1 + 1):
        for x in range(badge_x0, badge_x1 + 1):
            clear(bm, x, y)

    for r in range(GH):
        sh = italic(r, GH)
        y = GY + r
        for c, v in enumerate(l_pix[r]):
            if v:
                dot(bm, lx + c + sh, y)
        for c, v in enumerate(x_pix[r]):
            if v:
                dot(bm, xx + c + sh, y)


def draw_badge_dos(bm):
    gy = 22
    glyphs = {
        'D': [
            [1,1,1,0],
            [1,0,1,1],
            [1,0,0,1],
            [1,0,0,1],
            [1,0,0,1],
            [1,0,1,1],
            [1,1,1,0],
        ],
        'O': [
            [0,1,1,0],
            [1,0,0,1],
            [1,0,0,1],
            [1,0,0,1],
            [1,0,0,1],
            [1,0,0,1],
            [0,1,1,0],
        ],
        'S': [
            [0,1,1,1],
            [1,0,0,0],
            [1,1,1,0],
            [0,0,0,1],
            [0,0,0,1],
            [1,0,0,1],
            [0,1,1,0],
        ],
    }
    text = 'DOS'
    letter_spacing = 1
    total_width = sum(len(glyphs[ch][0]) for ch in text) + letter_spacing * (len(text) - 1)
    start_x = 26
    badge_x0 = start_x - 1
    badge_x1 = start_x + total_width
    badge_y0 = gy - 1
    badge_y1 = gy + len(glyphs['D'])

    for y in range(badge_y0, badge_y1 + 1):
        for x in range(badge_x0, badge_x1 + 1):
            clear(bm, x, y)

    x = start_x
    for ch in text:
        glyph = glyphs[ch]
        for row_index, row in enumerate(glyph):
            for col_index, value in enumerate(row):
                if value:
                    dot(bm, x + col_index, gy + row_index)
        x += len(glyph[0]) + letter_spacing


def build_icon(draw_badge):
    bm = new_bitmap()
    draw_w(bm)
    draw_badge(bm)
    return bm


def write_icon(path, bm):
    header = bytes([0x01, 0x00, 0x01, 0x00, 0x2C, 0x00, 0x20, 0x00])
    pixels = bytearray()
    for row in bm:
        rb = bytearray(BPR)
        for x in range(W):
            if row[x]:
                rb[x // 8] |= 0x80 >> (x % 8)
        pixels.extend(rb)

    with open(path, 'wb') as f:
        f.write(header + pixels)

    return 8 + len(pixels)


def print_preview(title, bm):
    print(f'\n  Preview {title} ({W}×{H}):')
    print('  ' + ''.join(str(x % 10) for x in range(W)))
    for y, row in enumerate(bm):
        print(f'{y:2d}' + ''.join('█' if p else ' ' for p in row))


def output_dir_from_argv():
    if len(sys.argv) <= 1:
        return os.path.dirname(os.path.abspath(__file__))

    target = os.path.abspath(sys.argv[1])
    if os.path.isdir(target):
        return target

    _, ext = os.path.splitext(target)
    if ext:
        return os.path.dirname(target)

    return target


out_dir = output_dir_from_argv()
os.makedirs(out_dir, exist_ok=True)

variants = [
    ('WORDLELX.ICN', 'LX', draw_badge_lx),
    ('WORDLDOS.ICN', 'DOS', draw_badge_dos),
]

for filename, title, draw_badge in variants:
    bm = build_icon(draw_badge)
    out = os.path.join(out_dir, filename)
    size = write_icon(out, bm)
    print(f'Wrote {out}  ({size} bytes, expected 200)')
    print_preview(title, bm)
