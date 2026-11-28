#!/usr/bin/env python3
"""Update plugins/Mushin/skins/TUI.json with 4-theme SCOPE switcher (Option 2)."""

import copy
import json
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PLUGIN_DIR = os.path.dirname(SCRIPT_DIR)
SDK_ROOT = os.path.dirname(os.path.dirname(PLUGIN_DIR))
sys.path.insert(0, os.path.join(SDK_ROOT, "tools"))

import gen_ui  # noqa: E402


def to_obj(val):
    return json.loads(json.dumps(val), object_pairs_hook=gen_ui.Obj)


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


def make_bounds_v2(bounds_str, invalid_handle, focus="No"):
    return to_obj({
        "version": 2,
        "acceptsHWFocus": focus,
        "showWhenDataModelInvalid": "Hide",
        "whenVisible": "Always",
        "boundsType": "Absolute",
        "bounds": bounds_str,
        "additionalInvalidatingHandles": [invalid_handle],
    })


def make_radio_vtheme_local_def():
    comps = [
        {
            "version": 2,
            "componentData": {
                "version": 1,
                "name": "focus",
                "type": "Focus",
                "data": {
                    "version": 1,
                    "backgroundColour": "19ffffff",
                    "outlineColour": "ff00e5ff",
                    "backgroundInset": 2.0,
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
                "bounds": "0 0 276 32",
            },
        }
    ]
    for i in range(4):
        comps.append({
            "version": 2,
            "componentData": {
                "version": 1,
                "name": f"seg {i}",
                "type": "Button",
                "data": {
                    "version": 2,
                    "onImage": f"btn_vtheme_{i}_on.png",
                    "offImage": f"btn_vtheme_{i}_off.png",
                    "buttonId": i,
                    "numButtonsInGroup": 4,
                    "handleName": "Data",
                    "gestureBehaviour": "Instant",
                },
            },
            "handle remapping": {"version": 1, "map": []},
            "bounds": {
                "version": 1,
                "acceptsHWFocus": "No",
                "showWhenDataModelInvalid": "Show",
                "whenVisible": "Always",
                "boundsType": "Absolute",
                "bounds": f"{i * 70} 0 66 32",
            },
        })

    return to_obj({
        "key": "radioVTheme",
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
            "componentsData": comps,
        },
    })


def main():
    tui_path = os.path.join(PLUGIN_DIR, "skins", "TUI.json")
    with open(tui_path, "r", encoding="utf-8") as f:
        tui = json.loads(f.read(), object_pairs_hook=gen_ui.Obj)

    page_data = obj_get(tui, "pageData")
    comp_defs = obj_get(page_data, "componentDefinitions")
    local_defs = obj_get(comp_defs, "localComponentDefinitions")

    # Replace or append radioVTheme local definition
    local_defs[:] = [d for d in local_defs if obj_get(d, "key") != "radioVTheme"]
    local_defs.append(make_radio_vtheme_local_def())

    tabs = obj_get(page_data, "tabs")
    scope_tab = None
    for t in tabs:
        if obj_get(t, "tabName") == "SCOPE":
            scope_tab = t
            break
    if scope_tab is None:
        raise RuntimeError("SCOPE tab not found in TUI.json")

    cdef = obj_get(scope_tab, "componentDefinition")
    orig_comps = obj_get(cdef, "componentsData")

    # Find template components from the original NEON SCOPE layout
    # (exclude any previously added vtheme_* or bg_* components so script is idempotent)
    def find_comp_by_type_and_prefix(ctype, name_prefix=None):
        matches = []
        for c in orig_comps:
            cd = obj_get(c, "componentData")
            t = obj_get(cd, "type")
            nm = obj_get(cd, "name") or ""
            if t == ctype and not nm.startswith(("bg_theme_", "solar_", "cyber_", "void_", "neon_")):
                if name_prefix is None or name_prefix in nm:
                    matches.append(copy.deepcopy(c))
        return matches

    img_templates = find_comp_by_type_and_prefix("Image")
    xy_templates = find_comp_by_type_and_prefix("XYPad")
    meter_templates = find_comp_by_type_and_prefix("Meter")

    # Separate tunnel vs mandala from FilmStrip images (24 vs 20 frames)
    tunnel_tpl = None
    core_tpl = None
    for c in img_templates:
        cd = obj_get(c, "componentData")
        data = obj_get(cd, "data")
        nf = obj_get(data, "numFrames")
        if nf == 24 and tunnel_tpl is None:
            tunnel_tpl = c
        elif nf == 20 and core_tpl is None:
            core_tpl = c

    if tunnel_tpl is None or core_tpl is None or len(xy_templates) < 2 or len(meter_templates) < 2:
        raise RuntimeError(
            f"Unexpected SCOPE template layout: tunnel={bool(tunnel_tpl)} core={bool(core_tpl)} "
            f"xy={len(xy_templates)} meter={len(meter_templates)}"
        )

    themes = [
        (
            0, "neon",
            "bg_scope.png",
            "scope_tunnel_strip.png",
            "scope_mandala_strip.png",
            "scope_orb_cyan.png",
            "scope_orb_mag.png",
            "meter_wing_l_off.png", "meter_wing_l_on.png",
            "meter_wing_r_off.png", "meter_wing_r_on.png",
        ),
        (
            1, "solar",
            "bg_scope_solar.png",
            "scope_solar_tunnel_strip.png",
            "scope_solar_core_strip.png",
            "scope_orb_solar_1.png",
            "scope_orb_solar_2.png",
            "meter_wing_solar_l_off.png", "meter_wing_solar_l_on.png",
            "meter_wing_solar_r_off.png", "meter_wing_solar_r_on.png",
        ),
        (
            2, "cyber",
            "bg_scope_cyber.png",
            "scope_cyber_tunnel_strip.png",
            "scope_cyber_core_strip.png",
            "scope_orb_cyber_1.png",
            "scope_orb_cyber_2.png",
            "meter_wing_cyber_l_off.png", "meter_wing_cyber_l_on.png",
            "meter_wing_cyber_r_off.png", "meter_wing_cyber_r_on.png",
        ),
        (
            3, "void",
            "bg_scope_void.png",
            "scope_void_tunnel_strip.png",
            "scope_void_core_strip.png",
            "scope_orb_void_1.png",
            "scope_orb_void_2.png",
            "meter_wing_void_l_off.png", "meter_wing_void_l_on.png",
            "meter_wing_void_r_off.png", "meter_wing_void_r_on.png",
        ),
    ]

    new_comps = []
    for idx, tname, bg_img, tun_img, cor_img, orb1, orb2, ml_off, ml_on, mr_off, mr_on in themes:
        inv = f"IndexedEnabling/{idx}/4/Parameter 134"

        # 1. Explicit 0 0 1280 628 background image layer for this theme
        new_comps.append(to_obj({
            "version": 2,
            "componentData": {
                "version": 1,
                "name": f"bg_theme_{tname}",
                "type": "Image",
                "data": {
                    "version": 2,
                    "imageType": "Regular",
                    "colour": "0",
                    "image": bg_img,
                },
            },
            "handle remapping": {"version": 1, "map": []},
            "bounds": json.loads(gen_ui.dump_json(make_bounds_v2("0 0 1280 628", inv))),
        }))

        # 2. Tunnel FilmStrip (Parameter 126)
        tun = copy.deepcopy(tunnel_tpl)
        cd = obj_get(tun, "componentData")
        obj_set(cd, "name", f"{tname}_tunnel")
        obj_set(obj_get(cd, "data"), "image", tun_img)
        obj_set(tun, "bounds", make_bounds_v2("0 0 1280 628", inv))
        new_comps.append(tun)

        # 3. Core FilmStrip (Parameter 127)
        cor = copy.deepcopy(core_tpl)
        cd = obj_get(cor, "componentData")
        obj_set(cd, "name", f"{tname}_core")
        obj_set(obj_get(cd, "data"), "image", cor_img)
        obj_set(cor, "bounds", make_bounds_v2("380 54 520 520", inv))
        new_comps.append(cor)

        # 4 & 5. Twin XYPad Orbs (Parameters 128/129 & 130/131)
        for oi, (xy_tpl, orb_img) in enumerate([(xy_templates[0], orb1), (xy_templates[1], orb2)], start=1):
            xy = copy.deepcopy(xy_tpl)
            cd = obj_get(xy, "componentData")
            obj_set(cd, "name", f"{tname}_orb_{oi}")
            obj_set(obj_get(cd, "data"), "image", orb_img)
            obj_set(xy, "bounds", make_bounds_v2("40 24 1200 580", inv))
            new_comps.append(xy)

        # 6 & 7. Left and Right Wing Meters (Parameters 132 & 133)
        for mi, (m_tpl, off_img, on_img, bstr, side_tag) in enumerate([
            (meter_templates[0], ml_off, ml_on, "20 584 610 24", "l"),
            (meter_templates[1], mr_off, mr_on, "650 584 610 24", "r"),
        ]):
            m = copy.deepcopy(m_tpl)
            cd = obj_get(m, "componentData")
            obj_set(cd, "name", f"{tname}_wing_{side_tag}")
            mdata = obj_get(cd, "data")
            obj_set(mdata, "offImage", off_img)
            obj_set(mdata, "onImage", on_img)
            obj_set(m, "bounds", make_bounds_v2(bstr, inv))
            new_comps.append(m)

    # 8. Corner Theme Switcher (top-right, always visible above all theme layers)
    new_comps.append(to_obj({
        "version": 2,
        "componentData": {
            "version": 1,
            "name": "Theme",
            "type": "radioVTheme",
            "data": {
                "version": 1,
                "handleName": "Data",
            },
        },
        "handle remapping": {
            "version": 1,
            "map": [
                {"key": "Data", "value": "Parameter 134"},
            ],
        },
        "bounds": {
            "version": 1,
            "acceptsHWFocus": "Yes",
            "showWhenDataModelInvalid": "Hide",
            "whenVisible": "Always",
            "boundsType": "Absolute",
            "bounds": "974 22 276 32",
        },
    }))

    obj_set(cdef, "componentsData", new_comps)

    with open(tui_path, "w", encoding="utf-8") as f:
        f.write(gen_ui.dump_json(tui) + "\n")
    print(f"Updated {tui_path}: SCOPE now has {len(new_comps)} components (4 themes + live Theme switcher)")


if __name__ == "__main__":
    main()
