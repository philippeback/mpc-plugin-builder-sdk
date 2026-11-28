# Mushin — Engineering Chronology & Hardware Lessons

Distilled changelog of the port/expansion from initial SDK bring-up (`be6fce2`) through the 9-tab, 136-parameter, 168-preset standalone MPC engine — plus the hardware/host discoveries that shaped it.

---

## 1. Development Chronology Across Phases

| Phase | Key Commits | Major Capabilities & Milestones Landed |
|---|---|---|
| **Phase 1: Port & UI Bring-Up** | `be6fce2` → `1293925` | Raw DSP port to pure dependency-free MIT `mpc::dsp` C++; solved black-screen MPC `TUI.json` issues (duplicate `"version"` key preservation via `gen_ui.Obj`, `"Invalid"` static label handles, removing unsupported widgets); built `preview_ui.py`, vector button icons (`make_button_icons.ps1`), and initial `MAIN` 3D macro orbs + product site (`b884712`). |
| **Phase 2: 6-Tab Architecture & Macro Calibration** | `cc0389c` → `4f89890` | Consolidated UI into 6 hardware function keys (`F1`–`F6`) with sub-tabs (`LFO MTX`, `DLY+LIM`); combined orbs + knobs into `180×180` orb-knobs; added bidirectional `baseValues_[]` macro reflection, parallel-aware `CHOKE`, `WHY_MUSHIN` docs (`EN`/`FR`), transparent `Init`, `Filter Mix`, and Waveshaper reset snap. |
| **Phase 3: PUMP, Vowel Formants, 128 Patterns & 96 Presets** | `d92b6da` → `0ddf22f` | Built `PUMP` (`F5` SubTab 2) with `PEAK/RMS`, `SC Ratio`, `±6` octave Filter `Cut`, independent `SC Drive/Cut/Gain/Reso` toggles, and `IN`/`DUCK` telemetry meters; added `#FF4D8D` `Info Decay` visualizer strip; added 3-band morphing Vocal `VOWL` filter, `128` gate masks, `16` touch-tappable step buttons, `Gate Offset`, `LIMITER GR` bar, and expanded factory bank to `96` presets with full control sync on load. |
| **Phase 4: Topologies, Input Stager, Delay Color, SCOPE & 168 Presets** | `7458dfa` → `1282345` | Balanced saturation drive & Gain curves; added `Auto In` Gain Stager (`Target`/`Trim`), `STEREO` (`L→A/R→B`) & `M/S` (`Mid→A/Side→B`) filter routing, `Flt->Sat` order swap, `Env Follower` + 4 `LFO MTX` Env destinations, `4x4` self-clocked pump test pulse, `Delay Color` (Dub Tape ↔ Shimmer), `SCOPE` real-time visualizer (`F6.2`), and expanded factory presets cleanly through `PSY:`, `L/R:`, `M/S:`, `PAD:`, `SYN:`, `SWV:`, `VOX:` to **`168` total programs**. |
| **Phase 5: Organic Art, Multi-Theme SCOPE, Gate Playhead, Manuals & Publishing** | `8490baa` → `HEAD` | Upgraded `MAIN` orbs to `22`-frame cyber-mecha concave-recessed strips; replaced geometric backdrops with pure domain-warped FBM/Bayer organic surfaces across 8 tabs; wrote canonical `doc/manual_spec.md` & `Mushin - User Guide.md`; added Live 4-Theme `SCOPE` Switcher (**NEON/SOLAR/CYBER/VOID** via `IndexedEnabling` + preset persistence fix for `Param 134`); added 17-frame `GATE` live step playhead (`Param 135`); updated `plugins/Mushin/site/` & published site + build `.zip` to `origin/gh-pages`. |

---

## 2. Hard-Won MPC Hardware & Host Contract Lessons (`192.168.87.37`)

These constraints were uncovered directly during iterative hardware tests on an **Akai MPC Live Mk II** (`MPC OS 3.10.1.12`, 32-bit ARMv7) and verified in `host_sim`:

1. **Preserve Duplicate JSON Keys in `TUI.json`:**
   - Standard `json.load()` / `json.dump()` silently drops duplicate keys (`"version"`) required by MPC skin overlay definitions, causing broken/black tabs on hardware. Always manipulate `TUI.json` via `tools/gen_ui.py` (`gen_ui.Obj` + `gen_ui.dump_json`).
2. **Wait Before Notifying Read-Only Telemetry (`blocksSinceReset_ > 4`):**
   - Calling `set()`/`notifyHostParameterChanged()` for meters or visualizers immediately inside `reset()` or block `0` corrupts `effGetChunk` → `effSetChunk` round-trip verification (`host_sim` Check 3) because chunk reload expects clean settled state first. Gating telemetry after `blocksSinceReset_ > 4` keeps both `host_sim` and MPC meters deterministic.
3. **Strict `FilmStrip` Height Limit (`<= 4096 px`):**
   - Embedded OpenGL ES max texture height on 32-bit MPC units caps vertical strips at `4096 px`. Keep dimensions compact:
     - `MAIN` 22-frame orbs: `180 × 3960`
     - `SCOPE` 20-frame cores: `192 × 3840`
     - `SCOPE` 24-frame tunnels: `240 × 2880`
     - `GATE` 17-frame playhead: `880 × 1462`
4. **Never Let Full-Span Overlays Steal Touch Events:**
   - An overlay widget positioned *above* interactive `Button`s can intercept touch taps. Place non-interactive status strips (like `gate_playhead`) **before/behind** `Button` entries with `"acceptsHWFocus": "No"`, letting outer glow/chaser bars peek cleanly around button edges while keeping tap accuracy 100% intact.
5. **Static `Text` Labels Need `"handleName": "Invalid"`:**
   - MPC OS hides `Label` widgets of `"type": "Text"` whose `"handleName"` is `"Data"` without a valid bound parameter. Set `"handleName": "Invalid"` and `"showWhenDataModelInvalid": "Show"`.
6. **Exclude UI-Only Preferences From Factory Presets:**
   - Appending `kVizTheme` (`134`) as an automatable parameter keeps `host_sim` chunk save/restore working across projects, while stopping `addPreset()` and program-change host notifications at `< kVizTheme` ensures flipping between all `168` factory presets never alters the active visualizer theme.
7. **Instant Event Notification vs. `~30 Hz` Smooth Polling:**
   - Smoothly sweeping metrics (`SCOPE`, `IN/DUCK`, `LIMITER GR`, `Decay Viz`) should notify at `~30 Hz` (`sr / 30.0`) with deadbands (`0.005f–0.008f`) to avoid flooding MPC's `audioMasterAutomate` queue. Conversely, discrete step indicators (`Gate Step` `135`) only change `~4–12` times/sec and benefit from immediate notification upon step change (<`3 ms` transition accuracy).
8. **Parameter Index Permanence:**
   - Always **append** new parameters at the end of `enum Params` (`115..125`, `126..133`, `134`, `135`) so saved MPC projects and existing `TUI.json` / `Q-Links.json` handle bindings never shift.
9. **Same-Track Multi-Instance `.xpl` Preset Loading Quirk (`MPC OS 3.10.1.12`):**
   - Verified in C++ (`VSTPluginMain` with two simultaneous `AEffect` instances) that `Mushin` + `mpc_plugin.h` keep all parameter storage, programs, and chunks completely isolated per instance.
   - On hardware, **built-in programs** (`effSetProgram`) target only the active insert slot cleanly, even when two instances sit on two slots of the same track. However, **loading a `.xpl` preset file from disk** causes MPC OS's browser loader (matching `<PLUGIN uid="..."/>` without an insert slot index) to apply the preset across all matching instances on that track.
   - *Workarounds:* Use built-in presets + per-slot edits for same-track chains, or route the second instance on a separate Submix / Audio Track when loading `.xpl` files from disk.

---

## 3. DSP Gain-Staging & Musical Calibration Lessons

- **Pre-Limiter Pump Integrity:** Early pumping was crushed when post-limiter drive ran too hot; staging `Auto In` (`-20..0 dBFS`) and calibration trims preserved dynamic pump depth without hitting brickwall ceilings prematurely.
- **Reversible Macros (`baseValues_[]`):** Storing un-modulated user values separates performance macro sweeps (`EXHAUSTION`..`SMASH`) from preset structure, letting performers throw extremes live and snap cleanly back to patch zero.
- **ARMv7 CPU Budget:** Updating filter/nonlinear coefficients on **16-sample sub-blocks** (`pow`, `exp`, `tanh`/trigonometric calls hoisted out of inner sample loops) keeps desktop CPU around `~0.16%` per block and preserves comfortable multi-instance headroom on 32-bit `1.6 GHz` MPC hardware.
