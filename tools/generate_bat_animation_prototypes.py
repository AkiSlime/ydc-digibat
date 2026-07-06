from pathlib import Path
import struct
import zlib


ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "assets" / "tamagotchi" / "bat_prototypes"
FRAME = 32
FRAMES = 4


class Canvas:
    def __init__(self, width, height):
        self.width = width
        self.height = height
        self.pixels = [[0 for _ in range(width)] for _ in range(height)]

    def px(self, x, y):
        if 0 <= x < self.width and 0 <= y < self.height:
            self.pixels[y][x] = 1

    def line(self, x0, y0, x1, y1):
        dx = abs(x1 - x0)
        sx = 1 if x0 < x1 else -1
        dy = -abs(y1 - y0)
        sy = 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            self.px(x0, y0)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def poly(self, pts):
        for a, b in zip(pts, pts[1:]):
            self.line(a[0], a[1], b[0], b[1])

    def paste(self, other, ox, oy):
        for y in range(other.height):
            for x in range(other.width):
                if other.pixels[y][x]:
                    self.px(ox + x, oy + y)

    def scaled(self, factor):
        out = Canvas(self.width * factor, self.height * factor)
        for y in range(self.height):
            for x in range(self.width):
                if self.pixels[y][x]:
                    for yy in range(factor):
                        for xx in range(factor):
                            out.px(x * factor + xx, y * factor + yy)
        return out


def write_png(path, canvas):
    def chunk(kind, data):
        return (
            struct.pack(">I", len(data))
            + kind
            + data
            + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
        )

    raw = bytearray()
    for row in canvas.pixels:
        raw.append(0)
        for bit in row:
            raw.extend((255, 255, 255) if bit else (0, 0, 0))

    ihdr = struct.pack(">IIBBBBB", canvas.width, canvas.height, 8, 2, 0, 0, 0)
    data = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )
    path.write_bytes(data)


def draw_sleeping(c, f):
    sway = [0, -1, 0, 1][f]
    cx = 16 + sway
    c.line(cx - 2, 4, cx + 2, 4)
    c.line(cx - 2, 4, cx - 4, 6)
    c.line(cx + 2, 4, cx + 4, 6)
    c.line(cx - 1, 4, cx - 2, 8)
    c.line(cx + 1, 4, cx + 2, 8)

    c.poly([(cx - 3, 7), (cx - 8, 10), (cx - 11, 16), (cx - 8, 22), (cx - 3, 26)])
    c.poly([(cx + 3, 7), (cx + 8, 10), (cx + 11, 16), (cx + 8, 22), (cx + 3, 26)])
    c.poly([(cx - 3, 9), (cx - 5, 15), (cx - 3, 22)])
    c.poly([(cx + 3, 9), (cx + 5, 15), (cx + 3, 22)])
    c.line(cx - 3, 26, cx + 3, 26)

    c.line(cx - 4, 17, cx - 2, 15)
    c.line(cx + 4, 17, cx + 2, 15)
    c.line(cx - 4, 19, cx - 2, 19)
    c.line(cx + 2, 19, cx + 4, 19)
    if f in (1, 2):
        c.px(cx + 9, 7)
        c.line(cx + 11, 5, cx + 14, 5)
        c.line(cx + 14, 5, cx + 11, 8)
        c.line(cx + 11, 8, cx + 14, 8)


def draw_hunting(c, f):
    dx = [-2, 0, 2, 0][f]
    dy = [0, -1, 0, 1][f]
    cx = 16 + dx
    cy = 16 + dy
    c.poly([(cx - 1, cy - 7), (cx - 8, cy - 10), (cx - 15, cy - 5), (cx - 12, cy - 1), (cx - 15, cy + 3), (cx - 7, cy + 5)])
    c.poly([(cx + 1, cy - 7), (cx + 8, cy - 10), (cx + 15, cy - 5), (cx + 12, cy - 1), (cx + 15, cy + 3), (cx + 7, cy + 5)])
    c.line(cx - 7, cy + 5, cx - 3, cy + 8)
    c.line(cx + 7, cy + 5, cx + 3, cy + 8)
    c.line(cx - 3, cy - 6, cx - 1, cy - 9)
    c.line(cx + 3, cy - 6, cx + 1, cy - 9)
    c.line(cx - 3, cy - 3, cx + 3, cy - 3)
    c.line(cx - 3, cy + 6, cx + 3, cy + 6)
    c.line(cx - 4, cy, cx - 2, cy + 1)
    c.line(cx + 4, cy, cx + 2, cy + 1)
    c.px(cx - 1, cy + 2)
    c.px(cx + 1, cy + 2)
    if f == 2:
        c.line(cx - 2, cy + 5, cx, cy + 7)
        c.line(cx + 2, cy + 5, cx, cy + 7)
    prey = [(27, 21), (25, 20), (23, 19), (26, 22)][f]
    c.px(prey[0], prey[1])
    c.px(prey[0] + 1, prey[1] - 1)


def draw_eating(c, f):
    cx = 16
    cy = 15 + [0, 1, 0, 1][f]
    c.poly([(cx - 2, cy - 7), (cx - 7, cy - 5), (cx - 12, cy), (cx - 9, cy + 7), (cx - 3, cy + 8)])
    c.poly([(cx + 2, cy - 7), (cx + 7, cy - 5), (cx + 12, cy), (cx + 9, cy + 7), (cx + 3, cy + 8)])
    c.line(cx - 3, cy - 5, cx - 5, cy - 8)
    c.line(cx + 3, cy - 5, cx + 5, cy - 8)
    c.line(cx - 4, cy - 2, cx - 2, cy - 2)
    c.line(cx + 2, cy - 2, cx + 4, cy - 2)
    if f % 2 == 0:
        c.line(cx - 1, cy + 2, cx + 1, cy + 2)
    else:
        c.px(cx, cy + 3)
        c.px(cx - 1, cy + 2)
        c.px(cx + 1, cy + 2)
    c.line(cx - 2, cy + 8, cx - 1, cy + 11)
    c.line(cx + 2, cy + 8, cx + 1, cy + 11)
    foods = [
        [(15, 25), (16, 25), (17, 25), (15, 26), (17, 26), (16, 27)],
        [(15, 25), (16, 25), (17, 25), (16, 26)],
        [(16, 25), (17, 25), (16, 26)],
        [(16, 25)],
    ][f]
    for x, y in foods:
        c.px(x, y)


def draw_idle_variant(c, f):
    cx = 16
    cy = [15, 16, 15, 14][f]
    spread = [0, 1, 0, -1][f]
    c.line(cx - 1, cy - 8, cx - 2, cy - 11)
    c.line(cx + 1, cy - 8, cx + 2, cy - 11)
    c.poly([(cx - 2, cy - 6), (cx - 7, cy - 8 - spread), (cx - 14, cy - 4), (cx - 11, cy), (cx - 15, cy + 4), (cx - 7, cy + 6)])
    c.poly([(cx + 2, cy - 6), (cx + 7, cy - 8 - spread), (cx + 14, cy - 4), (cx + 11, cy), (cx + 15, cy + 4), (cx + 7, cy + 6)])
    c.line(cx - 5, cy - 4, cx + 5, cy - 4)
    c.line(cx - 4, cy + 5, cx + 4, cy + 5)
    if f == 1:
        c.line(cx - 4, cy, cx - 2, cy)
        c.line(cx + 2, cy, cx + 4, cy)
    elif f == 2:
        c.px(cx - 3, cy)
        c.px(cx + 5, cy)
    else:
        c.px(cx - 4, cy)
        c.px(cx + 4, cy)
    c.px(cx - 1, cy + 2)
    c.px(cx + 1, cy + 2)
    c.line(cx - 2, cy + 7, cx - 4, cy + 9)
    c.line(cx + 2, cy + 7, cx + 4, cy + 9)


ANIMATIONS = [
    ("sleeping", draw_sleeping),
    ("hunting", draw_hunting),
    ("eating", draw_eating),
    ("idle_variant", draw_idle_variant),
]


def make_animation(drawer):
    sheet = Canvas(FRAME * FRAMES, FRAME)
    for f in range(FRAMES):
        frame = Canvas(FRAME, FRAME)
        drawer(frame, f)
        sheet.paste(frame, f * FRAME, 0)
    return sheet


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    master = Canvas(FRAME * FRAMES, FRAME * len(ANIMATIONS))
    for row, (name, drawer) in enumerate(ANIMATIONS):
        sheet = make_animation(drawer)
        master.paste(sheet, 0, row * FRAME)
        write_png(OUT_DIR / f"bat_{name}_prototype.png", sheet)
        write_png(OUT_DIR / f"bat_{name}_prototype_8x.png", sheet.scaled(8))
    write_png(OUT_DIR / "bat_new_animation_prototypes.png", master)
    write_png(OUT_DIR / "bat_new_animation_prototypes_8x.png", master.scaled(8))


if __name__ == "__main__":
    main()
