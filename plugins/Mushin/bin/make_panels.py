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
    "J": [0b00111, 0b00010, 0b00010, 0b00010, 0b10010, 0b10010, 0b01100],
    "K": [0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001],
    "L": [0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111],
    "M": [0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001],
    "N": [0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001],
    "O": [0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
    "P": [0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000],
    "Q": [0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101],
    "R": [0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001],
    "S": [0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110],
    "T": [0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100],
    "U": [0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
    "V": [0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100],
    "W": [0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001],
    "X": [0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001],
    "Y": [0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100],
    "Z": [0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111],
    ",": [0b00000, 0b00000, 0b00000, 0b00000, 0b00100, 0b00100, 0b01000],
    "/": [0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000],
    ">": [0b10000, 0b01000, 0b00100, 0b00010, 0b00100, 0b01000, 0b10000],
    ":": [0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000],
    ".": [0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00100, 0b00000],
}

PAGES = [
    ("bg_scurve.png",  "S-CURVE",      "INFO DECAY"),
    ("bg_filter.png",  "FILTER A",     "FILTER B"),
    ("bg_lfo.png",     "LFO 1 & 2",    "NOISE OSC"),
    ("bg_matrix.png",  "LFO 1 MATRIX", "LFO 2 MATRIX"),
    ("bg_gate.png",    "TRANCE GATE",  "GATE ENV"),
    ("bg_dly_lim.png", "DELAY",        "LIMITER"),
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


def draw_panel(buf, x0, y0, pw, ph, title, title_col=TITLE_COL):
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
                                    buf[idx : idx + 4] = bytes(title_col)


GATE_PATTERNS = [
    ("gate_pat_0.png", 0xAAAA),
    ("gate_pat_1.png", 0x5555),
    ("gate_pat_2.png", 0xEEEE),
    ("gate_pat_3.png", 0x9999),
    ("gate_pat_4.png", 0xF0F0),
    ("gate_pat_5.png", 0xD7D7),
    ("gate_pat_6.png", 0x8888),
    ("gate_pat_7.png", 0x8912),
]


def generate_gate_patterns(assets_dir):
    gw, gh = 918, 107
    card_bg = (0x10, 0x14, 0x1A, 255)
    step_off = (0x16, 0x1B, 0x22, 255)
    step_on = (0x00, 0xE5, 0xFF, 255)
    step_border = (0x26, 0x2E, 0x3A, 255)

    pad_x = 16
    gap = 8
    step_w = (gw - pad_x * 2 - gap * 15) // 16
    step_h = 56
    sy0 = (gh - step_h) // 2
    sy1 = sy0 + step_h

    for fname, mask in GATE_PATTERNS:
        buf = bytearray(bytes(card_bg) * (gw * gh))
        # Outer subtle card border
        for x in range(gw):
            for y in (0, gh - 1):
                idx = (y * gw + x) * 4
                buf[idx : idx + 4] = bytes(step_border)
        for y in range(gh):
            for x in (0, gw - 1):
                idx = (y * gw + x) * 4
                buf[idx : idx + 4] = bytes(step_border)

        for step in range(16):
            active = (mask & (1 << step)) != 0
            fill = step_on if active else step_off
            sx0 = pad_x + step * (step_w + gap)
            sx1 = sx0 + step_w
            for y in range(sy0, sy1):
                row_off = y * gw
                for x in range(sx0, sx1):
                    is_edge = (x == sx0 or x == sx1 - 1 or y == sy0 or y == sy1 - 1)
                    col = step_border if (is_edge and not active) else fill
                    idx = (row_off + x) * 4
                    buf[idx : idx + 4] = bytes(col)

        out_path = os.path.join(assets_dir, fname)
        write_png(out_path, gw, gh, buf)
        print(f"Wrote {out_path}")


def draw_text_5x7(buf, w, h, x0, y0, text, col, scale=2):
    char_w = 5 * scale
    gap = 2 * scale
    for ci, ch in enumerate(text):
        glyph = FONT_5X7.get(ch.upper(), FONT_5X7[" "])
        gx = x0 + ci * (char_w + gap)
        for r in range(7):
            bits = glyph[r]
            for c in range(5):
                if bits & (1 << (4 - c)):
                    for sy in range(scale):
                        py = y0 + r * scale + sy
                        if 0 <= py < h:
                            for sx in range(scale):
                                px = gx + c * scale + sx
                                if 0 <= px < w:
                                    idx = (py * w + px) * 4
                                    buf[idx : idx + 4] = bytes(col)


def draw_aa_line(buf, w, h, x0, y0, x1, y1, col, thickness=2.0):
    min_x = max(0, int(min(x0, x1) - thickness - 2))
    max_x = min(w - 1, int(max(x0, x1) + thickness + 2))
    min_y = max(0, int(min(y0, y1) - thickness - 2))
    max_y = min(h - 1, int(max(y0, y1) + thickness + 2))
    dx = x1 - x0
    dy = y1 - y0
    len_sq = dx * dx + dy * dy
    if len_sq < 1e-6:
        return
    half_t = thickness * 0.5
    for py in range(min_y, max_y + 1):
        row_off = py * w
        for px in range(min_x, max_x + 1):
            t = max(0.0, min(1.0, ((px + 0.5 - x0) * dx + (py + 0.5 - y0) * dy) / len_sq))
            dist = math.hypot(px + 0.5 - (x0 + t * dx), py + 0.5 - (y0 + t * dy)) - half_t
            if dist < 1.0:
                a = max(0.0, min(1.0, 1.0 - dist)) * (col[3] / 255.0)
                idx = (row_off + px) * 4
                cur = (buf[idx], buf[idx + 1], buf[idx + 2], 255)
                buf[idx : idx + 4] = bytes(blend(cur, col, a))


def draw_polyline(buf, w, h, pts, col, thickness=2.2):
    for i in range(len(pts) - 1):
        draw_aa_line(buf, w, h, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], col, thickness)


def draw_filled_box(buf, w, h, x0, y0, bw, bh, fill_col, border_col=None):
    x1 = min(w, x0 + bw)
    y1 = min(h, y0 + bh)
    x0 = max(0, x0)
    y0 = max(0, y0)
    for py in range(y0, y1):
        row_off = py * w
        for px in range(x0, x1):
            is_edge = border_col and (px == x0 or px == x1 - 1 or py == y0 or py == y1 - 1)
            col = border_col if is_edge else fill_col
            idx = (row_off + px) * 4
            buf[idx : idx + 4] = bytes(col)


def generate_main_bg(assets_dir):
    buf = bytearray(bytes((0x08, 0x0A, 0x0E, 255)) * (W * H))
    cyan = (0x00, 0xE5, 0xFF, 255)
    cyan_glow = (0x00, 0xE5, 0xFF, 90)
    magenta = (0xFF, 0x4D, 0x8D, 255)
    mag_glow = (0xFF, 0x4D, 0x8D, 90)
    grid_col = (0x18, 0x20, 0x2A, 255)
    box_bg = (0x0C, 0x10, 0x16, 255)
    box_border = (0x22, 0x2C, 0x38, 255)

    # Subtle scanlines & micro-grid
    for y in range(0, H, 4):
        for x in range(W):
            idx = (y * W + x) * 4
            buf[idx : idx + 4] = bytes((0x0A, 0x0D, 0x12, 255))

    # Top hero header (right-aligned so it never collides with Bank 1's title)
    draw_text_5x7(buf, W, H, 510, 10, "S-CURVE > FILTER > DECAY > GATE > DLY > LIM", (0x8B, 0x94, 0x9E, 255), scale=1)
    draw_text_5x7(buf, W, H, 886, 6, "MUSHIN // MACRO PERFORMANCE", cyan, scale=2)

    # Bank 1 & Bank 2 outer stylized panels (spaced cleanly clear of Q-Link highlights)
    draw_panel(buf, 8, 24, 1264, 288, "BANK 1 - CORE DESTRUCTION & CHOKE")
    draw_panel(buf, 8, 324, 1264, 296, "BANK 2 - MOTION, SPACE & RHYTHM")

    # Left accent rails
    draw_filled_box(buf, W, H, 8, 38, 4, 260, cyan)
    draw_filled_box(buf, W, H, 8, 338, 4, 268, magenta)

    out_path = os.path.join(assets_dir, "bg_main.png")
    write_png(out_path, W, H, buf)
    print(f"Wrote {out_path}")


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    assets_dir = os.path.join(os.path.dirname(script_dir), "assets")
    os.makedirs(assets_dir, exist_ok=True)
    generate_main_bg(assets_dir)
    generate_gate_patterns(assets_dir)

    cyan = (0x00, 0xE5, 0xFF, 255)
    magenta = (0xFF, 0x4D, 0x8D, 255)
    for filename, top_title, bot_title in PAGES:
        buf = bytearray(bytes(BG) * (W * H))
        draw_panel(buf, 8, 12, 1264, 300, top_title, TITLE_COL)
        if bot_title is not None:
            bot_col = magenta if bot_title in ("FILTER B", "LFO 2 MATRIX") else TITLE_COL
            draw_panel(buf, 8, 320, 1264, 302, bot_title, bot_col)
        if filename == "bg_filter.png":
            draw_filled_box(buf, W, H, 8, 26, 4, 272, cyan)
            draw_filled_box(buf, W, H, 8, 334, 4, 274, magenta)
            draw_panel(buf, 975, 478, 294, 136, "ROUTING", TITLE_COL)
            # Filter A & B Freq x Res XY telemetry bays (Row 1, cols 2-3: 656..1254, y=180..287)
            for bx, col, lbl in [(656, cyan, "A FREQ X RES"), (962, magenta, "B FREQ X RES")]:
                draw_filled_box(buf, W, H, bx, 180, 292, 107, (0x0C, 0x10, 0x16, 255))
                border = (col[0] // 2, col[1] // 2, col[2] // 2, 255)
                draw_filled_box(buf, W, H, bx, 180, 292, 1, border)
                draw_filled_box(buf, W, H, bx, 286, 292, 1, border)
                draw_filled_box(buf, W, H, bx, 180, 1, 107, border)
                draw_filled_box(buf, W, H, bx + 291, 180, 1, 107, border)
                # Subtle log-frequency & resonance grid lines
                for gx in (bx + 73, bx + 146, bx + 219):
                    draw_filled_box(buf, W, H, gx, 181, 1, 105, (0x18, 0x20, 0x2A, 255))
                for gy in (180 + 35, 180 + 70):
                    draw_filled_box(buf, W, H, bx + 1, gy, 290, 1, (0x18, 0x20, 0x2A, 255))
                draw_text_5x7(buf, W, H, bx + 8, 186, lbl, col, scale=1)
        elif filename == "bg_matrix.png":
            draw_filled_box(buf, W, H, 8, 26, 4, 272, cyan)
            draw_filled_box(buf, W, H, 8, 334, 4, 274, magenta)
        elif filename == "bg_lfo.png":
            draw_filled_box(buf, W, H, 16, 44, 606, 3, cyan)
            draw_filled_box(buf, W, H, 656, 44, 606, 3, magenta)
        out_path = os.path.join(assets_dir, filename)
        write_png(out_path, W, H, buf)
        print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
