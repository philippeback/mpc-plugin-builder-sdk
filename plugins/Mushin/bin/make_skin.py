#!/usr/bin/env python3
"""Generate plugins/Mushin/skins/ from ui.json + params.json with DRY panel-scoped labels.

Runs tools/gen_ui.py to build the base TUI.json, Q-Links.json, and kit PNGs,
then replaces each widget's global parameter Name label with a concise
panel-scoped Text label (e.g. "Active", "Mix", "Cutoff", "Reso", "Drive")
so section prefixes are not repeated inside labeled panels.
"""

import json
import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PLUGIN_DIR = os.path.dirname(SCRIPT_DIR)
SDK_ROOT = os.path.dirname(os.path.dirname(PLUGIN_DIR))
BUILD_DIR = os.path.join(PLUGIN_DIR, "build")
SKINS_DIR = os.path.join(PLUGIN_DIR, "skins")
PARAMS_PATH = os.path.join(BUILD_DIR, "params.json")
UI_PATH = os.path.join(PLUGIN_DIR, "ui.json")
GEN_UI_PATH = os.path.join(SDK_ROOT, "tools", "gen_ui.py")
sys.path.insert(0, os.path.join(SDK_ROOT, "tools"))
import gen_ui  # noqa: E402

# Map global parameter name -> concise panel-scoped display label
DRY_LABELS = {
    # S-Curve
    "Dry/Wet Mix": "Mix",
    # Info Decay
    "Decay Active": "Active",
    "Decay Mix": "Mix",
    # Filter A & B
    "A Cutoff": "Cutoff",
    "A Reso": "Reso",
    "A Drive": "Drive",
    "A Grit": "Grit",
    "A Type": "Type",
    "A Mode": "Mode",
    "B Cutoff": "Cutoff",
    "B Reso": "Reso",
    "B Drive": "Drive",
    "B Grit": "Grit",
    "B Type": "Type",
    "B Mode": "Mode",
    "Routing": "Path",
    # LFO Matrix
    "L1 A Cut": "A Cut",
    "L1 A Res": "A Res",
    "L1 A Grit": "A Grit",
    "L1 B Cut": "B Cut",
    "L1 B Res": "B Res",
    "L1 B Grit": "B Grit",
    "L2 A Cut": "A Cut",
    "L2 A Res": "A Res",
    "L2 A Grit": "A Grit",
    "L2 B Cut": "B Cut",
    "L2 B Res": "B Res",
    "L2 B Grit": "B Grit",
    # Noise Osc
    "Noise Active": "Active",
    "Noise Type": "Type",
    "Noise Freq": "Freq",
    "Noise Level": "Level",
    "Noise Route": "Route",
    # Sidechain
    "SC Active": "Active",
    "SC Source": "Source",
    "SC Target": "Target",
    "SC Mode": "Mode",
    "SC Thresh": "Thresh",
    "SC Amount": "Amount",
    "SC Attack": "Attack",
    "SC Release": "Release",
    "SC HPF": "HPF",
    "SC LPF": "LPF",
    # Trance Gate
    "Gate Active": "Active",
    "Gate Pattern": "Pattern",
    "Gate Rate": "Rate",
    "Gate Mix": "Mix",
    "Gate Depth": "Depth",
    "Gate Start": "Start",
    "Gate Hold": "Hold",
    "Gate End": "End",
    # Delay
    "Delay Active": "Active",
    "Delay Sync": "Sync",
    "Delay FB": "Feedback",
    "Delay Time": "Time",
    "Delay Tempo": "Tempo",
    "Delay Mix": "Mix",
    # Limiter
    "Limit Active": "Active",
    "Limit Mode": "Mode",
    "Limit Drive": "Drive",
    "Limit Ceil": "Ceiling",
    "Limit Rel": "Release",
    "Limit Mix": "Mix",
}


def to_obj(v):
    if isinstance(v, gen_ui.Obj):
        return v
    if isinstance(v, dict):
        return gen_ui.Obj([(k, to_obj(val)) for k, val in v.items()])
    if isinstance(v, list):
        return [to_obj(x) for x in v]
    return v


def obj_get(o, key, default=None):
    for k, v in o:
        if k == key:
            return v
    return default


def obj_set(o, key, val):
    for i, (k, _) in enumerate(o):
        if k == key:
            o[i] = (key, val)
            return
    o.append((key, val))


def make_label_entry(name, text, x, y, w, h, height, colour, when_visible):
    return to_obj({
        "version": 2,
        "componentData": {
            "version": 1,
            "name": name,
            "type": "Label",
            "data": {
                "version": 1,
                "textStyle": {
                    "version": 1,
                    "font": {
                        "version": 1,
                        "name": "Titillium Web",
                        "style": "Regular",
                        "height": height,
                    },
                    "colour": colour,
                    "justification": "left",
                    "case": "Original",
                },
                "type": "Text",
                "handleName": "Data",
                "text": text,
            },
        },
        "handle remapping": {
            "version": 1,
            "map": [],
        },
        "bounds": {
            "version": 1,
            "acceptsHWFocus": "No",
            "showWhenDataModelInvalid": "Show",
            "whenVisible": when_visible,
            "boundsType": "Absolute",
            "bounds": f"{x} {y} {w} {h}",
        },
    })


def make_orb_local_def(orb_idx):
    return to_obj({
        "key": f"orbStrip{orb_idx}",
        "value": {
            "version": 2,
            "actions": [],
            "backgroundData": {
                "version": 1,
                "focussed": {"version": 1, "colour": "00000000", "image": ""},
                "unfocussed": {"version": 1, "colour": "00000000", "image": ""},
            },
            "ignoreMousePresses": True,
            "disableCoarseDataWheel": True,
            "componentsData": [
                {
                    "version": 2,
                    "componentData": {
                        "version": 1,
                        "name": "knob",
                        "type": "Knob",
                        "data": {
                            "version": 2,
                            "knobType": "FilmStrip",
                            "filmStrip": f"orb_strip_{orb_idx}.png",
                            "numFrames": 16,
                            "invert": False,
                            "handleName": "Data",
                        },
                    },
                    "handle remapping": {"version": 1, "map": []},
                    "bounds": {
                        "version": 1,
                        "acceptsHWFocus": "No",
                        "showWhenDataModelInvalid": "Show",
                        "whenVisible": "Always",
                        "boundsType": "Absolute",
                        "bounds": "0 0 114 114",
                    },
                }
            ],
        },
    })


def make_macro_orb_strip(orb_idx, param_idx, cx, cy):
    size = 114
    x = cx - size // 2
    y = cy - size // 2
    return to_obj({
        "version": 2,
        "componentData": {
            "version": 1,
            "name": f"macro_orb_{orb_idx}",
            "type": f"orbStrip{orb_idx}",
            "data": {
                "version": 1,
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
            "bounds": f"{x} {y} {size} {size}",
        },
    })


def make_gate_pat_local_def():
    return to_obj({
        "key": "gatePatStrip",
        "value": {
            "version": 2,
            "actions": [],
            "backgroundData": {
                "version": 1,
                "focussed": {"version": 1, "colour": "00000000", "image": ""},
                "unfocussed": {"version": 1, "colour": "00000000", "image": ""},
            },
            "ignoreMousePresses": True,
            "disableCoarseDataWheel": True,
            "componentsData": [
                {
                    "version": 2,
                    "componentData": {
                        "version": 1,
                        "name": "knob",
                        "type": "Knob",
                        "data": {
                            "version": 2,
                            "knobType": "FilmStrip",
                            "filmStrip": "gate_pat_strip.png",
                            "numFrames": 8,
                            "invert": False,
                            "handleName": "Data",
                        },
                    },
                    "handle remapping": {"version": 1, "map": []},
                    "bounds": {
                        "version": 1,
                        "acceptsHWFocus": "No",
                        "showWhenDataModelInvalid": "Show",
                        "whenVisible": "Always",
                        "boundsType": "Absolute",
                        "bounds": "0 0 918 107",
                    },
                }
            ],
        },
    })


def make_gate_pattern_strip(idx_pattern):
    return to_obj({
        "version": 2,
        "componentData": {
            "version": 1,
            "name": "gate_pat_strip",
            "type": "gatePatStrip",
            "data": {
                "version": 1,
                "handleName": "Data",
            },
        },
        "handle remapping": {
            "version": 1,
            "map": [
                {"key": "Data", "value": f"Parameter {idx_pattern}"},
            ],
        },
        "bounds": {
            "version": 1,
            "acceptsHWFocus": "No",
            "showWhenDataModelInvalid": "Show",
            "whenVisible": "Always",
            "boundsType": "Absolute",
            "bounds": "23 196 918 107",
        },
    })


def make_gate_envelope(idx_start, idx_hold, idx_end):
    return to_obj({
        "version": 2,
        "componentData": {
            "version": 1,
            "name": "gate_env",
            "type": "Envelope",
            "data": {
                "version": 1,
                "envelopeType": "AHDS",
                "envelopeStyle": "WithoutHeader",
                "envelopeName": "GATE ENV",
                "backgroundColour": "ff10141a",
                "scaleColour": "ff262e3a",
                "lineColour": "ff00e5ff",
                "fillColour": "3300e5ff",
                "highlightColour": "ffffffff",
                "lineWidth": 3,
                "handleWidth": 15,
                "handleHeight": 10,
                "handleShape": "Circle",
                "handleLineWidth": 2,
                "handleLineColour": "ffffffff",
                "handleFillColour": "ff10141a",
                "handleTextColour": "ffffffff",
                "handleLineWidthOnPressed": 2,
                "handleLineColourOnPressed": "ffffffff",
                "handleFillColourOnPressed": "ff00e5ff",
                "handleTextColourOnPressed": "ffffffff",
                "handleArrowLineColourOnPressed": "ffffffff",
                "handleArrowFillColourOnPressed": "ff00e5ff",
                "attackHandle": f"Parameter {idx_start}",
                "holdHandle": f"Parameter {idx_hold}",
                "decayHandle": f"Parameter {idx_end}",
                "sustainHandle": "Invalid",
                "attackShapeHandle": "Invalid",
                "decayShapeHandle": "Invalid",
                "forceHideShapeHandles": True,
                "forceCurveShape": 0.5,
                "curveType": "Quadratic",
            },
        },
        "handle remapping": {
            "version": 1,
            "map": [],
        },
        "bounds": {
            "version": 1,
            "acceptsHWFocus": "No",
            "showWhenDataModelInvalid": "Show",
            "whenVisible": "Always",
            "boundsType": "Absolute",
            "bounds": "23 498 1238 107",
        },
    })


def main():
    if not os.path.isfile(PARAMS_PATH):
        print(f"ERROR: {PARAMS_PATH} not found. Run build.py first.", file=sys.stderr)
        return 1

    os.makedirs(SKINS_DIR, exist_ok=True)
    cmd = [
        sys.executable,
        GEN_UI_PATH,
        "--params", PARAMS_PATH,
        "--ui", UI_PATH,
        "--out", SKINS_DIR,
        "--kit-cache", BUILD_DIR,
    ]
    res = subprocess.run(cmd, check=False)
    if res.returncode != 0:
        return res.returncode

    tui_path = os.path.join(SKINS_DIR, "TUI.json")
    with open(tui_path, "r", encoding="utf-8") as f:
        raw_text = f.read()

    # Preserve duplicate keys (such as duplicate "version" keys in overlay widgets)
    # using gen_ui.Obj and gen_ui.dump_json so the MPC firmware parses TUI.json.
    tui = json.loads(raw_text, object_pairs_hook=gen_ui.Obj)

    page_data = obj_get(tui, "pageData")
    comp_defs = obj_get(page_data, "componentDefinitions")
    local_defs = obj_get(comp_defs, "localComponentDefinitions")

    # Record each localDef's "name" Label geometry, then remove it from the localDef
    name_specs = {}
    for ldef in local_defs:
        key = obj_get(ldef, "key")
        val = obj_get(ldef, "value")
        comps = obj_get(val, "componentsData")
        kept = []
        for c in comps:
            cd = obj_get(c, "componentData")
            cdata = obj_get(cd, "data")
            if obj_get(cd, "type") == "Label" and obj_get(cdata, "type") == "Name":
                bounds_obj = obj_get(c, "bounds")
                bx, by, bw, bh = [int(v) for v in obj_get(bounds_obj, "bounds").split()]
                ts = obj_get(cdata, "textStyle")
                font = obj_get(ts, "font")
                name_specs[key] = (bx, by, bw, bh, obj_get(font, "height"), obj_get(ts, "colour"))
            else:
                kept.append(c)
        obj_set(val, "componentsData", kept)

    # Add a panel-scoped Text Label for each control on every tab
    tabs = obj_get(page_data, "tabs")
    for tab in tabs:
        cdef = obj_get(tab, "componentDefinition")
        comps = obj_get(cdef, "componentsData")
        new_comps = []
        for idx, c in enumerate(comps):
            new_comps.append(c)
            cd = obj_get(c, "componentData")
            ctype = obj_get(cd, "type")
            if ctype in name_specs:
                bounds_obj = obj_get(c, "bounds")
                bx, by, bw, bh = [int(v) for v in obj_get(bounds_obj, "bounds").split()]
                nx, ny, nw, nh, fh, col = name_specs[ctype]
                orig_name = obj_get(cd, "name", "")
                short_name = DRY_LABELS.get(orig_name, orig_name)
                when_vis = obj_get(bounds_obj, "whenVisible", "Always")
                new_comps.append(
                    make_label_entry(
                        f"lbl_{idx}",
                        short_name,
                        bx + nx,
                        by + ny,
                        nw,
                        nh,
                        fh,
                        col,
                        when_vis,
                    )
                )
        obj_set(cdef, "componentsData", new_comps)

    with open(PARAMS_PATH, "r", encoding="utf-8") as f:
        params_by_name = {p["name"]: p["index"] for p in json.load(f)["params"]}

    macro_names = [
        "EXHAUSTION", "CHOKE", "INFO DECAY", "CORRUPT MIX",
        "KINETIC MOD", "RESONATOR", "GATE CARVE", "SMASH",
    ]
    col_centers = [162, 482, 802, 1122]
    row_centers = [244, 548]

    for i in range(8):
        local_defs.append(make_orb_local_def(i))
    local_defs.append(make_gate_pat_local_def())

    for tab in tabs:
        tname = obj_get(tab, "tabName")
        if tname == "MAIN":
            cdef = obj_get(tab, "componentDefinition")
            comps = obj_get(cdef, "componentsData")
            for i, mname in enumerate(macro_names):
                comps.append(
                    make_macro_orb_strip(
                        i,
                        params_by_name[mname],
                        col_centers[i % 4],
                        row_centers[i // 4],
                    )
                )
        elif tname == "GATE":
            cdef = obj_get(tab, "componentDefinition")
            comps = obj_get(cdef, "componentsData")
            comps.append(
                make_gate_pattern_strip(params_by_name["Gate Pattern"])
            )

    with open(tui_path, "w", encoding="utf-8") as f:
        f.write(gen_ui.dump_json(tui) + "\n")

    print(f"Patched DRY labels into {tui_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
