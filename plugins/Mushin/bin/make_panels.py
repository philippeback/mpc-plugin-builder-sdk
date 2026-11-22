#!/usr/bin/env python3
"""Generate 1280x628 rounded-panel background PNGs for Mushin's ui.json pages.

Each page has two half-screen zones (top and bottom), each framed by a
rounded-corner rectangle with the section title inserted into the top border.
Pure Python standard library only (zlib + struct).
"""

import math
import os
import struct
import zlib

W = 1280
H = 628

BG = (0x0D, 0x0F, 0x12, 255)
PANEL_FILL = (0x10, 0x14, 0x1A, 255)
BORDER_COL = (0x2E, 0x3A, 0x48, 255)
TITLE_COL = (0x00, 0xE5, 0xFF, 255)

# Clean 5x7 bitmap font (each char is 7 rows of 5 bits, MSB on left)
FONT_5X7 = {
    " ": [0, 0, 0, 0, 0, 0, 0],
    "-": [0, 0, 0, 0b11111, 0, 0, 0],
    "&": [0b01100, 0b10010, 0b10100, 0b01000, 0b10101, 0b10010, 0b01101],
    "0": [0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110],
    "1": [0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
    "2": [0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111],
    "A": [0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001],
    "B": [0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110],
    "C": [0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110],
    "D": [0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110],
    "E": [0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111],
    "F": [0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000],
    "G": [0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110],
    "H": [0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001],
    "I": [0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
    "L": [0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111],
    "M": [0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001],
    "N": [0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001],
    "O": [0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
    "R": [0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001],
    "S": [0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110],
    "T": [0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100],
    "U": [0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
    "V": [0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100],
    "X": [0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001],
    "Y": [0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100],
}

PAGES = [
    ("bg_scurve.png",  "S-CURVE",      "INFO DECAY"),
    ("bg_filter.png",  "FILTER A",     "FILTER B"),
    ("bg_lfo.png",     "LFO 1 & 2",    "NOISE OSC"),
    ("bg_matrix.png",  "LFO 1 MATRIX", "LFO 2 MATRIX"),
    ("bg_sidechn.png", "SIDECHAIN",    "SC ENV & FILT"),
    ("bg_gate.png",    "TRANCE GATE",  "GATE ENV"),
    ("bg_dly.png",     "DELAY",        None),
    ("bg_lim.png",     "LIMITER",      None),
]


def write_png(path, width, height, rgba_bytes):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        raw.extend(rgba_bytes[y * stride : (y + 1) * stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def blend(dst, src, alpha):
    if alpha <= 0.0:
        return dst
    if alpha >= 1.0:
        return src
    inv = 1.0 - alpha
    return (
        int(dst[0] * inv + src[0] * alpha + 0.5),
        int(dst[1] * inv + src[1] * alpha + 0.5),
        int(dst[2] * inv + src[2] * alpha + 0.5),
        255,
    )


def draw_panel(buf, x0, y0, pw, ph, title):
    radius = 10.0
    thickness = 1.5
    scale = 2
    char_w = 5 * scale
    char_h = 7 * scale
    gap = 2 * scale
    pad_x = 10
    label_w = len(title) * (char_w + gap) - gap
    cut_x0 = x0 + 22
    cut_x1 = cut_x0 + label_w + pad_x * 2

    cx = x0 + pw * 0.5
    cy = y0 + ph * 0.5
    hx = pw * 0.5 - radius
    hy = ph * 0.5 - radius

    y_start = max(0, y0 - 2)
    y_end = min(H, y0 + ph + 3)
    x_start = max(0, x0 - 2)
    x_end = min(W, x0 + pw + 3)

    for py in range(y_start, y_end):
        dy = abs(py + 0.5 - cy) - hy
        row_off = py * W
        for px in range(x_start, x_end):
            dx = abs(px + 0.5 - cx) - hx
            qx = max(dx, 0.0)
            qy = max(dy, 0.0)
            dist = math.hypot(qx, qy) + min(max(dx, dy), 0.0) - radius

            # Fill interior
            if dist < 0.0:
                fill_a = min(1.0, -dist)
                idx = (row_off + px) * 4
                cur = (buf[idx], buf[idx + 1], buf[idx + 2], 255)
                col = blend(cur, PANEL_FILL, fill_a)
                buf[idx : idx + 4] = bytes(col)

            # Border stroke (skip top cutout for title)
            in_cutout = (py < y0 + 6) and (cut_x0 <= px <= cut_x1)
            if not in_cutout:
                b_dist = abs(dist) - thickness * 0.5
                if b_dist < 1.0:
                    ba = max(0.0, min(1.0, 1.0 - b_dist))
                    idx = (row_off + px) * 4
                    cur = (buf[idx], buf[idx + 1], buf[idx + 2], 255)
                    col = blend(cur, BORDER_COL, ba)
                    buf[idx : idx + 4] = bytes(col)

    # Draw title centered vertically on top border (y0)
    tx0 = cut_x0 + pad_x
    ty0 = y0 - char_h // 2
    for ci, ch in enumerate(title):
        glyph = FONT_5X7.get(ch.upper(), FONT_5X7[" "])
        gx = tx0 + ci * (char_w + gap)
        for r in range(7):
            bits = glyph[r]
            for c in range(5):
                if bits & (1 << (4 - c)):
                    for sy in range(scale):
                        py = ty0 + r * scale + sy
                        if 0 <= py < H:
                            for sx in range(scale):
                                px = gx + c * scale + sx
                                if 0 <= px < W:
                                    idx = (py * W + px) * 4
                                    buf[idx : idx + 4] = bytes(TITLE_COL)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    assets_dir = os.path.join(os.path.dirname(script_dir), "assets")
    os.makedirs(assets_dir, exist_ok=True)

    for filename, top_title, bot_title in PAGES:
        buf = bytearray(bytes(BG) * (W * H))
        draw_panel(buf, 8, 12, 1264, 300, top_title)
        if bot_title is not None:
            draw_panel(buf, 8, 330, 1264, 292, bot_title)
        if filename == "bg_filter.png":
            draw_panel(buf, 975, 478, 294, 136, "ROUTING")
        out_path = os.path.join(assets_dir, filename)
        write_png(out_path, W, H, buf)
        print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
