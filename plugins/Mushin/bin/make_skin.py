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


def make_label_entry(name, text, x, y, w, h, height, colour, when_visible):
    return {
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
    }


def make_gate_envelope(idx_start, idx_hold, idx_end):
    return {
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
    }


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

    # gen_ui.py emits valid JSON (except duplicate "version" keys in overlays if any,
    # which json.loads handles by keeping the last; we only need to preserve structure
    # or check if overlays exist). Here TUI.json has no duplicate keys in tabs/localDefs.
    tui = json.loads(raw_text)

    # Record each localDef's "name" Label geometry, then remove it from the localDef
    name_specs = {}
    for ldef in tui["pageData"]["componentDefinitions"]["localComponentDefinitions"]:
        key = ldef["key"]
        comps = ldef["value"]["componentsData"]
        kept = []
        for c in comps:
            cd = c["componentData"]
            if cd.get("type") == "Label" and cd.get("data", {}).get("type") == "Name":
                bx, by, bw, bh = [int(v) for v in c["bounds"]["bounds"].split()]
                ts = cd["data"]["textStyle"]
                name_specs[key] = (bx, by, bw, bh, ts["font"]["height"], ts["colour"])
            else:
                kept.append(c)
        ldef["value"]["componentsData"] = kept

    # Add a panel-scoped Text Label for each control on every tab
    for tab in tui["pageData"]["tabs"]:
        comps = tab["componentDefinition"]["componentsData"]
        new_comps = []
        for idx, c in enumerate(comps):
            new_comps.append(c)
            ctype = c["componentData"].get("type")
            if ctype in name_specs:
                bx, by, bw, bh = [int(v) for v in c["bounds"]["bounds"].split()]
                nx, ny, nw, nh, fh, col = name_specs[ctype]
                orig_name = c["componentData"].get("name", "")
                short_name = DRY_LABELS.get(orig_name, orig_name)
                when_vis = c["bounds"].get("whenVisible", "Always")
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
        tab["componentDefinition"]["componentsData"] = new_comps

    with open(PARAMS_PATH, "r", encoding="utf-8") as f:
        params_by_name = {p["name"]: p["index"] for p in json.load(f)["params"]}

    for tab in tui["pageData"]["tabs"]:
        if tab.get("tabName") == "GATE":
            tab["componentDefinition"]["componentsData"].append(
                make_gate_envelope(
                    params_by_name["Gate Start"],
                    params_by_name["Gate Hold"],
                    params_by_name["Gate End"],
                )
            )

    with open(tui_path, "w", encoding="utf-8") as f:
        json.dump(tui, f, indent=4)

    print(f"Patched DRY labels into {tui_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
