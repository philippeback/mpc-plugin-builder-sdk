#!/usr/bin/env python3
"""Generate 1280x628 themed architectural background PNGs for Mushin's UI pages.

Each page (except SCOPE, where the live visualizer IS the graphic) receives a
bespoke, unobtrusive sci-fi DSP schematic backdrop rendered in Mushin's Cyan
(#00E5FF) and Magenta (#FF4D8D) accent palette inside the panel frames so
every control and label remains 100% legible. Pure Python stdlib only.
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
CYAN = (0x00, 0xE5, 0xFF, 255)
MAGENTA = (0xFF, 0x4D, 0x8D, 255)
MUTED_GRID = (0x20, 0x2A, 0x36, 255)

# Clean 5x7 bitmap font (each char is 7 rows of 5 bits, MSB on left)
FONT_5X7 = {
    " ": [0, 0, 0, 0, 0, 0, 0],
    "-": [0, 0, 0, 0b11111, 0, 0, 0],
    "&": [0b01100, 0b10010, 0b10100, 0b01000, 0b10101, 0b10010, 0b01101],
    "0": [0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110],
    "1": [0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
    "2": [0b01110, 0b10001, 0b00001, 0b00110, 0b01000, 0b10000, 0b11111],
    "3": [0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110],
    "4": [0b10010, 0b10010, 0b10010, 0b11111, 0b00010, 0b00010, 0b00010],
    "5": [0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110],
    "6": [0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110],
    "7": [0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000],
    "8": [0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110],
    "9": [0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110],
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
    "+": [0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000],
}

PAGES = [
    ("bg_scurve.png",  "S-CURVE",        "INFO DECAY"),
    ("bg_filter.png",  "FILTER A",       "FILTER B"),
    ("bg_lfo.png",     "LFO 1 & 2",      "NOISE OSC"),
    ("bg_matrix.png",  "LFO 1 MATRIX",   "LFO 2 MATRIX"),
    ("bg_gate.png",    "TRANCE GATE",    "GATE ENV"),
    ("bg_pump.png",    "SIDECHAIN PUMP", "DETECTOR FILTER"),
    ("bg_dly_lim.png", "DELAY",          "LIMITER"),
]


def write_png(path, width, height, rgba_bytes):
    def chunk(tag, data):
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        raw.extend(rgba_bytes[y * stride : (y + 1) * stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 6))
        + chunk(b"IEND", b"")
    )
    with open(path, "wb") as f:
        f.write(png)


def blend_px(buf, px, py, src, alpha, clip=None):
    if alpha <= 0.002:
        return
    if clip is not None:
        cx0, cy0, cx1, cy1 = clip
        if px < cx0 or px >= cx1 or py < cy0 or py >= cy1:
            return
    if not (0 <= px < W and 0 <= py < H):
        return
    a = 1.0 if alpha >= 1.0 else alpha
    inv = 1.0 - a
    idx = (py * W + px) * 4
    buf[idx] = int(buf[idx] * inv + src[0] * a + 0.5)
    buf[idx + 1] = int(buf[idx + 1] * inv + src[1] * a + 0.5)
    buf[idx + 2] = int(buf[idx + 2] * inv + src[2] * a + 0.5)
    buf[idx + 3] = 255


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


def draw_panel(buf, x0, y0, pw, ph, title, title_col=TITLE_COL, fill=True):
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

            if fill and dist < 0.0:
                fill_a = min(1.0, -dist)
                idx = (row_off + px) * 4
                cur = (buf[idx], buf[idx + 1], buf[idx + 2], 255)
                col = blend(cur, PANEL_FILL, fill_a)
                buf[idx : idx + 4] = bytes(col)

            in_cutout = (py < y0 + 6) and (cut_x0 <= px <= cut_x1)
            if not in_cutout:
                b_dist = abs(dist) - thickness * 0.5
                if b_dist < 1.0:
                    ba = max(0.0, min(1.0, 1.0 - b_dist))
                    idx = (row_off + px) * 4
                    cur = (buf[idx], buf[idx + 1], buf[idx + 2], 255)
                    col = blend(cur, BORDER_COL, ba)
                    buf[idx : idx + 4] = bytes(col)

    tx0 = cut_x0 + pad_x
    ty0 = y0 - char_h // 2
    draw_text_5x7(buf, W, H, tx0, ty0, title, title_col, scale=scale)


def draw_text_5x7(buf, w, h, x0, y0, text, col, scale=2, alpha=1.0):
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
                                    if alpha >= 0.999:
                                        idx = (py * w + px) * 4
                                        buf[idx : idx + 4] = bytes(col)
                                    else:
                                        blend_px(buf, px, py, col, alpha)


def draw_aa_line(buf, x0, y0, x1, y1, col, thickness=1.5, alpha=1.0, clip=None):
    min_x = max(0, int(min(x0, x1) - thickness - 2))
    max_x = min(W - 1, int(max(x0, x1) + thickness + 2))
    min_y = max(0, int(min(y0, y1) - thickness - 2))
    max_y = min(H - 1, int(max(y0, y1) + thickness + 2))
    if clip is not None:
        min_x = max(min_x, clip[0])
        min_y = max(min_y, clip[1])
        max_x = min(max_x, clip[2] - 1)
        max_y = min(max_y, clip[3] - 1)
    dx = x1 - x0
    dy = y1 - y0
    len_sq = dx * dx + dy * dy
    if len_sq < 1e-6:
        return
    half_t = thickness * 0.5
    base_a = alpha * (col[3] / 255.0)
    for py in range(min_y, max_y + 1):
        row_off = py * W
        for px in range(min_x, max_x + 1):
            t = ((px + 0.5 - x0) * dx + (py + 0.5 - y0) * dy) / len_sq
            if t < 0.0:
                t = 0.0
            elif t > 1.0:
                t = 1.0
            dist = math.hypot(px + 0.5 - (x0 + t * dx), py + 0.5 - (y0 + t * dy)) - half_t
            if dist < 1.0:
                a = (1.0 - dist if dist > 0.0 else 1.0) * base_a
                idx = (row_off + px) * 4
                inv = 1.0 - a
                buf[idx] = int(buf[idx] * inv + col[0] * a + 0.5)
                buf[idx + 1] = int(buf[idx + 1] * inv + col[1] * a + 0.5)
                buf[idx + 2] = int(buf[idx + 2] * inv + col[2] * a + 0.5)


def draw_polyline(buf, pts, col, thickness=1.5, alpha=1.0, glow=0.0, clip=None):
    if glow > 0.0:
        for i in range(len(pts) - 1):
            draw_aa_line(buf, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1],
                         col, thickness=thickness + 5.0, alpha=glow, clip=clip)
    for i in range(len(pts) - 1):
        draw_aa_line(buf, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1],
                     col, thickness=thickness, alpha=alpha, clip=clip)


def draw_dashed_line(buf, x0, y0, x1, y1, col, dash=8.0, gap=6.0, thickness=1.0, alpha=0.14, clip=None):
    dist = math.hypot(x1 - x0, y1 - y0)
    if dist < 1e-4:
        return
    ux = (x1 - x0) / dist
    uy = (y1 - y0) / dist
    pos = 0.0
    while pos < dist:
        end = min(dist, pos + dash)
        draw_aa_line(buf, x0 + ux * pos, y0 + uy * pos, x0 + ux * end, y0 + uy * end,
                     col, thickness=thickness, alpha=alpha, clip=clip)
        pos += dash + gap


def draw_circle(buf, cx, cy, rad, col, thickness=1.4, alpha=0.15, filled=False, clip=None):
    r_pad = rad + thickness + 2.0
    x0 = max(0 if clip is None else clip[0], int(cx - r_pad))
    x1 = min(W if clip is None else clip[2], int(cx + r_pad) + 1)
    y0 = max(0 if clip is None else clip[1], int(cy - r_pad))
    y1 = min(H if clip is None else clip[3], int(cy + r_pad) + 1)
    half_t = thickness * 0.5
    base_a = alpha * (col[3] / 255.0)
    for py in range(y0, y1):
        dy = py + 0.5 - cy
        row_off = py * W
        for px in range(x0, x1):
            d = math.hypot(px + 0.5 - cx, dy)
            if filled:
                diff = d - rad
                if diff < 1.0:
                    a = (1.0 - diff if diff > 0.0 else 1.0) * base_a
                    idx = (row_off + px) * 4
                    inv = 1.0 - a
                    buf[idx] = int(buf[idx] * inv + col[0] * a + 0.5)
                    buf[idx + 1] = int(buf[idx + 1] * inv + col[1] * a + 0.5)
                    buf[idx + 2] = int(buf[idx + 2] * inv + col[2] * a + 0.5)
            else:
                diff = abs(d - rad) - half_t
                if diff < 1.0:
                    a = (1.0 - diff if diff > 0.0 else 1.0) * base_a
                    idx = (row_off + px) * 4
                    inv = 1.0 - a
                    buf[idx] = int(buf[idx] * inv + col[0] * a + 0.5)
                    buf[idx + 1] = int(buf[idx + 1] * inv + col[1] * a + 0.5)
                    buf[idx + 2] = int(buf[idx + 2] * inv + col[2] * a + 0.5)


def draw_radial_aura(buf, cx, cy, rad, col, peak_alpha=0.07, clip=None):
    x0 = max(0 if clip is None else clip[0], int(cx - rad))
    x1 = min(W if clip is None else clip[2], int(cx + rad) + 1)
    y0 = max(0 if clip is None else clip[1], int(cy - rad))
    y1 = min(H if clip is None else clip[3], int(cy + rad) + 1)
    inv_r = 1.0 / max(1.0, rad)
    for py in range(y0, y1):
        dy = py + 0.5 - cy
        row_off = py * W
        for px in range(x0, x1):
            t = math.hypot(px + 0.5 - cx, dy) * inv_r
            if t < 1.0:
                w = (1.0 - t)
                a = peak_alpha * (w * w)
                idx = (row_off + px) * 4
                inv = 1.0 - a
                buf[idx] = int(buf[idx] * inv + col[0] * a + 0.5)
                buf[idx + 1] = int(buf[idx + 1] * inv + col[1] * a + 0.5)
                buf[idx + 2] = int(buf[idx + 2] * inv + col[2] * a + 0.5)


def draw_diamond(buf, cx, cy, r, col, alpha=0.22, fill_alpha=0.08, clip=None):
    if fill_alpha > 0.0:
        x0 = max(0 if clip is None else clip[0], int(cx - r))
        x1 = min(W if clip is None else clip[2], int(cx + r) + 1)
        y0 = max(0 if clip is None else clip[1], int(cy - r))
        y1 = min(H if clip is None else clip[3], int(cy + r) + 1)
        for py in range(y0, y1):
            dy = abs(py + 0.5 - cy)
            for px in range(x0, x1):
                if abs(px + 0.5 - cx) + dy <= r:
                    blend_px(buf, px, py, col, fill_alpha)
    pts = [(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy), (cx, cy - r)]
    draw_polyline(buf, pts, col, thickness=1.2, alpha=alpha, clip=clip)


def draw_grid_bg(buf, x0, y0, x1, y1, step, col, alpha=0.04):
    clip = (x0, y0, x1, y1)
    for gx in range(x0 + step, x1, step):
        for py in range(y0, y1):
            blend_px(buf, gx, py, col, alpha, clip=clip)
    for gy in range(y0 + step, y1, step):
        for px in range(x0, x1):
            blend_px(buf, px, gy, col, alpha, clip=clip)


def fill_curve_wash(buf, xs, ys, base_y, col, top_alpha=0.09, clip=None):
    # Subtle vertical gradient wash between curve ys and horizontal base_y
    n = len(xs)
    if n < 2:
        return
    for i in range(n - 1):
        px0 = int(xs[i])
        px1 = int(xs[i + 1])
        if px1 <= px0:
            continue
        for px in range(px0, px1):
            t = (px - xs[i]) / max(1e-4, xs[i + 1] - xs[i])
            cy = ys[i] + (ys[i + 1] - ys[i]) * t
            ya = int(min(cy, base_y))
            yb = int(max(cy, base_y))
            span = max(1.0, abs(base_y - cy))
            for py in range(ya, yb + 1):
                dist_norm = 1.0 - abs(py - cy) / span
                a = top_alpha * (dist_norm ** 1.4)
                blend_px(buf, px, py, col, a, clip=clip)


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


def generate_gate_step_strips(assets_dir):
    sw, sh = 47, 56
    step_off = (0x16, 0x1B, 0x22, 255)
    step_on = (0x00, 0xE5, 0xFF, 255)
    step_border = (0x26, 0x2E, 0x3A, 255)
    beat_pip_off = (0x45, 0x55, 0x6A, 255)
    beat_pip_on = (0xFF, 0xFF, 0xFF, 255)

    for prefix, is_beat in [("step", False), ("step_beat", True)]:
        for state, active in [("off", False), ("on", True)]:
            buf = bytearray(sw * sh * 4)
            fill = step_on if active else step_off
            for py in range(sh):
                row_off = py * sw
                for px in range(sw):
                    is_edge = (px == 0 or px == sw - 1 or py == 0 or py == sh - 1)
                    col = step_border if (is_edge and not active) else fill
                    if is_beat and 2 <= py <= 5 and 15 <= px <= 31:
                        col = beat_pip_on if active else beat_pip_off
                    idx = (row_off + px) * 4
                    buf[idx : idx + 4] = bytes(col)
            out_path = os.path.join(assets_dir, f"{prefix}_{state}.png")
            write_png(out_path, sw, sh, buf)
            print(f"Wrote {out_path}")


# ---------------------------------------------------------------------------
# Per-Page Architectural Graphics (Unobtrusive Cyber Schematics)
# ---------------------------------------------------------------------------

def generate_main_bg(assets_dir):
    buf = bytearray(bytes((0x08, 0x0A, 0x0E, 255)) * (W * H))
    for y in range(0, H, 4):
        for x in range(W):
            idx = (y * W + x) * 4
            buf[idx : idx + 4] = bytes((0x0A, 0x0D, 0x12, 255))

    draw_text_5x7(buf, W, H, 510, 10, "S-CURVE > FILTER > DECAY > GATE > DLY > LIM", (0x8B, 0x94, 0x9E, 255), scale=1)
    draw_text_5x7(buf, W, H, 886, 6, "MUSHIN // MACRO PERFORMANCE", CYAN, scale=2)

    draw_panel(buf, 8, 24, 1264, 288, "BANK 1 - CORE DESTRUCTION & CHOKE", TITLE_COL)
    draw_panel(buf, 8, 324, 1264, 296, "BANK 2 - MOTION, SPACE & RHYTHM", MAGENTA)

    top_clip = (14, 30, 1266, 306)
    bot_clip = (14, 330, 1266, 614)
    orb_xs = [155, 475, 795, 1115]

    # Top bus architecture & illuminated pockets for Bank 1 orbs
    draw_grid_bg(buf, 14, 30, 1266, 306, 40, CYAN, alpha=0.025)
    for ox in orb_xs:
        draw_radial_aura(buf, ox, 142, 130, CYAN, peak_alpha=0.08, clip=top_clip)
        draw_circle(buf, ox, 142, 104, CYAN, thickness=1.0, alpha=0.10, clip=top_clip)
    draw_aa_line(buf, 26, 142, 1254, 142, CYAN, thickness=1.2, alpha=0.12, clip=top_clip)
    draw_polyline(buf, [(40, 68), (105, 68), (127, 90)], CYAN, thickness=1.0, alpha=0.11, clip=top_clip)
    draw_polyline(buf, [(1240, 68), (1165, 68), (1143, 90)], CYAN, thickness=1.0, alpha=0.11, clip=top_clip)
    for jx in [315, 635, 955]:
        draw_diamond(buf, jx, 142, 7.0, CYAN, alpha=0.22, fill_alpha=0.09, clip=top_clip)

    # Bottom bus architecture & illuminated pockets for Bank 2 orbs
    draw_grid_bg(buf, 14, 330, 1266, 614, 40, MAGENTA, alpha=0.025)
    for ox in orb_xs:
        draw_radial_aura(buf, ox, 444, 130, MAGENTA, peak_alpha=0.08, clip=bot_clip)
        draw_circle(buf, ox, 444, 104, MAGENTA, thickness=1.0, alpha=0.10, clip=bot_clip)
    draw_aa_line(buf, 26, 444, 1254, 444, MAGENTA, thickness=1.2, alpha=0.12, clip=bot_clip)
    draw_polyline(buf, [(40, 520), (105, 520), (127, 498)], MAGENTA, thickness=1.0, alpha=0.11, clip=bot_clip)
    draw_polyline(buf, [(1240, 520), (1165, 520), (1143, 498)], MAGENTA, thickness=1.0, alpha=0.11, clip=bot_clip)
    for jx in [315, 635, 955]:
        draw_diamond(buf, jx, 444, 7.0, MAGENTA, alpha=0.22, fill_alpha=0.09, clip=bot_clip)

    # Draw outer left accent rails cleanly on top
    draw_filled_box(buf, W, H, 8, 38, 4, 260, CYAN)
    draw_filled_box(buf, W, H, 8, 338, 4, 268, MAGENTA)

    out_path = os.path.join(assets_dir, "bg_main.png")
    write_png(out_path, W, H, buf)
    print(f"Wrote {out_path}")


def decorate_scurve(buf):
    # Top (S-CURVE, Cyan): Multi-drive tanh transfer curve & Exhaustion hard-clip asymptote
    top_clip = (14, 18, 1266, 306)
    draw_grid_bg(buf, 14, 18, 1266, 306, 48, CYAN, alpha=0.03)
    cx, cy = 640.0, 162.0
    draw_radial_aura(buf, cx, cy, 240, CYAN, peak_alpha=0.06, clip=top_clip)
    draw_dashed_line(buf, 40, cy, 1240, cy, CYAN, dash=10, gap=8, alpha=0.12, clip=top_clip)
    draw_dashed_line(buf, cx, 26, cx, 298, CYAN, dash=10, gap=8, alpha=0.12, clip=top_clip)
    draw_dashed_line(buf, cx - 220, cy + 120, cx + 220, cy - 120, CYAN, dash=6, gap=6, alpha=0.09, clip=top_clip)

    # Soft-clip S-Curves (gentle + high drive) across full width
    for drive, al, gw in [(1.4, 0.10, 0.0), (3.5, 0.16, 0.04), (9.0, 0.22, 0.06)]:
        pts = []
        for i in range(180):
            u = -1.0 + 2.0 * (i / 179.0)
            px = 60.0 + (1160.0) * (i / 179.0)
            y_val = math.tanh(u * drive)
            py = cy - y_val * 114.0
            pts.append((px, py))
        draw_polyline(buf, pts, CYAN, thickness=1.5, alpha=al, glow=gw, clip=top_clip)
    # Hard-clip exhaustion dashed ceiling/floor lines
    draw_dashed_line(buf, 80, cy - 114, 1200, cy - 114, CYAN, dash=12, gap=10, alpha=0.12, clip=top_clip)
    draw_dashed_line(buf, 80, cy + 114, 1200, cy + 114, CYAN, dash=12, gap=10, alpha=0.12, clip=top_clip)
    draw_diamond(buf, cx, cy, 8.0, CYAN, alpha=0.25, fill_alpha=0.10, clip=top_clip)

    # Bottom-Left (INFO DECAY, Magenta): Bit-crushed staircase overlay on sine wave
    bot_clip = (14, 326, 1266, 614)
    draw_grid_bg(buf, 14, 326, 1266, 614, 48, MAGENTA, alpha=0.028)
    wave_y = 402.0
    xs_smooth, ys_smooth = [], []
    for i in range(160):
        u = i / 159.0
        px = 24.0 + u * 1230.0
        py = wave_y - math.sin(u * 4.0 * math.pi) * 62.0
        xs_smooth.append(px)
        ys_smooth.append(py)
    draw_polyline(buf, list(zip(xs_smooth, ys_smooth)), MAGENTA, thickness=1.2, alpha=0.09, clip=bot_clip)

    # Quantized staircase waveform
    stair_pts = []
    steps = 36
    q_levels = 5.0
    for s in range(steps):
        u0 = s / float(steps)
        u1 = (s + 1) / float(steps)
        mid_u = 0.5 * (u0 + u1)
        raw = math.sin(mid_u * 4.0 * math.pi)
        q = round(raw * q_levels) / q_levels
        x0 = 24.0 + u0 * 1230.0
        x1 = 24.0 + u1 * 1230.0
        py = wave_y - q * 62.0
        stair_pts.append((x0, py))
        stair_pts.append((x1, py))
    draw_polyline(buf, stair_pts, MAGENTA, thickness=1.6, alpha=0.18, glow=0.04, clip=bot_clip)

    # Input Stage subpanel schematic (x: 342..1260, y: 492..608): Auto-Level dB ruler
    in_clip = (342, 488, 1260, 608)
    draw_dashed_line(buf, 360, 548, 1244, 548, MAGENTA, dash=8, gap=6, alpha=0.14, clip=in_clip)
    for k, frac in enumerate([0.12, 0.28, 0.44, 0.60, 0.76, 0.90]):
        tx = int(360 + frac * 880)
        draw_aa_line(buf, tx, 536, tx, 560, MAGENTA, thickness=1.2, alpha=0.16, clip=in_clip)
        if k == 3:
            draw_diamond(buf, tx, 548, 6.0, MAGENTA, alpha=0.25, fill_alpha=0.10, clip=in_clip)


def decorate_filter(buf):
    # Top (FILTER A, Cyan): Resonant Bode LPF + Formant peak schematic on left/full span
    top_clip = (14, 18, 1266, 306)
    draw_grid_bg(buf, 14, 18, 1266, 306, 48, CYAN, alpha=0.03)
    base_y = 212.0
    # Log-frequency vertical ticks
    for frac in (0.08, 0.18, 0.30, 0.45, 0.62, 0.80, 0.93):
        gx = 24.0 + frac * 1230.0
        draw_dashed_line(buf, gx, 26, gx, 300, CYAN, dash=4, gap=8, alpha=0.08, clip=top_clip)
    draw_dashed_line(buf, 24, base_y, 1254, base_y, CYAN, dash=8, gap=6, alpha=0.10, clip=top_clip)

    # Main resonant LPF Bode curve
    xs, ys = [], []
    fc = 0.44
    for i in range(180):
        u = i / 179.0
        px = 24.0 + u * 1230.0
        ratio = (u / fc) ** 4.0
        mag_db = 1.0 / math.sqrt((1.0 - ratio) ** 2 + 0.24 * (u / fc) ** 2 + 0.12)
        h_norm = mag_db
        py = base_y - (h_norm - 1.0) * 62.0 + max(0.0, u - fc) * 145.0
        xs.append(px)
        ys.append(py)
    fill_curve_wash(buf, xs, ys, 296.0, CYAN, top_alpha=0.07, clip=top_clip)
    draw_polyline(buf, list(zip(xs, ys)), CYAN, thickness=1.7, alpha=0.20, glow=0.05, clip=top_clip)

    # Vowel 3-formant ghost contour
    v_pts = []
    for i in range(160):
        u = i / 159.0
        px = 24.0 + u * 1230.0
        f1 = math.exp(-((u - 0.18) / 0.055) ** 2) * 55.0
        f2 = math.exp(-((u - 0.38) / 0.070) ** 2) * 48.0
        f3 = math.exp(-((u - 0.68) / 0.080) ** 2) * 38.0
        v_pts.append((px, 245.0 - f1 - f2 - f3))
    draw_polyline(buf, v_pts, CYAN, thickness=1.1, alpha=0.10, clip=top_clip)

    # Bottom (FILTER B, Magenta): Twin Bandpass + Notch response schematic
    bot_clip = (14, 326, 1266, 614)
    draw_grid_bg(buf, 14, 326, 1266, 614, 48, MAGENTA, alpha=0.028)
    b_base = 520.0
    xs_b, ys_b = [], []
    for i in range(180):
        u = i / 179.0
        px = 24.0 + u * 940.0
        bp = math.exp(-((u - 0.56) / 0.14) ** 2) * 125.0
        notch = math.exp(-((u - 0.25) / 0.04) ** 2) * 48.0
        py = b_base - bp + notch
        xs_b.append(px)
        ys_b.append(py)
    fill_curve_wash(buf, xs_b, ys_b, 598.0, MAGENTA, top_alpha=0.07, clip=bot_clip)
    draw_polyline(buf, list(zip(xs_b, ys_b)), MAGENTA, thickness=1.7, alpha=0.20, glow=0.05, clip=bot_clip)

    # Routing mini topology emblem inside ROUTING subpanel (982..1260, 492..608)
    rt_clip = (982, 488, 1260, 608)
    draw_aa_line(buf, 1005, 548, 1236, 548, CYAN, thickness=1.2, alpha=0.14, clip=rt_clip)
    draw_polyline(buf, [(1050, 548), (1080, 520), (1160, 520), (1190, 548)], CYAN, thickness=1.1, alpha=0.14, clip=rt_clip)
    draw_polyline(buf, [(1050, 548), (1080, 576), (1160, 576), (1190, 548)], MAGENTA, thickness=1.1, alpha=0.14, clip=rt_clip)
    draw_diamond(buf, 1120, 520, 7.0, CYAN, alpha=0.22, fill_alpha=0.10, clip=rt_clip)
    draw_diamond(buf, 1120, 576, 7.0, MAGENTA, alpha=0.22, fill_alpha=0.10, clip=rt_clip)


def decorate_lfo(buf):
    # Top (LFO 1 & 2): LFO1 Cyan wave left -> LFO2 Magenta S&H / Saw right
    top_clip = (14, 18, 1266, 192)
    draw_grid_bg(buf, 14, 18, 1266, 192, 44, CYAN, alpha=0.025)
    mid_y = 114.0
    draw_dashed_line(buf, 22, mid_y, 1256, mid_y, MUTED_GRID, dash=8, gap=6, alpha=0.25, clip=top_clip)

    # Left half: flowing Sine + Triangle cycles (Cyan)
    pts_l1 = []
    for i in range(120):
        u = i / 119.0
        px = 22.0 + u * 598.0
        py = mid_y - math.sin(u * 5.0 * math.pi) * 44.0
        pts_l1.append((px, py))
    draw_polyline(buf, pts_l1, CYAN, thickness=1.6, alpha=0.19, glow=0.04, clip=top_clip)

    # Right half: Saw + Random stepped S&H waveform (Magenta)
    pts_l2 = []
    steps = 16
    sh_vals = [0.6, -0.4, 0.9, 0.2, -0.8, 0.4, -0.2, 0.85, -0.7, 0.3, 0.95, -0.5, 0.1, -0.9, 0.55, -0.25]
    for s in range(steps):
        x0 = 656.0 + (s / float(steps)) * 600.0
        x1 = 656.0 + ((s + 1) / float(steps)) * 600.0
        py = mid_y - sh_vals[s] * 42.0
        pts_l2.append((x0, py))
        pts_l2.append((x1, py))
    draw_polyline(buf, pts_l2, MAGENTA, thickness=1.6, alpha=0.19, glow=0.04, clip=top_clip)

    # Bottom-left (NOISE OSC, Magenta): Stochastic spectrum scatter + FM carrier lines
    bot_clip = (14, 326, 650, 614)
    draw_grid_bg(buf, 14, 326, 650, 614, 48, MAGENTA, alpha=0.025)
    for idx in range(72):
        hx = ((idx * 73856093) ^ 0x4C11DB7) & 0xFFFF
        hy = ((idx * 19349663) ^ 0x9E3779B9) & 0xFFFF
        px = 26 + int((hx / 65536.0) * 610)
        py = 340 + int((hy / 65536.0) * 260)
        draw_circle(buf, px, py, 1.5 + (idx % 3) * 0.8, MAGENTA, alpha=0.18, filled=True, clip=bot_clip)
    # Spectral pink-noise rolloff contour
    n_pts = []
    for i in range(90):
        u = i / 89.0
        px = 26.0 + u * 610.0
        jitter = math.sin(i * 1.9) * math.cos(i * 3.1) * (18.0 - 8.0 * u)
        py = 420.0 + u * 75.0 + jitter
        n_pts.append((px, py))
    draw_polyline(buf, n_pts, MAGENTA, thickness=1.4, alpha=0.15, clip=bot_clip)

    # Bottom-right (ENV FOLLOWER, Magenta): Snappy 5ms attack -> smooth exponential release curve
    env_clip = (660, 486, 1262, 608)
    env_pts = []
    for i in range(100):
        u = i / 99.0
        px = 672.0 + u * 576.0
        e = (u / 0.10) if u < 0.10 else math.exp(-((u - 0.10) * 4.5))
        py = 594.0 - e * 82.0
        env_pts.append((px, py))
    fill_curve_wash(buf, [p[0] for p in env_pts], [p[1] for p in env_pts], 596.0, MAGENTA, top_alpha=0.08, clip=env_clip)
    draw_polyline(buf, env_pts, MAGENTA, thickness=1.6, alpha=0.22, glow=0.05, clip=env_clip)


def decorate_matrix(buf):
    # Top-Left (LFO 1 MATRIX, Cyan) & Bottom-Left (LFO 2 MATRIX, Magenta)
    cols_x = [142, 462, 782]
    row_top_ys = [95, 240]
    row_bot_ys = [402, 548]

    tl_clip = (14, 18, 952, 306)
    bl_clip = (14, 326, 952, 614)
    draw_grid_bg(buf, 14, 18, 952, 306, 48, CYAN, alpha=0.025)
    draw_grid_bg(buf, 14, 326, 952, 614, 48, MAGENTA, alpha=0.025)

    for ry in row_top_ys:
        draw_aa_line(buf, 30, ry, 938, ry, CYAN, thickness=1.1, alpha=0.10, clip=tl_clip)
    for cx in cols_x:
        draw_aa_line(buf, cx, 32, cx, 294, CYAN, thickness=1.1, alpha=0.10, clip=tl_clip)
        for ry in row_top_ys:
            draw_circle(buf, cx, ry, 44.0, CYAN, thickness=1.0, alpha=0.09, clip=tl_clip)
            draw_diamond(buf, cx, ry, 7.0, CYAN, alpha=0.20, fill_alpha=0.08, clip=tl_clip)

    for ry in row_bot_ys:
        draw_aa_line(buf, 30, ry, 938, ry, MAGENTA, thickness=1.1, alpha=0.10, clip=bl_clip)
    for cx in cols_x:
        draw_aa_line(buf, cx, 340, cx, 602, MAGENTA, thickness=1.1, alpha=0.10, clip=bl_clip)
        for ry in row_bot_ys:
            draw_circle(buf, cx, ry, 44.0, MAGENTA, thickness=1.0, alpha=0.09, clip=bl_clip)
            draw_diamond(buf, cx, ry, 7.0, MAGENTA, alpha=0.20, fill_alpha=0.08, clip=bl_clip)

    # Top-Right (ENV > FLT, Cyan) & Bottom-Right (ENV > LFO, Magenta) dynamic modulation arcs
    tr_clip = (972, 18, 1266, 306)
    br_clip = (972, 326, 1266, 614)
    for clip, col, base_y in [(tr_clip, CYAN, 162.0), (br_clip, MAGENTA, 470.0)]:
        draw_grid_bg(buf, clip[0], clip[1], clip[2], clip[3], 44, col, alpha=0.025)
        draw_polyline(buf, [(990, base_y + 70), (1080, base_y - 45), (1245, base_y - 25)], col, thickness=1.5, alpha=0.18, glow=0.04, clip=clip)
        draw_polyline(buf, [(990, base_y + 70), (1105, base_y + 5), (1245, base_y + 45)], col, thickness=1.2, alpha=0.13, clip=clip)
        draw_diamond(buf, 1118, base_y, 8.0, col, alpha=0.22, fill_alpha=0.08, clip=clip)


def decorate_gate(buf):
    # Top (TRANCE GATE, Cyan): 16-column sequencer ruler across full panel behind knobs + step strip
    top_clip = (14, 18, 1266, 306)
    step_w = 918.0 / 16.0
    for s in range(17):
        gx = int(23 + s * step_w)
        is_beat = (s % 4 == 0)
        draw_aa_line(buf, gx, 24, gx, 300, CYAN, thickness=(1.4 if is_beat else 1.0),
                     alpha=(0.11 if is_beat else 0.04), clip=top_clip)
    # Rhythmic connecting bus to Mix fader on Col 4
    draw_aa_line(buf, 945, 162, 1245, 162, CYAN, thickness=1.2, alpha=0.11, clip=top_clip)
    draw_diamond(buf, 962, 162, 6.0, CYAN, alpha=0.20, fill_alpha=0.08, clip=top_clip)

    # Bottom (GATE ENV, Magenta): Crisp multi-pulse trapezoidal Start->Hold->End gate envelopes
    bot_clip = (14, 326, 1266, 614)
    draw_grid_bg(buf, 14, 326, 1266, 614, 48, MAGENTA, alpha=0.025)
    bot_y = 576.0
    top_y = 374.0
    pts_gate = [(24.0, bot_y)]
    xs_g, ys_g = [], []
    pulses = 4
    span = 1228.0 / pulses
    for p in range(pulses):
        px0 = 24.0 + p * span
        p_pts = [
            (px0, bot_y),
            (px0 + span * 0.14, top_y),
            (px0 + span * 0.62, top_y),
            (px0 + span * 0.84, bot_y),
            (px0 + span, bot_y),
        ]
        for x, y in p_pts:
            pts_gate.append((x, y))
            xs_g.append(x)
            ys_g.append(y)
    fill_curve_wash(buf, xs_g, ys_g, bot_y, MAGENTA, top_alpha=0.08, clip=bot_clip)
    draw_polyline(buf, pts_gate, MAGENTA, thickness=1.8, alpha=0.20, glow=0.05, clip=bot_clip)
    # Vertex diamonds on pulse 1 highlighting Start / Hold / End envelope stages
    for vx, vy in [(24.0 + span * 0.14, top_y), (24.0 + span * 0.62, top_y), (24.0 + span * 0.84, bot_y)]:
        draw_diamond(buf, vx, vy, 6.0, MAGENTA, alpha=0.26, fill_alpha=0.10, clip=bot_clip)


def decorate_pump(buf):
    # Top (SIDECHAIN PUMP, Cyan): 4-on-the-floor ducking waveform envelopes
    top_clip = (14, 18, 1266, 306)
    draw_grid_bg(buf, 14, 18, 1266, 306, 48, CYAN, alpha=0.025)
    ceil_y = 66.0
    floor_y = 268.0
    xs, ys = [], []
    beats = 4
    per_beat = 1228.0 / beats
    samples_per_beat = 44
    for b in range(beats):
        bx = 24.0 + b * per_beat
        draw_dashed_line(buf, bx, 26, bx, 298, CYAN, dash=5, gap=7, alpha=0.10, clip=top_clip)
        for s in range(samples_per_beat):
            u = s / float(samples_per_beat - 1)
            px = bx + u * (per_beat - 1.0)
            # Fast attack dip then exponential recovery pump
            if u < 0.08:
                duck = 1.0 - (u / 0.08)
            else:
                duck = 1.0 - math.exp(-((u - 0.08) * 5.2))
            py = floor_y - duck * (floor_y - ceil_y)
            xs.append(px)
            ys.append(py)
    fill_curve_wash(buf, xs, ys, floor_y, CYAN, top_alpha=0.08, clip=top_clip)
    draw_polyline(buf, list(zip(xs, ys)), CYAN, thickness=1.8, alpha=0.21, glow=0.05, clip=top_clip)

    # Bottom (DETECTOR FILTER, Magenta): HPF <-> LPF bandpass key tunnel curve
    bot_clip = (14, 326, 976, 614)
    draw_grid_bg(buf, 14, 326, 976, 614, 48, MAGENTA, alpha=0.025)
    b_floor = 588.0
    xs_f, ys_f = [], []
    for i in range(140):
        u = i / 139.0
        px = 24.0 + u * 944.0
        # Smooth flat-top bandpass window isolating sub/kick energy
        hp = 1.0 / (1.0 + (0.18 / max(0.01, u)) ** 4.0)
        lp = 1.0 / (1.0 + (u / 0.68) ** 4.0)
        resp = hp * lp
        py = b_floor - resp * 195.0
        xs_f.append(px)
        ys_f.append(py)
    fill_curve_wash(buf, xs_f, ys_f, b_floor, MAGENTA, top_alpha=0.08, clip=bot_clip)
    draw_polyline(buf, list(zip(xs_f, ys_f)), MAGENTA, thickness=1.7, alpha=0.21, glow=0.05, clip=bot_clip)
    # HPF / LPF corner markers
    draw_diamond(buf, 24.0 + 0.18 * 944.0, b_floor - 135.0, 6.0, MAGENTA, alpha=0.25, fill_alpha=0.09, clip=bot_clip)
    draw_diamond(buf, 24.0 + 0.68 * 944.0, b_floor - 135.0, 6.0, MAGENTA, alpha=0.25, fill_alpha=0.09, clip=bot_clip)


def decorate_dly_lim(buf):
    # Top (DELAY, Cyan): Stereo L <-> R Ping-Pong bounce arcs & decaying echo taps
    top_clip = (14, 18, 1266, 306)
    draw_grid_bg(buf, 14, 18, 1266, 306, 48, CYAN, alpha=0.025)
    l_y = 84.0
    r_y = 244.0
    draw_dashed_line(buf, 24, l_y, 1254, l_y, CYAN, dash=6, gap=8, alpha=0.09, clip=top_clip)
    draw_dashed_line(buf, 24, r_y, 1254, r_y, CYAN, dash=6, gap=8, alpha=0.09, clip=top_clip)

    taps = 7
    tap_x = [80.0 + i * 184.0 for i in range(taps)]
    bounce_pts = []
    for i in range(taps - 1):
        y0 = l_y if (i % 2 == 0) else r_y
        y1 = r_y if (i % 2 == 0) else l_y
        amp_scale = 0.88 ** i
        # Curved trajectory from rail to opposite rail
        for k in range(25):
            t = k / 24.0
            px = tap_x[i] + (tap_x[i + 1] - tap_x[i]) * t
            py = y0 + (y1 - y0) * (0.5 - 0.5 * math.cos(t * math.pi))
            bounce_pts.append((px, py))
        draw_circle(buf, tap_x[i], y0, 22.0 * amp_scale, CYAN, thickness=1.3, alpha=0.19 * amp_scale, clip=top_clip)
        draw_diamond(buf, tap_x[i], y0, 6.0 * amp_scale + 2.0, CYAN, alpha=0.25 * amp_scale, fill_alpha=0.09 * amp_scale, clip=top_clip)
    draw_polyline(buf, bounce_pts, CYAN, thickness=1.5, alpha=0.17, glow=0.04, clip=top_clip)

    # Bottom (LIMITER, Magenta): Brickwall Ceiling horizon + peak-limited waveform silhouette
    bot_clip = (14, 326, 1266, 614)
    draw_grid_bg(buf, 14, 326, 1266, 614, 48, MAGENTA, alpha=0.025)
    ceil_y = 376.0
    axis_y = 468.0
    # Brickwall ceiling horizon line
    draw_aa_line(buf, 24, ceil_y, 1254, ceil_y, MAGENTA, thickness=1.6, alpha=0.22, clip=bot_clip)
    draw_aa_line(buf, 24, ceil_y, 1254, ceil_y, MAGENTA, thickness=5.0, alpha=0.05, clip=bot_clip)
    # Pre-limiter ghost peaks exceeding ceiling (dashed look) + post-limiter brickwalled envelope
    raw_pts, lim_pts = [], []
    for i in range(200):
        u = i / 199.0
        px = 24.0 + u * 1230.0
        env = abs(math.sin(u * 5.0 * math.pi)) * (0.65 + 0.55 * abs(math.sin(u * 13.0 * math.pi)))
        raw_y = axis_y - env * 124.0
        lim_y = max(ceil_y + 1.5, raw_y)
        raw_pts.append((px, raw_y))
        lim_pts.append((px, lim_y))
    draw_polyline(buf, raw_pts, MAGENTA, thickness=1.0, alpha=0.09, clip=bot_clip)
    fill_curve_wash(buf, [p[0] for p in lim_pts], [p[1] for p in lim_pts], axis_y, MAGENTA, top_alpha=0.08, clip=bot_clip)
    draw_polyline(buf, lim_pts, MAGENTA, thickness=1.6, alpha=0.20, glow=0.04, clip=bot_clip)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    assets_dir = os.path.join(os.path.dirname(script_dir), "assets")
    os.makedirs(assets_dir, exist_ok=True)
    generate_main_bg(assets_dir)
    generate_gate_step_strips(assets_dir)
    # Keep decay_viz_strip.png generation helper accessible from original script flow if absent
    for filename, top_title, bot_title in PAGES:
        buf = bytearray(bytes(BG) * (W * H))
        if filename == "bg_matrix.png":
            draw_panel(buf, 8, 12, 950, 300, top_title, TITLE_COL)
            draw_panel(buf, 966, 12, 306, 300, "ENV > FLT", TITLE_COL)
            draw_panel(buf, 8, 320, 950, 302, bot_title, MAGENTA)
            draw_panel(buf, 966, 320, 306, 302, "ENV > LFO", MAGENTA)
        else:
            draw_panel(buf, 8, 12, 1264, 300, top_title, TITLE_COL)
            if bot_title is not None:
                draw_panel(buf, 8, 320, 1264, 302, bot_title, MAGENTA)

        # Apply themed subpage schematics cleanly inside the panel interiors
        if filename == "bg_scurve.png":
            decorate_scurve(buf)
        elif filename == "bg_filter.png":
            decorate_filter(buf)
        elif filename == "bg_lfo.png":
            decorate_lfo(buf)
        elif filename == "bg_matrix.png":
            decorate_matrix(buf)
        elif filename == "bg_gate.png":
            decorate_gate(buf)
        elif filename == "bg_pump.png":
            decorate_pump(buf)
        elif filename == "bg_dly_lim.png":
            decorate_dly_lim(buf)

        # Draw crisp embedded subpanels, accent left rails, and meter/XY bays on top
        if filename == "bg_matrix.png":
            draw_filled_box(buf, W, H, 8, 26, 4, 272, CYAN)
            draw_filled_box(buf, W, H, 966, 26, 4, 272, CYAN)
            draw_filled_box(buf, W, H, 8, 334, 4, 274, MAGENTA)
            draw_filled_box(buf, W, H, 966, 334, 4, 274, MAGENTA)
        else:
            draw_filled_box(buf, W, H, 8, 26, 4, 272, CYAN)
            draw_filled_box(buf, W, H, 8, 334, 4, 274, MAGENTA)

        if filename == "bg_filter.png":
            draw_panel(buf, 975, 478, 294, 136, "ROUTING", TITLE_COL, fill=False)
            for bx, col, lbl in [(656, CYAN, "A FREQ X RES"), (962, MAGENTA, "B FREQ X RES")]:
                draw_filled_box(buf, W, H, bx, 180, 292, 107, (0x0C, 0x10, 0x16, 255))
                border = (col[0] // 2, col[1] // 2, col[2] // 2, 255)
                draw_filled_box(buf, W, H, bx, 180, 292, 1, border)
                draw_filled_box(buf, W, H, bx, 286, 292, 1, border)
                draw_filled_box(buf, W, H, bx, 180, 1, 107, border)
                draw_filled_box(buf, W, H, bx + 291, 180, 1, 107, border)
                for gx in (bx + 73, bx + 146, bx + 219):
                    draw_filled_box(buf, W, H, gx, 181, 1, 105, (0x18, 0x20, 0x2A, 255))
                for gy in (180 + 35, 180 + 70):
                    draw_filled_box(buf, W, H, bx + 1, gy, 290, 1, (0x18, 0x20, 0x2A, 255))
                draw_text_5x7(buf, W, H, bx + 8, 186, lbl, col, scale=1)
        elif filename == "bg_scurve.png":
            draw_panel(buf, 335, 478, 934, 136, "INPUT STAGE", MAGENTA, fill=False)
        elif filename == "bg_lfo.png":
            draw_panel(buf, 655, 478, 614, 136, "ENV FOLLOWER", MAGENTA, fill=False)
            draw_filled_box(buf, W, H, 16, 44, 606, 3, CYAN)
            draw_filled_box(buf, W, H, 656, 44, 606, 3, MAGENTA)
            bx, by, bw, bh = 23, 196, 1238, 107
            draw_filled_box(buf, W, H, bx, by, bw, bh, (0x0C, 0x10, 0x16, 255))
            border = (0x22, 0x2C, 0x38, 255)
            draw_filled_box(buf, W, H, bx, by, bw, 1, border)
            draw_filled_box(buf, W, H, bx, by + bh - 1, bw, 1, border)
            draw_filled_box(buf, W, H, bx, by, 1, bh, border)
            draw_filled_box(buf, W, H, bx + bw - 1, by, 1, bh, border)
            for frac in (0.25, 0.5, 0.75):
                gx = bx + int(bw * frac)
                draw_filled_box(buf, W, H, gx, by + 1, 1, bh - 2, (0x18, 0x20, 0x2A, 255))
            for frac in (0.33, 0.66):
                gy = by + int(bh * frac)
                draw_filled_box(buf, W, H, bx + 1, gy, bw - 2, 1, (0x18, 0x20, 0x2A, 255))
            draw_text_5x7(buf, W, H, bx + 10, by + 6, "LFO 1 X", CYAN, scale=1)
            draw_text_5x7(buf, W, H, bx + 68, by + 6, "X  LFO 2 Y", MAGENTA, scale=1)
        elif filename == "bg_pump.png":
            bx, by, bw, bh = 983, 346, 278, 107
            draw_filled_box(buf, W, H, bx, by, bw, bh, (0x0C, 0x10, 0x16, 255))
            border = (0x22, 0x2C, 0x38, 255)
            draw_filled_box(buf, W, H, bx, by, bw, 1, border)
            draw_filled_box(buf, W, H, bx, by + bh - 1, bw, 1, border)
            draw_filled_box(buf, W, H, bx, by, 1, bh, border)
            draw_filled_box(buf, W, H, bx + bw - 1, by, 1, bh, border)
            draw_text_5x7(buf, W, H, 1051, 351, "IN", CYAN, scale=2)
            draw_text_5x7(buf, W, H, 1159, 351, "DUCK", MAGENTA, scale=2)
        elif filename == "bg_gate.png":
            draw_filled_box(buf, W, H, 23, 196, 918, 107, (0x10, 0x14, 0x1A, 255), (0x26, 0x2E, 0x3A, 255))
        elif filename == "bg_dly_lim.png":
            bx, by, bw, bh = 343, 497, 598, 107
            draw_filled_box(buf, W, H, bx, by, bw, bh, (0x0C, 0x10, 0x16, 255))
            border = (0x22, 0x2C, 0x38, 255)
            draw_filled_box(buf, W, H, bx, by, bw, 1, border)
            draw_filled_box(buf, W, H, bx, by + bh - 1, bw, 1, border)
            draw_filled_box(buf, W, H, bx, by, 1, bh, border)
            draw_filled_box(buf, W, H, bx + bw - 1, by, 1, bh, border)
            draw_text_5x7(buf, W, H, 358, 506, "LIMITER GR", MAGENTA, scale=2)

        out_path = os.path.join(assets_dir, filename)
        write_png(out_path, W, H, buf)
        print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
