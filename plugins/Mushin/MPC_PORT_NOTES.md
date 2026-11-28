# Mushin MPC Standalone Port — Technical Notes & Lessons Learned

This document captures the root causes behind the UI rendering issues encountered on Akai MPC standalone hardware (MPC Live Mk 2, MPC OS 3.10.1.12) and the verified patterns used across Mushin's current 8-screen, 115-parameter UI.

## Why the UI Rendered Black on Hardware (While DSP Worked)

On the MPC, if `Plugin Skins/TUI.json` fails to parse or contains an unrecognized component/schema mismatch, the `.so` still loads and processes audio, but the touch screen shows no tabs (a black screen). Key rules verified on hardware:

### 1. Duplicate `"version"` Keys Stripped by `json.loads()` / `json.dump()`
- **Cause:** MPC overlay definitions (`knob overlay`, `knob overlay bipolar`, `menu overlay`) require two consecutive `"version"` keys (`"version": 2` followed by `"version": 1`) inside the same JSON object. Standard `json.loads()` + `json.dump()` collapses duplicate keys and breaks `TUI.json` parsing on MPC hardware.
- **Fix:** `bin/make_skin.py` parses `TUI.json` with `object_pairs_hook=gen_ui.Obj` and writes it with `gen_ui.dump_json()` so duplicate keys and key ordering are preserved.

### 2. Custom `FilmStrip` Widgets, `Image` Strips & Embedded GPU Limits
- **Static backgrounds:** Use `pages[].background` (`bg_*.png`).
- **Tappable graphics & `Knob` `FilmStrip`s:** Wrap `"knobType": "FilmStrip"` in `localComponentDefinitions` (`orbStrip0..7` on `MAIN`, `acceptsHWFocus: "No"`, `ignoreMousePresses: true`).
- **`Image` `FilmStrip` overlays:** Version 2 `"type": "Image"` with `"imageType": "FilmStrip"` and `"acceptsHWFocus": "No"` works cleanly behind controls (`decay_viz_strip.png`, `629×1600`, 25 frames, inserted at index `0` of `S-CURVE`'s `componentsData`).
- **128-Pattern Gate scaling:** A 128-frame `918×107` filmstrip (`918×13,696`) exceeds embedded GPU max texture height (`4096 px`). Replacing it with 16 individual `Button` toggles (`47×56` `step_{on,off}.png` / `step_beat_{on,off}.png`) supports all 128 patterns cleanly and lets users tap individual steps live on the touchscreen.

### 3. `Envelope` (`AHDS`) Component Stage Handles on `GATE`
- `"type": "Envelope"` requires exact per-type stage-handle keys in order.
- `bin/make_skin.py` (`make_gate_envelope`) configures `envelopeType: "AHDS"` with `attackHandle`, `holdHandle`, `decayHandle`, and `sustainHandle` bound to `Gate Start`, `Gate Hold`, `Gate End`, and `Gate Floor` (with shape handles set to `"Invalid"` and `forceHideShapeHandles: true`).

### 4. Telemetry (`XYPad`, `Meter`) & `host_sim` Check 3 State Round-Trip
- `XYPad` (`FILTER` cut/res, `LFO` live X/Y) and `Meter` (`PUMP` `IN`/`DUCK`, `DLY+LIM` `LIMITER GR`) widgets read non-automatable parameters updated via `notifyHostParameterChanged()` (~30 Hz).
- **Important:** `host_sim` Check 3 renders 2 blocks right after `effSetChunk` and verifies chunk byte-identity and parameter restoration. Always gate meter/visualizer updates behind `if (++blocksSinceReset_ > 4)` so `EffSetChunk` round-trip tests pass cleanly.

### 5. Segmented Button Icons, Q-Badges & Static `Text` Visibility
- `"type": "Text"` labels need `"handleName": "Invalid"` and `"showWhenDataModelInvalid": "Show"`.
- `indent_top_left_labels()` shifts card labels right (`+18 px`) so the MPC's yellow Q-Link `Q` badge doesn't cover text.
- `bin/make_button_icons.ps1` bakes crisp vector icons directly onto `seg2`/`seg3`/`seg4` and `46×40` / `38×40` pill canvases (`90` PNGs total, removing blank `seg *` `Text` overlays).

### 6. Preset Switching UI Synchronization (`effSetProgram`)
- `mpc_plugin.h`'s `effSetProgram` updates parameter storage under `bulkLoading_` without emitting host notifications.
- Checking `currentProgram() != lastProgram_` in `process()` (after `blocksSinceReset_ > 4`) and calling `requestDisplayUpdate()` + `notifyHostParameterChanged(i)` for all automatable parameters ensures all knobs, icon switches, 16 gate steps, and macro orbs snap immediately on the MPC when switching presets.
- **Note on Third-Party VST2 Preset Folders:** Per `docs/presets.md`, the MPC does not browse `.xpl` folders inside third-party plugin folders, so built-in `addProgram` entries use category prefixes (`SAT:`, `FLT:`, `VOWL:`, `DECY:`, `LFO:`, `GATE:`, `PUMP:`, `COMBO:`, `MUSH:`).

### 7. Consecutive Q-Link Banks & `make_skin.py` Order
- `bin/make_skin.py` compacts `Screen Mode Q-Links` on `MAIN` and `LFO` so empty visual rows don't create dead Q-Link banks, and remaps `Tab`/`SubTab` indices across all 8 tabs onto the 6 function keys (`LFO MTX` → `F4 SubTab 2`, `PUMP` → `F5 SubTab 2`).
- **Build note:** When `plugins/Mushin/skins/` exists, `tools/build.py` copies it verbatim without running `gen_ui.py`; after changing parameters or `ui.json`, run `build.py` (to update `params.json`), then `make_skin.py`, then `build.py` again.
