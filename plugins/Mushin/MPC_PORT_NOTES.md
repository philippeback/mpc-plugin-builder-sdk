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

### 3. Untested `Envelope` (`AHDS`) Component Sensitivity
- **Cause:** As noted in `docs/ui-reference.md` (§6.13), `"type": "Envelope"` requires exact per-type stage-handle keys on hardware and can fail the entire `TUI.json` load if any key mismatches.
- **Fix:** Removed the experimental `AHDS` `Envelope` widget from `GATE` and kept the `Start` / `Hold` / `End` faders alongside the 8-frame `gate_pat_strip.png` pattern display.

### 4. Custom `FilmStrip` Widgets Wrapped in `localComponentDefinitions`
- **Cause:** `gen_ui.py` instantiates controls on tabs via `localComponentDefinitions` with `"handle remapping"`, rather than placing raw `"type": "Knob"` entries directly on a tab.
- **Fix:** `bin/make_skin.py` defines `orbStrip0..7` and `gatePatStrip` in `localComponentDefinitions` (`acceptsHWFocus: "No"`, `ignoreMousePresses: true`) and instantiates them on `MAIN` and `GATE`.

## Additional Port Design Decisions

- **Permanent Parameter Identity & Hidden Sidechain:** All 77 original parameters stay in their exact order (`0..76`), with the 8 performance macros appended at `77..84`. `SIDECHN` is omitted from `ui.json` (hidden from the UI) while remaining saved and automatable.
- **DRY Panel Labels:** `Mushin.cpp` keeps unique parameter names for DAW/MPC automation, while `bin/make_skin.py` removes `"type": "Name"` from `localComponentDefinitions` and places concise panel-scoped `"type": "Text"` labels (`Active`, `Mix`, `Cutoff`, `Reso`, etc.) inside each rounded panel.
- **Note on `preview_ui.py` vs. Hardware:** `bin/preview_ui.py` is a permissive browser Canvas previewer; always treat `tools/validate.py` and hardware testing on the MPC as the source of truth for `TUI.json` compatibility.
