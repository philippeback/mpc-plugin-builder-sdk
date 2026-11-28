#!/usr/bin/env python3
"""Generate 17-frame gate_playhead_strip.png and wire it into GATE tab in TUI.json."""

import json
import math
import os
import struct
import sys
import zlib

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PLUGIN_DIR = os.path.dirname(SCRIPT_DIR)
SDK_ROOT = os.path.dirname(os.path.dirname(PLUGIN_DIR))
sys.path.insert(0, os.path.join(SDK_ROOT, "tools"))

import gen_ui  # noqa: E402

FW = 880
FH = 86
NF = 17  # 0..15 = active step 0..15, 16 = gate inactive


def write_png(path, w, h, rgba):
    stride = w * 4
    rows = []
    for y in range(h):
        row = rgba[y * stride : (y + 1) * stride]
        filt = bytearray(stride + 1)
        filt[0] = 1
        filt[1:5] = row[0:4]
        for i in range(4, stride):
            filt[1 + i] = (row[i] - row[i - 4]) & 0xFF
        rows.append(bytes(filt))
    comp = zlib.compress(b"".join(rows), 9)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", comp) + chunk(b"IEND", b""))


def blend_px(buf, w, h, x, y, r, g, b, a):
    if x < 0 or x >= w or y < 0 or y >= h or a <= 0:
        return
    idx = (y * w + x) * 4
    sa = a / 255.0
    da = buf[idx + 3] / 255.0
    out_a = sa + da * (1.0 - sa)
    if out_a <= 1e-6:
        return
    buf[idx + 0] = int((r * sa + buf[idx + 0] * da * (1.0 - sa)) / out_a)
    buf[idx + 1] = int((g * sa + buf[idx + 1] * da * (1.0 - sa)) / out_a)
    buf[idx + 2] = int((b * sa + buf[idx + 2] * da * (1.0 - sa)) / out_a)
    buf[idx + 3] = int(out_a * 255.0)


def fill_glow_pill(buf, total_h, y_off, x0, y0, pw, ph, core_rgb, halo_rgb, radius=4):
    for dy in range(-radius, ph + radius):
        py = y0 + dy
        if py < 0 or py >= FH:
            continue
        for dx in range(-radius, pw + radius):
            px = x0 + dx
            if px < 0 or px >= FW:
                continue
            cx = 0 if 0 <= dx < pw else (-dx if dx < 0 else dx - (pw - 1))
            cy = 0 if 0 <= dy < ph else (-dy if dy < 0 else dy - (ph - 1))
            dist = math.hypot(cx, cy)
            if dist == 0:
                blend_px(buf, FW, total_h, px, y_off + py, core_rgb[0], core_rgb[1], core_rgb[2], 255)
            elif dist <= radius:
                t = max(0.0, 1.0 - dist / (radius + 0.5))
                alpha = int(205.0 * (t * t))
                blend_px(buf, FW, total_h, px, y_off + py, halo_rgb[0], halo_rgb[1], halo_rgb[2], alpha)


def generate_strip(out_path):
    total_h = FH * NF
    buf = bytearray(FW * total_h * 4)

    for frame in range(NF):
        y_off = frame * FH

        # Subtle ruler ticks above beats 1, 5, 9, 13 across all frames
        for s in range(0, 16, 4):
            tx = 4 + s * 55 + 16
            for py in range(5, 8):
                for px in range(tx, tx + 15):
                    blend_px(buf, FW, total_h, px, y_off + py, 75, 95, 118, 95)

        if frame < 16:
            bx = 4 + frame * 55
            # 1. Radiant surround halo behind 47x56 button (peaks cleanly 4px outside button edges)
            fill_glow_pill(buf, total_h, y_off, bx - 1, 14, 49, 58, (255, 115, 165), (255, 77, 141), radius=4)
            # 2. Bright top chaser pill (high contrast warm-white core with neon magenta/gold aura)
            fill_glow_pill(buf, total_h, y_off, bx + 2, 4, 43, 6, (255, 246, 200), (255, 96, 152), radius=3)
            # 3. Sleek bottom laser underline below the active step
            fill_glow_pill(buf, total_h, y_off, bx + 8, 77, 31, 4, (255, 210, 120), (255, 77, 141), radius=2)

    write_png(out_path, FW, total_h, buf)
    print(f"Wrote {out_path} ({FW}x{total_h}, {os.path.getsize(out_path)} bytes)")


def obj_get(obj, key, default=None):
    for k, v in obj:
        if k == key:
            return v
    return default


def obj_set(obj, key, val):
    for i, (k, _) in enumerate(obj):
        if k == key:
            obj[i] = (k, val)
            return
    obj.append((key, val))


def update_tui(tui_path, param_idx=135):
    with open(tui_path, "r", encoding="utf-8") as f:
        tui = json.loads(f.read(), object_pairs_hook=gen_ui.Obj)

    tabs = obj_get(obj_get(tui, "pageData"), "tabs")
    gate_tab = None
    for t in tabs:
        if obj_get(t, "tabName") == "GATE":
            gate_tab = t
            break
    if gate_tab is None:
        raise RuntimeError("GATE tab not found in TUI.json")

    cdef = obj_get(gate_tab, "componentDefinition")
    comps = obj_get(cdef, "componentsData")

    # Remove existing gate_playhead if present so script is idempotent
    comps = [c for c in comps if obj_get(obj_get(c, "componentData"), "name") != "gate_playhead"]

    playhead_comp = json.loads(json.dumps({
        "version": 2,
        "componentData": {
            "version": 1,
            "name": "gate_playhead",
            "type": "Image",
            "data": {
                "version": 2,
                "imageType": "FilmStrip",
                "colour": "0",
                "image": "gate_playhead_strip.png",
                "numFrames": NF,
                "handleName": "Data",
            },
        },
        "handle remapping": {
            "version": 1,
            "map": [
                {"key": "Data", "value": f"Parameter {param_idx}"},
            ],
        },
        "bounds": {
            "version": 1,
            "acceptsHWFocus": "No",
            "showWhenDataModelInvalid": "Show",
            "whenVisible": "Always",
            "boundsType": "Absolute",
            "bounds": "42 206 880 86",
        },
    }), object_pairs_hook=gen_ui.Obj)

    insert_at = next(
        (i for i, c in enumerate(comps) if obj_get(obj_get(c, "componentData"), "name") == "gate_step_0"),
        len(comps),
    )
    comps.insert(insert_at, playhead_comp)
    obj_set(cdef, "componentsData", comps)

    with open(tui_path, "w", encoding="utf-8") as f:
        f.write(gen_ui.dump_json(tui) + "\n")
    print(f"Updated {tui_path}: inserted gate_playhead behind gate_step_0 at index {insert_at} -> Parameter {param_idx}")


def main():
    strip_out = os.path.join(PLUGIN_DIR, "assets", "gate_playhead_strip.png")
    generate_strip(strip_out)
    update_tui(os.path.join(PLUGIN_DIR, "skins", "TUI.json"), 135)


if __name__ == "__main__":
    main()
