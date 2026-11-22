# Mushin MPC Standalone Port — Technical Notes & Lessons Learned

This document captures the root causes behind the UI rendering issues encountered on Akai MPC standalone hardware (MPC Live Mk 2, MPC OS 3.10.1.12) and how they were resolved.

## Why the UI Rendered Black on Hardware (While DSP Worked)

On the MPC, if `Plugin Skins/TUI.json` fails to parse or contains an unrecognized component/schema mismatch, the `.so` still loads and processes audio, but the touch screen shows no tabs (a black screen). Three `TUI.json` issues caused this:

### 1. Duplicate `"version"` Keys Stripped by `json.loads()` / `json.dump()`
- **Cause:** MPC overlay definitions (`knob overlay`, `knob overlay bipolar`, `menu overlay`) require two consecutive `"version"` keys (`"version": 2` followed by `"version": 1`) inside the same JSON object. Standard `json.loads()` + `json.dump()` collapses duplicate keys, which breaks `TUI.json` parsing on the MPC firmware.
- **Fix:** `bin/make_skin.py` parses `TUI.json` with `object_pairs_hook=gen_ui.Obj` and writes it with `gen_ui.dump_json()` so duplicate keys and key ordering are preserved.

### 2. `"type": "Image"` Does Not Exist in `TUI.json`
- **Cause:** `docs/ui-reference.md` has no standalone `"type": "Image"` component. Placing `"type": "Image"` entries on the `GATE` tab (for the 8 gate pattern previews) caused `TUI.json` to be rejected on hardware.
- **Fix:**
  - Static backgrounds use `pages[].background` (`bg_*.png`).
  - Dynamic/stepped graphics use `"type": "Knob"` with `"knobType": "FilmStrip"` inside `localComponentDefinitions`:
    - `gate_pat_strip.png` (8-frame vertical filmstrip bound to `Gate Pattern`)
    - `orb_strip_0.png` .. `orb_strip_7.png` (16-frame vertical filmstrips bound to the 8 `MAIN` macro parameters)

### 3. `Envelope` (`AHDS`) Component Stage Handles on `GATE`
- **Cause:** As noted in `docs/ui-reference.md` (§6.13), `"type": "Envelope"` requires exact per-type stage-handle keys in order and can fail `TUI.json` loading if any key mismatches.
- **Fix:** `bin/make_skin.py` (`make_gate_envelope`) configures `envelopeType: "AHDS"` with `attackHandle`, `holdHandle`, `decayHandle` bound to `Gate Start`, `Gate Hold`, `Gate End` and `sustainHandle` / `attackShapeHandle` / `decayShapeHandle` set to `"Invalid"` (`forceHideShapeHandles: true`), placed in Row 4 (`23 498 1238 107`) inside the `GATE ENV` panel below the `Start` / `Hold` / `End` faders.

### 4. Custom `FilmStrip` Widgets Wrapped in `localComponentDefinitions`
- **Cause:** `gen_ui.py` instantiates controls on tabs via `localComponentDefinitions` with `"handle remapping"`, rather than placing raw `"type": "Knob"` entries directly on a tab.
- **Fix:** `bin/make_skin.py` defines `orbStrip0..7` and `gatePatStrip` in `localComponentDefinitions` (`acceptsHWFocus: "No"`, `ignoreMousePresses: true`) and instantiates them on `MAIN` and `GATE`.

### 5. Segmented Button Icons & Static `Text` Label Visibility
- **Cause:** On MPC hardware, `"type": "Text"` labels with `"handleName": "Data"` are hidden as unbound handles unless `"handleName": "Invalid"` and `"showWhenDataModelInvalid": "Show"` are set. Additionally, segmented switch `Text` overlays over `seg2/3/4_{on,off}.png` rendered as blank blue/gray boxes on hardware.
- **Fix:**
  - `bin/make_skin.py` sets `"handleName": "Invalid"` and `"showWhenDataModelInvalid": "Show"` on static `Text` labels.
  - `bin/make_button_icons.ps1` bakes crisp anti-aliased icons/shapes directly onto the `seg2`/`seg3`/`seg4` pill bases (`44` `btn_*_{on,off}.png` files), and `bin/make_skin.py` binds them via per-switch `localComponentDefinitions` (`radioFType`, `radioFMode`, `radioRoute`, `radioNRoute`, `radioGRate`, `radioDTempo`, `radioLMode`) while removing the redundant `seg *` `Text` overlays.

### 6. Consecutive Q-Link Banks When Leaving Empty Visual Rows
- **Cause:** `gen_ui.py` maps each 4-slot row directly to a Q-Link bank (`Q-Link 13..16`, `9..12`, `5..8`, `1..4`). Leaving an all-`"empty"` visual row (such as Row 2 on `LFO` between `LFO 1 & 2` and `NOISE OSC`) leaves an all-`-1` bank gap in `Q-Links.json`, causing MPC Q-Link bank cycling to skip the next row.
- **Fix:** `bin/make_skin.py` compacts `Screen Mode Q-Links` on `MAIN` (Tab 1) and `LFO` (Tab 4) into consecutive top-down banks (`13..16` -> `9..12` -> `5..8`) with trailing unused slots set to `-1`.

## Additional Port Design Decisions

- **Permanent Parameter Identity & Hidden Sidechain:** All 77 original parameters stay in their exact order (`0..76`), with the 8 performance macros appended at `77..84`. `SIDECHN` is omitted from `ui.json` (hidden from the UI) while remaining saved and automatable.
- **CHOKE Macro DSP:** `CHOKE` sweeps Filter A's cutoff downward across 7 octaves while simultaneously sweeping Filter B's cutoff upward (high-pass choke) and boosting resonance (`+0.50`), ensuring an audible sweep even when Filter A starts wide open.
- **Native `Name` Labels with Unique `id`s:** `Mushin.cpp` keeps unique parameter `id` strings for state/DAW compatibility while using concise panel-scoped `Name` strings (`Active`, `Mix`, `Cutoff`, `Reso`, `Drive`, `Grit`, `Type`, `Mode`, etc.) so native `"type": "Name"` labels render cleanly inside each panel. `bin/make_skin.py` maps parameter indices via `UI_PARAM_KEYS` into `build/params_ui.json` so `gen_ui.py` binds every control unambiguously.
- **Skin Pipeline (`bin/make_skin.py`):** Because `tools/build.py` copies `plugins/Mushin/skins/` verbatim when that folder exists, always run `bin/make_skin.py` after editing `ui.json` or `Mushin.cpp` parameters; it invokes `gen_ui.py`, `bin/make_button_icons.ps1`, and post-processes `TUI.json` + `Q-Links.json`.
- **Note on `preview_ui.py` vs. Hardware:** `bin/preview_ui.py` is a permissive browser Canvas previewer; always treat `tools/validate.py` and hardware testing on the MPC as the source of truth for `TUI.json` compatibility.
