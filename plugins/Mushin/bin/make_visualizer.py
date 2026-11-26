#!/usr/bin/env python3
"""Generate SNES-sprite / Milkdrop-style visualizer assets for Mushin's SCOPE tab.

Produces compact FilmStrip PNGs (all heights <= 4096 px for embedded ARM GL ES2):
  - bg_scope.png           (1280x628 static cyber-void backdrop)
  - scope_tunnel_strip.png (240x120 x 24 frames = 240x2880 seamless vortex tunnel)
  - scope_mandala_strip.png(192x192 x 20 frames = 192x3840 audio-bloom kaleidoscope)
  - scope_orb_cyan.png     (88x88 plasma comet sprite)
  - scope_orb_mag.png      (80x80 neon magenta star sprite)
  - meter_wing_l_off/on.png, meter_wing_r_off/on.png (610x24 mirrored LED wings)
"""

import math
import os
import struct
import zlib

W = 1280
H = 628

CYAN = (0, 229, 255)
MAG = (255, 77, 141)
WHITE = (240, 244, 248)
VIOLET = (118, 44, 195)


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


def clamp01(x):
    if x < 0.0:
        return 0.0
    if x > 1.0:
        return 1.0
    return x


def draw_rect(buf, bw, bh, x0, y0, rw, rh, col):
    x1 = min(bw, x0 + rw)
    y1 = min(bh, y0 + rh)
    x0 = max(0, x0)
    y0 = max(0, y0)
    if x0 >= x1 or y0 >= y1:
        return
    row = bytes(col) * (x1 - x0)
    for py in range(y0, y1):
        idx = (py * bw + x0) * 4
        buf[idx : idx + (x1 - x0) * 4] = row


FONT_5X7 = {
    " ": [0, 0, 0, 0, 0, 0, 0],
    "/": [0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000],
    ":": [0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000],
    "+": [0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000],
    "<": [0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010],
    ">": [0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000],
    "-": [0, 0, 0, 0b11111, 0, 0, 0],
    "A": [0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001],
    "C": [0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110],
    "D": [0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110],
    "E": [0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111],
    "F": [0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000],
    "G": [0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110],
    "H": [0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001],
    "I": [0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
    "K": [0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001],
    "L": [0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111],
    "M": [0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001],
    "N": [0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001],
    "O": [0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
    "P": [0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000],
    "R": [0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001],
    "S": [0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110],
    "U": [0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110],
    "V": [0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100],
    "Y": [0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100],
    "6": [0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110],
}


def draw_text(buf, bw, bh, x, y, text, col, scale=1):
    px = x
    for ch in text.upper():
        g = FONT_5X7.get(ch, FONT_5X7[" "])
        for r in range(7):
            bits = g[r]
            for c in range(5):
                if bits & (1 << (4 - c)):
                    draw_rect(buf, bw, bh, px + c * scale, y + r * scale, scale, scale, col)
        px += 7 * scale


def gen_bg_scope(assets_dir):
    buf = bytearray(bytes((6, 8, 13, 255)) * (W * H))
    # Subtle scanlines & horizon radial vignette
    for y in range(H):
        dy = (y - H * 0.5) / (H * 0.5)
        scan = 0.85 if (y & 3) == 0 else 1.0
        for x in range(W):
            dx = (x - W * 0.5) / (W * 0.5)
            d2 = dx * dx + dy * dy
            glow = max(0.0, 1.0 - d2 * 0.85)
            r = int((6 + 10 * glow) * scan)
            g = int((8 + 14 * glow) * scan)
            b = int((14 + 26 * glow) * scan)
            idx = (y * W + x) * 4
            buf[idx : idx + 4] = bytes((r, g, b, 255))

    # Faint perspective rings
    cx, cy = W * 0.5, H * 0.5
    for rad in (95, 190, 290):
        for step in range(360):
            a = step * math.pi / 180.0
            px = int(cx + math.cos(a) * rad)
            py = int(cy + math.sin(a) * rad * 0.92)
            if 0 <= px < W and 0 <= py < H:
                idx = (py * W + px) * 4
                buf[idx : idx + 4] = bytes((22, 34, 52, 255))

    # Corner HUD brackets
    c_line = (0, 175, 210, 255)
    m_line = (195, 58, 112, 255)
    draw_rect(buf, W, H, 14, 14, 48, 2, c_line)
    draw_rect(buf, W, H, 14, 14, 2, 32, c_line)
    draw_rect(buf, W, H, W - 62, 14, 48, 2, m_line)
    draw_rect(buf, W, H, W - 16, 14, 2, 32, m_line)
    draw_rect(buf, W, H, 14, H - 16, 48, 2, c_line)
    draw_rect(buf, W, H, 14, H - 46, 2, 32, c_line)
    draw_rect(buf, W, H, W - 62, H - 16, 48, 2, m_line)
    draw_rect(buf, W, H, W - 16, H - 46, 2, 32, m_line)

    draw_text(buf, W, H, 26, 40, "MUSHIN // SCOPE", (120, 220, 240, 255), scale=1)
    draw_text(buf, W, H, W - 186, 40, "F6: DLY+LIM <> SCOPE", (215, 110, 155, 255), scale=1)

    out = os.path.join(assets_dir, "bg_scope.png")
    write_png(out, W, H, buf)
    print(f"Wrote {out}")


def gen_tunnel_strip(assets_dir):
    # 240x120 x 24 frames = 240x2880 (<= 4096 max GL texture height)
    fw, fh = 240, 120
    nframes = 24
    total_h = fh * nframes
    buf = bytearray(fw * total_h * 4)

    cx, cy = fw * 0.5, fh * 0.5
    aspect = (W / float(H)) / (fw / float(fh))

    for fr in range(nframes):
        ph = 2.0 * math.pi * fr / float(nframes)
        y_off = fr * fh
        for py in range(fh):
            ny = (py + 0.5 - cy) / (fh * 0.5)
            for px in range(fw):
                nx = ((px + 0.5 - cx) / (fw * 0.5)) * (W / float(H)) * (fh / float(fw)) * 1.0
                # Aspect-normalized screen space (-1.78..1.78, -1..1)
                sx = (px + 0.5 - cx) / (fw * 0.5) * 1.78
                sy = (py + 0.5 - cy) / (fh * 0.5)
                r = math.hypot(sx, sy)
                a = math.atan2(sy, sx)
                # Mode-7 / Milkdrop infinite warped tunnel coordinate
                inv_r = 1.0 / (r + 0.14)
                warp = 0.35 * math.sin(a * 4.0 + ph)
                u = inv_r * 2.2 - ph * 1.0 + warp
                v = a * 6.0 / math.pi + 0.55 * math.sin(inv_r * 1.2 - ph)

                rings = max(0.0, math.cos(u * math.pi * 2.0)) ** 3.0
                spokes = max(0.0, math.cos(v * math.pi)) ** 4.0
                plasma = 0.5 + 0.5 * math.sin(inv_r * 2.8 - ph + math.sin(a * 3.0 - ph))

                fade = clamp01(r * 1.45) * clamp01(1.35 - r * 0.42)
                intensity = (0.68 * rings + 0.45 * spokes + 0.25 * plasma) * fade

                # Cyan-to-Magenta neon shift over phase & depth
                c_mix = 0.5 + 0.5 * math.sin(inv_r * 1.6 - ph + a * 2.0)
                rr = int(clamp01(intensity * (0.15 + 0.85 * c_mix)) * 245)
                gg = int(clamp01(intensity * (0.90 - 0.62 * c_mix)) * 235)
                bb = int(clamp01(intensity * (0.98 - 0.32 * c_mix)) * 255)
                aa = int(clamp01(intensity * 0.82) * 255)

                idx = ((y_off + py) * fw + px) * 4
                buf[idx : idx + 4] = bytes((rr, gg, bb, aa))

    out = os.path.join(assets_dir, "scope_tunnel_strip.png")
    write_png(out, fw, total_h, buf)
    print(f"Wrote {out}")


def gen_mandala_strip(assets_dir):
    # 192x192 x 20 frames = 192x3840 (<= 4096)
    fw, fh = 192, 192
    nframes = 20
    total_h = fh * nframes
    buf = bytearray(fw * total_h * 4)
    cx, cy = fw * 0.5, fh * 0.5

    for fr in range(nframes):
        t = fr / float(nframes - 1)  # 0..1 audio bloom
        rot = t * math.pi * 0.5
        y_off = fr * fh
        outer_base = 0.24 + 0.54 * (t ** 0.75)
        inner_base = 0.14 + 0.34 * (t ** 0.85)
        w_outer = 0.032 + 0.030 * t
        w_inner = 0.024 + 0.022 * t

        for py in range(fh):
            ny = (py + 0.5 - cy) / (fh * 0.5)
            for px in range(fw):
                nx = (px + 0.5 - cx) / (fw * 0.5)
                r = math.hypot(nx, ny)
                if r > 0.98:
                    continue
                a = math.atan2(ny, nx)

                # Outer 8-petal neon magenta flower
                r_pet = outer_base * (0.78 + 0.26 * math.cos(8.0 * (a + rot)))
                d_pet = abs(r - r_pet)
                g_pet = math.exp(-(d_pet * d_pet) / (w_outer * w_outer))

                # Mid 12-lobe cyan kaleidoscope star
                r_mid = (outer_base * 0.68) * (0.76 + 0.28 * math.sin(12.0 * (a - rot * 1.4)))
                d_mid = abs(r - r_mid)
                g_mid = math.exp(-(d_mid * d_mid) / (w_inner * w_inner))

                # Inner reactor core diamond
                r_core = inner_base * (0.72 + 0.30 * math.cos(4.0 * (a + rot * 2.0)))
                d_core = abs(r - r_core)
                g_core = math.exp(-(d_core * d_core) / (w_inner * w_inner))
                core_fill = math.exp(-(r * r) / ((0.06 + 0.14 * t) ** 2))

                e = clamp01(0.32 + 0.75 * t)
                rr = clamp01((g_pet * 1.00 + g_core * 0.70 + core_fill * 0.90) * e)
                gg = clamp01((g_pet * 0.28 + g_mid * 0.92 + g_core * 0.95 + core_fill * 0.95) * e)
                bb = clamp01((g_pet * 0.58 + g_mid * 1.00 + g_core * 1.00 + core_fill * 1.00) * e)
                aa = clamp01((g_pet * 0.95 + g_mid * 0.92 + g_core * 0.95 + core_fill * 0.85) * e)

                if aa > 0.01:
                    idx = ((y_off + py) * fw + px) * 4
                    buf[idx : idx + 4] = bytes(
                        (int(rr * 255), int(gg * 255), int(bb * 255), int(aa * 255))
                    )

    out = os.path.join(assets_dir, "scope_mandala_strip.png")
    write_png(out, fw, total_h, buf)
    print(f"Wrote {out}")


def gen_orb_sprite(skins_dir, filename, rgb, diamond=False):
    sz = 84 if not diamond else 76
    c = sz * 0.5
    buf = bytearray(sz * sz * 4)
    for py in range(sz):
        ny = (py + 0.5 - c) / (sz * 0.5)
        for px in range(sz):
            nx = (px + 0.5 - c) / (sz * 0.5)
            r = math.hypot(nx, ny)
            if r > 1.0:
                continue
            halo = math.exp(-(r * r) / 0.14)
            ring = math.exp(-((r - 0.56) ** 2) / 0.012) * 0.55
            if diamond:
                flare = math.exp(-abs(nx + ny) * 9.0 - r * 2.0) + math.exp(-abs(nx - ny) * 9.0 - r * 2.0)
            else:
                flare = math.exp(-abs(nx) * 10.0 - r * 2.2) + math.exp(-abs(ny) * 10.0 - r * 2.2)
            core = math.exp(-(r * r) / 0.018)
            glow = clamp01(halo + ring + flare * 0.65)
            edge_fade = clamp01(1.0 - r)
            aa = clamp01(glow * edge_fade)
            rr = clamp01((rgb[0] / 255.0) * glow + core * 0.95)
            gg = clamp01((rgb[1] / 255.0) * glow + core * 0.95)
            bb = clamp01((rgb[2] / 255.0) * glow + core * 0.95)
            idx = (py * sz + px) * 4
            buf[idx : idx + 4] = bytes((int(rr * 255), int(gg * 255), int(bb * 255), int(aa * 255)))

    out = os.path.join(skins_dir, filename)
    write_png(out, sz, sz, buf)
    print(f"Wrote {out}")


def gen_wing_meters(skins_dir):
    mw, mh = 610, 24
    for side in ("l", "r"):
        for state in ("off", "on"):
            buf = bytearray(bytes((10, 14, 20, 220)) * (mw * mh))
            alpha = 245 if state == "on" else 36
            for x in range(4, mw - 6, 12):
                # t=0 at center edge, t=1 at outer wing tip
                t = (mw - 1 - x) / float(mw) if side == "l" else x / float(mw)
                r = int(CYAN[0] * (1.0 - t) + MAG[0] * t)
                g = int(CYAN[1] * (1.0 - t) + MAG[1] * t)
                b = int(CYAN[2] * (1.0 - t) + MAG[2] * t)
                draw_rect(buf, mw, mh, x, 3, 9, mh - 6, (r, g, b, alpha))
            out = os.path.join(skins_dir, f"meter_wing_{side}_{state}.png")
            write_png(out, mw, mh, buf)
            print(f"Wrote {out}")


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    plugin_dir = os.path.dirname(script_dir)
    assets_dir = os.path.join(plugin_dir, "assets")
    skins_dir = os.path.join(plugin_dir, "skins")
    os.makedirs(assets_dir, exist_ok=True)
    os.makedirs(skins_dir, exist_ok=True)

    gen_bg_scope(assets_dir)
    gen_tunnel_strip(assets_dir)
    gen_mandala_strip(assets_dir)
    gen_orb_sprite(skins_dir, "scope_orb_cyan.png", CYAN, diamond=False)
    gen_orb_sprite(skins_dir, "scope_orb_mag.png", MAG, diamond=True)
    gen_wing_meters(skins_dir)


if __name__ == "__main__":
    main()
