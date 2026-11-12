#!/usr/bin/env python3
"""Generates all pixel art for Terrarium as DS-ready 4bpp tile data.

Writes:
  source/art.c, include/art.h  - palettes, BG tiles, sprite graphics
  tools/art_preview.png        - contact sheet of every graphic (4x zoom)

Everything is drawn procedurally with a fixed random seed, so the output is
reproducible. Run from the project root:  python3 tools/make_art.py
"""
import math
import os
import random
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rng = random.Random(1126)

# ---------------------------------------------------------------- palettes --
def rgb15(c):
    r, g, b = c
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)

# BG palette 0: coco chips (dry). BG palette 1: the same chips, damp.
FLOOR_DRY = [
    (0, 0, 0),
    (46, 26, 16),    # 1 deep gap
    (74, 43, 25),    # 2 gap
    (100, 60, 34),   # 3 chip shadow side
    (128, 78, 44),   # 4 chip mid
    (152, 96, 54),   # 5 chip
    (178, 118, 70),  # 6 chip light
    (204, 148, 96),  # 7 chip highlight
    (118, 88, 58),   # 8 fibre
    (90, 70, 48),    # 9 fibre dark
    (215, 190, 150), # 10 grid dot
    (40, 24, 16),    # 11 frame dark
    (88, 56, 34),    # 12 frame wood
    (130, 90, 56),   # 13 frame wood light
    (190, 220, 230), # 14 glass glint
    (120, 150, 160), # 15 glass edge
]
def dampen(c):
    r, g, b = c
    return (int(r * 0.62), int(g * 0.58), int(b * 0.55))
FLOOR_DAMP = [FLOOR_DRY[0]] + [dampen(c) for c in FLOOR_DRY[1:10]] + FLOOR_DRY[10:]

# Sprite palette 0: items
ITEM_PAL = [
    (0, 0, 0),
    (34, 22, 16),    # 1 outline
    (66, 38, 22),    # 2 drop shadow
    (92, 58, 34),    # 3 wood dark
    (140, 92, 52),   # 4 wood
    (190, 136, 82),  # 5 wood light
    (84, 84, 90),    # 6 stone dark
    (130, 128, 126), # 7 stone
    (184, 180, 170), # 8 stone light
    (32, 80, 40),    # 9 leaf dark
    (62, 132, 54),   # 10 leaf
    (126, 186, 82),  # 11 leaf light
    (36, 84, 136),   # 12 water deep
    (104, 168, 212), # 13 water light
    (230, 226, 214), # 14 ceramic
    (214, 92, 112),  # 15 accent (flower / terracotta)
]
# Sprite palette 1: snake (default "Classic" morph; the game swaps it per save)
SNAKE_PAL = [
    (0, 0, 0),
    (24, 16, 12),    # 1 outline
    (70, 46, 26),    # 2 base dark
    (112, 78, 44),   # 3 base
    (156, 116, 70),  # 4 base light
    (38, 26, 18),    # 5 pattern dark
    (196, 160, 104), # 6 pattern light
    (226, 210, 170), # 7 belly
    (20, 14, 10),    # 8 eye
    (210, 40, 60),   # 9 tongue
    (240, 240, 240), # 10 eye shine
] + [(0, 0, 0)] * 5
# Sprite palette 2: rat, debris, mist, cursor. Palette 3 = same with red cursor.
FX_PAL = [
    (0, 0, 0),
    (40, 30, 26),    # 1 outline
    (168, 160, 158), # 2 fur shade
    (232, 228, 222), # 3 fur
    (236, 150, 162), # 4 pink
    (16, 10, 10),    # 5 eye
    (228, 216, 178), # 6 shed light
    (176, 160, 122), # 7 shed dark
    (58, 40, 24),    # 8 dropping dark
    (96, 66, 40),    # 9 dropping
    (242, 240, 228), # 10 urate
    (206, 234, 255), # 11 mist light
    (140, 196, 240), # 12 mist
    (250, 250, 240), # 13 cursor
    (30, 22, 16),    # 14 cursor edge
    (250, 200, 60),  # 15 gold
]
FX_PAL_RED = list(FX_PAL)
FX_PAL_RED[13] = (240, 60, 50)

# --------------------------------------------------------------- canvas --
class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.p = [[0] * w for _ in range(h)]

    def put(self, x, y, c):
        x, y = int(x), int(y)
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y][x] = c

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.p[y][x]
        return 0

    def ellipse(self, cx, cy, rx, ry, shade):
        """shade(nx, ny, x, y) -> colour or None, nx/ny in -1..1"""
        for y in range(self.h):
            for x in range(self.w):
                nx = (x + 0.5 - cx) / rx
                ny = (y + 0.5 - cy) / ry
                if nx * nx + ny * ny <= 1.0:
                    c = shade(nx, ny, x, y) if callable(shade) else shade
                    if c is not None:
                        self.p[y][x] = c

    def rect(self, x0, y0, x1, y1, c):
        for y in range(int(y0), int(y1)):
            for x in range(int(x0), int(x1)):
                self.put(x, y, c)

    def line(self, x0, y0, x1, y1, c, thick=1):
        n = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
        for i in range(n + 1):
            t = i / n
            x = x0 + (x1 - x0) * t
            y = y0 + (y1 - y0) * t
            for dy in range(thick):
                for dx in range(thick):
                    self.put(x + dx - thick // 2, y + dy - thick // 2, c)

    def outline(self, col=1, skip=(0,)):
        """Dark 1px outline around every opaque pixel not in `skip`."""
        out = [row[:] for row in self.p]
        for y in range(self.h):
            for x in range(self.w):
                if self.p[y][x] in skip:
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        v = self.get(x + dx, y + dy)
                        if v not in skip and v != col:
                            out[y][x] = col
                            break
        self.p = out

    def flip_h(self):
        c = Canvas(self.w, self.h)
        c.p = [row[::-1] for row in self.p]
        return c


def to_tiles(cv):
    """Indexed canvas -> 4bpp bytes, 8x8 tiles in row-major (1D sprite order)."""
    out = bytearray()
    for ty in range(cv.h // 8):
        for tx in range(cv.w // 8):
            for y in range(8):
                for x in range(0, 8, 2):
                    a = cv.p[ty * 8 + y][tx * 8 + x] & 15
                    b = cv.p[ty * 8 + y][tx * 8 + x + 1] & 15
                    out.append(a | (b << 4))
    return bytes(out)

def tile_bytes(cv, tx, ty):
    out = bytearray()
    for y in range(8):
        for x in range(0, 8, 2):
            a = cv.p[ty * 8 + y][tx * 8 + x]
            b = cv.p[ty * 8 + y][tx * 8 + x + 1]
            out.append(a | (b << 4))
    return bytes(out)

# --------------------------------------------------------------- shaders --
def lit(dark, mid, light, bias=0.0):
    """Light from the top-left, as in a 3/4 view."""
    def f(nx, ny, x, y):
        s = nx * 0.55 + ny * 0.85 + bias
        if s < -0.45:
            return light
        if s > 0.45:
            return dark
        return mid
    return f

def speckle(cv, x0, y0, x1, y1, colours, amount, only=None):
    for _ in range(amount):
        x = rng.randrange(x0, x1)
        y = rng.randrange(y0, y1)
        if only is None or cv.get(x, y) in only:
            cv.put(x, y, rng.choice(colours))

def shadow(cv, cx, cy, rx, ry):
    cv.ellipse(cx, cy, rx, ry, 2)

def slab(cv, x0, y0, w, d, h, top, edge, front, front_dark):
    """A block seen in 3/4: top face (depth d) then front face (height h)."""
    cv.rect(x0, y0, x0 + w, y0 + d, top)
    cv.rect(x0, y0, x0 + w, y0 + 1, edge)
    cv.rect(x0, y0 + d, x0 + w, y0 + d + h, front)
    cv.rect(x0, y0 + d + h - 1, x0 + w, y0 + d + h, front_dark)

# ----------------------------------------------------------------- floor --
def make_floor():
    """Three 16x16 coco-chip variants. All share the same 2px border band, so
    any variant sits flush against any other on all four sides."""
    base = Canvas(16, 16)
    for y in range(16):
        for x in range(16):
            base.p[y][x] = rng.choice((1, 2, 2, 2, 9))

    def chip(cv, cx, cy, w, h, wrap, interior=False):
        light = rng.choice((5, 6, 6, 7))
        mid = light - 1
        for dy in range(h):
            for dx in range(w):
                if (dx in (0, w - 1)) and (dy in (0, h - 1)) and w > 2 and h > 1:
                    continue  # rounded corners
                x, y = cx + dx, cy + dy
                if wrap:
                    x, y = x % 16, y % 16
                c = mid
                if dy == 0:
                    c = light
                elif dy == h - 1:
                    c = 3
                elif dx == 0 and rng.random() < 0.5:
                    c = light
                cv.put(x, y, c)

    for _ in range(26):
        w, h = rng.randint(2, 5), rng.randint(2, 3)
        chip(base, rng.randrange(16), rng.randrange(16), w, h, wrap=True)
    # fibres
    for _ in range(4):
        x, y = rng.randrange(16), rng.randrange(16)
        ln = rng.randint(3, 6)
        for i in range(ln):
            base.put((x + i) % 16, y, 8)

    variants = [base]
    for v in range(3):
        cv = Canvas(16, 16)
        cv.p = [row[:] for row in base.p]
        # only repaint the interior (2..13) so the borders stay identical
        for y in range(2, 14):
            for x in range(2, 14):
                cv.p[y][x] = rng.choice((1, 2, 2, 2, 9))
        for _ in range(11):
            w, h = rng.randint(2, 4), rng.randint(2, 3)
            chip(cv, rng.randint(2, 14 - w), rng.randint(2, 14 - h), w, h, wrap=False)
        variants.append(cv)
    return variants

def make_grid():
    """16x16 outline cell drawn on BG1: dotted top and left edges."""
    cv = Canvas(16, 16)
    for i in range(16):
        if i % 2 == 0:
            cv.put(i, 0, 10)
            cv.put(0, i, 10)
    return cv

def make_frame():
    """Tank rim drawn on BG0 over the floor: wood top rim with a glass glint,
    thin glass side and bottom edges. Returns (unique tiles, 32x24 map)."""
    cv = Canvas(256, 192)
    for x in range(256):
        cv.put(x, 0, 11)
        cv.put(x, 1, 12)
        cv.put(x, 2, 13 if x % 7 else 12)
        cv.put(x, 3, 11)
        cv.put(x, 190, 15)
        cv.put(x, 191, 11)
    for y in range(192):
        cv.put(0, y, 11)
        cv.put(1, y, 15)
        cv.put(254, y, 15)
        cv.put(255, y, 11)
    for y in range(4, 190):
        if 8 <= y < 40:
            cv.put(3, y, 14)
        if 12 <= y < 28:
            cv.put(252, y, 14)
    tiles = [bytes(32)]
    index = {tiles[0]: 0}
    tmap = []
    for ty in range(24):
        for tx in range(32):
            t = tile_bytes(cv, tx, ty)
            if t not in index:
                index[t] = len(tiles)
                tiles.append(t)
            tmap.append(index[t])
    return tiles, tmap

# ----------------------------------------------------------------- items --
def boulder(cv, cx, cy, rx, ry):
    cv.ellipse(cx, cy, rx, ry, lit(6, 7, 8))
    # flat lit top face
    cv.ellipse(cx - rx * 0.15, cy - ry * 0.35, rx * 0.55, ry * 0.35,
               lambda nx, ny, x, y: 8 if ny < 0.3 else None)
    speckle(cv, int(cx - rx), int(cy - ry), int(cx + rx), int(cy + ry), (6, 7), int(rx * ry / 8), only=(7, 8))
    # a crack
    x, y = cx + rx * 0.3, cy - ry * 0.1
    for i in range(int(ry * 0.6)):
        if cv.get(int(x), int(y)) in (6, 7, 8):
            cv.put(x, y, 6)
        x += rng.choice((-1, 0, 1)) * 0.6
        y += 1

def item_rock_pile():
    cv = Canvas(64, 64)
    shadow(cv, 33, 54, 30, 9)
    boulder(cv, 34, 26, 21, 17)
    boulder(cv, 17, 38, 14, 12)
    boulder(cv, 48, 40, 14, 12)
    boulder(cv, 31, 49, 10, 8)
    # moss on the big rock
    for _ in range(20):
        x = rng.randint(24, 42)
        y = rng.randint(11, 18)
        if cv.get(x, y) in (7, 8):
            cv.put(x, y, rng.choice((10, 11, 9)))
    cv.outline(skip=(0, 2))
    return cv

def item_cave_hide():
    cv = Canvas(64, 64)
    shadow(cv, 32, 55, 31, 8)
    cv.ellipse(32, 36, 30, 24, lit(6, 7, 8))
    cv.rect(2, 36, 62, 58, 0)
    cv.ellipse(32, 36, 30, 22, lambda nx, ny, x, y: (6 if ny > 0.55 else 7) if y >= 36 else None)
    speckle(cv, 4, 12, 60, 58, (6, 8), 140, only=(7,))
    # stacked-stone seams
    for row_y in (24, 34, 44):
        for x in range(4, 60):
            if cv.get(x, row_y) in (7, 8) and (x // 9 + row_y) % 3 != 0:
                cv.put(x, row_y, 6)
    # moss cap
    cv.ellipse(30, 18, 16, 7, lambda nx, ny, x, y: rng.choice((10, 10, 11, 9)))
    cv.ellipse(30, 16, 10, 3, lambda nx, ny, x, y: 11 if rng.random() < 0.6 else None)
    # entrance arch at the front
    cv.ellipse(32, 52, 11, 12, lambda nx, ny, x, y: 1 if y < 57 else None)
    cv.ellipse(32, 52, 8, 9, lambda nx, ny, x, y: 1 if y < 57 else None)
    cv.rect(21, 56, 44, 58, 2)
    cv.outline(skip=(0, 2))
    return cv

def item_log_hide():
    cv = Canvas(32, 32)
    shadow(cv, 16, 27, 15, 5)
    # bark top going back
    cv.ellipse(16, 15, 14, 12, lambda nx, ny, x, y: (5 if nx < -0.4 else 3 if nx > 0.5 else 4) if y < 22 else None)
    for x in range(4, 29, 3):
        for y in range(4, 22):
            if cv.get(x, y) in (4, 5, 3) and rng.random() < 0.7:
                cv.put(x, y, 3)
    # cut face (front) with growth rings and the opening
    cv.ellipse(16, 22, 13, 8, lambda nx, ny, x, y: 5 if y < 27 else None)
    cv.ellipse(16, 22, 11, 6, lambda nx, ny, x, y: 4 if y < 27 else None)
    cv.ellipse(16, 23, 8, 5, lambda nx, ny, x, y: 1 if y < 27 else None)
    cv.rect(3, 27, 30, 28, 0)
    cv.outline(skip=(0, 2))
    return cv

def item_corner_ledge():
    cv = Canvas(64, 64)
    shadow(cv, 30, 56, 30, 7)
    slab(cv, 0, 4, 60, 18, 7, 7, 8, 6, 1)
    slab(cv, 0, 22, 46, 14, 7, 7, 8, 6, 1)
    slab(cv, 0, 36, 30, 12, 7, 7, 8, 6, 1)
    speckle(cv, 0, 4, 60, 56, (6, 8), 120, only=(7,))
    # moss tufts along the edges
    for (x0, x1, y) in ((2, 58, 5), (2, 44, 23), (2, 28, 37)):
        for x in range(x0, x1):
            if rng.random() < 0.35:
                cv.put(x, y, rng.choice((10, 11)))
                if rng.random() < 0.5:
                    cv.put(x, y + 1, 9)
    # a branch climbing the corner
    cv.line(6, 50, 52, 6, 4, thick=3)
    cv.line(7, 49, 51, 7, 5, thick=1)
    cv.line(30, 28, 44, 30, 4, thick=2)
    for (x, y) in ((44, 30), (52, 6), (20, 38)):
        cv.ellipse(x + 2, y - 1, 4, 3, lambda nx, ny, xx, yy: 11 if ny < 0 else 10)
    cv.outline(skip=(0, 2))
    return cv

def bowl(cv, cx, cy, rx, ry, depth, front, rim, water=True):
    cv.ellipse(cx, cy + depth, rx, ry, front)
    cv.rect(cx - rx, cy, cx + rx + 1, cy + depth, 0)
    for y in range(int(cy), int(cy + depth) + 1):
        for x in range(int(cx - rx) + 1, int(cx + rx)):
            cv.put(x, y, front)
    cv.ellipse(cx, cy, rx, ry, rim)
    if water:
        cv.ellipse(cx, cy + 0.5, rx - 3, ry - 2.5, lambda nx, ny, x, y: 13 if ny < -0.5 else 12)
        cv.ellipse(cx - rx * 0.35, cy - ry * 0.2, rx * 0.22, 1.2, 13)
        cv.put(cx + rx * 0.3, cy + ry * 0.2, 14)

def item_round_bowl():
    cv = Canvas(32, 32)
    shadow(cv, 16, 26, 15, 5)
    bowl(cv, 16, 15, 13, 9, 7, 8, 14)
    cv.outline(skip=(0, 2))
    return cv

def item_stone_dish():
    cv = Canvas(32, 32)
    shadow(cv, 16, 26, 15, 5)
    bowl(cv, 16, 16, 14, 9, 5, 6, 7)
    speckle(cv, 2, 6, 30, 28, (6, 8), 40, only=(7,))
    cv.outline(skip=(0, 2))
    return cv

def item_soak_tub():
    cv = Canvas(64, 64)
    shadow(cv, 32, 56, 31, 7)
    bowl(cv, 32, 30, 29, 20, 14, 3, 5)
    # plank seams on the front face
    for x in range(8, 60, 7):
        for y in range(34, 64):
            if cv.get(x, y) == 3:
                cv.put(x, y, 1)
    # rim highlight
    for x in range(10, 54):
        if cv.get(x, 11) == 5:
            cv.put(x, 11, 14)
    cv.outline(skip=(0, 2))
    return cv

def leaf(cv, x, y, ang, length, width, dark=9, mid=10, light=11):
    ca, sa = math.cos(ang), math.sin(ang)
    for i in range(int(length * 2)):
        t = i / (length * 2)
        w = width * math.sin(math.pi * min(1, t * 1.15))
        px = x + ca * t * length
        py = y + sa * t * length
        for j in range(-int(w), int(w) + 1):
            qx = px - sa * j * 0.8
            qy = py + ca * j * 0.8
            c = light if j < 0 else mid
            if abs(j) >= int(w):
                c = dark
            cv.put(qx, qy, c)
        cv.put(px, py, dark if t > 0.15 else mid)

def item_fern():
    cv = Canvas(16, 16)
    shadow(cv, 8, 13, 6, 2)
    for a in (-2.6, -2.1, -1.57, -1.05, -0.55, -3.0, -0.15):
        ln = rng.uniform(6, 8)
        x0, y0 = 8, 12
        for i in range(int(ln)):
            x = x0 + math.cos(a) * i
            y = y0 + math.sin(a) * i
            cv.put(x, y, 9)
            if i > 1 and i % 2 == 0:
                cv.put(x - math.sin(a) * 1.5, y + math.cos(a) * 1.5, 10)
                cv.put(x + math.sin(a) * 1.5, y - math.cos(a) * 1.5, 11)
    cv.outline(skip=(0, 2))
    return cv

def item_grass():
    cv = Canvas(16, 16)
    shadow(cv, 8, 14, 6, 2)
    for i in range(9):
        x = 3 + i * 1.2
        h = rng.randint(6, 11)
        lean = rng.uniform(-0.35, 0.35)
        for j in range(h):
            cv.put(x + lean * j, 14 - j, (9, 10, 11)[min(2, j * 3 // h)])
    cv.outline(skip=(0, 2))
    return cv

def item_moss():
    cv = Canvas(16, 16)
    cv.ellipse(8, 9, 7, 5, lambda nx, ny, x, y: rng.choice((9, 10, 10, 11)) if ny > -0.4 else rng.choice((10, 11, 11)))
    for _ in range(4):
        cv.put(rng.randint(3, 12), rng.randint(5, 12), 15)
    cv.outline(skip=(0, 2))
    return cv

def item_succulent():
    cv = Canvas(16, 16)
    shadow(cv, 8, 13, 6, 2)
    for k in range(8):
        a = k * math.pi / 4 + 0.3
        leaf(cv, 8, 9, a, 6, 1.6, 9, 10, 11)
    for k in range(5):
        a = k * 2 * math.pi / 5
        leaf(cv, 8, 9, a, 3.5, 1.2, 10, 11, 11)
    for k in range(8):
        a = k * math.pi / 4 + 0.3
        cv.put(8 + math.cos(a) * 5.5, 9 + math.sin(a) * 5.5, 15)
    cv.outline(skip=(0, 2))
    return cv

def item_monstera():
    cv = Canvas(32, 32)
    shadow(cv, 16, 27, 13, 4)
    for (a, ln) in ((-2.4, 12), (-0.8, 12), (-1.6, 13), (-2.9, 10), (-0.25, 10)):
        x0, y0 = 16, 25
        ex, ey = x0 + math.cos(a) * ln, y0 + math.sin(a) * ln
        cv.line(x0, y0, ex, ey, 9)
        cv.ellipse(ex, ey, 6, 5, lit(9, 10, 11))
        # splits in the leaf
        for s in (-1, 1):
            cv.line(ex, ey, ex + s * 5, ey + 2, 0)
    cv.outline(skip=(0, 2))
    return cv

def item_pothos():
    cv = Canvas(32, 32)
    shadow(cv, 16, 26, 14, 4)
    path = [(4, 24), (8, 18), (14, 20), (18, 13), (24, 16), (28, 9)]
    for (a, b) in zip(path, path[1:]):
        cv.line(a[0], a[1], b[0], b[1], 9)
    for i, (x, y) in enumerate(path):
        cv.ellipse(x + 1, y + 1, 3.5, 3, lit(9, 10, 11))
        cv.put(x + 1, y + 3, 9)
        if i % 2:
            cv.put(x, y, 14)  # variegation
    cv.outline(skip=(0, 2))
    return cv

def item_bromeliad():
    cv = Canvas(32, 32)
    shadow(cv, 16, 27, 13, 4)
    for k in range(9):
        a = math.pi + k * math.pi / 8
        leaf(cv, 16, 22, a, rng.uniform(10, 13), 2.2)
    for k in range(3):
        leaf(cv, 16, 22, -math.pi / 2 + (k - 1) * 0.35, 9, 1.8, 15, 15, 14)
    cv.outline(skip=(0, 2))
    return cv

def item_cork_flat():
    cv = Canvas(32, 32)
    shadow(cv, 16, 25, 15, 4)
    # a curved slab of cork bark lying flat
    cv.ellipse(16, 16, 14, 8, lambda nx, ny, x, y: 5 if ny < -0.2 else 4)
    cv.ellipse(16, 20, 14, 5, lambda nx, ny, x, y: 3 if y > 18 else None)
    for _ in range(40):
        x, y = rng.randint(3, 29), rng.randint(9, 22)
        if cv.get(x, y) in (4, 5):
            cv.put(x, y, 3)
    cv.outline(skip=(0, 2))
    return cv

# (C name, label, w tiles, h tiles, kind, price, maker)
# kind: 0 passable, 1 enterable bowl, 2 hide, 3 solid, 4 solid corner-only
ITEMS = [
    ("LOG_HIDE",    "Log hide",      2, 2, 2, 0,   item_log_hide),
    ("CAVE_HIDE",   "Stone cave",    4, 4, 2, 60,  item_cave_hide),
    ("ROCK_PILE",   "Rock pile",     4, 4, 3, 40,  item_rock_pile),
    ("CORNER",      "Corner ledge",  4, 4, 4, 50,  item_corner_ledge),
    ("BOWL",        "Water bowl",    2, 2, 1, 0,   item_round_bowl),
    ("STONE_DISH",  "Stone dish",    2, 2, 1, 25,  item_stone_dish),
    ("SOAK_TUB",    "Soaking tub",   4, 4, 1, 80,  item_soak_tub),
    ("CORK",        "Cork bark",     2, 2, 3, 15,  item_cork_flat),
    ("FERN",        "Fern",          1, 1, 0, 0,   item_fern),
    ("GRASS",       "Grass tuft",    1, 1, 0, 0,   item_grass),
    ("MOSS",        "Moss patch",    1, 1, 0, 5,   item_moss),
    ("SUCCULENT",   "Succulent",     1, 1, 0, 10,  item_succulent),
    ("MONSTERA",    "Monstera",      2, 2, 0, 30,  item_monstera),
    ("POTHOS",      "Pothos vine",   2, 2, 0, 20,  item_pothos),
    ("BROMELIAD",   "Bromeliad",     2, 2, 0, 35,  item_bromeliad),
]

# ----------------------------------------------------------------- snake --
def snake_body(radius, variant):
    cv = Canvas(16, 16)
    c = 8
    def sh(nx, ny, x, y):
        s = nx * 0.4 + ny * 0.9
        col = 4 if s < -0.4 else 2 if s > 0.5 else 3
        if ny > 0.7:
            col = 7
        d = math.hypot(nx * 1.1, ny + 0.15)
        if variant == 0 and d < 0.55 and ny < 0.6:
            col = 5 if d > 0.3 else 6
        if variant == 1 and abs(abs(nx) - 0.6) < 0.25 and abs(ny) < 0.4:
            col = 5
        return col
    cv.ellipse(c, c, radius, radius * 0.92, sh)
    # No full outline: segments overlap along the body, and a ring around each
    # one makes the snake look like a string of beads. Only the shadow side and
    # the far edge get a dark rim, so neighbouring segments blend together.
    rim = Canvas(16, 16)
    rim.p = [row[:] for row in cv.p]
    for y in range(16):
        for x in range(16):
            if cv.p[y][x] == 0:
                continue
            ny = (y + 0.5 - c) / (radius * 0.92)
            edge = any(cv.get(x + dx, y + dy) == 0 for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            if edge:
                rim.p[y][x] = 1 if ny > 0.1 else 2
    return rim

def snake_head(angle, tongue):
    cv = Canvas(16, 16)
    ca, sa = math.cos(angle), math.sin(angle)
    L, W = 6.2, 4.4
    for y in range(16):
        for x in range(16):
            dx, dy = x + 0.5 - 8, y + 0.5 - 8
            u = dx * ca + dy * sa + 1.0
            v = -dx * sa + dy * ca
            w = W * (1 - 0.3 * max(0.0, u) / L)
            if (u / L) ** 2 + (v / w) ** 2 <= 1:
                s = dy / 4.4
                col = 4 if s < -0.35 else 2 if s > 0.55 else 3
                if -3.5 < u < 1.5 and abs(v) < 1.3:
                    col = 5          # dark crown marking
                if u > 1.5 and abs(abs(v) - 2.6) < 0.9 and u < 4.2:
                    col = 5          # eye stripe
                if abs(u - 2.4) < 0.85 and abs(abs(v) - 2.7) < 0.85:
                    col = 8          # eyes
                cv.p[y][x] = col
    # eye shine on the eye nearest the light
    best = None
    for y in range(16):
        for x in range(16):
            if cv.p[y][x] == 8 and (best is None or x + y < best[0] + best[1]):
                best = (x, y)
    if best:
        cv.p[best[1]][best[0]] = 10
    if tongue:
        tx, ty = 8 + ca * 6.2, 8 + sa * 6.2
        for i in range(3):
            cv.put(tx + ca * i, ty + sa * i, 9)
        ex, ey = tx + ca * 3, ty + sa * 3
        for s in (-1, 1):
            cv.put(ex + ca * 1 - sa * s, ey + sa * 1 + ca * s, 9)
    cv.outline(skip=(0, 9))
    return cv

# ------------------------------------------------------------- fx / rat --
def rat(frame):
    cv = Canvas(16, 16)
    cv.ellipse(8, 14, 6, 1.5, 0)  # no shadow, keeps it small
    # tail
    for i in range(7):
        cv.put(3 - i * 0.5, 12 - math.sin(i * 0.7 + frame) * 1.2, 4)
    cv.ellipse(7, 10.5, 5, 3.5, lambda nx, ny, x, y: 3 if ny < 0.3 else 2)
    cv.ellipse(12, 9.5, 3, 2.6, lambda nx, ny, x, y: 3 if ny < 0.4 else 2)
    cv.ellipse(11, 6.8, 1.6, 1.6, 4)   # ear
    cv.put(15, 10, 4)                  # nose
    cv.put(13, 9, 5)                   # eye
    feet = (5, 9) if frame == 0 else (6, 10)
    for fx in feet:
        cv.put(fx, 14, 4)
    cv.outline(skip=(0, 4))
    return cv

def shed_skin():
    cv = Canvas(16, 16)
    for i in range(26):
        t = i / 25
        x = 1.5 + t * 13
        y = 8 + math.sin(t * 7) * 3.5
        cv.put(x, y, 6)
        cv.put(x, y + 1, 7 if i % 3 else 6)
    cv.outline(col=7, skip=(0,))
    return cv

def dropping():
    cv = Canvas(16, 16)
    cv.ellipse(7, 10, 4, 2.5, lambda nx, ny, x, y: 9 if ny < 0 else 8)
    cv.ellipse(11, 9, 2, 1.6, 10)
    cv.outline()
    return cv

def mist_drop():
    cv = Canvas(8, 8)
    cv.ellipse(4, 4.5, 2.5, 2.5, lambda nx, ny, x, y: 11 if nx + ny < -0.3 else 12)
    cv.put(4, 1, 12)
    return cv

def cursor_corner():
    cv = Canvas(8, 8)
    for i in range(6):
        cv.put(i + 1, 1, 13)
        cv.put(1, i + 1, 13)
        cv.put(i + 1, 2, 13)
        cv.put(2, i + 1, 13)
    cv.outline(col=14)
    return cv

def coin():
    cv = Canvas(8, 8)
    cv.ellipse(4, 4, 3, 3, lambda nx, ny, x, y: 13 if nx + ny < -0.6 else 15)
    cv.outline(col=14)
    return cv

def heart():
    cv = Canvas(8, 8)
    for (x, y) in ((2, 2), (5, 2)):
        cv.ellipse(x + 0.5, y + 1, 1.8, 1.8, 4)
    for y in range(3, 7):
        for x in range(1 + (y - 3), 7 - (y - 3)):
            cv.put(x, y, 4)
    cv.outline(col=14)
    return cv

# ----------------------------------------------------------------- emit --
def c_array(name, data, ctype="u8", per_line=16):
    lines = []
    for i in range(0, len(data), per_line):
        chunk = data[i:i + per_line]
        if ctype == "u8":
            lines.append("    " + ",".join("0x%02x" % b for b in chunk) + ",")
        else:
            lines.append("    " + ",".join("0x%04x" % b for b in chunk) + ",")
    return "const %s %s[%d] __attribute__((aligned(4))) = {\n%s\n};\n" % (ctype, name, len(data), "\n".join(lines))

def main():
    floors = make_floor()
    grid = make_grid()
    frame_tiles, frame_map = make_frame()

    items = []
    for (cname, label, w, h, kind, price, maker) in ITEMS:
        cv = maker()
        items.append((cname, label, w, h, kind, price, cv))

    bodies = []
    for variant in (0, 1):
        for r in (6.5, 6.0, 5.5, 4.5, 3.5, 2.5):
            bodies.append(snake_body(r, variant))
    lump = snake_body(7.6, 0)
    heads = [snake_head(i * 2 * math.pi / 16, t) for t in (False, True) for i in range(16)]
    rats = [rat(0), rat(1)]

    c = ['#include "art.h"', ""]
    h = ["#pragma once", "#include <stdint.h>", "typedef uint8_t u8;", "typedef uint16_t u16;", ""]

    def emit(name, data, ctype="u8"):
        c.append(c_array(name, data, ctype))
        h.append("extern const %s %s[%d];" % (ctype, name, len(data)))

    emit("pal_floor_dry", [rgb15(x) for x in FLOOR_DRY], "u16")
    emit("pal_floor_damp", [rgb15(x) for x in FLOOR_DAMP], "u16")
    emit("pal_items", [rgb15(x) for x in ITEM_PAL], "u16")
    emit("pal_snake_default", [rgb15(x) for x in SNAKE_PAL], "u16")
    emit("pal_fx", [rgb15(x) for x in FX_PAL], "u16")
    emit("pal_fx_red", [rgb15(x) for x in FX_PAL_RED], "u16")

    floor_bytes = b"".join(to_tiles(f) for f in floors)
    emit("gfx_floor", floor_bytes)
    h.append("#define FLOOR_VARIANTS %d" % len(floors))
    emit("gfx_grid", to_tiles(grid))
    emit("gfx_frame_tiles", b"".join(frame_tiles))
    h.append("#define FRAME_TILE_COUNT %d" % len(frame_tiles))
    emit("map_frame", frame_map, "u16")

    h.append("")
    h.append("enum ItemKind { KIND_PLANT = 0, KIND_BOWL = 1, KIND_HIDE = 2, KIND_SOLID = 3, KIND_CORNER = 4 };")
    h.append("enum ItemType {")
    for i, it in enumerate(items):
        h.append("    ITEM_%s = %d," % (it[0], i))
    h.append("    ITEM_COUNT = %d" % len(items))
    h.append("};")
    h.append("typedef struct { const char *name; u8 w, h, kind; u16 price; const u8 *gfx; u16 gfx_size; u8 spr_w, spr_h; } ItemDef;")
    h.append("extern const ItemDef item_defs[ITEM_COUNT];")
    defs = []
    for (cname, label, w, hh, kind, price, cv) in items:
        name = "gfx_item_" + cname.lower()
        data = to_tiles(cv)
        c.append(c_array(name, data))
        defs.append('    {"%s", %d, %d, %d, %d, %s, %d, %d, %d},' % (label, w, hh, kind, price, name, len(data), cv.w, cv.h))
    c.append("const ItemDef item_defs[ITEM_COUNT] = {\n" + "\n".join(defs) + "\n};\n")

    emit("gfx_snake_body", b"".join(to_tiles(b) for b in bodies))
    h.append("#define SNAKE_BODY_SIZES 6")
    emit("gfx_snake_lump", to_tiles(lump))
    emit("gfx_snake_head", b"".join(to_tiles(x) for x in heads))
    h.append("#define SNAKE_HEAD_DIRS 16")
    emit("gfx_rat", b"".join(to_tiles(x) for x in rats))
    emit("gfx_shed", to_tiles(shed_skin()))
    emit("gfx_dropping", to_tiles(dropping()))
    emit("gfx_mist", to_tiles(mist_drop()))
    emit("gfx_cursor", to_tiles(cursor_corner()))
    emit("gfx_coin", to_tiles(coin()))
    emit("gfx_heart", to_tiles(heart()))

    with open(os.path.join(ROOT, "source", "art.c"), "w") as f:
        f.write("// Generated by tools/make_art.py. Do not edit by hand.\n" + "\n".join(c))
    with open(os.path.join(ROOT, "include", "art.h"), "w") as f:
        f.write("// Generated by tools/make_art.py. Do not edit by hand.\n" + "\n".join(h) + "\n")

    # ---- preview sheet
    try:
        from PIL import Image
    except ImportError:
        return
    sheet = []
    def add(cv, pal):
        sheet.append((cv, pal))
    for f in floors:
        add(f, FLOOR_DRY)
    add(floors[0], FLOOR_DAMP)
    for it in items:
        add(it[6], ITEM_PAL)
    for b in bodies + [lump]:
        add(b, SNAKE_PAL)
    for hd in heads:
        add(hd, SNAKE_PAL)
    for r in rats:
        add(r, FX_PAL)
    for cv in (shed_skin(), dropping(), mist_drop(), cursor_corner(), coin(), heart()):
        add(cv, FX_PAL)
    W = 640
    x = y = rowh = 0
    placed = []
    for cv, pal in sheet:
        if x + cv.w * 2 + 4 > W // 2:
            x, y, rowh = 0, y + rowh + 4, 0
        placed.append((x, y, cv, pal))
        x += cv.w + 4
        rowh = max(rowh, cv.h)
    H = y + rowh + 4
    img = Image.new("RGB", (W // 2, H), (60, 60, 70))
    for (px, py, cv, pal) in placed:
        for yy in range(cv.h):
            for xx in range(cv.w):
                v = cv.p[yy][xx]
                if v:
                    img.putpixel((px + xx, py + yy), pal[v])
    # tile the floor 6x4 with mixed variants to show the seams are flush
    floor_img = Image.new("RGB", (96, 64))
    for ty in range(4):
        for tx in range(6):
            fv = floors[rng.randrange(3)]
            for yy in range(16):
                for xx in range(16):
                    floor_img.putpixel((tx * 16 + xx, ty * 16 + yy), FLOOR_DRY[fv.p[yy][xx]])
    out = Image.new("RGB", (W // 2, H + 70), (60, 60, 70))
    out.paste(img, (0, 0))
    out.paste(floor_img, (2, H + 2))
    out = out.resize((out.width * 3, out.height * 3), Image.NEAREST)
    out.save(os.path.join(ROOT, "tools", "art_preview.png"))

if __name__ == "__main__":
    main()
