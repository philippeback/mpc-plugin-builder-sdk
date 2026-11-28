# Mushin — Visual System, Asset Generators & Publishing Pipeline

Reference for Mushin’s touchscreen skinning, procedural shader/sprite toolchain (`plugins/Mushin/bin/`), live visualizers (`SCOPE` & `GATE`), manual generation, and web/binary packaging.

---

## 1. 9-Tab Function-Key Grid & Skin Layout (`TUI.json` / `Q-Links.json`)

| Tab | `fnKeyIndex` | `fnKeySubIndex` | Tab Name | Background PNG | Signature Visual Elements |
|---|---:|---:|---|---|---|
| 1 | `0` | `0` | `MAIN` | `bg_main.png` | `8` × `180×180` 22-frame concave macro orb-knobs (`orb_strip_0..7.png`), `2` Q-Link bank rects |
| 2 | `1` | `0` | `S-CURVE` | `bg_scurve.png` | `S-Curve`, `Info Decay` (with 25-frame `#FF4D8D` `decay_viz_strip.png` strip), `Auto In` bay |
| 3 | `2` | `0` | `FILTER` | `bg_filter.png` | Cyan Filter A / Magenta Filter B, baked icon `CLN/VOWL/ACID/DIGI` & shape buttons, twin `XYPad`s |
| 4 | `3` | `0` | `LFO` | `bg_lfo.png` | 5-wave icon LFOs, full-width `1238×107` `LFO 1 × LFO 2` `XYPad`, 6-wave `Noise Osc` + `Env Follower` |
| 5 | `3` | `1` | `LFO MTX` | `bg_matrix.png` | `2×6` bipolar LFO knobs + `4` Column-4 `Env Follower` matrix targets |
| 6 | `4` | `0` | `GATE` | `bg_gate.png` | 128-pattern menu, `16` tappable step toggles, `gate_playhead_strip.png` chaser, `AHDS` `Envelope` graph |
| 7 | `4` | `1` | `PUMP` | `bg_pump.png` | `SC Ratio`, `INT/4x4` source switch, `4` target toggles, vertical cyan `IN` & magenta `DUCK` LED meters |
| 8 | `5` | `0` | `DLY+LIM` | `bg_dly_lim.png` | Conditional `Time`/`Tempo` via `IndexedEnabling` (`Param 69`), `Delay Color`, horizontal `LIMITER GR` meter |
| 9 | `5` | `1` | `SCOPE` | `bg_scope*.png` | 4-theme `IndexedEnabling` layer stack (`Param 134`), `radioVTheme` pills, 16-Q-Link stage controls |

### Visual Conventions Enforced in `bin/make_skin.py`
- **Engine A / B Color Split:** Engine A / LFO 1 / Input uses **Cyan (`#00E5FF`)**; Engine B / LFO 2 / Ducking / Decay uses **Magenta (`#FF4D8D`)** across knobs, active pills, XY pucks (`puck_cyan.png`, `puck_magenta.png`, `puck_lfo.png`), and LED meters.
- **Zero-Clip Q-Badge Clearance:** Native `Name` and `Value` labels in reusable `localComponentDefinitions` are indented `+18 px` right (`indent_top_left_labels`) so the MPC’s hardware yellow `Q` focus badge never overlaps knob titles.
- **Duplicate JSON Key Preservation:** All `TUI.json` scripts parse and write through `tools/gen_ui.py` (`gen_ui.Obj` / `gen_ui.dump_json`) because MPC overlay widgets contain duplicate `"version"` keys that standard Python `json.dump()` collapses.

---

## 2. Procedural Asset & Skin Pipeline (`plugins/Mushin/bin/`)

| Script / Shader | Compiles / Reads | Produces (`assets/` or `skins/`) | Purpose |
|---|---|---|---|
| `gen_orbs.cpp` + `composite_orbs.ps1` | Zig C++ (`-O3`) | `orb_strip_0..7.png`, `orbs_sheet.jpg` | Renders `22`-frame `180×180` (`180×3960` <= `4096 px`) bioluminescent orbs with dark concave recesses |
| `gen_panels.cpp` + `make_panels.py` | Zig C++ (`-O3`) + Python | `bg_{main,scurve,filter,lfo,matrix,gate,pump,dly_lim}.png`, `step*.png` | Pure organic domain-warped FBM/cellular textures + 4×4 Bayer dithering inside recessed studio bays |
| `make_button_icons.ps1` | `System.Drawing` | `btn_{ftype,fmode,route,nroute,grate,dtempo,lmode,scmode,lfowave,noisetype}*` | Anti-aliased vector icon glyphs baked onto segmented switch states |
| `make_visualizer.py` | Pure Python | `bg_scope.png`, `scope_tunnel_strip.png`, `scope_mandala_strip.png`, `scope_orb_{cyan,mag}.png`, `meter_wing_*` | Generates the **NEON** visualizer suite (`F6` label kept top-left away from top-right theme pills) |
| `gen_scope.cpp` + `make_scope_themes.py` | Zig C++ (`-O3`) + PS + Python | `bg_scope_{solar,cyber,void}.png`, `scope_{solar,cyber,void}_{tunnel,core}_strip.png`, `scope_orb_*`, `meter_wing_*`, `btn_vtheme_0..3_{off,on}.png` | Renders the **SOLAR**, **CYBER**, and **VOID** shader suites + `66×32` corner switcher pill buttons (~`5.9 MB` total for all 3 extra suites) |
| `make_gate_playhead.py` | Pure Python + `gen_ui` | `gate_playhead_strip.png`, `TUI.json` (`gate_playhead`) | Renders `17`-frame (`880×1462`, `15.9 KB`) step chaser + halo strip and wires it behind `gate_step_0..15` |
| `make_skin.py` | `gen_ui.py` + `TUI.json` + `Q-Links.json` | `TUI.json`, `Q-Links.json` | Builds base layout from `ui.json`, clones Magenta/Orb defs, aligns Q-Links & sub-tabs |
| `update_scope_tui.py` | `gen_ui.py` | `TUI.json` (`SCOPE` tab + `radioVTheme`) | Wraps all 4 visualizer themes in `IndexedEnabling/0..3/4/Parameter 134` stacks + corner pills |
| `preview_ui.py` | `TUI.json` + `Q-Links.json` + images | `build/ui_preview.html` | Offline HTML interactive tab/sub-tab/skin inspector matching native coordinates |

---

## 3. Live Visualizers Architecture & ARM Hardware Behavior

### `SCOPE` Live 4-Theme Switcher (`Option 2`: In-Place Switching)
- **Why Single Tab + `IndexedEnabling`:** All 4 themes (**NEON**, **SOLAR**, **CYBER**, **VOID**) live inside the single `SCOPE` tab (`F6` SubTab 2) without sacrificing another F-key slot. Because tab `backgroundData` cannot use `IndexedEnabling`, each theme owns an explicit `"0 0 1280 628"` `Regular` `Image` backdrop layer behind its animated layers.
- **Layer Visibility Gating:** Every layer for theme `idx ∈ {0,1,2,3}` uses `bounds` `version: 2`, `"acceptsHWFocus": "No"`, `"showWhenDataModelInvalid": "Hide"`, and `"additionalInvalidatingHandles": ["IndexedEnabling/<idx>/4/Parameter 134"]`.
- **Corner Switcher (`radioVTheme`):** Pinned last in `SCOPE` at `"974 22 276 32"`, mapped to `Parameter 134` (`viz_theme`), remaining visible over every theme.
- **4 Distinct Visual Languages:**
  1. **NEON (`0`):** Cyan/magenta Lissajous warp tunnel, 6-fold blooming sacred mandala core, star/diamond orbs, cyan-to-magenta wings.
  2. **SOLAR (`1`):** Obsidian-crimson plasma vortex, 12-point eclipse corona + shockwaves, molten gold & red-dwarf orbs, flame wing bars.
  3. **CYBER (`2`):** CRT perspective grid, 3D projected wireframe octagonal corridor, spinning 3D octahedron/cube lock reticle, lime/ice-mint HUD bars.
  4. **VOID (`3`):** Midnight velvet abyss + micro-stars, bioluminescent liquid caustic ribbons, 8-petal lotus nebula + 4 orbiting pearl moons, ultraviolet/ice bars.
- **Hardware Smoothness Notes (MPC Live Mk II ARMv7 vs. Live III ARM64):**
  - OpenGL ES texture height limit (`<= 4096 px`) is strictly respected (`orb_strip_*` `3960`, `scope_*_core_strip` `3840`, `scope_*_tunnel_strip` `2880`, `gate_playhead_strip` `1462`).
  - `Mushin.cpp` throttles the 8 `SCOPE` telemetry parameters (`126..133`) at `~30 Hz` (`sr / 30.0`, `0.008f` delta threshold). Moving from 32-bit ARMv7 (MPC Live II) to 64-bit ARMv8 Cortex-A55/A76 (MPC Live III) gives more CPU/memory headroom against stutter, while maximum visual cadence stays bounded by `~30 Hz` host automation updates, `20–24` strip frames, and full-screen `1280×628` alpha blending.

### `GATE` Live Step Playhead (`gate_playhead`)
- Positioned at `"42 206 880 86"` behind `gate_step_0..15` (`x = 46 + step * 55, y = 221, 47×56`) with `acceptsHWFocus: "No"` so touch-tapping individual step toggles is never blocked.
- Draws a warm-gold/white top chaser pill, 4-sided magenta halo, and bottom accent underline per active step `0..15`, falling back to subtle beat ticks on frame `16` when `GATE` is bypassed.
- Unlike the ~30 Hz smooth visualizers, `kTgPlayStep` (`Parameter 135`) triggers `notifyHostParameterChanged` **immediately on integer step changes** (<`3 ms` latency, `~4–12` messages/sec at typical BPMs).

---

## 4. Manuals, Website & Distribution Packaging

- **Manual Specification & User Guide:**
  - [doc/manual_spec.md](file:///C:/Dev/github/inmusic-software/mpc-plugin-builder-sdk/doc/manual_spec.md): Synthesized canonical formatting rules modeled on Akai / AIR / Digidesign PDF manuals (hierarchy, 4-column parameter tables, Q-Link matrix grids, preset directories, troubleshooting matrix, trademarks).
  - [Mushin - User Guide.md](Mushin%20-%20User%20Guide.md): Complete `~50 KB` user manual (`136` params, `168` presets, Q-Link tables, workflows, troubleshooting).
- **Product Website (`plugins/Mushin/site/`) & GitHub Pages (`origin/gh-pages`):**
  - Static dark-studio showcase (`index.html`, `style.css`, `assets/orbs_sheet.jpg`, `assets/scope_{neon,solar,cyber,void}.png`, bundled `Mushin - User Guide.md`).
  - Published along with a direct download copy of [PhilippeBack - VST - Mushin.zip](../build/PhilippeBack%20-%20VST%20-%20Mushin.zip) (~`22 MB` universal ARM32 + ARM64 bundle) via `origin/gh-pages`, plus [mushin-1.0.0-mpc.zip](../dist/mushin-1.0.0-mpc.zip) (`tools/package.py` bundle with `INSTALL.txt`).
