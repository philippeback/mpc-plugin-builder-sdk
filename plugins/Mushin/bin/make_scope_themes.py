#!/usr/bin/env python3
"""Generate SOLAR, CYBER, VOID SCOPE visualizer themes + NEON/SOLAR/CYBER/VOID corner switcher pills."""

import math
import os
import shutil
import struct
import subprocess
import tempfile
import zlib

W = 1280
H = 628

# Reuse minimal 5x7 font matching make_visualizer.py
FONT = {
    " ": [0, 0, 0, 0, 0, 0, 0],
    "/": [0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000],
    ":": [0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000],
    "<": [0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010],
    ">": [0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000],
    "+": [0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000],
    "6": [0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110],
    "A": [0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001],
    "B": [0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110],
    "C": [0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110],
    "D": [0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110],
    "E": [0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111],
    "F": [0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000],
    "H": [0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001],
    "I": [0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110],
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
}


def clamp01(x):
    return 0.0 if x < 0.0 else (1.0 if x > 1.0 else x)


def write_png(path, w, h, rgba, sub_filter=True):
    """Write RGBA PNG using scanline Sub filter (type 1) + max deflate for compact ARM assets."""
    stride = w * 4
    rows = []
    for y in range(h):
        row = rgba[y * stride : (y + 1) * stride]
        if sub_filter:
            filt = bytearray(stride + 1)
            filt[0] = 1
            for i in range(4):
                filt[1 + i] = row[i]
            for i in range(4, stride):
                filt[1 + i] = (row[i] - row[i - 4]) & 0xFF
            rows.append(bytes(filt))
        else:
            rows.append(b"\x00" + bytes(row))
    comp = zlib.compress(b"".join(rows), 9)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", comp) + chunk(b"IEND", b""))


def draw_rect(buf, w, h, x, y, rw, rh, col):
    x0 = max(0, x)
    y0 = max(0, y)
    x1 = min(w, x + rw)
    y1 = min(h, y + rh)
    px = bytes(col)
    for py in range(y0, y1):
        row = py * w
        for p in range(x0, x1):
            idx = (row + p) * 4
            buf[idx : idx + 4] = px


def draw_text(buf, w, h, x, y, text, col, scale=1):
    cx = x
    for ch in text:
        glyph = FONT.get(ch.upper(), FONT[" "])
        for gy, bits in enumerate(glyph):
            for gx in range(5):
                if (bits >> (4 - gx)) & 1:
                    draw_rect(buf, w, h, cx + gx * scale, y + gy * scale, scale, scale, col)
        cx += 6 * scale


def add_bg_hud(buf, title, primary, secondary):
    draw_rect(buf, W, H, 14, 14, 48, 2, primary)
    draw_rect(buf, W, H, 14, 14, 2, 32, primary)
    draw_rect(buf, W, H, W - 62, 14, 48, 2, secondary)
    draw_rect(buf, W, H, W - 16, 14, 2, 32, secondary)
    draw_rect(buf, W, H, 14, H - 16, 48, 2, primary)
    draw_rect(buf, W, H, 14, H - 46, 2, 32, primary)
    draw_rect(buf, W, H, W - 62, H - 16, 48, 2, secondary)
    draw_rect(buf, W, H, W - 16, H - 46, 2, 32, secondary)
    draw_text(buf, W, H, 26, 26, title, primary, scale=1)
    draw_text(buf, W, H, 26, 42, "F6: DLY+LIM <> SCOPE", secondary, scale=1)


def gen_orb_sprite(out_dir, filename, rgb, diamond=False):
    sz = 76 if diamond else 84
    c = sz * 0.5
    buf = bytearray(sz * sz * 4)
    for py in range(sz):
        ny = (py + 0.5 - c) / c
        for px in range(sz):
            nx = (px + 0.5 - c) / c
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
            aa = clamp01(glow * clamp01(1.0 - r))
            rr = clamp01((rgb[0] / 255.0) * glow + core * 0.95)
            gg = clamp01((rgb[1] / 255.0) * glow + core * 0.95)
            bb = clamp01((rgb[2] / 255.0) * glow + core * 0.95)
            idx = (py * sz + px) * 4
            buf[idx : idx + 4] = bytes((int(rr * 255), int(gg * 255), int(bb * 255), int(aa * 255)))
    out = os.path.join(out_dir, filename)
    write_png(out, sz, sz, buf)
    print(f"Wrote {out}")


def gen_wing_meters(out_dir, tag, col_inner, col_outer, bg_col):
    mw, mh = 610, 24
    for side in ("l", "r"):
        for state in ("off", "on"):
            buf = bytearray(bytes(bg_col) * (mw * mh))
            alpha = 245 if state == "on" else 38
            for x in range(4, mw - 6, 12):
                t = (mw - 1 - x) / float(mw) if side == "l" else x / float(mw)
                r = int(col_inner[0] * (1.0 - t) + col_outer[0] * t)
                g = int(col_inner[1] * (1.0 - t) + col_outer[1] * t)
                b = int(col_inner[2] * (1.0 - t) + col_outer[2] * t)
                draw_rect(buf, mw, mh, x, 3, 9, mh - 6, (r, g, b, alpha))
            out = os.path.join(out_dir, f"meter_wing_{tag}_{side}_{state}.png")
            write_png(out, mw, mh, buf)
            print(f"Wrote {out}")


def gen_theme_pills(skins_dir):
    """Render 66x32 sleek translucent corner switcher pills btn_vtheme_0..3_{off,on}.png."""
    ps_script = r"""
param([string]$SkinsDir)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"

$themes = @(
    @{ Id = 0; Label = "NEON";  R = 0;   G = 229; B = 255 },
    @{ Id = 1; Label = "SOLAR"; R = 255; G = 148; B = 42  },
    @{ Id = 2; Label = "CYBER"; R = 0;   G = 255; B = 128 },
    @{ Id = 3; Label = "VOID";  R = 176; G = 102; B = 255 }
)

function New-RoundedRectPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$rad) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $rad * 2.0
    $path.AddArc($x, $y, $d, $d, 180, 90)
    $path.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $path.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $path.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    return $path
}

$w = 66; $h = 32
$font = New-Object System.Drawing.Font("Segoe UI", 11, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$sf = New-Object System.Drawing.StringFormat
$sf.Alignment = [System.Drawing.StringAlignment]::Center
$sf.LineAlignment = [System.Drawing.StringAlignment]::Center
$rectF = New-Object System.Drawing.RectangleF(0, 0, $w, $h)

foreach ($th in $themes) {
    foreach ($isOn in @($false, $true)) {
        $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
        $g.Clear([System.Drawing.Color]::Transparent)

        $path = New-RoundedRectPath 1.5 1.5 ($w - 3.0) ($h - 3.0) 6.0
        if ($isOn) {
            $fillCol = [System.Drawing.Color]::FromArgb(242, $th.R, $th.G, $th.B)
            $borderCol = [System.Drawing.Color]::FromArgb(255, 255, 255, 255)
            $textCol = [System.Drawing.Color]::FromArgb(255, 10, 12, 16)
        } else {
            $fillCol = [System.Drawing.Color]::FromArgb(185, 14, 18, 26)
            $borderCol = [System.Drawing.Color]::FromArgb(145, $th.R, $th.G, $th.B)
            $textCol = [System.Drawing.Color]::FromArgb(215, 200, 212, 224)
        }
        $fillBrush = New-Object System.Drawing.SolidBrush($fillCol)
        $pen = New-Object System.Drawing.Pen($borderCol, 1.4)
        $textBrush = New-Object System.Drawing.SolidBrush($textCol)

        $g.FillPath($fillBrush, $path)
        $g.DrawPath($pen, $path)
        $g.DrawString($th.Label, $font, $textBrush, $rectF, $sf)

        $state = if ($isOn) { "on" } else { "off" }
        $out = Join-Path $SkinsDir ("btn_vtheme_{0}_{1}.png" -f $th.Id, $state)
        $g.Dispose()
        $fillBrush.Dispose(); $pen.Dispose(); $textBrush.Dispose(); $path.Dispose()
        $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
    }
}
$font.Dispose(); $sf.Dispose()
"""
    fd, tmp_ps1 = tempfile.mkstemp(suffix=".ps1")
    os.close(fd)
    try:
        with open(tmp_ps1, "w", encoding="utf-8") as f:
            f.write(ps_script)
        subprocess.check_call(
            ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", tmp_ps1, "-SkinsDir", skins_dir]
        )
        print(f"Wrote 8 theme pill buttons in {skins_dir}")
    finally:
        if os.path.isfile(tmp_ps1):
            os.remove(tmp_ps1)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    plugin_dir = os.path.dirname(script_dir)
    sdk_root = os.path.dirname(os.path.dirname(plugin_dir))
    assets_dir = os.path.join(plugin_dir, "assets")
    skins_dir = os.path.join(plugin_dir, "skins")
    os.makedirs(assets_dir, exist_ok=True)
    os.makedirs(skins_dir, exist_ok=True)

    # Refresh bg_scope.png (moved F6 label cleanly out of top-right corner)
    subprocess.check_call([os.path.join(sdk_root, ".sdk-tools", "zigenv", "Scripts", "python.exe")
                           if os.path.isfile(os.path.join(sdk_root, ".sdk-tools", "zigenv", "Scripts", "python.exe"))
                           else "python",
                           "-c",
                           "import sys; sys.path.insert(0, r'" + script_dir + "'); import make_visualizer as mv; mv.gen_bg_scope(r'" + assets_dir + "')"])

    zig_py = os.path.join(sdk_root, ".sdk-tools", "zigenv", "Scripts", "python.exe")
    gen_cpp = os.path.join(script_dir, "gen_scope.cpp")
    work_dir = tempfile.mkdtemp(prefix="mushin_scope_")
    try:
        exe_path = os.path.join(work_dir, "gen_scope.exe" if os.name == "nt" else "gen_scope")
        subprocess.check_call([zig_py, "-m", "ziglang", "c++", "-O3", "-std=c++17", gen_cpp, "-o", exe_path])
        subprocess.check_call([exe_path, work_dir])

        # Convert 3 backgrounds with HUD chrome
        bgs = [
            ("bg_scope_solar", "MUSHIN // SOLAR", (255, 178, 72, 255), (210, 52, 72, 255)),
            ("bg_scope_cyber", "MUSHIN // CYBER", (0, 235, 130, 255), (140, 220, 70, 255)),
            ("bg_scope_void",  "MUSHIN // VOID",  (176, 108, 255, 255), (120, 190, 255, 255)),
        ]
        for raw, title, pri, sec in bgs:
            with open(os.path.join(work_dir, f"{raw}.rgba"), "rb") as f:
                buf = bytearray(f.read())
            add_bg_hud(buf, title, pri, sec)
            out = os.path.join(assets_dir, f"{raw}.png")
            write_png(out, W, H, buf)
            print(f"Wrote {out}")

        # Convert 6 FilmStrips
        strips = [
            ("scope_solar_tunnel_strip", 240, 120 * 24),
            ("scope_cyber_tunnel_strip", 240, 120 * 24),
            ("scope_void_tunnel_strip",  240, 120 * 24),
            ("scope_solar_core_strip",   192, 192 * 20),
            ("scope_cyber_core_strip",   192, 192 * 20),
            ("scope_void_core_strip",    192, 192 * 20),
        ]
        for name, sw, sh in strips:
            with open(os.path.join(work_dir, f"{name}.rgba"), "rb") as f:
                raw = f.read()
            out = os.path.join(assets_dir, f"{name}.png")
            write_png(out, sw, sh, raw)
            print(f"Wrote {out}")
    finally:
        shutil.rmtree(work_dir, ignore_errors=True)

    # Orbs (XYPad puck sprites)
    gen_orb_sprite(assets_dir, "scope_orb_solar_1.png", (255, 196, 60), diamond=False)
    gen_orb_sprite(assets_dir, "scope_orb_solar_2.png", (245, 58, 72), diamond=True)
    gen_orb_sprite(assets_dir, "scope_orb_cyber_1.png", (164, 255, 62), diamond=False)
    gen_orb_sprite(assets_dir, "scope_orb_cyber_2.png", (128, 255, 212), diamond=True)
    gen_orb_sprite(assets_dir, "scope_orb_void_1.png",  (178, 92, 255), diamond=False)
    gen_orb_sprite(assets_dir, "scope_orb_void_2.png",  (145, 222, 255), diamond=True)

    # Wing meters
    gen_wing_meters(assets_dir, "solar", (255, 210, 80), (225, 42, 68), (16, 8, 10, 220))
    gen_wing_meters(assets_dir, "cyber", (0, 255, 130),  (180, 255, 60), (6, 16, 10, 220))
    gen_wing_meters(assets_dir, "void",  (175, 95, 255), (140, 225, 255), (8, 8, 22, 220))

    # Pill switcher buttons
    gen_theme_pills(skins_dir)


if __name__ == "__main__":
    main()
