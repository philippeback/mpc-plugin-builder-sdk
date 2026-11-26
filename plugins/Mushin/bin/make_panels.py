#!/usr/bin/env python3
"""Generate 1280x628 organic ambient DSP backgrounds for Mushin's UI pages.

Uses plugins/Mushin/bin/gen_panels.cpp (compiled with the SDK's Zig C++ compiler)
to render silky, dithered 32-bit float bioluminescent Cyan/Magenta backdrops cleanly
inside each panel bay, then overlays razor-sharp rounded frames, titles, accent rails,
and meter/XY bay cutouts in pure Python.
"""

import math
import os
import shutil
import struct
import subprocess
import tempfile
import zlib

W = 1280
H = 628

BORDER_COL = (0x2E, 0x3A, 0x48, 255)
TITLE_COL = (0x00, 0xE5, 0xFF, 255)
CYAN = (0x00, 0xE5, 0xFF, 255)
MAGENTA = (0xFF, 0x4D, 0x8D, 255)

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
    ("bg_scurve.png",  "bg_scurve",  "S-CURVE",        "INFO DECAY"),
    ("bg_filter.png",  "bg_filter",  "FILTER A",       "FILTER B"),
    ("bg_lfo.png",     "bg_lfo",     "LFO 1 & 2",      "NOISE OSC"),
    ("bg_matrix.png",  "bg_matrix",  "LFO 1 MATRIX",   "LFO 2 MATRIX"),
    ("bg_gate.png",    "bg_gate",    "TRANCE GATE",    "GATE ENV"),
    ("bg_pump.png",    "bg_pump",    "SIDECHAIN PUMP", "DETECTOR FILTER"),
    ("bg_dly_lim.png", "bg_dly_lim", "DELAY",          "LIMITER"),
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


def draw_panel_frame(buf, x0, y0, pw, ph, title, title_col=TITLE_COL):
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
            in_cutout = (py < y0 + 6) and (cut_x0 <= px <= cut_x1)
            if in_cutout:
                continue
            dx = abs(px + 0.5 - cx) - hx
            qx = max(dx, 0.0)
            qy = max(dy, 0.0)
            dist = math.hypot(qx, qy) + min(max(dx, dy), 0.0) - radius
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


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    plugin_dir = os.path.dirname(script_dir)
    sdk_root = os.path.dirname(os.path.dirname(plugin_dir))
    assets_dir = os.path.join(plugin_dir, "assets")
    os.makedirs(assets_dir, exist_ok=True)

    zig_py = os.path.join(sdk_root, ".sdk-tools", "zigenv", "Scripts", "python.exe")
    gen_cpp = os.path.join(script_dir, "gen_panels.cpp")
    work_dir = tempfile.mkdtemp(prefix="mushin_panels_")
    try:
        exe_path = os.path.join(work_dir, "gen_panels.exe" if os.name == "nt" else "gen_panels")
        subprocess.check_call([zig_py, "-m", "ziglang", "c++", "-O3", "-std=c++17", gen_cpp, "-o", exe_path])
        subprocess.check_call([exe_path, work_dir])

        # 1. bg_main.png
        with open(os.path.join(work_dir, "bg_main.rgba"), "rb") as f:
            buf = bytearray(f.read())
        draw_text_5x7(buf, W, H, 510, 10, "S-CURVE > FILTER > DECAY > GATE > DLY > LIM", (0x8B, 0x94, 0x9E, 255), scale=1)
        draw_text_5x7(buf, W, H, 886, 6, "MUSHIN // MACRO PERFORMANCE", CYAN, scale=2)
        draw_panel_frame(buf, 8, 24, 1264, 288, "BANK 1 - CORE DESTRUCTION & CHOKE", TITLE_COL)
        draw_panel_frame(buf, 8, 324, 1264, 296, "BANK 2 - MOTION, SPACE & RHYTHM", MAGENTA)
        draw_filled_box(buf, W, H, 8, 38, 4, 260, CYAN)
        draw_filled_box(buf, W, H, 8, 338, 4, 268, MAGENTA)
        out_path = os.path.join(assets_dir, "bg_main.png")
        write_png(out_path, W, H, buf)
        print(f"Wrote {out_path}")

        generate_gate_step_strips(assets_dir)

        # 2. All 7 themed subpages
        for filename, raw_name, top_title, bot_title in PAGES:
            with open(os.path.join(work_dir, f"{raw_name}.rgba"), "rb") as f:
                buf = bytearray(f.read())
            if filename == "bg_matrix.png":
                draw_panel_frame(buf, 8, 12, 950, 300, top_title, TITLE_COL)
                draw_panel_frame(buf, 966, 12, 306, 300, "ENV > FLT", TITLE_COL)
                draw_panel_frame(buf, 8, 320, 950, 302, bot_title, MAGENTA)
                draw_panel_frame(buf, 966, 320, 306, 302, "ENV > LFO", MAGENTA)
                draw_filled_box(buf, W, H, 8, 26, 4, 272, CYAN)
                draw_filled_box(buf, W, H, 966, 26, 4, 272, CYAN)
                draw_filled_box(buf, W, H, 8, 334, 4, 274, MAGENTA)
                draw_filled_box(buf, W, H, 966, 334, 4, 274, MAGENTA)
            else:
                draw_panel_frame(buf, 8, 12, 1264, 300, top_title, TITLE_COL)
                draw_panel_frame(buf, 8, 320, 1264, 302, bot_title, MAGENTA)
                draw_filled_box(buf, W, H, 8, 26, 4, 272, CYAN)
                draw_filled_box(buf, W, H, 8, 334, 4, 274, MAGENTA)

            if filename == "bg_filter.png":
                draw_panel_frame(buf, 975, 478, 294, 136, "ROUTING", TITLE_COL)
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
                draw_panel_frame(buf, 335, 478, 934, 136, "INPUT STAGE", MAGENTA)
            elif filename == "bg_lfo.png":
                draw_panel_frame(buf, 655, 478, 614, 136, "ENV FOLLOWER", MAGENTA)
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
    finally:
        shutil.rmtree(work_dir, ignore_errors=True)


if __name__ == "__main__":
    main()
