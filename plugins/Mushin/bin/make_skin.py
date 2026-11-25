#!/usr/bin/env python3
"""Generate plugins/Mushin/skins/ from ui.json + params.json with DRY panel-scoped labels.

Runs tools/gen_ui.py to build the base TUI.json, Q-Links.json, and kit PNGs,
then replaces each widget's global parameter Name label with a concise
panel-scoped Text label (e.g. "Active", "Mix", "Cutoff", "Reso", "Drive")
so section prefixes are not repeated inside labeled panels.
"""

import copy
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
    "Filt Mix": "Mix",
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
    "SC Mix": "Mix",
    "SC Ratio": "Ratio",
    "SC Drive": "Drive",
    "SC Cut": "Cut",
    "SC Gain": "Gain",
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


def make_icon_radio_def(local_defs, base_key, new_key, prefix, count):
    base_def = None
    for entry in local_defs:
        if obj_get(entry, "key") == base_key:
            base_def = entry
            break
    if base_def is None:
        raise RuntimeError(f"Missing base localComponentDefinition {base_key}")
    cloned = copy.deepcopy(base_def)
    obj_set(cloned, "key", new_key)
    val = obj_get(cloned, "value")
    for c in obj_get(val, "componentsData"):
        cd = obj_get(c, "componentData")
        if obj_get(cd, "type") == "Button":
            bdata = obj_get(cd, "data")
            bid = obj_get(bdata, "buttonId")
            if 0 <= bid < count:
                obj_set(bdata, "onImage", f"{prefix}_{bid}_on.png")
                obj_set(bdata, "offImage", f"{prefix}_{bid}_off.png")
    return cloned


def make_multi_icon_radio_def(local_defs, new_key, prefix, xs, width):
    base_def = None
    for entry in local_defs:
        if obj_get(entry, "key") == "radio4":
            base_def = entry
            break
    if base_def is None:
        raise RuntimeError("Missing base localComponentDefinition radio4")
    cloned = copy.deepcopy(base_def)
    obj_set(cloned, "key", new_key)
    val = obj_get(cloned, "value")
    count = len(xs)
    new_comps = []
    template_btn = None
    for c in obj_get(val, "componentsData"):
        cd = obj_get(c, "componentData")
        if obj_get(cd, "type") == "Button":
            if template_btn is None:
                template_btn = c
        else:
            new_comps.append(c)
    for idx, x in enumerate(xs):
        btn = copy.deepcopy(template_btn)
        cd = obj_get(btn, "componentData")
        obj_set(cd, "name", f"seg {idx}")
        bdata = obj_get(cd, "data")
        obj_set(bdata, "onImage", f"{prefix}_{idx}_on.png")
        obj_set(bdata, "offImage", f"{prefix}_{idx}_off.png")
        obj_set(bdata, "buttonId", idx)
        obj_set(bdata, "numButtonsInGroup", count)
        bounds_obj = obj_get(btn, "bounds")
        obj_set(bounds_obj, "bounds", f"{x} 54 {width} 40")
        new_comps.append(btn)
    obj_set(val, "componentsData", new_comps)
    return cloned


def make_magenta_variant(local_defs, base_key, new_key):
    base_def = None
    for entry in local_defs:
        if obj_get(entry, "key") == base_key:
            base_def = entry
            break
    if base_def is None:
        raise RuntimeError(f"Missing base localComponentDefinition {base_key}")
    cloned = copy.deepcopy(base_def)
    obj_set(cloned, "key", new_key)
    val = obj_get(cloned, "value")
    for c in obj_get(val, "componentsData"):
        cd = obj_get(c, "componentData")
        ctype = obj_get(cd, "type")
        cname = obj_get(cd, "name")
        if ctype == "Focus":
            obj_set(obj_get(cd, "data"), "outlineColour", "ffff4d8d")
        elif ctype == "Label" and cname == "value":
            tstyle = obj_get(obj_get(cd, "data"), "textStyle")
            obj_set(tstyle, "colour", "ffff4d8d")
    return cloned


def indent_top_left_labels(local_defs):
    """Reserve top-left space in knob, fader, toggle, and radio cells for the host's yellow Q badge."""
    for entry in local_defs:
        key = str(obj_get(entry, "key") or "")
        is_knob = key.startswith("knob") and "overlay" not in key
        if not (is_knob or key in ("faderUni", "toggle") or key.startswith("radio")):
            continue
        val = obj_get(entry, "value")
        if val is None:
            continue
        for c in obj_get(val, "componentsData") or []:
            cd = obj_get(c, "componentData")
            ctype = obj_get(cd, "type")
            cname = obj_get(cd, "name")
            bounds_obj = obj_get(c, "bounds")
            parts = str(obj_get(bounds_obj, "bounds") or "").split()
            if len(parts) != 4:
                continue
            x, y, w, h = (int(p) for p in parts)
            if is_knob:
                if ctype == "Knob" and x < 30:
                    obj_set(bounds_obj, "bounds", f"{x + 19} {y} {w} {h}")
                elif ctype == "Label" and x >= 90:
                    obj_set(bounds_obj, "bounds", f"{x + 16} {y} {max(40, w - 16)} {h}")
            elif ctype == "Label" and cname == "name" and x == 15:
                obj_set(bounds_obj, "bounds", f"{x + 19} {y} {max(40, w - 19)} {h}")


def make_pump_meter(name, param_idx, direction, off_img, on_img, x, y, w, h):
    return to_obj({
        "version": 2,
        "componentData": {
            "version": 1,
            "name": name,
            "type": "Meter",
            "data": {
                "version": 3,
                "direction": direction,
                "invert": False,
                "inactiveImage": off_img,
                "peakImage": on_img,
                "rmsImage": on_img,
                "peakHandle": f"Parameter {param_idx}",
                "rmsHandle": f"Parameter {param_idx}",
                "peakHoldHandle": f"Hold/Parameter {param_idx}",
                "peakHoldSize": 4,
                "peakHoldColour": "ffffffff",
            },
        },
        "handle remapping": {"version": 1, "map": []},
        "bounds": {
            "version": 1,
            "acceptsHWFocus": "No",
            "showWhenDataModelInvalid": "Show",
            "whenVisible": "Always",
            "boundsType": "Absolute",
            "bounds": f"{x} {y} {w} {h}",
        },
    })


def make_filter_xypad(name, x_param, y_param, puck_img, x, y, w, h):
    return to_obj({
        "version": 2,
        "componentData": {
            "version": 1,
            "name": name,
            "type": "XYPad",
            "data": {
                "version": 1,
                "xPositionHandle": f"Parameter {x_param}",
                "yPositionHandle": f"Parameter {y_param}",
                "image": puck_img,
            },
        },
        "handle remapping": {"version": 1, "map": []},
        "bounds": {
            "version": 1,
            "acceptsHWFocus": "No",
            "showWhenDataModelInvalid": "Show",
            "whenVisible": "Always",
            "boundsType": "Absolute",
            "bounds": f"{x} {y} {w} {h}",
        },
    })


def make_orb_local_def(orb_idx):
    accent = "ff00e5ff" if orb_idx < 4 else "ffff4d8d"
    return to_obj({
        "key": f"macroOrb{orb_idx}",
        "value": {
            "version": 2,
            "actions": [
                {
                    "version": 1,
                    "onAction": "Mouse Down",
                    "handler": "Q-Link",
                    "handleName": "Data",
                    "additionalData": "",
                },
                {
                    "version": 1,
                    "onAction": "Double Click",
                    "handler": "Show Overlay",
                    "handleName": "Data",
                    "additionalData": "knob overlay",
                },
                {
                    "version": 1,
                    "onAction": "Enter Pressed",
                    "handler": "Show Overlay",
                    "handleName": "Data",
                    "additionalData": "knob overlay",
                },
            ],
            "backgroundData": {
                "version": 1,
                "focussed": {"version": 1, "colour": "00000000", "image": ""},
                "unfocussed": {"version": 1, "colour": "00000000", "image": ""},
            },
            "ignoreMousePresses": False,
            "disableCoarseDataWheel": False,
            "componentsData": [
                {
                    "version": 2,
                    "componentData": {
                        "version": 1,
                        "name": "focus",
                        "type": "Focus",
                        "data": {
                            "version": 1,
                            "backgroundColour": "19ffffff",
                            "outlineColour": accent,
                            "backgroundInset": 4.0,
                            "outlineThickness": 1.0,
                        },
                    },
                    "handle remapping": {"version": 1, "map": []},
                    "bounds": {
                        "version": 1,
                        "acceptsHWFocus": "No",
                        "showWhenDataModelInvalid": "Show",
                        "whenVisible": "WhenFocussed",
                        "boundsType": "Absolute",
                        "bounds": "0 0 278 250",
                    },
                },
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
                        "bounds": "49 6 180 180",
                    },
                },
                {
                    "version": 2,
                    "componentData": {
                        "version": 1,
                        "name": "name",
                        "type": "Label",
                        "data": {
                            "version": 1,
                            "textStyle": {
                                "version": 1,
                                "font": {
                                    "version": 1,
                                    "name": "Titillium Web",
                                    "style": "Regular",
                                    "height": 24.0,
                                },
                                "colour": "fff0f4f8",
                                "justification": "horizontallyCentred",
                                "case": "Original",
                            },
                            "type": "Name",
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
                        "bounds": "10 188 258 30",
                    },
                },
                {
                    "version": 2,
                    "componentData": {
                        "version": 1,
                        "name": "value",
                        "type": "Label",
                        "data": {
                            "version": 1,
                            "textStyle": {
                                "version": 1,
                                "font": {
                                    "version": 1,
                                    "name": "Titillium Web",
                                    "style": "Regular",
                                    "height": 24.0,
                                },
                                "colour": accent,
                                "justification": "horizontallyCentred",
                                "case": "Original",
                            },
                            "type": "Value",
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
                        "bounds": "10 216 258 28",
                    },
                },
            ],
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


def make_gate_envelope(idx_start, idx_hold, idx_end, idx_floor):
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
                "sustainHandle": f"Parameter {idx_floor}",
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


# Map parameter index -> unique ui.json key name (so gen_ui.py can bind controls
# unambiguously while Mushin.cpp exposes concise DRY display names via Name).
UI_PARAM_KEYS = {
    5: "Dry/Wet Mix",
    6: "A Cutoff", 7: "A Reso", 8: "A Drive", 9: "A Grit", 10: "A Type", 11: "A Mode",
    12: "B Cutoff", 13: "B Reso", 14: "B Drive", 15: "B Grit", 16: "B Type", 17: "B Mode",
    18: "Routing",
    23: "L1 A Cut", 24: "L1 A Res", 25: "L1 A Grit", 26: "L1 B Cut", 27: "L1 B Res", 28: "L1 B Grit",
    29: "L2 A Cut", 30: "L2 A Res", 31: "L2 A Grit", 32: "L2 B Cut", 33: "L2 B Res", 34: "L2 B Grit",
    35: "SC Active", 36: "SC Source", 37: "SC Thresh", 38: "SC Attack", 39: "SC Release",
    40: "SC Mode", 41: "SC Amount", 42: "SC Target", 43: "SC HPF", 44: "SC LPF",
    45: "Noise Active", 46: "Noise Type", 47: "Noise Freq", 48: "Noise Level", 49: "Noise Route",
    51: "Gate Active", 52: "Gate Mix", 53: "Gate Pattern", 54: "Gate Rate",
    55: "Gate Start", 56: "Gate Hold", 57: "Gate End", 58: "Gate Depth",
    59: "Decay Active", 62: "Decay Mix",
    64: "Delay Active", 65: "Delay Time", 66: "Delay FB", 67: "Delay Mix",
    69: "Delay Sync", 70: "Delay Tempo",
    71: "Limit Active", 72: "Limit Mode", 73: "Limit Drive", 74: "Limit Ceil",
    75: "Limit Rel", 76: "Limit Mix",
}


def main():
    if not os.path.isfile(PARAMS_PATH):
        print(f"ERROR: {PARAMS_PATH} not found. Run build.py first.", file=sys.stderr)
        return 1

    with open(PARAMS_PATH, "r", encoding="utf-8") as f:
        params_data = json.load(f)
    for p in params_data["params"]:
        if p["index"] in UI_PARAM_KEYS:
            p["name"] = UI_PARAM_KEYS[p["index"]]
    params_ui_path = os.path.join(BUILD_DIR, "params_ui.json")
    with open(params_ui_path, "w", encoding="utf-8") as f:
        json.dump(params_data, f, indent=2)

    os.makedirs(SKINS_DIR, exist_ok=True)
    cmd = [
        sys.executable,
        GEN_UI_PATH,
        "--params", params_ui_path,
        "--ui", UI_PATH,
        "--out", SKINS_DIR,
        "--kit-cache", BUILD_DIR,
    ]
    res = subprocess.run(cmd, check=False)
    if res.returncode != 0:
        return res.returncode

    icon_ps1 = os.path.join(SCRIPT_DIR, "make_button_icons.ps1")
    res_icons = subprocess.run(
        ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", icon_ps1, "-SkinsDir", SKINS_DIR],
        check=False,
    )
    if res_icons.returncode != 0:
        return res_icons.returncode

    tui_path = os.path.join(SKINS_DIR, "TUI.json")
    with open(tui_path, "r", encoding="utf-8") as f:
        raw_text = f.read()

    # Preserve duplicate keys (such as duplicate "version" keys in overlay widgets)
    # using gen_ui.Obj and gen_ui.dump_json so the MPC firmware parses TUI.json.
    tui = json.loads(raw_text, object_pairs_hook=gen_ui.Obj)

    page_data = obj_get(tui, "pageData")
    comp_defs = obj_get(page_data, "componentDefinitions")
    local_defs = obj_get(comp_defs, "localComponentDefinitions")
    tabs = obj_get(page_data, "tabs")

    params_by_name = {p["name"]: p["index"] for p in params_data["params"]}

    macro_names = [
        "EXHAUSTION", "CHOKE", "INFO DECAY", "CORRUPT MIX",
        "KINETIC MOD", "RESONATOR", "GATE CARVE", "SMASH",
    ]
    col_centers = [162, 482, 802, 1122]
    row_centers = [244, 548]

    for i in range(8):
        local_defs.append(make_orb_local_def(i))
    local_defs.append(make_gate_pat_local_def())

    # Bake crisp shapes/icons into segmented switch buttons
    icon_radio_specs = [
        ("radio4", "radioFType", "btn_ftype", 4),
        ("radio4", "radioFMode", "btn_fmode", 4),
        ("radio2", "radioRoute", "btn_route", 2),
        ("radio3", "radioNRoute", "btn_nroute", 3),
        ("radio3", "radioGRate", "btn_grate", 3),
        ("radio4", "radioDTempo", "btn_dtempo", 4),
        ("radio2", "radioLMode", "btn_lmode", 2),
        ("radio2", "radioScMode", "btn_scmode", 2),
    ]
    for base_k, new_k, prefix, cnt in icon_radio_specs:
        local_defs.append(make_icon_radio_def(local_defs, base_k, new_k, prefix, cnt))
    local_defs.append(make_multi_icon_radio_def(local_defs, "radioLfoWave", "btn_lfowave", [16, 66, 116, 166, 216], 46))
    local_defs.append(make_multi_icon_radio_def(local_defs, "radioNoiseType", "btn_noisetype", [17, 58, 99, 140, 181, 222], 38))

    # Reserve top-left margin for host's yellow Q badge before cloning magenta variants
    indent_top_left_labels(local_defs)

    # Magenta Engine B / LFO 2 variants
    for base_k, mag_k in [
        ("knobUni", "knobUniMag"),
        ("knobBi", "knobBiMag"),
        ("radioFType", "radioFTypeMag"),
        ("radioFMode", "radioFModeMag"),
        ("radioLfoWave", "radioLfoWaveMag"),
    ]:
        local_defs.append(make_magenta_variant(local_defs, base_k, mag_k))

    switch_type_map = {
        "A Type": "radioFType",
        "B Type": "radioFTypeMag",
        "A Mode": "radioFMode",
        "B Mode": "radioFModeMag",
        "Routing": "radioRoute",
        "Noise Route": "radioNRoute",
        "Gate Rate": "radioGRate",
        "Delay Tempo": "radioDTempo",
        "Limit Mode": "radioLMode",
        "LFO 1 Wave": "radioLfoWave",
        "LFO 2 Wave": "radioLfoWaveMag",
        "Noise Type": "radioNoiseType",
        "SC Mode": "radioScMode",
    }
    magenta_knob_names = {
        "B Cutoff", "B Reso", "B Drive", "B Grit",
        "LFO 2 Freq",
        "L2 A Cut", "L2 A Res", "L2 A Grit", "L2 B Cut", "L2 B Res", "L2 B Grit",
    }
    seg_prefixes = tuple(f"{k} seg " for k in switch_type_map)

    for tab in tabs:
        cdef = obj_get(tab, "componentDefinition")
        comps = obj_get(cdef, "componentsData")
        filtered = []
        for c in comps:
            cd = obj_get(c, "componentData")
            cname = obj_get(cd, "name") or ""
            ctype = str(obj_get(cd, "type") or "")
            if ctype == "Label" and cname.startswith(seg_prefixes):
                continue
            if cname in switch_type_map and (ctype.startswith("radio") or cname in ("LFO 1 Wave", "LFO 2 Wave", "Noise Type")):
                obj_set(cd, "type", switch_type_map[cname])
            elif cname in magenta_knob_names:
                if ctype == "knobUni":
                    obj_set(cd, "type", "knobUniMag")
                elif ctype == "knobBi":
                    obj_set(cd, "type", "knobBiMag")
            filtered.append(c)
        obj_set(cdef, "componentsData", filtered)

    for tab in tabs:
        tname = obj_get(tab, "tabName")
        if tname == "MAIN":
            slot_x = [16, 336, 656, 976]
            cdef = obj_get(tab, "componentDefinition")
            comps = obj_get(cdef, "componentsData")
            for c in comps:
                cd = obj_get(c, "componentData")
                cname = obj_get(cd, "name") or ""
                if cname in macro_names:
                    idx = macro_names.index(cname)
                    obj_set(cd, "type", f"macroOrb{idx}")
                    bounds_obj = obj_get(c, "bounds")
                    y = 46 if idx < 4 else 348
                    obj_set(bounds_obj, "bounds", f"{slot_x[idx % 4]} {y} 278 250")
            obj_set(tab, "qlinkBoundsData", ["12 42 1246 258", "12 344 1246 258"])
        elif tname == "FILTER":
            cdef = obj_get(tab, "componentDefinition")
            comps = obj_get(cdef, "componentsData")
            comps.append(
                make_filter_xypad(
                    "filter_a_xy",
                    params_by_name["A Cutoff"],
                    params_by_name["A Reso"],
                    "puck_cyan.png",
                    656, 180, 292, 107,
                )
            )
            comps.append(
                make_filter_xypad(
                    "filter_b_xy",
                    params_by_name["B Cutoff"],
                    params_by_name["B Reso"],
                    "puck_magenta.png",
                    962, 180, 292, 107,
                )
            )
        elif tname == "LFO":
            cdef = obj_get(tab, "componentDefinition")
            comps = obj_get(cdef, "componentsData")
            comps.append(
                make_filter_xypad(
                    "lfo_freq_xy",
                    params_by_name["LFO 1 Freq"],
                    params_by_name["LFO 2 Freq"],
                    "puck_lfo.png",
                    23, 196, 1238, 107,
                )
            )
        elif tname == "PUMP":
            cdef = obj_get(tab, "componentDefinition")
            comps = obj_get(cdef, "componentsData")
            comps.append(
                make_pump_meter(
                    "sc_in_meter",
                    params_by_name["SC In"],
                    "Up",
                    "meter_in_off.png",
                    "meter_in_on.png",
                    1038, 384, 40, 190,
                )
            )
            comps.append(
                make_pump_meter(
                    "sc_duck_meter",
                    params_by_name["SC Duck"],
                    "Down",
                    "meter_duck_off.png",
                    "meter_duck_on.png",
                    1166, 384, 40, 190,
                )
            )
        elif tname == "GATE":
            cdef = obj_get(tab, "componentDefinition")
            comps = obj_get(cdef, "componentsData")
            comps.append(
                make_gate_pattern_strip(params_by_name["Gate Pattern"])
            )
            comps.append(
                make_gate_envelope(
                    params_by_name["Gate Start"],
                    params_by_name["Gate Hold"],
                    params_by_name["Gate End"],
                    params_by_name["Gate Floor"],
                )
            )

    # Ensure all static Text labels (including gen_ui segment button captions)
    # use handleName "Invalid" and showWhenDataModelInvalid "Show" so the MPC
    # never hides them as unbound "Data" handles.
    for tab in tabs:
        cdef = obj_get(tab, "componentDefinition")
        for c in obj_get(cdef, "componentsData"):
            cd = obj_get(c, "componentData")
            if obj_get(cd, "type") == "Label":
                cdata = obj_get(cd, "data")
                if obj_get(cdata, "type") == "Text":
                    obj_set(cdata, "handleName", "Invalid")
                    bounds_obj = obj_get(c, "bounds")
                    obj_set(bounds_obj, "showWhenDataModelInvalid", "Show")

    # Assign 6 function-key tabs with LFO MTX as LFO SubTab 2 and PUMP as GATE SubTab 2
    fn_key_map = {
        "MAIN": (0, 0),
        "S-CURVE": (1, 0),
        "FILTER": (2, 0),
        "LFO": (3, 0),
        "LFO MTX": (3, 1),
        "GATE": (4, 0),
        "PUMP": (4, 1),
        "DLY+LIM": (5, 0),
    }
    for tab in tabs:
        tname = obj_get(tab, "tabName")
        if tname in fn_key_map:
            fk, sub = fn_key_map[tname]
            obj_set(tab, "fnKeyIndex", fk)
            obj_set(tab, "fnKeySubIndex", sub)

    with open(tui_path, "w", encoding="utf-8") as f:
        f.write(gen_ui.dump_json(tui) + "\n")

    # Map MAIN (Tab 1) and LFO (Tab 4) Q-Links to consecutive top-down banks,
    # and remap Tab/SubTab to match the 6 function-key tabs (LFO MTX -> Tab 4 SubTab 2).
    qlinks_path = os.path.join(SKINS_DIR, "Q-Links.json")
    with open(qlinks_path, "r", encoding="utf-8") as f:
        qlinks = json.load(f)
    for entry in qlinks["Screen Mode Q-Links"]["map"]:
        t = entry.get("Tab")
        if t == 1:
            ql = entry["Q-Links"]
            for i in range(4):
                ql[f"Q-Link {13 + i}"] = params_by_name[macro_names[i]]
                ql[f"Q-Link {9 + i}"] = params_by_name[macro_names[4 + i]]
                ql[f"Q-Link {5 + i}"] = -1
                ql[f"Q-Link {1 + i}"] = -1
        elif t == 4:
            ql = entry["Q-Links"]
            lfo_b1 = [
                params_by_name["LFO 1 Freq"],
                params_by_name["LFO 1 Wave"],
                params_by_name["LFO 2 Freq"],
                params_by_name["LFO 2 Wave"],
            ]
            lfo_b2 = [
                params_by_name["Noise Active"],
                params_by_name["Noise Type"],
                params_by_name["Noise Freq"],
                params_by_name["Noise Level"],
            ]
            lfo_b3 = [
                params_by_name["Noise Route"],
                params_by_name["Filter FM"],
                -1,
                -1,
            ]
            for i in range(4):
                ql[f"Q-Link {13 + i}"] = lfo_b1[i]
                ql[f"Q-Link {9 + i}"] = lfo_b2[i]
                ql[f"Q-Link {5 + i}"] = lfo_b3[i]
                ql[f"Q-Link {1 + i}"] = -1
        elif t == 5:
            entry["Tab"] = 4
            entry["SubTab"] = 2
        elif t == 6:
            entry["Tab"] = 5
            entry["SubTab"] = 1
        elif t == 7:
            entry["Tab"] = 5
            entry["SubTab"] = 2
        elif t == 8:
            entry["Tab"] = 6
            entry["SubTab"] = 1
    with open(qlinks_path, "w", encoding="utf-8") as f:
        json.dump(qlinks, f, indent=4)

    print(f"Generated {SKINS_DIR} with native Name bindings and MAIN Q-Links")
    return 0


if __name__ == "__main__":
    sys.exit(main())
